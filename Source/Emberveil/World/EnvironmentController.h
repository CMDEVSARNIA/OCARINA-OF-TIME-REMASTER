#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnvironmentController.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UStaticMeshComponent;

/**
 * World clock + sky + tide. Rotates a sun and moon through a physical sky, and moves a
 * water plane up and down with two tides per day. The Tidecall can hold the tide low.
 */
UCLASS()
class EMBERVEIL_API AEnvironmentController : public AActor
{
	GENERATED_BODY()

public:
	AEnvironmentController();

	/** Real-time minutes for one full in-game day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (ClampMin = "0.5"))
	float DayLengthMinutes = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time", meta = (ClampMin = "0", ClampMax = "24"))
	float StartHour = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time")
	float SunYaw = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float SeaLevel = 0.f;

	/** Water moves between SeaLevel - TideAmplitude and SeaLevel + TideAmplitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float TideAmplitude = 200.f;

	/** How fast the water surface may move, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float TideSpeed = 60.f;

	/** Half size of the square water surface around this actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float WaterHalfExtent = 200000.f;

	static AEnvironmentController* Find(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetHour() const { return TimeOfDay; }

	UFUNCTION(BlueprintPure, Category = "Time")
	int32 GetDay() const { return Day; }

	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsNight() const;

	UFUNCTION(BlueprintPure, Category = "Time")
	FString GetClockText() const;

	UFUNCTION(BlueprintPure, Category = "Tide")
	FString GetTideText() const;

	UFUNCTION(BlueprintPure, Category = "Tide")
	float GetWaterLevel() const { return SeaLevel + CurrentTide; }

	/** False when the location is outside the water surface (e.g. an interior far away). */
	bool GetWaterLevelAt(const FVector& WorldLocation, float& OutLevel) const;

	UFUNCTION(BlueprintCallable, Category = "Tide")
	void ForceLowTide(float Seconds);

	void SetWaterHalfExtent(float NewHalfExtent);

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UStaticMeshComponent> WaterMesh;

private:
	void UpdateSky();
	void UpdateTide(float DeltaSeconds);
	void ApplyWaterTransform();
	float GetSunElevation() const;
	float GetTidePhase() const;

	float TimeOfDay = 7.5f;
	int32 Day = 1;
	float CurrentTide = 0.f;
	float ForcedLowUntil = -1.f;
};
