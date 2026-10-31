#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EmberveilGameMode.generated.h"

class ATestDungeonBuilder;

UCLASS()
class EMBERVEIL_API AEmberveilGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEmberveilGameMode();

	/** Spawn the greybox test dungeon if the level doesn't already contain one. Turn off for real levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emberveil")
	bool bAutoBuildTestDungeon = true;

	/** Raised off the origin so it doesn't z-fight with a template level's floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Emberveil")
	FVector TestDungeonLocation = FVector(0.f, 0.f, 2000.f);

	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	UPROPERTY()
	TObjectPtr<ATestDungeonBuilder> Dungeon;
};
