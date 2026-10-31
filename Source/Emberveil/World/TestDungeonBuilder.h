#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestDungeonBuilder.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Builds a four-room greybox test dungeon at runtime from engine basic shapes, so the prototype
 * is playable without any authored level assets:
 *
 *   [Start] -> [Sentinels, one drops a key] -> locked door -> [Melody gate room] -> gate -> [Relic]
 *
 * Local X runs through the rooms. Room size 20 m x 20 m, walls 5 m high.
 */
UCLASS()
class EMBERVEIL_API ATestDungeonBuilder : public AActor
{
	GENERATED_BODY()

public:
	ATestDungeonBuilder();

	/** Adds directional + fill light if the level has no directional light (e.g. an empty level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon")
	bool bSpawnLightingIfMissing = true;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	FTransform GetPlayerStartTransform() const;

	/** Teleports every player pawn to the dungeon entrance. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void MovePlayersToStart();

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	UStaticMeshComponent* AddBlock(const FVector& LocalCenter, const FVector& Size, const FLinearColor& Color);
	void AddDividerWall(float X);
	void AddBrazier(const FVector& LocalLocation);
	void AddLighting();
	template <class T> T* SpawnLocal(const FVector& LocalLocation, const FRotator& LocalRotation = FRotator::ZeroRotator);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	bool bBuilt = false;
};
