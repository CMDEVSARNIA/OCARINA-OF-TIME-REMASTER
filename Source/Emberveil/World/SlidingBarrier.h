#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlidingBarrier.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;

/**
 * Base for anything that blocks a doorway and sinks into the floor when opened.
 * The actor's origin is the floor centre of the doorway; X is the walk-through axis.
 */
UCLASS(Abstract)
class EMBERVEIL_API ASlidingBarrier : public AActor
{
	GENERATED_BODY()

public:
	ASlidingBarrier();

	/** X = thickness, Y = width, Z = height (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier")
	FVector BarrierSize = FVector(40.f, 400.f, 350.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier")
	FLinearColor BarrierColor = FLinearColor(0.25f, 0.18f, 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Barrier", meta = (ClampMin = "0.05"))
	float OpenDuration = 1.2f;

	UFUNCTION(BlueprintCallable, Category = "Barrier")
	void Open();

	UFUNCTION(BlueprintPure, Category = "Barrier")
	bool IsOpen() const { return bOpening; }

	void SetBarrierSize(const FVector& NewSize);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Called whenever the player pawn steps into the approach trigger. */
	virtual void OnPlayerApproach(APawn* Player) {}

	UPROPERTY(VisibleAnywhere, Category = "Barrier")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Barrier")
	TObjectPtr<UStaticMeshComponent> BarrierMesh;

	UPROPERTY(VisibleAnywhere, Category = "Barrier")
	TObjectPtr<UBoxComponent> ApproachTrigger;

private:
	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplySize();

	bool bOpening = false;
	float OpenAlpha = 0.f;
};
