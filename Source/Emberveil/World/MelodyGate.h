#pragma once

#include "CoreMinimal.h"
#include "World/SlidingBarrier.h"
#include "World/MelodyReceiver.h"
#include "MelodyGate.generated.h"

/** A sealed gate that sinks when the required cadence is played nearby. Shows its hint on approach. */
UCLASS()
class EMBERVEIL_API AMelodyGate : public ASlidingBarrier, public IMelodyReceiver
{
	GENERATED_BODY()

public:
	AMelodyGate();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	FName RequiredCadence = TEXT("Tidecall");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	FString HintText = TEXT("Waves are carved into the gate. An inscription reads:");

	virtual void OnCadenceHeard(FName CadenceId, AActor* Player) override;

protected:
	virtual void OnPlayerApproach(APawn* Player) override;

private:
	float LastHintTime = -100.f;
};
