#include "EmberveilStatics.h"

#include "Emberveil.h"
#include "Core/EmberveilHUD.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

void UEmberveilStatics::ShowMessage(const UObject* WorldContextObject, const FString& Message, float Duration)
{
	UE_LOG(LogEmberveil, Log, TEXT("%s"), *Message);

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (AEmberveilHUD* HUD = Cast<AEmberveilHUD>(PC->GetHUD()))
		{
			HUD->ShowMessage(Message, Duration);
		}
	}
}

void UEmberveilStatics::TintMesh(UMeshComponent* Mesh, FLinearColor Color)
{
	if (!Mesh || Mesh->GetNumMaterials() == 0)
	{
		return;
	}

	// Reuses the existing dynamic instance if slot 0 already holds one.
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
