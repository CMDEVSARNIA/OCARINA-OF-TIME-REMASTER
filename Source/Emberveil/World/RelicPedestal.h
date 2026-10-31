#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MelodyReceiver.h"
#include "RelicPedestal.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPrimitiveComponent;

/** Goal of the test dungeon: dormant until its cadence is played, then claimable. */
UCLASS()
class EMBERVEIL_API ARelicPedestal : public AActor, public IMelodyReceiver
{
	GENERATED_BODY()

public:
	ARelicPedestal();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Relic")
	FName WakeCadence = TEXT("Emberwake");

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void OnCadenceHeard(FName CadenceId, AActor* Player) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Relic")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Relic")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Relic")
	TObjectPtr<UStaticMeshComponent> RelicMesh;

	UPROPERTY(VisibleAnywhere, Category = "Relic")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(VisibleAnywhere, Category = "Relic")
	TObjectPtr<USphereComponent> Trigger;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bAwake = false;
	bool bClaimed = false;
	float LastHintTime = -100.f;
};
