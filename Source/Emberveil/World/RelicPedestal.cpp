#include "World/RelicPedestal.h"

#include "EmberveilStatics.h"
#include "Components/InventoryComponent.h"
#include "Components/MelodyComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ARelicPedestal::ARelicPedestal()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(Root);
	BaseMesh->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	BaseMesh->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
	if (Cube.Succeeded()) { BaseMesh->SetStaticMesh(Cube.Object); }

	RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
	RelicMesh->SetupAttachment(Root);
	RelicMesh->SetRelativeLocation(FVector(0.f, 0.f, 135.f));
	RelicMesh->SetRelativeScale3D(FVector(0.45f));
	RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sphere.Succeeded()) { RelicMesh->SetStaticMesh(Sphere.Object); }

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RelicMesh);
	Glow->SetIntensity(0.f);
	Glow->SetAttenuationRadius(900.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.5f, 0.15f));
	Glow->SetCastShadows(false);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	Trigger->InitSphereRadius(220.f);
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	Trigger->SetGenerateOverlapEvents(true);
}

void ARelicPedestal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UEmberveilStatics::TintMesh(BaseMesh, FLinearColor(0.2f, 0.2f, 0.22f));
	UEmberveilStatics::TintMesh(RelicMesh, FLinearColor(0.15f, 0.1f, 0.08f));
}

void ARelicPedestal::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ARelicPedestal::HandleOverlap);
}

void ARelicPedestal::OnCadenceHeard(FName CadenceId, AActor* Player)
{
	if (bAwake || CadenceId != WakeCadence)
	{
		return;
	}

	bAwake = true;
	UEmberveilStatics::TintMesh(RelicMesh, FLinearColor(1.f, 0.45f, 0.1f));
	Glow->SetIntensity(8000.f);
	UEmberveilStatics::ShowMessage(this, TEXT("The relic kindles with a warm light."), 4.f);
}

void ARelicPedestal::HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled() || bClaimed)
	{
		return;
	}

	if (!bAwake)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastHintTime > 6.f)
		{
			LastHintTime = Now;
			const UMelodyComponent* Melody = Pawn->FindComponentByClass<UMelodyComponent>();
			const FString Notes = Melody ? Melody->DescribeCadence(WakeCadence) : FString();
			UEmberveilStatics::ShowMessage(this, FString::Printf(TEXT("A cold relic rests here. Scratched into the stone:  [ %s ]"), Notes.IsEmpty() ? TEXT("?") : *Notes), 6.f);
		}
		return;
	}

	if (UInventoryComponent* Inventory = Pawn->FindComponentByClass<UInventoryComponent>())
	{
		bClaimed = true;
		Inventory->GrantRelic();
		RelicMesh->SetVisibility(false);
		Glow->SetIntensity(0.f);
		UEmberveilStatics::ShowMessage(this, TEXT("You claimed the Ember Relic. Test dungeon complete!"), 8.f);
	}
}
