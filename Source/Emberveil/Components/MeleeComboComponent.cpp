#include "Components/MeleeComboComponent.h"

#include "Components/HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UMeleeComboComponent::UMeleeComboComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ComboDamage = { 10.f, 12.f, 20.f };
}

void UMeleeComboComponent::RequestAttack()
{
	if (ComboDamage.Num() == 0)
	{
		return;
	}

	if (const UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>())
	{
		if (Health->IsDead())
		{
			return;
		}
	}

	const int32 LastIndex = ComboDamage.Num() - 1;
	if (bSwinging)
	{
		bQueued = ComboIndex < LastIndex;
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bChain = (Now - LastSwingEndTime) <= ComboWindow && ComboIndex >= 0 && ComboIndex < LastIndex;
	StartSwing(bChain ? ComboIndex + 1 : 0);
}

float UMeleeComboComponent::GetSwingAlpha() const
{
	if (!bSwinging || SwingDuration <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp((GetWorld()->GetTimeSeconds() - SwingStartTime) / SwingDuration, 0.f, 1.f);
}

void UMeleeComboComponent::StartSwing(int32 Index)
{
	bSwinging = true;
	bQueued = false;
	ComboIndex = Index;
	SwingStartTime = GetWorld()->GetTimeSeconds();

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.SetTimer(HitTimer, this, &UMeleeComboComponent::PerformHit, FMath::Max(HitDelay, KINDA_SMALL_NUMBER), false);
	Timers.SetTimer(EndTimer, this, &UMeleeComboComponent::FinishSwing, FMath::Max(SwingDuration, KINDA_SMALL_NUMBER), false);

	OnSwingStarted.Broadcast(ComboIndex);
}

void UMeleeComboComponent::PerformHit()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	const FVector Center = Owner->GetActorLocation() + Owner->GetActorForwardVector() * Reach;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeHit), false, Owner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(HitRadius), QueryParams);

	const APawn* OwnerPawn = Cast<APawn>(Owner);
	AController* Instigator = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	const float Damage = ComboDamage.IsValidIndex(ComboIndex) ? ComboDamage[ComboIndex] : 0.f;

	TSet<AActor*> AlreadyHit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (!IsValid(Victim) || Victim == Owner || AlreadyHit.Contains(Victim))
		{
			continue;
		}
		AlreadyHit.Add(Victim);
		UGameplayStatics::ApplyDamage(Victim, Damage, Instigator, Owner, UDamageType::StaticClass());
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(World, Center, HitRadius, 12, AlreadyHit.Num() > 0 ? FColor::Red : FColor::White, false, 0.2f);
	}
}

void UMeleeComboComponent::FinishSwing()
{
	bSwinging = false;
	LastSwingEndTime = GetWorld()->GetTimeSeconds();

	if (bQueued && ComboIndex < ComboDamage.Num() - 1)
	{
		StartSwing(ComboIndex + 1);
	}
	bQueued = false;
}
