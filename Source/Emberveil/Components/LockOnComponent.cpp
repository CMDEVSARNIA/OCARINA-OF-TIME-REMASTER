#include "Components/LockOnComponent.h"

#include "Components/HealthComponent.h"
#include "Components/LockOnTargetComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

ULockOnComponent::ULockOnComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

APlayerController* ULockOnComponent::GetPlayerController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool ULockOnComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	if (const APlayerController* PC = GetPlayerController())
	{
		PC->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}
	return false;
}

bool ULockOnComponent::IsTargetUsable(const ULockOnTargetComponent* Target, float MaxDistance) const
{
	if (!IsValid(Target) || !Target->IsRegistered() || !Target->bCanBeTargeted)
	{
		return false;
	}

	AActor* TargetOwner = Target->GetOwner();
	AActor* Owner = GetOwner();
	if (!IsValid(TargetOwner) || TargetOwner == Owner || !Owner)
	{
		return false;
	}

	if (const UHealthComponent* Health = TargetOwner->FindComponentByClass<UHealthComponent>())
	{
		if (Health->IsDead())
		{
			return false;
		}
	}

	const FVector From = Owner->GetActorLocation();
	const FVector To = Target->GetComponentLocation();
	if (FVector::DistSquared(From, To) > FMath::Square(MaxDistance))
	{
		return false;
	}

	// Line of sight from roughly eye height; walls and closed doors break the lock.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LockOnSight), false, Owner);
	Params.AddIgnoredActor(TargetOwner);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From + FVector(0.f, 0.f, 60.f), To, ECC_Visibility, Params);
}

void ULockOnComponent::GatherCandidates(TArray<ULockOnTargetComponent*>& OutCandidates) const
{
	const UWorld* World = GetWorld();
	for (TObjectIterator<ULockOnTargetComponent> It; It; ++It)
	{
		ULockOnTargetComponent* Target = *It;
		if (Target->GetWorld() == World && IsTargetUsable(Target, SearchRadius))
		{
			OutCandidates.Add(Target);
		}
	}
}

float ULockOnComponent::GetViewYawOffset(const ULockOnTargetComponent* Target, const FVector& ViewLocation, const FRotator& ViewRotation) const
{
	const FVector Dir = (Target->GetComponentLocation() - ViewLocation).GetSafeNormal2D();
	const FRotator YawOnly(0.f, ViewRotation.Yaw, 0.f);
	const FVector Forward = YawOnly.Vector();
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Dir, Right), FVector::DotProduct(Dir, Forward)));
}

ULockOnTargetComponent* ULockOnComponent::FindBestTarget() const
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetView(ViewLocation, ViewRotation))
	{
		return nullptr;
	}

	TArray<ULockOnTargetComponent*> Candidates;
	GatherCandidates(Candidates);

	ULockOnTargetComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	const FVector OwnerLocation = GetOwner()->GetActorLocation();

	for (ULockOnTargetComponent* Candidate : Candidates)
	{
		const float Angle = FMath::Abs(GetViewYawOffset(Candidate, ViewLocation, ViewRotation));
		if (Angle > MaxSearchAngle)
		{
			continue;
		}

		// Prefer targets near the centre of the screen, then nearby ones.
		const float Distance = FVector::Dist(OwnerLocation, Candidate->GetComponentLocation());
		const float Score = Angle / MaxSearchAngle + Distance / SearchRadius;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

void ULockOnComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}

	if (ULockOnTargetComponent* Target = FindBestTarget())
	{
		SetTarget(Target);
	}
	else
	{
		RecenterCamera();
	}
}

void ULockOnComponent::CycleTarget(bool bToRight)
{
	ULockOnTargetComponent* Current = CurrentTarget.Get();
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!IsLockedOn() || !GetView(ViewLocation, ViewRotation))
	{
		return;
	}

	TArray<ULockOnTargetComponent*> Candidates;
	GatherCandidates(Candidates);

	const float CurrentOffset = GetViewYawOffset(Current, ViewLocation, ViewRotation);
	ULockOnTargetComponent* Next = nullptr;
	ULockOnTargetComponent* Wrap = nullptr;
	float NextOffset = bToRight ? TNumericLimits<float>::Max() : -TNumericLimits<float>::Max();
	float WrapOffset = NextOffset;

	for (ULockOnTargetComponent* Candidate : Candidates)
	{
		if (Candidate == Current)
		{
			continue;
		}

		const float Offset = GetViewYawOffset(Candidate, ViewLocation, ViewRotation);
		if (bToRight)
		{
			if (Offset > CurrentOffset && Offset < NextOffset) { NextOffset = Offset; Next = Candidate; }
			if (Offset < WrapOffset) { WrapOffset = Offset; Wrap = Candidate; }
		}
		else
		{
			if (Offset < CurrentOffset && Offset > NextOffset) { NextOffset = Offset; Next = Candidate; }
			if (Offset > WrapOffset) { WrapOffset = Offset; Wrap = Candidate; }
		}
	}

	if (ULockOnTargetComponent* Chosen = Next ? Next : Wrap)
	{
		SetTarget(Chosen);
	}
}

void ULockOnComponent::ClearLockOn()
{
	SetTarget(nullptr);
}

void ULockOnComponent::RecenterCamera()
{
	if (APlayerController* PC = GetPlayerController())
	{
		FRotator Rotation = PC->GetControlRotation();
		Rotation.Yaw = GetOwner()->GetActorRotation().Yaw;
		Rotation.Pitch = CameraPitchOffset;
		PC->SetControlRotation(Rotation);
	}
}

void ULockOnComponent::SetTarget(ULockOnTargetComponent* NewTarget)
{
	const bool bWasLocked = bIsLocked;
	if ((NewTarget && bWasLocked && CurrentTarget.Get() == NewTarget) || (!NewTarget && !bWasLocked))
	{
		return;
	}

	CurrentTarget = NewTarget;
	bIsLocked = NewTarget != nullptr;

	// While locked the character faces the controller (i.e. the target) and strafes.
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (bIsLocked && !bWasLocked)
		{
			bSavedOrientToMovement = Movement->bOrientRotationToMovement;
			bSavedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
			Movement->bOrientRotationToMovement = false;
			Movement->bUseControllerDesiredRotation = true;
		}
		else if (!bIsLocked && bWasLocked)
		{
			Movement->bOrientRotationToMovement = bSavedOrientToMovement;
			Movement->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		}
	}

	OnTargetChanged.Broadcast(NewTarget);
}

void ULockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsLocked)
	{
		return;
	}

	ULockOnTargetComponent* Target = CurrentTarget.Get();
	if (!IsTargetUsable(Target, BreakDistance))
	{
		// Target died or vanished: hop to the next one in view, otherwise release.
		ULockOnTargetComponent* Next = FindBestTarget();
		if (Next && Next != Target)
		{
			SetTarget(Next);
		}
		else
		{
			ClearLockOn();
		}
		return;
	}

	if (APlayerController* PC = GetPlayerController())
	{
		FRotator Desired = (Target->GetComponentLocation() - GetOwner()->GetActorLocation()).Rotation();
		Desired.Pitch = FMath::Clamp(Desired.Pitch + CameraPitchOffset, -50.f, 30.f);
		Desired.Roll = 0.f;
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaTime, CameraInterpSpeed));
	}
}
