#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MelodyReceiver.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UMelodyReceiver : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by world actors that react when the player completes a cadence nearby. */
class EMBERVEIL_API IMelodyReceiver
{
	GENERATED_BODY()

public:
	virtual void OnCadenceHeard(FName CadenceId, AActor* Player) = 0;
};
