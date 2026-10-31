#include "World/SlidingBarrier.h"

#include "EmberveilStatics.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ASlidingBarrier::ASlidingBarrier()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetupAttachment(Root);
	BarrierMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		BarrierMesh->SetStaticMesh(Cube.Object);
	}

	ApproachTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ApproachTrigger"));
	ApproachTrigger->SetupAttachment(Root);
	ApproachTrigger->SetCollisionProfileName(TEXT("Trigger"));
	ApproachTrigger->SetGenerateOverlapEvents(true);

	ApplySize();
}

void ASlidingBarrier::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySize();
	UEmberveilStatics::TintMesh(BarrierMesh, BarrierColor);
}

void ASlidingBarrier::BeginPlay()
{
	Super::BeginPlay();
	ApproachTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASlidingBarrier::HandleTriggerOverlap);
}

void ASlidingBarrier::SetBarrierSize(const FVector& NewSize)
{
	BarrierSize = NewSize;
	ApplySize();
}

void ASlidingBarrier::ApplySize()
{
	BarrierMesh->SetRelativeLocation(FVector(0.f, 0.f, BarrierSize.Z * 0.5f));
	BarrierMesh->SetRelativeScale3D(BarrierSize / 100.f);

	// Trigger reaches 1.5 m out on both sides of the barrier.
	ApproachTrigger->SetRelativeLocation(FVector(0.f, 0.f, BarrierSize.Z * 0.5f));
	ApproachTrigger->SetBoxExtent(FVector(BarrierSize.X * 0.5f + 150.f, BarrierSize.Y * 0.5f, BarrierSize.Z * 0.5f));
}

void ASlidingBarrier::Open()
{
	bOpening = true;
}

void ASlidingBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bOpening || OpenAlpha >= 1.f)
	{
		return;
	}

	OpenAlpha = FMath::Min(1.f, OpenAlpha + DeltaSeconds / OpenDuration);
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, OpenAlpha, 2.f);
	BarrierMesh->SetRelativeLocation(FVector(0.f, 0.f, BarrierSize.Z * 0.5f - Eased * (BarrierSize.Z + 10.f)));

	if (OpenAlpha >= 1.f)
	{
		BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BarrierMesh->SetVisibility(false);
	}
}

void ASlidingBarrier::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		OnPlayerApproach(Pawn);
	}
}
