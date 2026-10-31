#include "World/OverworldBuilder.h"

#include "Emberveil.h"
#include "EmberveilStatics.h"
#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "World/EmberSentinel.h"
#include "World/EnvironmentController.h"
#include "World/NightSpawner.h"
#include "World/TestDungeonBuilder.h"
#include "World/TideShrine.h"
#include "World/WarpPortal.h"

namespace Overworld
{
	enum EBiome : uint8 { Grass, Village, Sand, Dunes, Mangrove, Ash, Rock, Road, Lagoon, BiomeCount };

	const FLinearColor BiomeColors[BiomeCount] =
	{
		FLinearColor(0.26f, 0.40f, 0.22f), // Grass   - salt grass of the Shallows
		FLinearColor(0.33f, 0.44f, 0.20f), // Village - kept meadow
		FLinearColor(0.60f, 0.54f, 0.40f), // Sand    - beaches and seabed
		FLinearColor(0.86f, 0.84f, 0.76f), // Dunes   - salt-white
		FLinearColor(0.18f, 0.24f, 0.15f), // Mangrove mud
		FLinearColor(0.13f, 0.11f, 0.10f), // Ash
		FLinearColor(0.38f, 0.36f, 0.34f), // Rock    - steep slopes
		FLinearColor(0.48f, 0.43f, 0.36f), // Road    - old causeway stone
		FLinearColor(0.34f, 0.40f, 0.38f), // Lagoon floor
	};

	enum ERegion : int32 { RVillage, RLagoon, RVolcano, RMangrove, RDunes, RegionCount };

	struct FRegion
	{
		const TCHAR* Name;
		const TCHAR* Subtitle;
		float CX;
		float CY;
		float Radius;
		EBiome Biome;
	};

	// Positions and radii are fractions of the island half-size.
	const FRegion Regions[RegionCount] =
	{
		{ TEXT("Lanternhold"),      TEXT("The last lit village"),                      -0.60f,  0.05f, 0.20f, Village  },
		{ TEXT("Drowned Spires"),   TEXT("Cross when the tide runs low"),               0.05f,  0.62f, 0.25f, Lagoon   },
		{ TEXT("Cinder Rise"),      TEXT("The mountain that never cooled"),             0.35f, -0.60f, 0.28f, Ash      },
		{ TEXT("Mangrove Lattice"), TEXT("Roots over black water"),                     0.65f,  0.15f, 0.22f, Mangrove },
		{ TEXT("Saltglass Dunes"),  TEXT("Where the old sea dried to glass"),          -0.55f,  0.55f, 0.25f, Dunes    },
	};

	constexpr float HubRadius = 0.45f;
	constexpr float RoadHalfWidth = 450.f;
	constexpr float RoadBlend = 1400.f;
	constexpr float NaturalZ = -1.e9f;
	constexpr int32 RegionHub = -1;
	constexpr int32 RegionSea = -2;

	float Fbm(FVector2D P, int32 Octaves = 3)
	{
		float Sum = 0.f;
		float Amplitude = 1.f;
		float Norm = 0.f;
		for (int32 i = 0; i < Octaves; ++i)
		{
			Sum += Amplitude * FMath::PerlinNoise2D(P);
			Norm += Amplitude;
			Amplitude *= 0.5f;
			P *= 2.03f;
		}
		return Sum / Norm;
	}

	/** 1 inside 60% of the radius, fading to 0 at the radius. */
	float InsideWeight(float Distance, float Radius)
	{
		return 1.f - FMath::SmoothStep(0.6f * Radius, Radius, Distance);
	}
}

template <class T>
T* AOverworldBuilder::SpawnAtLocal(const FVector& LocalLocation, const FRotator& LocalRotation)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator WorldRotation = (GetActorRotation().Quaternion() * LocalRotation.Quaternion()).Rotator();
	return GetWorld()->SpawnActor<T>(ToWorld(LocalLocation), WorldRotation, Params);
}

AOverworldBuilder::AOverworldBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
	SphereMesh = Sphere.Object;
	BaseMaterial = Material.Object;
}

float AOverworldBuilder::GetHalfSize() const
{
	return ChunksPerSide * QuadsPerChunk * QuadSize * 0.5f;
}

FVector2D AOverworldBuilder::RegionCenter(int32 RegionIndex) const
{
	return Frac(Overworld::Regions[RegionIndex].CX, Overworld::Regions[RegionIndex].CY);
}

float AOverworldBuilder::RegionRadius(int32 RegionIndex) const
{
	return Overworld::Regions[RegionIndex].Radius * GetHalfSize();
}

FVector AOverworldBuilder::ToWorld(const FVector& Local) const
{
	return GetActorTransform().TransformPosition(Local);
}

FVector AOverworldBuilder::GroundLocal(const FVector2D& P, float ZOffset) const
{
	return FVector(P.X, P.Y, SampleLocal(P).Height + ZOffset);
}

// ---------------------------------------------------------------------------------------------
// Terrain maths
// ---------------------------------------------------------------------------------------------

float AOverworldBuilder::HeightNoRoads(const FVector2D& P, int32* OutDominantRegion) const
{
	using namespace Overworld;
	const float H = GetHalfSize();
	const FVector2D N = P + NoiseOffset;

	// The Shallows: low rolling plain, mostly above high tide.
	float Z = 350.f + 250.f * Fbm(N / 60000.f) + 80.f * Fbm(N / 12000.f + FVector2D(17.f, 3.f));

	// Coastline falls away to deep water.
	const float R = static_cast<float>(P.Size()) / H;
	Z = FMath::Lerp(Z, -900.f, FMath::SmoothStep(0.8f, 0.95f, R));

	int32 Dominant = INDEX_NONE;
	float DominantWeight = 0.f;

	for (int32 i = 0; i < RegionCount; ++i)
	{
		const FVector2D Center = RegionCenter(i);
		const float Radius = RegionRadius(i);
		const float D = static_cast<float>(FVector2D::Distance(P, Center));
		const float W = InsideWeight(D, Radius);
		if (W <= 0.f)
		{
			continue;
		}

		float RZ = 0.f;
		switch (i)
		{
		case RVillage:
			RZ = 950.f + 40.f * Fbm(N / 8000.f);
			break;

		case RLagoon:
		{
			RZ = -150.f + 60.f * Fbm(N / 9000.f);
			const float IslandRadius = 0.07f * H;
			const float IslandWeight = 1.f - FMath::SmoothStep(0.6f * IslandRadius, IslandRadius, D);
			RZ = FMath::Lerp(RZ, 300.f + 30.f * Fbm(N / 5000.f), IslandWeight);
			break;
		}

		case RVolcano:
		{
			const float T = D / Radius;
			float Cone = 9000.f * FMath::Pow(FMath::Max(0.f, 1.f - T), 1.4f);
			if (T < 0.18f)
			{
				Cone -= 3500.f * FMath::Square(1.f - T / 0.18f); // crater bowl
			}
			RZ = 300.f + Cone + 60.f * Fbm(N / 7000.f);
			break;
		}

		case RMangrove:
			RZ = 120.f + 160.f * Fbm(N / 8000.f);
			break;

		case RDunes:
			RZ = 550.f + 220.f * FMath::Sin((P.X * 0.6f + P.Y * 0.8f) / 4500.f + 3.f * Fbm(N / 25000.f)) + 80.f * Fbm(N / 6000.f);
			break;

		default:
			break;
		}

		Z = FMath::Lerp(Z, RZ, W);
		if (W > DominantWeight)
		{
			DominantWeight = W;
			Dominant = i;
		}
	}

	// Hard edge of the playable island.
	Z = FMath::Lerp(Z, -900.f, FMath::SmoothStep(0.95f, 1.02f, R));

	if (OutDominantRegion)
	{
		*OutDominantRegion = DominantWeight > 0.5f ? Dominant : INDEX_NONE;
	}
	return Z;
}

AOverworldBuilder::FTerrainSample AOverworldBuilder::SampleLocal(const FVector2D& P) const
{
	using namespace Overworld;

	int32 Dominant = INDEX_NONE;
	float Z = HeightNoRoads(P, &Dominant);

	float BestWeight = 0.f;
	float RoadZ = 0.f;
	float MinDistance = TNumericLimits<float>::Max();
	for (const TArray<FRoadPoint>& Road : Roads)
	{
		for (int32 i = 0; i + 1 < Road.Num(); ++i)
		{
			const FRoadPoint& A = Road[i];
			const FRoadPoint& B = Road[i + 1];
			const FVector2D AB = B.P - A.P;
			const float LenSq = FMath::Max(static_cast<float>(AB.SizeSquared()), 1.f);
			const float T = FMath::Clamp(static_cast<float>((P - A.P) | AB) / LenSq, 0.f, 1.f);
			const float D = static_cast<float>(FVector2D::Distance(P, A.P + AB * T));
			MinDistance = FMath::Min(MinDistance, D);

			const float W = 1.f - FMath::SmoothStep(RoadHalfWidth, RoadHalfWidth + RoadBlend, D);
			if (W > BestWeight)
			{
				BestWeight = W;
				RoadZ = FMath::Lerp(A.Z, B.Z, T);
			}
		}
	}
	Z = FMath::Lerp(Z, RoadZ, BestWeight);

	FTerrainSample Sample;
	Sample.Height = Z;
	if (MinDistance < RoadHalfWidth)
	{
		Sample.Biome = Road;
	}
	else if (Dominant == RLagoon)
	{
		Sample.Biome = Z < 150.f ? Lagoon : Grass;
	}
	else if (Z < 60.f)
	{
		Sample.Biome = Sand;
	}
	else if (Dominant != INDEX_NONE)
	{
		Sample.Biome = Regions[Dominant].Biome;
	}
	else
	{
		Sample.Biome = Grass;
	}
	return Sample;
}

float AOverworldBuilder::DistanceToRoads(const FVector2D& P) const
{
	float MinDistance = TNumericLimits<float>::Max();
	for (const TArray<FRoadPoint>& Road : Roads)
	{
		for (int32 i = 0; i + 1 < Road.Num(); ++i)
		{
			const FVector2D AB = Road[i + 1].P - Road[i].P;
			const float LenSq = FMath::Max(static_cast<float>(AB.SizeSquared()), 1.f);
			const float T = FMath::Clamp(static_cast<float>((P - Road[i].P) | AB) / LenSq, 0.f, 1.f);
			MinDistance = FMath::Min(MinDistance, static_cast<float>(FVector2D::Distance(P, Road[i].P + AB * T)));
		}
	}
	return MinDistance;
}

void AOverworldBuilder::PrepareRoads()
{
	using namespace Overworld;
	constexpr float Nat = NaturalZ;

	// Waypoints as (x fraction, y fraction, fixed height or NaturalZ to follow the land).
	const TArray<TArray<FVector>> Defs =
	{
		// Shallows -> Lanternhold (ramp cut up the plateau cliff)
		{ FVector(0.f, 0.f, Nat), FVector(-0.25f, 0.03f, Nat), FVector(-0.42f, 0.05f, Nat), FVector(-0.56f, 0.05f, Nat) },
		// Tidal causeway -> Drowned Spires island (sits at sea level: dry only at low tide)
		{ FVector(0.01f, 0.05f, Nat), FVector(0.02f, 0.2f, Nat), FVector(0.03f, 0.38f, 0.f), FVector(0.045f, 0.53f, 0.f), FVector(0.05f, 0.6f, Nat) },
		// Shallows -> Cinder Rise crater rim
		{ FVector(0.f, 0.f, Nat), FVector(0.15f, -0.28f, Nat), FVector(0.27f, -0.47f, Nat), FVector(0.33f, -0.55f, Nat) },
		// Shallows -> Mangrove Lattice
		{ FVector(0.f, 0.f, Nat), FVector(0.3f, 0.08f, Nat), FVector(0.55f, 0.14f, Nat) },
		// Lanternhold road -> Saltglass Dunes
		{ FVector(-0.25f, 0.03f, Nat), FVector(-0.4f, 0.33f, Nat), FVector(-0.5f, 0.5f, Nat) },
	};

	Roads.Reset();
	for (const TArray<FVector>& Def : Defs)
	{
		TArray<FRoadPoint>& Road = Roads.AddDefaulted_GetRef();
		for (const FVector& Point : Def)
		{
			FRoadPoint RP;
			RP.P = Frac(static_cast<float>(Point.X), static_cast<float>(Point.Y));
			RP.Z = Point.Z == Nat ? HeightNoRoads(RP.P, nullptr) : static_cast<float>(Point.Z);
			Road.Add(RP);
		}
	}
}

int32 AOverworldBuilder::FindRegionIndex(const FVector2D& P) const
{
	using namespace Overworld;
	for (int32 i = 0; i < RegionCount; ++i)
	{
		if (FVector2D::Distance(P, RegionCenter(i)) < RegionRadius(i) * 0.8f)
		{
			return i;
		}
	}
	return P.Size() < GetHalfSize() * 0.9f ? RegionHub : RegionSea;
}

float AOverworldBuilder::GetTerrainHeight(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return ToWorld(FVector(Local.X, Local.Y, SampleLocal(FVector2D(Local.X, Local.Y)).Height)).Z;
}

FString AOverworldBuilder::GetRegionNameAt(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const int32 Index = FindRegionIndex(FVector2D(Local.X, Local.Y));
	if (Index >= 0)
	{
		return Overworld::Regions[Index].Name;
	}
	return Index == Overworld::RegionHub ? TEXT("The Shallows") : TEXT("Open Sea");
}

bool AOverworldBuilder::IsInHub(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const FVector2D P(Local.X, Local.Y);
	return FindRegionIndex(P) == Overworld::RegionHub && P.Size() < GetHalfSize() * Overworld::HubRadius;
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

void AOverworldBuilder::BeginPlay()
{
	Super::BeginPlay();
	Build();
	MovePlayersToStart();
	GetWorldTimerManager().SetTimer(RegionTimer, this, &AOverworldBuilder::UpdateRegionTitle, 0.5f, true, 1.f);
}

void AOverworldBuilder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RegionTimer);
	Super::EndPlay(EndPlayReason);
}

FTransform AOverworldBuilder::GetPlayerStartTransform() const
{
	// Just east of Lanternhold's square, facing down the road toward the Shallows.
	const FVector2D P = RegionCenter(Overworld::RVillage) + FVector2D(2600.f, 0.f);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), ToWorld(GroundLocal(P, 120.f)));
}

void AOverworldBuilder::MovePlayersToStart()
{
	const FTransform Start = GetPlayerStartTransform();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->TeleportTo(Start.GetLocation(), Start.Rotator());
		}
		PC->SetControlRotation(Start.Rotator());
	}
}

void AOverworldBuilder::Build()
{
	if (bBuilt || !CubeMesh || !CylinderMesh || !SphereMesh || !BaseMaterial)
	{
		return;
	}
	bBuilt = true;

	const double StartTime = FPlatformTime::Seconds();

	FRandomStream Rand(Seed);
	NoiseOffset = FVector2D(Rand.FRandRange(-1.e6f, 1.e6f), Rand.FRandRange(-1.e6f, 1.e6f));

	Environment = AEnvironmentController::Find(this);
	if (!Environment)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Environment = GetWorld()->SpawnActor<AEnvironmentController>(GetActorLocation(), FRotator::ZeroRotator, Params);
	}
	if (Environment)
	{
		Environment->SetWaterHalfExtent(GetHalfSize() * 1.3f);
	}

	LoadArt();
	PrepareRoads();
	BuildTerrain();
	BuildHub(Rand);
	BuildVillage(Rand);
	BuildLagoon(Rand);
	BuildVolcano(Rand);
	BuildMangroves(Rand);
	BuildDunes(Rand);
	BuildDungeonLink();
	SpawnDaytimeEnemies();

	if (ANightSpawner* Spawner = SpawnAtLocal<ANightSpawner>(FVector::ZeroVector))
	{
		Spawner->Init(this, Environment);
	}

	UE_LOG(LogEmberveil, Log, TEXT("Overworld built in %.2f s"), FPlatformTime::Seconds() - StartTime);
}

// ---------------------------------------------------------------------------------------------
// Terrain mesh
// ---------------------------------------------------------------------------------------------

void AOverworldBuilder::BuildTerrain()
{
	using namespace Overworld;

	BiomeMaterials.Reset();
	for (int32 b = 0; b < BiomeCount; ++b)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), BiomeColors[b]);
		BiomeMaterials.Add(MID);
	}

	const int32 QuadsPerSide = ChunksPerSide * QuadsPerChunk;
	const int32 V = QuadsPerSide + 1;
	const float H = GetHalfSize();
	auto Index = [V](int32 X, int32 Y) { return Y * V + X; };
	auto GridPos = [H, this](int32 G) { return -H + G * QuadSize; };

	TArray<float> Heights;
	TArray<uint8> Biomes;
	Heights.SetNumUninitialized(V * V);
	Biomes.SetNumUninitialized(V * V);

	ParallelFor(V, [&](int32 Y)
	{
		for (int32 X = 0; X < V; ++X)
		{
			const FTerrainSample Sample = SampleLocal(FVector2D(GridPos(X), GridPos(Y)));
			Heights[Index(X, Y)] = Sample.Height;
			Biomes[Index(X, Y)] = Sample.Biome;
		}
	});

	TArray<FVector> Normals;
	Normals.SetNumUninitialized(V * V);
	ParallelFor(V, [&](int32 Y)
	{
		for (int32 X = 0; X < V; ++X)
		{
			const float HL = Heights[Index(FMath::Max(X - 1, 0), Y)];
			const float HR = Heights[Index(FMath::Min(X + 1, V - 1), Y)];
			const float HD = Heights[Index(X, FMath::Max(Y - 1, 0))];
			const float HU = Heights[Index(X, FMath::Min(Y + 1, V - 1))];
			const FVector Normal = FVector(HL - HR, HD - HU, 2.f * QuadSize).GetSafeNormal();
			Normals[Index(X, Y)] = Normal;

			// Steep ground reads as bare rock.
			uint8& Biome = Biomes[Index(X, Y)];
			if (Normal.Z < 0.8f && Biome != Road && Biome != Sand && Biome != Lagoon)
			{
				Biome = Rock;
			}
		}
	});

	struct FSection
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> SectionNormals;
	};

	for (int32 CY = 0; CY < ChunksPerSide; ++CY)
	{
		for (int32 CX = 0; CX < ChunksPerSide; ++CX)
		{
			FSection Sections[BiomeCount];

			for (int32 QY = 0; QY < QuadsPerChunk; ++QY)
			{
				for (int32 QX = 0; QX < QuadsPerChunk; ++QX)
				{
					const int32 GX = CX * QuadsPerChunk + QX;
					const int32 GY = CY * QuadsPerChunk + QY;
					FSection& S = Sections[Biomes[Index(GX, GY)]];

					const int32 Base = S.Vertices.Num();
					const int32 Corners[4][2] = { { GX, GY }, { GX + 1, GY }, { GX + 1, GY + 1 }, { GX, GY + 1 } };
					for (const auto& C : Corners)
					{
						S.Vertices.Add(FVector(GridPos(C[0]), GridPos(C[1]), Heights[Index(C[0], C[1])]));
						S.SectionNormals.Add(Normals[Index(C[0], C[1])]);
					}

					// Clockwise seen from above (+X up, +Y right): 0 -> 1 -> 2, 0 -> 2 -> 3.
					if (!bFlipTerrainWinding)
					{
						S.Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
					}
					else
					{
						S.Triangles.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
					}
				}
			}

			UProceduralMeshComponent* Chunk = NewObject<UProceduralMeshComponent>(this);
			Chunk->bUseAsyncCooking = false; // collision must exist before the player lands
			Chunk->SetMobility(EComponentMobility::Movable);
			Chunk->SetupAttachment(RootComponent);
			Chunk->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Chunk->RegisterComponent();
			AddInstanceComponent(Chunk);

			int32 SectionIndex = 0;
			for (int32 b = 0; b < BiomeCount; ++b)
			{
				if (Sections[b].Triangles.Num() == 0)
				{
					continue;
				}
				Chunk->CreateMeshSection_LinearColor(SectionIndex, Sections[b].Vertices, Sections[b].Triangles, Sections[b].SectionNormals,
					TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
				Chunk->SetMaterial(SectionIndex, BiomeMaterials[b]);
				++SectionIndex;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Regions
// ---------------------------------------------------------------------------------------------

void AOverworldBuilder::BuildHub(FRandomStream& Rand)
{
	const FLinearColor Stone(0.42f, 0.41f, 0.39f);

	// The Waystone Circle: standing stones around an ember-lit monolith.
	for (int32 i = 0; i < 9; ++i)
	{
		const float Angle = FMath::DegreesToRadians(i * 40.f + 10.f);
		const FVector2D P = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 1600.f;
		const FRotator Rot(Rand.FRandRange(-5.f, 5.f), FMath::RadiansToDegrees(Angle), Rand.FRandRange(-5.f, 5.f));
		if (UStaticMesh* StoneArt = Art(TEXT("SM_StandingStone")))
		{
			AddMesh(StoneArt, GroundLocal(P, -30.f), Rot, FVector(1.f), Stone, true, false);
		}
		else
		{
			AddMesh(CubeMesh, GroundLocal(P, 200.f), Rot, FVector(1.6f, 0.8f, 5.f), Stone, true);
		}
	}
	const FVector Monolith = GroundLocal(FVector2D::ZeroVector);
	if (UStaticMesh* StoneArt = Art(TEXT("SM_StandingStone")))
	{
		AddMesh(StoneArt, Monolith - FVector(0.f, 0.f, 30.f), FRotator(0.f, 20.f, 0.f), FVector(1.6f, 0.9f, 1.5f), Stone, true, false);
	}
	else
	{
		AddMesh(CubeMesh, Monolith + FVector(0.f, 0.f, 350.f), FRotator(0.f, 20.f, 0.f), FVector(1.2f, 1.2f, 7.5f), FLinearColor(0.2f, 0.19f, 0.18f), true);
	}
	AddLight(Monolith + FVector(0.f, 0.f, 850.f), FLinearColor(1.f, 0.5f, 0.2f), 20000.f, 4000.f);

	// Boulders scattered across the plain.
	// Two boulder variants when the art kit is imported; unit spheres otherwise.
	UStaticMesh* BoulderA = Art(TEXT("SM_Boulder_A"));
	UStaticMesh* BoulderB = Art(TEXT("SM_Boulder_B"));
	UInstancedStaticMeshComponent* BoulderSets[2] =
	{
		AddInstancedMesh(BoulderA ? BoulderA : SphereMesh.Get(), Stone, true, BoulderA == nullptr),
		AddInstancedMesh(BoulderB ? BoulderB : (BoulderA ? BoulderA : SphereMesh.Get()), Stone, true, BoulderA == nullptr && BoulderB == nullptr),
	};
	const float H = GetHalfSize();
	for (int32 i = 0; i < 70; ++i)
	{
		const FVector2D P = FVector2D(Rand.FRandRange(-1.f, 1.f), Rand.FRandRange(-1.f, 1.f)) * H * 0.42f;
		if (P.Size() < 3000.f || DistanceToRoads(P) < 1500.f || FindRegionIndex(P) != Overworld::RegionHub)
		{
			continue;
		}
		const float S = Rand.FRandRange(1.5f, 5.f);
		BoulderSets[i % 2]->AddInstance(FTransform(FRotator(Rand.FRandRange(0.f, 360.f), Rand.FRandRange(0.f, 360.f), 0.f),
			GroundLocal(P, S * 15.f), FVector(S, S * Rand.FRandRange(0.7f, 1.2f), S * 0.7f)));
	}

	// Tide shrine where the causeway leaves the plain.
	SpawnAtLocal<ATideShrine>(GroundLocal(Frac(0.035f, 0.24f) + FVector2D(1600.f, 0.f)));
}

void AOverworldBuilder::BuildVillage(FRandomStream& Rand)
{
	const FVector2D Center = RegionCenter(Overworld::RVillage);

	// Lantern tower: the landmark visible from most of the island at night.
	const FVector TowerBase = GroundLocal(Center + FVector2D(0.f, 1800.f));
	if (UStaticMesh* TowerArt = Art(TEXT("SM_LanternTower")))
	{
		AddMesh(TowerArt, TowerBase - FVector(0.f, 0.f, 30.f), FRotator::ZeroRotator, FVector(1.f), FLinearColor::White, true, false);
		AddLight(TowerBase + FVector(0.f, 0.f, 2350.f), FLinearColor(1.f, 0.65f, 0.3f), 80000.f, 9000.f);
	}
	else
	{
		AddMesh(CylinderMesh, TowerBase + FVector(0.f, 0.f, 1250.f), FRotator::ZeroRotator, FVector(3.f, 3.f, 25.f), FLinearColor(0.5f, 0.47f, 0.42f), true);
		AddMesh(CubeMesh, TowerBase + FVector(0.f, 0.f, 2530.f), FRotator(0.f, 45.f, 0.f), FVector(4.f, 4.f, 0.6f), FLinearColor(0.2f, 0.16f, 0.13f), true);
		AddMesh(SphereMesh, TowerBase + FVector(0.f, 0.f, 2700.f), FRotator::ZeroRotator, FVector(1.5f), FLinearColor(1.f, 0.7f, 0.3f), false);
		AddLight(TowerBase + FVector(0.f, 0.f, 2700.f), FLinearColor(1.f, 0.65f, 0.3f), 80000.f, 9000.f);
	}
	UStaticMesh* HouseArt = Art(TEXT("SM_House"));

	// Houses around the square, leaving the east side open for the road.
	const FLinearColor Walls(0.62f, 0.56f, 0.46f);
	const FLinearColor Roofs(0.35f, 0.16f, 0.12f);
	for (int32 i = 0; i < 8; ++i)
	{
		const float AngleDeg = 40.f + i * (280.f / 7.f);
		const float Angle = FMath::DegreesToRadians(AngleDeg);
		const FVector2D P = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rand.FRandRange(3200.f, 5200.f);
		const FVector Base = GroundLocal(P);
		const float Yaw = AngleDeg + 180.f; // face the square
		const FRotator Facing(0.f, Yaw, 0.f);

		const FVector Front = Facing.Vector();
		if (HouseArt)
		{
			AddMesh(HouseArt, Base - FVector(0.f, 0.f, 25.f), Facing, FVector(1.f), Walls, true, false);
		}
		else
		{
			AddMesh(CubeMesh, Base + FVector(0.f, 0.f, 145.f), Facing, FVector(6.f, 5.f, 3.5f), Walls, true);
			AddMesh(CubeMesh, Base + FVector(0.f, 0.f, 320.f), FRotator(0.f, Yaw, 45.f), FVector(6.6f, 4.f, 4.f), Roofs, true);
			AddMesh(CubeMesh, Base + Front * 305.f + FVector(0.f, 0.f, 100.f), Facing, FVector(0.1f, 1.2f, 2.f), FLinearColor(0.15f, 0.1f, 0.07f), false);
		}
		AddLight(Base + Front * 420.f + FVector(0.f, 0.f, 260.f), FLinearColor(1.f, 0.6f, 0.3f), 3000.f, 900.f);
	}
}

void AOverworldBuilder::BuildLagoon(FRandomStream& Rand)
{
	const FVector2D Center = RegionCenter(Overworld::RLagoon);
	const float H = GetHalfSize();

	// Ruined spires leaning out of the water.
	for (int32 i = 0; i < 9; ++i)
	{
		FVector2D P;
		int32 Tries = 0;
		do
		{
			const float Angle = Rand.FRandRange(0.f, 2.f * PI);
			P = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rand.FRandRange(0.1f, 0.21f) * H;
		}
		while (DistanceToRoads(P) < 3000.f && ++Tries < 10);

		const float Height = Rand.FRandRange(3000.f, 7000.f);
		const FVector Base = GroundLocal(P);
		const FRotator Lean(Rand.FRandRange(-10.f, 10.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-10.f, 10.f));
		const float WidthX = Rand.FRandRange(3.f, 6.f);
		const float WidthY = Rand.FRandRange(3.f, 6.f);
		if (UStaticMesh* SpireArt = Art(TEXT("SM_Spire")))
		{
			// Art is 4.4 m wide and 46 m tall with its pivot at the base.
			AddMesh(SpireArt, Base - FVector(0.f, 0.f, 400.f), Lean, FVector(WidthX / 4.4f, WidthY / 4.4f, Height / 4600.f), FLinearColor::White, true, false);
		}
		else
		{
			AddMesh(CubeMesh, Base + FVector(0.f, 0.f, Height * 0.5f - 400.f), Lean, FVector(WidthX, WidthY, Height / 100.f), FLinearColor(0.45f, 0.5f, 0.55f), true);
		}
	}

	// A second tide shrine on the island so the player is never stranded by high water.
	SpawnAtLocal<ATideShrine>(GroundLocal(Center + FVector2D(1400.f, 600.f)));
}

void AOverworldBuilder::BuildVolcano(FRandomStream& Rand)
{
	const FVector2D Center = RegionCenter(Overworld::RVolcano);
	const float Radius = RegionRadius(Overworld::RVolcano);

	// Lava pool in the crater, lit from above.
	const FVector Floor = GroundLocal(Center);
	const float PoolDiameter = Radius * 0.18f * 2.f * 0.7f;
	AddMesh(CylinderMesh, Floor + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator, FVector(PoolDiameter / 100.f, PoolDiameter / 100.f, 0.3f), FLinearColor(1.f, 0.28f, 0.04f), false);
	for (int32 i = 0; i < 3; ++i)
	{
		const float Angle = i * 2.f * PI / 3.f;
		AddLight(Floor + FVector(FMath::Cos(Angle) * 1200.f, FMath::Sin(Angle) * 1200.f, 700.f), FLinearColor(1.f, 0.35f, 0.08f), 90000.f, 9000.f);
	}

	// Basalt columns on the flanks.
	UStaticMesh* ColumnArt = Art(TEXT("SM_BasaltColumn"));
	UInstancedStaticMeshComponent* Columns = AddInstancedMesh(ColumnArt ? ColumnArt : CylinderMesh.Get(), FLinearColor(0.09f, 0.08f, 0.08f), true, ColumnArt == nullptr);
	for (int32 i = 0; i < 140; ++i)
	{
		const float Angle = Rand.FRandRange(0.f, 2.f * PI);
		const FVector2D P = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rand.FRandRange(0.25f, 0.9f) * Radius;
		if (DistanceToRoads(P) < 1300.f)
		{
			continue;
		}
		const float Width = Rand.FRandRange(1.5f, 3.f);
		const float Height = Rand.FRandRange(4.f, 14.f);
		Columns->AddInstance(FTransform(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), GroundLocal(P, Height * 50.f - 100.f), FVector(Width, Width, Height)));
	}
}

void AOverworldBuilder::BuildMangroves(FRandomStream& Rand)
{
	const FVector2D Center = RegionCenter(Overworld::RMangrove);
	const float Radius = RegionRadius(Overworld::RMangrove);
	const FLinearColor Bark(0.2f, 0.15f, 0.1f);

	UInstancedStaticMeshComponent* Trunks = AddInstancedMesh(CylinderMesh, Bark, true);

	UStaticMesh* TreeArtA = Art(TEXT("SM_Mangrove_A"));
	UStaticMesh* TreeArtB = Art(TEXT("SM_Mangrove_B"));
	if (TreeArtA)
	{
		// Full tree meshes (no collision of their own); the trunk ISM stays as invisible collision.
		Trunks->SetVisibility(false);
		UInstancedStaticMeshComponent* Trees[2] =
		{
			AddInstancedMesh(TreeArtA, Bark, false, false),
			AddInstancedMesh(TreeArtB ? TreeArtB : TreeArtA, Bark, false, false),
		};
		for (int32 i = 0; i < 220; ++i)
		{
			const float Angle = Rand.FRandRange(0.f, 2.f * PI);
			const FVector2D P = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::Sqrt(Rand.FRand()) * Radius * 0.95f;
			if (DistanceToRoads(P) < 900.f)
			{
				continue;
			}
			const FVector Base = GroundLocal(P);
			const float S = Rand.FRandRange(800.f, 1600.f) / 1200.f; // art tree is ~12 m tall
			Trees[i % 2]->AddInstance(FTransform(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), Base - FVector(0.f, 0.f, 30.f), FVector(S)));
			Trunks->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, 525.f * S), FVector(0.6f * S, 0.6f * S, 8.f * S)));
		}
		return;
	}

	UInstancedStaticMeshComponent* Roots = AddInstancedMesh(CubeMesh, Bark, false);
	UInstancedStaticMeshComponent* Canopies = AddInstancedMesh(SphereMesh, FLinearColor(0.11f, 0.24f, 0.15f), false);

	for (int32 i = 0; i < 220; ++i)
	{
		const float Angle = Rand.FRandRange(0.f, 2.f * PI);
		const FVector2D P = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::Sqrt(Rand.FRand()) * Radius * 0.95f;
		if (DistanceToRoads(P) < 900.f)
		{
			continue;
		}

		const FVector Base = GroundLocal(P);
		const float Height = Rand.FRandRange(800.f, 1600.f);
		Trunks->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, Height * 0.5f + 150.f), FVector(0.8f, 0.8f, Height / 100.f)));

		const float CanopyWidth = Rand.FRandRange(6.f, 10.f);
		Canopies->AddInstance(FTransform(FRotator(0.f, Rand.FRandRange(0.f, 360.f), 0.f), Base + FVector(0.f, 0.f, Height + 150.f), FVector(CanopyWidth, CanopyWidth * Rand.FRandRange(0.8f, 1.f), Rand.FRandRange(2.5f, 4.f))));

		// Arching prop roots: the trunk stands on them above the water.
		for (int32 r = 0; r < 4; ++r)
		{
			const float RootYaw = r * 90.f + Rand.FRandRange(-20.f, 20.f);
			const FVector Out = FRotator(0.f, RootYaw, 0.f).Vector();
			Roots->AddInstance(FTransform(FRotator(-40.f, RootYaw, 0.f), Base + Out * 110.f + FVector(0.f, 0.f, 90.f), FVector(3.f, 0.25f, 0.25f)));
		}
	}
}

void AOverworldBuilder::BuildDunes(FRandomStream& Rand)
{
	const FVector2D Center = RegionCenter(Overworld::RDunes);
	const float Radius = RegionRadius(Overworld::RDunes);

	UStaticMesh* ShardArt = Art(TEXT("SM_GlassShard"));
	UInstancedStaticMeshComponent* Shards = AddInstancedMesh(ShardArt ? ShardArt : CubeMesh.Get(), FLinearColor(0.72f, 0.88f, 0.94f), true, ShardArt == nullptr);
	for (int32 i = 0; i < 150; ++i)
	{
		const float Angle = Rand.FRandRange(0.f, 2.f * PI);
		const FVector2D P = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::Sqrt(Rand.FRand()) * Radius * 0.9f;
		if (DistanceToRoads(P) < 1000.f)
		{
			continue;
		}
		const FVector Scale(Rand.FRandRange(0.4f, 1.2f), Rand.FRandRange(0.2f, 0.6f), Rand.FRandRange(3.f, 10.f));
		Shards->AddInstance(FTransform(FRotator(Rand.FRandRange(-25.f, 25.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-25.f, 25.f)), GroundLocal(P, Scale.Z * 30.f), Scale));
	}
}

void AOverworldBuilder::BuildDungeonLink()
{
	// The Sunken Vault (test dungeon) lives far off the island with a roof on; a portal on the
	// Drowned Spires island leads in, and one inside leads back.
	const FVector DungeonLocation = GetActorLocation() + FVector(0.f, -GetHalfSize() * 3.f, 3000.f);
	const FTransform DungeonTransform(FRotator::ZeroRotator, DungeonLocation);

	Dungeon = GetWorld()->SpawnActorDeferred<ATestDungeonBuilder>(ATestDungeonBuilder::StaticClass(), DungeonTransform, this);
	if (!Dungeon)
	{
		return;
	}
	Dungeon->bMovePlayersOnBuild = false;
	Dungeon->bSpawnLightingIfMissing = false;
	Dungeon->bAddCeiling = true;
	Dungeon->FinishSpawning(DungeonTransform);
	Dungeon->Build();

	// Vault arch on the island, opening toward the causeway (south, -Y).
	const FVector2D IslandCenter = RegionCenter(Overworld::RLagoon);
	const FVector Ground = GroundLocal(IslandCenter);
	const FLinearColor Stone(0.3f, 0.34f, 0.36f);
	AddMesh(CubeMesh, Ground + FVector(-350.f, 0.f, 300.f), FRotator::ZeroRotator, FVector(1.2f, 1.2f, 6.f), Stone, true);
	AddMesh(CubeMesh, Ground + FVector(350.f, 0.f, 300.f), FRotator::ZeroRotator, FVector(1.2f, 1.2f, 6.f), Stone, true);
	AddMesh(CubeMesh, Ground + FVector(0.f, 0.f, 660.f), FRotator::ZeroRotator, FVector(8.2f, 1.4f, 1.2f), Stone, true);

	if (AWarpPortal* Entrance = SpawnAtLocal<AWarpPortal>(Ground, FRotator(0.f, 90.f, 0.f)))
	{
		Entrance->Configure(Dungeon->GetPlayerStartTransform(), TEXT("The Sunken Vault"), FVector2D(580.f, 600.f));
	}

	const FTransform ReturnPoint(FRotator(0.f, -90.f, 0.f), ToWorld(Ground + FVector(0.f, -800.f, 120.f)));
	const FVector ExitLocation = DungeonTransform.TransformPosition(FVector(-930.f, 0.f, 0.f));
	FActorSpawnParameters Params;
	Params.Owner = this;
	if (AWarpPortal* Exit = GetWorld()->SpawnActor<AWarpPortal>(ExitLocation, FRotator::ZeroRotator, Params))
	{
		Exit->Configure(ReturnPoint, TEXT("Drowned Spires"), FVector2D(400.f, 350.f));
	}
}

void AOverworldBuilder::SpawnDaytimeEnemies()
{
	const FVector2D Spots[] =
	{
		Frac(0.2f, -0.36f), Frac(0.25f, -0.43f),   // Cinder Rise road
		Frac(0.6f, 0.21f),  Frac(0.7f, 0.06f),     // Mangrove Lattice
		Frac(-0.48f, 0.45f), Frac(-0.58f, 0.6f),   // Saltglass Dunes
	};
	for (const FVector2D& P : Spots)
	{
		SpawnAtLocal<AEmberSentinel>(GroundLocal(P, 85.f));
	}
}

void AOverworldBuilder::UpdateRegionTitle()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	// Ignore positions off the island (e.g. inside the vault).
	const FVector Local = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
	if (FMath::Abs(Local.X) > GetHalfSize() * 1.2f || FMath::Abs(Local.Y) > GetHalfSize() * 1.2f)
	{
		CurrentRegion.Reset();
		return;
	}

	const int32 Index = FindRegionIndex(FVector2D(Local.X, Local.Y));
	const FString Name = GetRegionNameAt(Player->GetActorLocation());
	if (Name != CurrentRegion)
	{
		CurrentRegion = Name;
		const FString Subtitle = Index >= 0 ? Overworld::Regions[Index].Subtitle
			: (Index == Overworld::RegionHub ? TEXT("The tidal plain") : TEXT("Turn back before the current takes you"));
		UEmberveilStatics::ShowTitle(this, Name, Subtitle);
	}
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

void AOverworldBuilder::LoadArt()
{
	static const TCHAR* Names[] =
	{
		TEXT("SM_Boulder_A"), TEXT("SM_Boulder_B"), TEXT("SM_BasaltColumn"), TEXT("SM_GlassShard"), TEXT("SM_Spire"),
		TEXT("SM_StandingStone"), TEXT("SM_House"), TEXT("SM_LanternTower"), TEXT("SM_Mangrove_A"), TEXT("SM_Mangrove_B"),
	};
	ArtMeshes.Reset();
	int32 Found = 0;
	for (const TCHAR* Name : Names)
	{
		UStaticMesh* Mesh = UEmberveilStatics::LoadArtMesh(Name);
		ArtMeshes.Add(FName(Name), Mesh);
		Found += Mesh ? 1 : 0;
	}
	UE_LOG(LogEmberveil, Log, TEXT("Overworld art meshes found: %d/%d (missing ones use greybox shapes)"), Found, static_cast<int32>(UE_ARRAY_COUNT(Names)));
}

UStaticMesh* AOverworldBuilder::Art(const TCHAR* Name) const
{
	const TObjectPtr<UStaticMesh>* Found = ArtMeshes.Find(FName(Name));
	return Found ? Found->Get() : nullptr;
}

UStaticMeshComponent* AOverworldBuilder::AddMesh(UStaticMesh* Mesh, const FVector& LocalLocation, const FRotator& LocalRotation, const FVector& Scale, const FLinearColor& Color, bool bCollision, bool bTint)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetRelativeTransform(FTransform(LocalRotation, LocalLocation, Scale));
	Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Component->RegisterComponent();
	AddInstanceComponent(Component);
	if (bTint)
	{
		UEmberveilStatics::TintMesh(Component, Color);
	}
	return Component;
}

UInstancedStaticMeshComponent* AOverworldBuilder::AddInstancedMesh(UStaticMesh* Mesh, const FLinearColor& Color, bool bCollision, bool bTint)
{
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Component->RegisterComponent();
	AddInstanceComponent(Component);
	if (bTint)
	{
		UEmberveilStatics::TintMesh(Component, Color);
	}
	return Component;
}

UPointLightComponent* AOverworldBuilder::AddLight(const FVector& LocalLocation, const FLinearColor& Color, float Intensity, float Radius)
{
	UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(LocalLocation);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetLightColor(Color);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	AddInstanceComponent(Light);
	return Light;
}
