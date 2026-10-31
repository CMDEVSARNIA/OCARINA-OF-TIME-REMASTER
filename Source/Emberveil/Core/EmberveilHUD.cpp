#include "Core/EmberveilHUD.h"

#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/LockOnComponent.h"
#include "Components/LockOnTargetComponent.h"
#include "Components/MelodyComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/EmberveilCharacter.h"

void AEmberveilHUD::ShowMessage(const FString& Message, float Duration)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Messages.Add({ Message, Now + Duration });
	if (Messages.Num() > 4)
	{
		Messages.RemoveAt(0);
	}
}

void AEmberveilHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	if (const AEmberveilCharacter* Player = Cast<AEmberveilCharacter>(GetOwningPawn()))
	{
		DrawStatus(Player);
		DrawLockOnReticle(Player);
	}
	DrawMessages();
	DrawControls();
}

void AEmberveilHUD::DrawStatus(const AEmberveilCharacter* Player)
{
	UFont* Font = GEngine->GetMediumFont();
	const float X = 40.f;
	float Y = 40.f;

	// Health bar.
	const float BarWidth = 300.f;
	const float Fraction = Player->GetHealth() ? Player->GetHealth()->GetHealthFraction() : 0.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Y - 2.f, BarWidth + 4.f, 22.f);
	DrawRect(FLinearColor(0.9f, 0.35f, 0.1f), X, Y, BarWidth * Fraction, 18.f);
	Y += 32.f;

	if (const UInventoryComponent* Inventory = Player->GetInventory())
	{
		DrawText(FString::Printf(TEXT("Small keys: %d"), Inventory->GetSmallKeys()), FLinearColor(1.f, 0.85f, 0.3f), X, Y, Font);
		Y += 24.f;
		if (Inventory->HasRelic())
		{
			DrawText(TEXT("Ember Relic: claimed"), FLinearColor(1.f, 0.55f, 0.2f), X, Y, Font);
			Y += 24.f;
		}
	}

	if (const UMelodyComponent* Melody = Player->GetMelody())
	{
		const FString Notes = UMelodyComponent::FormatNotes(Melody->GetRecentNotes());
		DrawText(FString::Printf(TEXT("Tidehorn: %s"), Notes.IsEmpty() ? TEXT("...") : *Notes), FLinearColor(0.5f, 0.8f, 1.f), X, Y, Font);
	}
}

void AEmberveilHUD::DrawLockOnReticle(const AEmberveilCharacter* Player)
{
	const ULockOnComponent* LockOn = Player->GetLockOn();
	const ULockOnTargetComponent* Target = LockOn ? LockOn->GetCurrentTarget() : nullptr;
	APlayerController* PC = GetOwningPlayerController();
	if (!Target || !LockOn->IsLockedOn() || !PC)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TargetLocation = Target->GetComponentLocation();
	if (FVector::DotProduct(ViewRotation.Vector(), TargetLocation - ViewLocation) <= 0.f)
	{
		return; // Behind the camera.
	}

	const FVector Screen = Project(TargetLocation);
	const float S = 18.f;
	const FLinearColor Color(1.f, 0.4f, 0.1f);
	DrawLine(Screen.X, Screen.Y - S, Screen.X + S, Screen.Y, Color, 2.f);
	DrawLine(Screen.X + S, Screen.Y, Screen.X, Screen.Y + S, Color, 2.f);
	DrawLine(Screen.X, Screen.Y + S, Screen.X - S, Screen.Y, Color, 2.f);
	DrawLine(Screen.X - S, Screen.Y, Screen.X, Screen.Y - S, Color, 2.f);
}

void AEmberveilHUD::DrawMessages()
{
	const float Now = GetWorld()->GetTimeSeconds();
	Messages.RemoveAll([Now](const FHudMessage& Message) { return Message.ExpireTime <= Now; });

	UFont* Font = GEngine->GetMediumFont();
	float Y = 110.f;
	for (const FHudMessage& Message : Messages)
	{
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Message.Text, Width, Height, Font);
		const float X = (Canvas->ClipX - Width) * 0.5f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 10.f, Y - 4.f, Width + 20.f, Height + 8.f);
		DrawText(Message.Text, FLinearColor::White, X, Y, Font);
		Y += Height + 14.f;
	}
}

void AEmberveilHUD::DrawControls()
{
	static const TCHAR* Lines[] =
	{
		TEXT("WASD move  |  Mouse look  |  Space jump  |  LMB attack (x3 combo)"),
		TEXT("Q / MMB lock-on (no target: recenter camera)  |  E / wheel cycle target"),
		TEXT("1-5 play Tidehorn notes"),
	};

	UFont* Font = GEngine->GetSmallFont();
	float Y = Canvas->ClipY - 20.f * static_cast<float>(UE_ARRAY_COUNT(Lines)) - 20.f;
	for (const TCHAR* Line : Lines)
	{
		DrawText(Line, FLinearColor(0.8f, 0.8f, 0.8f, 0.8f), 40.f, Y, Font);
		Y += 20.f;
	}
}
