#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EmberveilCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UInputMappingContext;
class UInputAction;
class UHealthComponent;
class ULockOnComponent;
class UMeleeComboComponent;
class UInventoryComponent;
class UMelodyComponent;
class AEnvironmentController;
struct FInputActionValue;

/**
 * The lantern-keeper. Greybox body (no skeletal mesh needed), third-person camera, lock-on,
 * three-hit combo, small-key inventory and the Tidehorn. Input actions and the mapping context
 * are created in code so the project runs without any authored input assets; swap them for
 * .uasset versions later if you want to rebind in the editor.
 */
UCLASS()
class EMBERVEIL_API AEmberveilCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEmberveilCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	UHealthComponent* GetHealth() const { return Health; }
	ULockOnComponent* GetLockOn() const { return LockOn; }
	UMeleeComboComponent* GetCombo() const { return Combo; }
	UInventoryComponent* GetInventory() const { return Inventory; }
	UMelodyComponent* GetMelody() const { return Melody; }

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USceneComponent> BladePivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UPointLightComponent> Lantern;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	TObjectPtr<UHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	TObjectPtr<ULockOnComponent> LockOn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	TObjectPtr<UMeleeComboComponent> Combo;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay")
	TObjectPtr<UMelodyComponent> Melody;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	float RunSpeed = 520.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	float WadeSpeed = 280.f;

	/** Deeper than this and the current drags you back to the last dry ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water")
	float MaxWadeDepth = 130.f;

private:
	void BuildInputMappings();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Attack();
	void ToggleLockOn();
	void CycleTargetRight();
	void CycleTargetLeft();
	void PlayNote(int32 Note);
	void PlayNote1() { PlayNote(0); }
	void PlayNote2() { PlayNote(1); }
	void PlayNote3() { PlayNote(2); }
	void PlayNote4() { PlayNote(3); }
	void PlayNote5() { PlayNote(4); }

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void RestartLevel();
	void UpdateWater();

	TWeakObjectPtr<AEnvironmentController> Environment;
	FVector LastSafeLocation = FVector::ZeroVector;
	bool bHasSafeLocation = false;
	float NextSafeSampleTime = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DefaultContext;

	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AttackAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LockOnAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleRightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleLeftAction;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> NoteActions;

	FTimerHandle RestartTimer;
};
