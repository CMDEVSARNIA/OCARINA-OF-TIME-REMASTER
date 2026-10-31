#include "Core/EmberveilGameMode.h"

#include "Core/EmberveilHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/EmberveilCharacter.h"
#include "World/TestDungeonBuilder.h"

AEmberveilGameMode::AEmberveilGameMode()
{
	DefaultPawnClass = AEmberveilCharacter::StaticClass();
	HUDClass = AEmberveilHUD::StaticClass();
}

void AEmberveilGameMode::StartPlay()
{
	if (TActorIterator<ATestDungeonBuilder> It(GetWorld()); It)
	{
		Dungeon = *It;
	}

	if (!Dungeon && bAutoBuildTestDungeon)
	{
		Dungeon = GetWorld()->SpawnActor<ATestDungeonBuilder>(TestDungeonLocation, FRotator::ZeroRotator);
	}

	// Dispatches BeginPlay, which builds the dungeon and moves already-spawned players into it.
	Super::StartPlay();
}

void AEmberveilGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Players that spawn after the dungeon was built (late join, restart) go to its entrance too.
	if (Dungeon && Dungeon->HasActorBegunPlay())
	{
		Dungeon->MovePlayersToStart();
	}
}
