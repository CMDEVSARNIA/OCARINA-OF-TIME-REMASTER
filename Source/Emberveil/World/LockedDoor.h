#pragma once

#include "CoreMinimal.h"
#include "World/SlidingBarrier.h"
#include "LockedDoor.generated.h"

/** Opens automatically when a player carrying a small key walks up to it (the key is consumed). */
UCLASS()
class EMBERVEIL_API ALockedDoor : public ASlidingBarrier
{
	GENERATED_BODY()

public:
	ALockedDoor();

protected:
	virtual void OnPlayerApproach(APawn* Player) override;

private:
	float LastHintTime = -100.f;
};
