#include "World/TideShrine.h"

#include "EmberveilStatics.h"
#include "Components/MelodyComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "World/EnvironmentController.h"

ATideShrine::ATideShrine()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Basin = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Basin"));
	Basin->SetupAttachment(Root);
	Basin->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	Basin->SetRelativeScale3D(FVector(3.f, 3.f, 0.6f));
	Basin->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	if (Cylinder.Succeeded()) { Basin->SetStaticMesh(Cylinder.Object); }

	Pillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pillar"));
	Pillar->SetupAttachment(Root);
	Pillar->SetRelativeLocation(FVector(0.f, 0.f, 225.f));
	Pillar->SetRelativeScale3D(FVector(0.7f, 0.7f, 3.5f));
	Pillar->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	if (Cube.Succeeded()) { Pillar->SetStaticMesh(Cube.Object); }

	Orb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Orb"));
	Orb->SetupAttachment(Root);
	Orb->SetRelativeLocation(FVector(0.f, 0.f, 450.f));
	Orb->SetRelativeScale3D(FVector(0.6f));
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sphere.Succeeded()) { Orb->SetStaticMesh(Sphere.Object); }

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Orb);
	Glow->SetIntensity(5000.f);
	Glow->SetAttenuationRadius(1200.f);
	Glow->SetLightColor(FLinearColor(0.3f, 0.8f, 1.f));
	Glow->SetCastShadows(false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(450.f);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
}

void ATideShrine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UEmberveilStatics::TintMesh(Basin, FLinearColor(0.3f, 0.32f, 0.33f));
	UEmberveilStatics::TintMesh(Pillar, FLinearColor(0.38f, 0.4f, 0.42f));
	UEmberveilStatics::TintMesh(Orb, FLinearColor(0.3f, 0.75f, 0.9f));
}

void ATideShrine::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATideShrine::HandleOverlap);
}

void ATideShrine::OnCadenceHeard(FName CadenceId, AActor* Player)
{
	if (CadenceId != Cadence)
	{
		return;
	}

	if (AEnvironmentController* Environment = AEnvironmentController::Find(this))
	{
		Environment->ForceLowTide(LowTideSeconds);
		UEmberveilStatics::ShowMessage(this, TEXT("The shrine hums. The sea draws back from the causeway."), 5.f);
	}
}

void ATideShrine::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Pawn || !Pawn->IsPlayerControlled() || Now - LastHintTime < 8.f)
	{
		return;
	}
	LastHintTime = Now;

	const UMelodyComponent* Melody = Pawn->FindComponentByClass<UMelodyComponent>();
	const FString Notes = Melody ? Melody->DescribeCadence(Cadence) : FString();
	UEmberveilStatics::ShowMessage(this, FString::Printf(TEXT("A tide shrine. Its rim is carved with notes:  [ %s ]"), Notes.IsEmpty() ? TEXT("?") : *Notes), 6.f);
}
