#include "Components/HealthComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	bInitialized = true;

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::HandleTakeAnyDamage);
	}
}

void UHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}

	const float Old = Health;
	Health = FMath::Min(Health + Amount, MaxHealth);
	OnHealthChanged.Broadcast(Health, Health - Old);
}

void UHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDamageTime < InvulnerabilityTime)
	{
		return;
	}
	LastDamageTime = Now;

	const float Old = Health;
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(Health, Health - Old);

	if (Health <= 0.f)
	{
		OnDeath.Broadcast(DamageCauser);
	}
}
