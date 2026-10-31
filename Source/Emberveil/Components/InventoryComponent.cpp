#include "Components/InventoryComponent.h"

void UInventoryComponent::AddSmallKeys(int32 Count)
{
	if (Count > 0)
	{
		SmallKeys += Count;
		OnInventoryChanged.Broadcast();
	}
}

bool UInventoryComponent::ConsumeSmallKey()
{
	if (SmallKeys <= 0)
	{
		return false;
	}
	--SmallKeys;
	OnInventoryChanged.Broadcast();
	return true;
}

void UInventoryComponent::GrantRelic()
{
	if (!bHasRelic)
	{
		bHasRelic = true;
		OnInventoryChanged.Broadcast();
	}
}
