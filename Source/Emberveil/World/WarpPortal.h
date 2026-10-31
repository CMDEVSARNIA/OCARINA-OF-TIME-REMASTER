#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WarpPortal.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

/** A shimmering doorway that moves the player to another place (e.g. overworld <-> dungeon). Local X is the walk-through axis. */
UCLASS()
class EMBERVEIL_API AWarpPortal : public AActor
{
	GENERATED_BODY()

public:
	AWarpPortal();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FTransform Destination;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FString DestinationName;

	/** Width (Y) and height (Z) of the doorway in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
	FVector2D PortalSize = FVector2D(500.f, 500.f);

	void Configure(const FTransform& InDestination, const FString& InDestinationName, const FVector2D& InSize);

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UStaticMeshComponent> Veil;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UBoxComponent> Trigger;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplySize();
};
