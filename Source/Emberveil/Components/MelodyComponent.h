#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MelodyComponent.generated.h"

/** A named sequence of notes (0-based indices into the instrument's note set). */
USTRUCT(BlueprintType)
struct FCadence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	TArray<uint8> Notes;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNotePlayed, int32, Note);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCadencePlayed, FName, CadenceId);

/**
 * The Tidehorn: the player plays notes one at a time; when the most recent notes match a known
 * cadence, every IMelodyReceiver within HearingRadius is notified.
 */
UCLASS(ClassGroup = (Emberveil), meta = (BlueprintSpawnableComponent))
class EMBERVEIL_API UMelodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMelodyComponent();

	static constexpr int32 NumNotes = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	TArray<FCadence> KnownCadences;

	/** A pause longer than this starts a new phrase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	float NoteResetTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	float HearingRadius = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melody")
	int32 MaxRememberedNotes = 8;

	UPROPERTY(BlueprintAssignable, Category = "Melody")
	FOnNotePlayed OnNotePlayed;

	UPROPERTY(BlueprintAssignable, Category = "Melody")
	FOnCadencePlayed OnCadencePlayed;

	UFUNCTION(BlueprintCallable, Category = "Melody")
	void PlayNote(int32 Note);

	const TArray<uint8>& GetRecentNotes() const { return RecentNotes; }

	/** Human-readable notes of a cadence, e.g. "1 - 3 - 2 - 3". Empty if unknown. */
	UFUNCTION(BlueprintPure, Category = "Melody")
	FString DescribeCadence(FName CadenceId) const;

	static FString FormatNotes(const TArray<uint8>& Notes);

private:
	void NotifyReceivers(FName CadenceId);

	TArray<uint8> RecentNotes;
	float LastNoteTime = -100.f;
};
