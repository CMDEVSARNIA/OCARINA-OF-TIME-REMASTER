#include "Components/MelodyComponent.h"

#include "EmberveilStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "World/MelodyReceiver.h"

UMelodyComponent::UMelodyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	FCadence Tidecall;
	Tidecall.Id = TEXT("Tidecall");
	Tidecall.DisplayName = TEXT("Tidecall");
	Tidecall.Notes = { 0, 2, 1, 2 };
	KnownCadences.Add(Tidecall);

	FCadence Emberwake;
	Emberwake.Id = TEXT("Emberwake");
	Emberwake.DisplayName = TEXT("Emberwake");
	Emberwake.Notes = { 4, 3, 0, 0 };
	KnownCadences.Add(Emberwake);
}

void UMelodyComponent::PlayNote(int32 Note)
{
	if (Note < 0 || Note >= NumNotes)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastNoteTime > NoteResetTime)
	{
		RecentNotes.Reset();
	}
	LastNoteTime = Now;

	RecentNotes.Add(static_cast<uint8>(Note));
	while (RecentNotes.Num() > FMath::Max(1, MaxRememberedNotes))
	{
		RecentNotes.RemoveAt(0);
	}
	OnNotePlayed.Broadcast(Note);

	for (const FCadence& Cadence : KnownCadences)
	{
		const int32 Len = Cadence.Notes.Num();
		if (Len == 0 || RecentNotes.Num() < Len)
		{
			continue;
		}

		bool bMatch = true;
		const int32 Start = RecentNotes.Num() - Len;
		for (int32 i = 0; i < Len && bMatch; ++i)
		{
			bMatch = RecentNotes[Start + i] == Cadence.Notes[i];
		}

		if (bMatch)
		{
			RecentNotes.Reset();
			UEmberveilStatics::ShowMessage(this, FString::Printf(TEXT("You played the %s."), *Cadence.DisplayName));
			OnCadencePlayed.Broadcast(Cadence.Id);
			NotifyReceivers(Cadence.Id);
			break;
		}
	}
}

FString UMelodyComponent::DescribeCadence(FName CadenceId) const
{
	for (const FCadence& Cadence : KnownCadences)
	{
		if (Cadence.Id == CadenceId)
		{
			return FormatNotes(Cadence.Notes);
		}
	}
	return FString();
}

FString UMelodyComponent::FormatNotes(const TArray<uint8>& Notes)
{
	TArray<FString> Parts;
	for (const uint8 Note : Notes)
	{
		Parts.Add(FString::FromInt(Note + 1));
	}
	return FString::Join(Parts, TEXT(" - "));
}

void UMelodyComponent::NotifyReceivers(FName CadenceId)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Collect first: receivers may spawn or destroy actors when they react.
	TArray<AActor*> Listeners;
	const float RadiusSq = FMath::Square(HearingRadius);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor != Owner && Actor->Implements<UMelodyReceiver>() &&
			FVector::DistSquared(Actor->GetActorLocation(), Owner->GetActorLocation()) <= RadiusSq)
		{
			Listeners.Add(Actor);
		}
	}

	for (AActor* Listener : Listeners)
	{
		if (IMelodyReceiver* Receiver = Cast<IMelodyReceiver>(Listener))
		{
			Receiver->OnCadenceHeard(CadenceId, Owner);
		}
	}
}
