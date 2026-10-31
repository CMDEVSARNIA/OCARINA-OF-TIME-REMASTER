#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OverworldBuilder.generated.h"

class AEnvironmentController;
class ATestDungeonBuilder;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Generates the Emberveil archipelago at runtime: a heightfield island split into chunked
 * procedural meshes, coloured per biome, with causeway roads, landmarks, enemies, tide shrines,
 * the day/night + tide controller, and a portal down into the Sunken Vault (the test dungeon).
 *
 * Regions (all original):
 *   The Shallows      - central tidal plain and hub, the Waystone Circle at its heart
 *   Lanternhold       - starting village on a western cliff plateau, with the lantern tower
 *   Drowned Spires    - northern lagoon, only crossable at low tide; holds the vault entrance
 *   Cinder Rise       - southern volcano with a lava crater and basalt columns
 *   Mangrove Lattice  - eastern flooded forest
 *   Saltglass Dunes   - north-western salt dunes studded with glass shards
 */
UCLASS()
class EMBERVEIL_API AOverworldBuilder : public AActor
{
	GENERATED_BODY()

public:
	AOverworldBuilder();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overworld")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overworld", meta = (ClampMin = "2", ClampMax = "32"))
	int32 ChunksPerSide = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overworld", meta = (ClampMin = "4", ClampMax = "128"))
	int32 QuadsPerChunk = 40;

	/** Terrain grid spacing in cm. World width = ChunksPerSide * QuadsPerChunk * QuadSize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overworld", meta = (ClampMin = "100"))
	float QuadSize = 800.f;

	/** Flip if the terrain renders inside-out on your engine version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overworld")
	bool bFlipTerrainWinding = false;

	/** Builds everything (idempotent). Called automatically in BeginPlay. */
	void Build();

	UFUNCTION(BlueprintPure, Category = "Overworld")
	bool IsBuilt() const { return bBuilt; }

	/** World-space ground height below/above a world location (analytic, works before collision exists). */
	UFUNCTION(BlueprintPure, Category = "Overworld")
	float GetTerrainHeight(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Overworld")
	FString GetRegionNameAt(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Overworld")
	bool IsInHub(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Overworld")
	FTransform GetPlayerStartTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Overworld")
	void MovePlayersToStart();

	float GetHalfSize() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FRoadPoint
	{
		FVector2D P;
		float Z = 0.f;
	};

	struct FTerrainSample
	{
		float Height = 0.f;
		uint8 Biome = 0;
	};

	// Terrain maths (local space, cm).
	float HeightNoRoads(const FVector2D& P, int32* OutDominantRegion) const;
	FTerrainSample SampleLocal(const FVector2D& P) const;
	float DistanceToRoads(const FVector2D& P) const;
	int32 FindRegionIndex(const FVector2D& P) const;
	FVector2D Frac(float X, float Y) const { return FVector2D(X, Y) * GetHalfSize(); }
	FVector2D RegionCenter(int32 RegionIndex) const;
	float RegionRadius(int32 RegionIndex) const;
	FVector GroundLocal(const FVector2D& P, float ZOffset = 0.f) const;
	FVector ToWorld(const FVector& Local) const;

	// Build steps.
	void PrepareRoads();
	void BuildTerrain();
	void BuildHub(FRandomStream& Rand);
	void BuildVillage(FRandomStream& Rand);
	void BuildLagoon(FRandomStream& Rand);
	void BuildVolcano(FRandomStream& Rand);
	void BuildMangroves(FRandomStream& Rand);
	void BuildDunes(FRandomStream& Rand);
	void BuildDungeonLink();
	void SpawnDaytimeEnemies();
	void UpdateRegionTitle();

	// Helpers.
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FVector& LocalLocation, const FRotator& LocalRotation, const FVector& Scale, const FLinearColor& Color, bool bCollision);
	UInstancedStaticMeshComponent* AddInstancedMesh(UStaticMesh* Mesh, const FLinearColor& Color, bool bCollision);
	UPointLightComponent* AddLight(const FVector& LocalLocation, const FLinearColor& Color, float Intensity, float Radius);
	template <class T> T* SpawnAtLocal(const FVector& LocalLocation, const FRotator& LocalRotation = FRotator::ZeroRotator);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BiomeMaterials;

	UPROPERTY(Transient)
	TObjectPtr<AEnvironmentController> Environment;

	UPROPERTY(Transient)
	TObjectPtr<ATestDungeonBuilder> Dungeon;

	TArray<TArray<FRoadPoint>> Roads;
	FVector2D NoiseOffset = FVector2D::ZeroVector;
	FString CurrentRegion;
	FTimerHandle RegionTimer;
	bool bBuilt = false;
};
