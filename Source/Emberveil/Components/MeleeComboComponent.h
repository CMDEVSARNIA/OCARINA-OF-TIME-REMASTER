#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeComboComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingStarted, int32, ComboIndex);

/**
 * Three-hit combo with input buffering. Pressing attack during a swing queues the next hit;
 * pressing within ComboWindow after a swing ends continues the chain, otherwise it restarts.
 * Hits are a sphere overlap in front of the owner, so no animation assets are required.
 */
UCLASS(ClassGroup = (Emberveil), meta = (BlueprintSpawnableComponent))
class EMBERVEIL_API UMeleeComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeComboComponent();

	/** Damage per hit in the chain; its length is the combo length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TArray<float> ComboDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float SwingDuration = 0.32f;

	/** Delay from swing start to the moment the hit is checked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float ComboWindow = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float Reach = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float HitRadius = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bDrawDebug = true;

	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnSwingStarted OnSwingStarted;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void RequestAttack();

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsSwinging() const { return bSwinging; }

	/** 0..1 progress through the current swing (0 when idle). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetSwingAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetComboIndex() const { return ComboIndex; }

private:
	void StartSwing(int32 Index);
	void PerformHit();
	void FinishSwing();

	bool bSwinging = false;
	bool bQueued = false;
	int32 ComboIndex = -1;
	float SwingStartTime = 0.f;
	float LastSwingEndTime = -100.f;
	FTimerHandle HitTimer;
	FTimerHandle EndTimer;
};
