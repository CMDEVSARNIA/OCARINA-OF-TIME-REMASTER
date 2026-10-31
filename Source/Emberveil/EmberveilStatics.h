#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmberveilStatics.generated.h"

class UMeshComponent;
class UStaticMesh;

UCLASS()
class EMBERVEIL_API UEmberveilStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Shows a short message on the player's HUD and writes it to the log. */
	UFUNCTION(BlueprintCallable, Category = "Emberveil", meta = (WorldContext = "WorldContextObject"))
	static void ShowMessage(const UObject* WorldContextObject, const FString& Message, float Duration = 3.f);

	/** Shows a large centred title (e.g. a region name) with an optional subtitle. */
	UFUNCTION(BlueprintCallable, Category = "Emberveil", meta = (WorldContext = "WorldContextObject"))
	static void ShowTitle(const UObject* WorldContextObject, const FString& Title, const FString& Subtitle, float Duration = 4.f);

	/**
	 * Returns /Game/Emberveil/Meshes/<Name> if the art kit has been imported
	 * (Tools/Unreal/import_assets.py), otherwise null so callers can fall back to greybox shapes.
	 */
	static UStaticMesh* LoadArtMesh(const TCHAR* Name);

	/** Loads /Game/Emberveil/<Folder>/<Name> of the given class if it exists; null otherwise. */
	static UObject* LoadArtObject(const TCHAR* Folder, const TCHAR* Name, UClass* Class);

	template <class T>
	static T* LoadArt(const TCHAR* Folder, const TCHAR* Name)
	{
		return Cast<T>(LoadArtObject(Folder, Name, T::StaticClass()));
	}

	/** Tints a mesh that uses the engine's BasicShapeMaterial (vector parameter "Color"). */
	UFUNCTION(BlueprintCallable, Category = "Emberveil")
	static void TintMesh(UMeshComponent* Mesh, FLinearColor Color);
};
