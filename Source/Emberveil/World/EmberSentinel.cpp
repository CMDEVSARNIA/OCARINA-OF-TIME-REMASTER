#include "World/EmberSentinel.h"

#include "EmberveilStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/LockOnTargetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "World/KeyPickup.h"

namespace
{
	const FLinearColor SentinelBodyColor(0.12f, 0.12f, 0.14f);
	const FLinearColor SentinelEyeIdle(1.f, 0.55f, 0.1f);
	const FLinearColor SentinelEyeWindup(1.f, 0.05f, 0.02f);
}

AEmberSentinel::AEmberSentinel()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(45.f, 80.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Capsule;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Capsule);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.6f));
	if (Cone.Succeeded()) { BodyMesh->SetStaticMesh(Cone.Object); }

	EyeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EyeMesh"));
	EyeMesh->SetupAttachment(Capsule);
	EyeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EyeMesh->SetRelativeLocation(FVector(30.f, 0.f, 35.f));
	EyeMesh->SetRelativeScale3D(FVector(0.25f));
	if (Sphere.Succeeded()) { EyeMesh->SetStaticMesh(Sphere.Object); }

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 30.f;
	Health->InvulnerabilityTime = 0.15f;

	LockOnPoint = CreateDefaultSubobject<ULockOnTargetComponent>(TEXT("LockOnPoint"));
	LockOnPoint->SetupAttachment(Capsule);
	LockOnPoint->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
}

void AEmberSentinel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (UStaticMesh* Art = UEmberveilStatics::LoadArtMesh(TEXT("SM_Sentinel")))
	{
		// Art pivot is the base; the capsule centre is 80 cm up. Eye socket lines up with EyeMesh.
		bUsingArt = true;
		BodyMesh->SetStaticMesh(Art);
		BodyMesh->SetRelativeTransform(FTransform(FVector(0.f, 0.f, -80.f)));
	}
	else
	{
		UEmberveilStatics::TintMesh(BodyMesh, SentinelBodyColor);
	}
	SetEyeColor(SentinelEyeIdle);
}

void AEmberSentinel::BeginPlay()
{
	Super::BeginPlay();
	Health->OnHealthChanged.AddDynamic(this, &AEmberSentinel::HandleHealthChanged);
	Health->OnDeath.AddDynamic(this, &AEmberSentinel::HandleDeath);
}

void AEmberSentinel::SetEyeColor(const FLinearColor& Color)
{
	UEmberveilStatics::TintMesh(EyeMesh, Color);
}

void AEmberSentinel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Health->IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Hit flash.
	if (bBodyFlashing && Now >= FlashUntil)
	{
		bBodyFlashing = false;
		if (bUsingArt)
		{
			SetEyeColor(State == ESentinelState::Windup ? SentinelEyeWindup : SentinelEyeIdle);
		}
		else
		{
			UEmberveilStatics::TintMesh(BodyMesh, SentinelBodyColor);
		}
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}
	if (const UHealthComponent* PlayerHealth = Player->FindComponentByClass<UHealthComponent>())
	{
		if (PlayerHealth->IsDead())
		{
			return;
		}
	}

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.f;
	const float Distance = ToPlayer.Size();
	const FRotator FacePlayer = ToPlayer.Rotation();

	switch (State)
	{
	case ESentinelState::Stunned:
		if (Now >= StateEndTime) { State = ESentinelState::Idle; }
		return;
	case ESentinelState::Windup:
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FacePlayer, DeltaSeconds, 3.f));
		if (Now >= StateEndTime) { ResolveAttack(Now, Player); }
		return;
	case ESentinelState::Recover:
		if (Now >= StateEndTime) { State = ESentinelState::Idle; }
		return;
	default:
		break;
	}

	if (Distance > AggroRadius)
	{
		return;
	}

	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FacePlayer, DeltaSeconds, 6.f));

	if (Distance <= AttackRange && Now >= NextAttackTime)
	{
		BeginWindup(Now);
		return;
	}

	if (Distance > AttackRange * 0.8f)
	{
		AddActorWorldOffset(ToPlayer.GetSafeNormal() * MoveSpeed * DeltaSeconds, true);
	}
}

void AEmberSentinel::BeginWindup(float Now)
{
	State = ESentinelState::Windup;
	StateEndTime = Now + WindupTime;
	SetEyeColor(SentinelEyeWindup);
}

void AEmberSentinel::ResolveAttack(float Now, APawn* Player)
{
	State = ESentinelState::Recover;
	StateEndTime = Now + 0.5f;
	NextAttackTime = Now + AttackCooldown;
	SetEyeColor(SentinelEyeIdle);

	FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.f;
	const FVector Dir = ToPlayer.GetSafeNormal();
	const bool bInRange = ToPlayer.Size() <= AttackRange + 40.f;
	const bool bInFront = FVector::DotProduct(GetActorForwardVector(), Dir) > 0.3f;

	if (bInRange && bInFront)
	{
		UGameplayStatics::ApplyDamage(Player, AttackDamage, nullptr, this, UDamageType::StaticClass());
		if (ACharacter* Character = Cast<ACharacter>(Player))
		{
			Character->LaunchCharacter(Dir * KnockbackStrength + FVector(0.f, 0.f, 250.f), true, true);
		}
	}
}

void AEmberSentinel::HandleHealthChanged(float NewHealth, float Delta)
{
	if (Delta >= 0.f || Health->IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	State = ESentinelState::Stunned;
	StateEndTime = Now + HitStunTime;
	SetEyeColor(SentinelEyeIdle);

	bBodyFlashing = true;
	FlashUntil = Now + 0.1f;
	if (bUsingArt)
	{
		SetEyeColor(FLinearColor::White);
	}
	else
	{
		UEmberveilStatics::TintMesh(BodyMesh, FLinearColor::White);
	}

	// Small shove away from the player so hits feel weighty.
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		FVector Away = GetActorLocation() - Player->GetActorLocation();
		Away.Z = 0.f;
		AddActorWorldOffset(Away.GetSafeNormal() * 60.f, true);
	}
}

void AEmberSentinel::HandleDeath(AActor* Killer)
{
	if (bDropsKey)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AKeyPickup>(GetActorLocation() - FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, Params);
		UEmberveilStatics::ShowMessage(this, TEXT("The sentinel crumbles... something glints on the floor."));
	}
	else
	{
		UEmberveilStatics::ShowMessage(this, TEXT("The sentinel crumbles."));
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.1f);
}
