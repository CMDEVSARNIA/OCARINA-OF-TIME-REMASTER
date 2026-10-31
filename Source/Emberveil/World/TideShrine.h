#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MelodyReceiver.h"
#include "TideShrine.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPrimitiveComponent;

/** Play the Tidecall nearby to hold the tide low for a while. Shows its cadence on approach. */
UCLASS()
class EMBERVEIL_API ATideShrine : public AActor, public IMelodyReceiver
{
	GENERATED_BODY()

public:
	ATideShrine();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	FName Cadence = TEXT("Tidecall");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float LowTideSeconds = 150.f;

	virtual void OnCadenceHeard(FName CadenceId, AActor* Player) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<UStaticMeshComponent> Basin;

	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<UStaticMeshComponent> Pillar;

	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<UStaticMeshComponent> Orb;

	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<USphereComponent> Trigger;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	float LastHintTime = -100.f;
};
