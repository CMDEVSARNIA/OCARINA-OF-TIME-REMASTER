#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnComponent.generated.h"

class ULockOnTargetComponent;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChanged, ULockOnTargetComponent*, NewTarget);

/**
 * Third-person target lock. While locked, the controller (and therefore the camera and the
 * character's facing) tracks the target, so movement input becomes strafing around it.
 * Toggling with nothing in view recenters the camera behind the character instead.
 */
UCLASS(ClassGroup = (Emberveil), meta = (BlueprintSpawnableComponent))
class EMBERVEIL_API ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnComponent();

	/** Max distance to acquire a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On")
	float SearchRadius = 1800.f;

	/** Lock breaks beyond this distance (larger than SearchRadius to avoid flicker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On")
	float BreakDistance = 2400.f;

	/** Half-angle (degrees, horizontal) in front of the camera that targets are searched in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On")
	float MaxSearchAngle = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On")
	float CameraInterpSpeed = 12.f;

	/** Added to the aim pitch so the camera looks slightly down at the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lock On")
	float CameraPitchOffset = -12.f;

	UPROPERTY(BlueprintAssignable, Category = "Lock On")
	FOnLockOnTargetChanged OnTargetChanged;

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void CycleTarget(bool bToRight);

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void ClearLockOn();

	UFUNCTION(BlueprintPure, Category = "Lock On")
	bool IsLockedOn() const { return bIsLocked && CurrentTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Lock On")
	ULockOnTargetComponent* GetCurrentTarget() const { return CurrentTarget.Get(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	APlayerController* GetPlayerController() const;
	bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
	bool IsTargetUsable(const ULockOnTargetComponent* Target, float MaxDistance) const;
	void GatherCandidates(TArray<ULockOnTargetComponent*>& OutCandidates) const;
	ULockOnTargetComponent* FindBestTarget() const;
	float GetViewYawOffset(const ULockOnTargetComponent* Target, const FVector& ViewLocation, const FRotator& ViewRotation) const;
	void SetTarget(ULockOnTargetComponent* NewTarget);
	void RecenterCamera();

	TWeakObjectPtr<ULockOnTargetComponent> CurrentTarget;
	bool bIsLocked = false;
	bool bSavedOrientToMovement = true;
	bool bSavedUseControllerDesiredRotation = false;
};
