#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/** Minimal dungeon inventory: small keys (consumed by locked doors) and the relic. */
UCLASS(ClassGroup = (Emberveil), meta = (BlueprintSpawnableComponent))
class EMBERVEIL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddSmallKeys(int32 Count = 1);

	/** Returns true and removes one key if the player has any. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ConsumeSmallKey();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void GrantRelic();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetSmallKeys() const { return SmallKeys; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasRelic() const { return bHasRelic; }

private:
	int32 SmallKeys = 0;
	bool bHasRelic = false;
};
