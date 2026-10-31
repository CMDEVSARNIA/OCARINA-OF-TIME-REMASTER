#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LockOnTargetComponent.generated.h"

/** Add to any actor to make it lockable. Its world location is the point the camera aims at. */
UCLASS(ClassGroup = (Emberveil), meta = (BlueprintSpawnableComponent))
class EMBERVEIL_API ULockOnTargetComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On")
	bool bCanBeTargeted = true;
};
