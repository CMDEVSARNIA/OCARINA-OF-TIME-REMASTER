#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EmberveilHUD.generated.h"

class AEmberveilCharacter;

/** Canvas HUD (no UMG assets needed): health, keys, Tidehorn notes, lock-on reticle, messages, controls. */
UCLASS()
class EMBERVEIL_API AEmberveilHUD : public AHUD
{
	GENERATED_BODY()

public:
	void ShowMessage(const FString& Message, float Duration);

	virtual void DrawHUD() override;

private:
	struct FHudMessage
	{
		FString Text;
		float ExpireTime = 0.f;
	};

	void DrawStatus(const AEmberveilCharacter* Player);
	void DrawLockOnReticle(const AEmberveilCharacter* Player);
	void DrawMessages();
	void DrawControls();

	TArray<FHudMessage> Messages;
};
