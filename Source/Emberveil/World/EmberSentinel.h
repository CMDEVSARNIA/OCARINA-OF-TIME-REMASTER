#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmberSentinel.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UHealthComponent;
class ULockOnTargetComponent;

enum class ESentinelState : uint8
{
	Idle,
	Windup,
	Recover,
	Stunned
};

/**
 * Training enemy: chases the player, telegraphs each attack (eye glows red) and is stunned
 * briefly when hit. Uses direct swept movement, so no NavMesh is required.
 */
UCLASS()
class EMBERVEIL_API AEmberSentinel : public AActor
{
	GENERATED_BODY()

public:
	AEmberSentinel();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	bool bDropsKey = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float AggroRadius = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float MoveSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float AttackRange = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float WindupTime = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float AttackCooldown = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float HitStunTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sentinel")
	float KnockbackStrength = 600.f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Sentinel")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "Sentinel")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Sentinel")
	TObjectPtr<UStaticMeshComponent> EyeMesh;

	UPROPERTY(VisibleAnywhere, Category = "Sentinel")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "Sentinel")
	TObjectPtr<ULockOnTargetComponent> LockOnPoint;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void BeginWindup(float Now);
	void ResolveAttack(float Now, APawn* Player);
	void SetEyeColor(const FLinearColor& Color);

	ESentinelState State = ESentinelState::Idle;
	float StateEndTime = 0.f;
	float NextAttackTime = 0.f;
	float FlashUntil = 0.f;
	bool bBodyFlashing = false;
};
