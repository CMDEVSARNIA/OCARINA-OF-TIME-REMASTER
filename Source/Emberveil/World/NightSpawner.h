#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NightSpawner.generated.h"

class AOverworldBuilder;
class AEnvironmentController;
class AEmberSentinel;

/** After dark, embers rise from the Shallows as sentinels around the player. They crumble at dawn. */
UCLASS()
class EMBERVEIL_API ANightSpawner : public AActor
{
	GENERATED_BODY()

public:
	ANightSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	int32 MaxAlive = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	float SpawnInterval = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	float MinDistance = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	float MaxDistance = 3200.f;

	void Init(AOverworldBuilder* InOverworld, AEnvironmentController* InEnvironment);

protected:
	virtual void BeginPlay() override;

private:
	void Step();

	TWeakObjectPtr<AOverworldBuilder> Overworld;
	TWeakObjectPtr<AEnvironmentController> Environment;
	TArray<TWeakObjectPtr<AEmberSentinel>> Spawned;
	FRandomStream Rand;
	FTimerHandle Timer;
	bool bWasNight = false;
};
