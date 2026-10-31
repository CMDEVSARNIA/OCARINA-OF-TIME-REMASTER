#include "World/WarpPortal.h"

#include "EmberveilStatics.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AWarpPortal::AWarpPortal()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Veil = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Veil"));
	Veil->SetupAttachment(Root);
	Veil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Veil->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Veil->SetStaticMesh(Cube.Object);
	}

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);

	ApplySize();
}

void AWarpPortal::Configure(const FTransform& InDestination, const FString& InDestinationName, const FVector2D& InSize)
{
	Destination = InDestination;
	DestinationName = InDestinationName;
	PortalSize = InSize;
	ApplySize();
}

void AWarpPortal::ApplySize()
{
	Veil->SetRelativeLocation(FVector(0.f, 0.f, PortalSize.Y * 0.5f));
	Veil->SetRelativeScale3D(FVector(0.05f, PortalSize.X / 100.f, PortalSize.Y / 100.f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, PortalSize.Y * 0.5f));
	Trigger->SetBoxExtent(FVector(60.f, PortalSize.X * 0.5f, PortalSize.Y * 0.5f));
}

void AWarpPortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySize();
	UEmberveilStatics::TintMesh(Veil, FLinearColor(0.2f, 0.9f, 0.85f));
}

void AWarpPortal::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AWarpPortal::HandleOverlap);
}

void AWarpPortal::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}

	const FRotator Facing(0.f, Destination.Rotator().Yaw, 0.f);
	Pawn->TeleportTo(Destination.GetLocation(), Facing);
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->SetControlRotation(Facing);
	}

	if (!DestinationName.IsEmpty())
	{
		UEmberveilStatics::ShowTitle(this, DestinationName, FString());
	}
}
