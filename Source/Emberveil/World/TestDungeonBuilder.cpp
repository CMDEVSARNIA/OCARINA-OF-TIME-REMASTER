#include "World/TestDungeonBuilder.h"

#include "EmberveilStatics.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "World/EmberSentinel.h"
#include "World/LockedDoor.h"
#include "World/MelodyGate.h"
#include "World/RelicPedestal.h"

namespace Dungeon
{
	constexpr float RoomSize = 2000.f;
	constexpr float WallHeight = 500.f;
	constexpr float WallThickness = 100.f;
	constexpr float DoorWidth = 400.f;
	constexpr float DoorHeight = 350.f;
	constexpr int32 NumRooms = 4;

	const FLinearColor FloorColor(0.18f, 0.17f, 0.16f);
	const FLinearColor WallColor(0.32f, 0.3f, 0.28f);
	const FLinearColor AccentColor(0.22f, 0.26f, 0.3f);
}

ATestDungeonBuilder::ATestDungeonBuilder()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	CylinderMesh = Cylinder.Object;
}

void ATestDungeonBuilder::BeginPlay()
{
	Super::BeginPlay();
	Build();
	if (bMovePlayersOnBuild)
	{
		MovePlayersToStart();
	}
}

FTransform ATestDungeonBuilder::GetPlayerStartTransform() const
{
	const FVector Location = GetActorTransform().TransformPosition(FVector(-600.f, 0.f, 120.f));
	const FRotator Rotation(0.f, GetActorRotation().Yaw, 0.f);
	return FTransform(Rotation, Location);
}

void ATestDungeonBuilder::MovePlayersToStart()
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

UStaticMeshComponent* ATestDungeonBuilder::AddBlock(const FVector& LocalCenter, const FVector& Size, const FLinearColor& Color)
{
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(CubeMesh);
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeLocation(LocalCenter);
	Mesh->SetRelativeScale3D(Size / 100.f);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->RegisterComponent();
	AddInstanceComponent(Mesh);
	UEmberveilStatics::TintMesh(Mesh, Color);
	return Mesh;
}

void ATestDungeonBuilder::AddDividerWall(float X)
{
	using namespace Dungeon;
	const float SideLength = (RoomSize - DoorWidth) * 0.5f;
	const float SideOffset = DoorWidth * 0.5f + SideLength * 0.5f;

	AddBlock(FVector(X, SideOffset, WallHeight * 0.5f), FVector(WallThickness, SideLength, WallHeight), WallColor);
	AddBlock(FVector(X, -SideOffset, WallHeight * 0.5f), FVector(WallThickness, SideLength, WallHeight), WallColor);

	const float LintelHeight = WallHeight - DoorHeight;
	AddBlock(FVector(X, 0.f, DoorHeight + LintelHeight * 0.5f), FVector(WallThickness, DoorWidth, LintelHeight), WallColor);
}

void ATestDungeonBuilder::AddBrazier(const FVector& LocalLocation)
{
	UStaticMeshComponent* Bowl = NewObject<UStaticMeshComponent>(this);
	Bowl->SetMobility(EComponentMobility::Movable);
	Bowl->SetStaticMesh(CylinderMesh);
	Bowl->SetupAttachment(RootComponent);
	Bowl->SetRelativeLocation(LocalLocation + FVector(0.f, 0.f, 60.f));
	Bowl->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.2f));
	Bowl->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Bowl->RegisterComponent();
	AddInstanceComponent(Bowl);
	UEmberveilStatics::TintMesh(Bowl, FLinearColor(0.08f, 0.07f, 0.06f));

	UPointLightComponent* Flame = NewObject<UPointLightComponent>(this);
	Flame->SetMobility(EComponentMobility::Movable);
	Flame->SetupAttachment(RootComponent);
	Flame->SetRelativeLocation(LocalLocation + FVector(0.f, 0.f, 170.f));
	Flame->SetIntensity(6000.f);
	Flame->SetAttenuationRadius(1400.f);
	Flame->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Flame->SetCastShadows(false);
	Flame->RegisterComponent();
	AddInstanceComponent(Flame);
}

void ATestDungeonBuilder::AddLighting()
{
	if (TActorIterator<ADirectionalLight>(GetWorld()))
	{
		return; // Level already has a sun; leave its lighting alone.
	}

	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetupAttachment(RootComponent);
	Sun->SetWorldRotation(FRotator(-55.f, 35.f, 0.f));
	Sun->SetIntensity(4.f);
	Sun->RegisterComponent();
	AddInstanceComponent(Sun);

	// Weak shadowless fill from the opposite side so shadowed areas aren't pitch black.
	UDirectionalLightComponent* Fill = NewObject<UDirectionalLightComponent>(this);
	Fill->SetMobility(EComponentMobility::Movable);
	Fill->SetupAttachment(RootComponent);
	Fill->SetWorldRotation(FRotator(-30.f, 215.f, 0.f));
	Fill->SetIntensity(0.8f);
	Fill->SetCastShadows(false);
	Fill->SetLightColor(FLinearColor(0.6f, 0.7f, 1.f));
	Fill->RegisterComponent();
	AddInstanceComponent(Fill);
}

template <class T>
T* ATestDungeonBuilder::SpawnLocal(const FVector& LocalLocation, const FRotator& LocalRotation)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector WorldLocation = GetActorTransform().TransformPosition(LocalLocation);
	const FRotator WorldRotation = (GetActorRotation().Quaternion() * LocalRotation.Quaternion()).Rotator();
	return GetWorld()->SpawnActor<T>(WorldLocation, WorldRotation, Params);
}

void ATestDungeonBuilder::Build()
{
	if (bBuilt || !CubeMesh || !CylinderMesh)
	{
		return;
	}
	bBuilt = true;

	using namespace Dungeon;
	const float Half = RoomSize * 0.5f;
	const float TotalLength = RoomSize * NumRooms;
	const float CenterX = -Half + TotalLength * 0.5f; // rooms span X in [-Half, -Half + TotalLength]

	// Floor and outer shell.
	AddBlock(FVector(CenterX, 0.f, -25.f), FVector(TotalLength, RoomSize, 50.f), FloorColor);
	AddBlock(FVector(CenterX, Half + WallThickness * 0.5f, WallHeight * 0.5f), FVector(TotalLength + 2.f * WallThickness, WallThickness, WallHeight), WallColor);
	AddBlock(FVector(CenterX, -Half - WallThickness * 0.5f, WallHeight * 0.5f), FVector(TotalLength + 2.f * WallThickness, WallThickness, WallHeight), WallColor);
	AddBlock(FVector(-Half - WallThickness * 0.5f, 0.f, WallHeight * 0.5f), FVector(WallThickness, RoomSize, WallHeight), WallColor);
	AddBlock(FVector(-Half + TotalLength + WallThickness * 0.5f, 0.f, WallHeight * 0.5f), FVector(WallThickness, RoomSize, WallHeight), WallColor);

	if (bAddCeiling)
	{
		AddBlock(FVector(CenterX, 0.f, WallHeight + 25.f), FVector(TotalLength + 2.f * WallThickness, RoomSize + 2.f * WallThickness, 50.f), WallColor);
	}

	// Dividers between rooms, each with a doorway.
	for (int32 Room = 1; Room < NumRooms; ++Room)
	{
		AddDividerWall(-Half + RoomSize * Room);
	}

	// Room 0 (start): a couple of low ledges to practise jumping.
	AddBlock(FVector(-200.f, 600.f, 30.f), FVector(300.f, 300.f, 60.f), AccentColor);
	AddBlock(FVector(150.f, 650.f, 75.f), FVector(250.f, 250.f, 150.f), AccentColor);

	// Room 1: two sentinels, one carries the key.
	SpawnLocal<AEmberSentinel>(FVector(1800.f, 500.f, 80.f), FRotator(0.f, 180.f, 0.f));
	if (AEmberSentinel* KeyHolder = SpawnLocal<AEmberSentinel>(FVector(2400.f, -450.f, 80.f), FRotator(0.f, 180.f, 0.f)))
	{
		KeyHolder->bDropsKey = true;
	}

	// Locked door between rooms 1 and 2.
	if (ALockedDoor* Door = SpawnLocal<ALockedDoor>(FVector(3000.f, 0.f, 0.f)))
	{
		Door->SetBarrierSize(FVector(40.f, DoorWidth, DoorHeight));
	}

	// Room 2: pillars for cover, one sentinel, and the melody gate.
	AddBlock(FVector(4300.f, 550.f, WallHeight * 0.5f), FVector(140.f, 140.f, WallHeight), AccentColor);
	AddBlock(FVector(4300.f, -550.f, WallHeight * 0.5f), FVector(140.f, 140.f, WallHeight), AccentColor);
	SpawnLocal<AEmberSentinel>(FVector(3700.f, 400.f, 80.f), FRotator(0.f, 180.f, 0.f));
	if (AMelodyGate* Gate = SpawnLocal<AMelodyGate>(FVector(5000.f, 0.f, 0.f)))
	{
		Gate->SetBarrierSize(FVector(40.f, DoorWidth, DoorHeight));
		Gate->RequiredCadence = TEXT("Tidecall");
	}

	// Room 3: dais and relic.
	AddBlock(FVector(6300.f, 0.f, 15.f), FVector(900.f, 900.f, 30.f), AccentColor);
	SpawnLocal<ARelicPedestal>(FVector(6300.f, 0.f, 30.f));

	// Braziers along the side walls of every room.
	for (int32 Room = 0; Room < NumRooms; ++Room)
	{
		const float X = -Half + RoomSize * Room + Half;
		AddBrazier(FVector(X, Half - 150.f, 0.f));
		AddBrazier(FVector(X, -Half + 150.f, 0.f));
	}

	if (bSpawnLightingIfMissing)
	{
		AddLighting();
	}
}
