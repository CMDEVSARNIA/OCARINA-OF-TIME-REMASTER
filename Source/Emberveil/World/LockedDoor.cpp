#include "World/LockedDoor.h"

#include "EmberveilStatics.h"
#include "Components/InventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ALockedDoor::ALockedDoor()
{
	BarrierColor = FLinearColor(0.35f, 0.2f, 0.08f);
}

void ALockedDoor::OnPlayerApproach(APawn* Player)
{
	if (IsOpen())
	{
		return;
	}

	UInventoryComponent* Inventory = Player->FindComponentByClass<UInventoryComponent>();
	if (Inventory && Inventory->ConsumeSmallKey())
	{
		Open();
		UEmberveilStatics::ShowMessage(this, TEXT("The small key turns. The door grinds open."));
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHintTime > 3.f)
	{
		LastHintTime = Now;
		UEmberveilStatics::ShowMessage(this, TEXT("Locked. A small key would open it."));
	}
}
