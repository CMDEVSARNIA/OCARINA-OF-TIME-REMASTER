#include "World/EnvironmentController.h"

#include "EmberveilStatics.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

AEnvironmentController::AEnvironmentController()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->SetIntensity(10.f);

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetIntensity(0.f);
	Moon->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.015f);
	Fog->SetFogHeightFalloff(0.15f);

	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(Root);
	WaterMesh->SetMobility(EComponentMobility::Movable);
	WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		WaterMesh->SetStaticMesh(Cube.Object);
	}
}

AEnvironmentController* AEnvironmentController::Find(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AEnvironmentController> It(World);
	return It ? *It : nullptr;
}

void AEnvironmentController::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UEmberveilStatics::TintMesh(WaterMesh, FLinearColor(0.04f, 0.17f, 0.24f));
	ApplyWaterTransform();
}

void AEnvironmentController::BeginPlay()
{
	Super::BeginPlay();
	TimeOfDay = StartHour;
	CurrentTide = TideAmplitude * FMath::Cos(GetTidePhase());
	UpdateSky();
	ApplyWaterTransform();
}

void AEnvironmentController::SetWaterHalfExtent(float NewHalfExtent)
{
	WaterHalfExtent = FMath::Max(1000.f, NewHalfExtent);
	ApplyWaterTransform();
}

void AEnvironmentController::ApplyWaterTransform()
{
	// Cube is 100 cm; keep it 20 cm thick with its top surface at the water level.
	WaterMesh->SetRelativeScale3D(FVector(WaterHalfExtent / 50.f, WaterHalfExtent / 50.f, 0.2f));
	WaterMesh->SetRelativeLocation(FVector(0.f, 0.f, SeaLevel + CurrentTide - 10.f));
}

float AEnvironmentController::GetSunElevation() const
{
	// 0 at 06:00, +1 at noon, 0 at 18:00, -1 at midnight.
	return FMath::Sin((TimeOfDay - 6.f) / 24.f * 2.f * PI);
}

float AEnvironmentController::GetTidePhase() const
{
	// Two tides a day: high at 00:00 and 12:00, low at 06:00 and 18:00.
	return TimeOfDay / 12.f * 2.f * PI;
}

bool AEnvironmentController::IsNight() const
{
	return GetSunElevation() < -0.05f;
}

void AEnvironmentController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TimeOfDay += DeltaSeconds * 24.f / (DayLengthMinutes * 60.f);
	while (TimeOfDay >= 24.f)
	{
		TimeOfDay -= 24.f;
		++Day;
	}

	UpdateSky();
	UpdateTide(DeltaSeconds);
}

void AEnvironmentController::UpdateSky()
{
	const float Angle = (TimeOfDay - 6.f) / 24.f * 360.f;
	Sun->SetWorldRotation(FRotator(-Angle, SunYaw, 0.f).Quaternion());
	Moon->SetWorldRotation(FRotator(-(Angle + 180.f), SunYaw + 20.f, 0.f).Quaternion());

	const float Elevation = GetSunElevation();
	Sun->SetIntensity(10.f * FMath::SmoothStep(-0.05f, 0.2f, Elevation));
	Moon->SetIntensity(0.5f * FMath::SmoothStep(-0.05f, 0.2f, -Elevation));
	Sun->SetVisibility(Elevation > -0.1f);
	Moon->SetVisibility(Elevation < 0.1f);
}

void AEnvironmentController::UpdateTide(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Natural = TideAmplitude * FMath::Cos(GetTidePhase());
	const float Target = Now < ForcedLowUntil ? -TideAmplitude : Natural;
	CurrentTide = FMath::FInterpConstantTo(CurrentTide, Target, DeltaSeconds, TideSpeed);
	ApplyWaterTransform();
}

bool AEnvironmentController::GetWaterLevelAt(const FVector& WorldLocation, float& OutLevel) const
{
	const FVector Local = WorldLocation - GetActorLocation();
	if (FMath::Abs(Local.X) > WaterHalfExtent || FMath::Abs(Local.Y) > WaterHalfExtent)
	{
		return false;
	}
	OutLevel = GetActorLocation().Z + GetWaterLevel();
	return true;
}

void AEnvironmentController::ForceLowTide(float Seconds)
{
	ForcedLowUntil = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Seconds);
}

FString AEnvironmentController::GetClockText() const
{
	const int32 Hours = FMath::FloorToInt(TimeOfDay);
	const int32 Minutes = FMath::FloorToInt((TimeOfDay - Hours) * 60.f);
	return FString::Printf(TEXT("Day %d  %02d:%02d"), Day, Hours, Minutes);
}

FString AEnvironmentController::GetTideText() const
{
	const float Fraction = TideAmplitude > 0.f ? CurrentTide / TideAmplitude : 0.f;
	const TCHAR* Level = Fraction > 0.5f ? TEXT("High") : (Fraction < -0.5f ? TEXT("Low") : TEXT("Mid"));

	if (GetWorld() && GetWorld()->GetTimeSeconds() < ForcedLowUntil)
	{
		return FString::Printf(TEXT("Tide: %s (held by the Tidecall)"), Level);
	}

	const bool bRising = -FMath::Sin(GetTidePhase()) > 0.f;
	return FString::Printf(TEXT("Tide: %s (%s)"), Level, bRising ? TEXT("rising") : TEXT("falling"));
}
