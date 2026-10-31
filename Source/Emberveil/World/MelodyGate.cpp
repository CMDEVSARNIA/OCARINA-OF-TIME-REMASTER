#include "World/MelodyGate.h"

#include "EmberveilStatics.h"
#include "Components/MelodyComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AMelodyGate::AMelodyGate()
{
	BarrierColor = FLinearColor(0.1f, 0.3f, 0.55f);
}

void AMelodyGate::OnCadenceHeard(FName CadenceId, AActor* Player)
{
	if (!IsOpen() && CadenceId == RequiredCadence)
	{
		Open();
		UEmberveilStatics::ShowMessage(this, TEXT("The tide answers. The gate sinks into the floor."), 4.f);
	}
}

void AMelodyGate::OnPlayerApproach(APawn* Player)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (IsOpen() || Now - LastHintTime < 6.f)
	{
		return;
	}
	LastHintTime = Now;

	const UMelodyComponent* Melody = Player->FindComponentByClass<UMelodyComponent>();
	const FString Notes = Melody ? Melody->DescribeCadence(RequiredCadence) : FString();
	UEmberveilStatics::ShowMessage(this, FString::Printf(TEXT("%s  [ %s ]"), *HintText, Notes.IsEmpty() ? TEXT("?") : *Notes), 6.f);
}
