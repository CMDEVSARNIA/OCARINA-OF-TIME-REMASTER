#include "World/NightSpawner.h"

#include "EmberveilStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/EmberSentinel.h"
#include "World/EnvironmentController.h"
#include "World/OverworldBuilder.h"

ANightSpawner::ANightSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANightSpawner::Init(AOverworldBuilder* InOverworld, AEnvironmentController* InEnvironment)
{
	Overworld = InOverworld;
	Environment = InEnvironment;
}

void ANightSpawner::BeginPlay()
{
	Super::BeginPlay();
	Rand.Initialize(FPlatformTime::Cycles());
	GetWorldTimerManager().SetTimer(Timer, this, &ANightSpawner::Step, FMath::Max(0.5f, SpawnInterval), true, SpawnInterval);
}

void ANightSpawner::Step()
{
	AOverworldBuilder* Island = Overworld.Get();
	AEnvironmentController* Env = Environment.Get();
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Island || !Env || !Player)
	{
		return;
	}

	const bool bNight = Env->IsNight();
	if (bNight != bWasNight)
	{
		bWasNight = bNight;
		if (bNight)
		{
			UEmberveilStatics::ShowMessage(this, TEXT("Night falls. Embers stir on the Shallows..."), 4.f);
		}
		else
		{
			for (const TWeakObjectPtr<AEmberSentinel>& Sentinel : Spawned)
			{
				if (Sentinel.IsValid())
				{
					Sentinel->SetLifeSpan(0.1f);
				}
			}
			Spawned.Reset();
			UEmberveilStatics::ShowMessage(this, TEXT("Dawn breaks. The embers sink back into the earth."), 4.f);
		}
	}

	Spawned.RemoveAll([](const TWeakObjectPtr<AEmberSentinel>& Sentinel) { return !Sentinel.IsValid(); });

	if (!bNight || Spawned.Num() >= MaxAlive || !Island->IsInHub(Player->GetActorLocation()))
	{
		return;
	}

	const float Angle = Rand.FRandRange(0.f, 2.f * PI);
	const FVector Offset(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
	FVector Location = Player->GetActorLocation() + Offset * Rand.FRandRange(MinDistance, MaxDistance);
	Location.Z = Island->GetTerrainHeight(Location);

	float Water = 0.f;
	if (Env->GetWaterLevelAt(Location, Water) && Location.Z < Water + 30.f)
	{
		return; // don't spawn in the sea
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	const FRotator Facing = (-Offset).Rotation();
	if (AEmberSentinel* Sentinel = GetWorld()->SpawnActor<AEmberSentinel>(Location + FVector(0.f, 0.f, 85.f), Facing, Params))
	{
		Sentinel->AggroRadius = 2600.f;
		Spawned.Add(Sentinel);
	}
}
