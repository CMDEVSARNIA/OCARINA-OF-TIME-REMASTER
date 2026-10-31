#include "Player/EmberveilCharacter.h"

#include "Emberveil.h"
#include "EmberveilStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/LockOnComponent.h"
#include "Components/MeleeComboComponent.h"
#include "Components/MelodyComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AEmberveilCharacter::AEmberveilCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 600.f, 0.f);
	Move->JumpZVelocity = 620.f;
	Move->AirControl = 0.35f;
	Move->MaxWalkSpeed = 520.f;
	Move->MinAnalogWalkSpeed = 20.f;
	Move->BrakingDecelerationWalking = 2000.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Greybox body: replace with a skeletal mesh + animation blueprint when art exists.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -21.f));
	BodyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.5f));
	if (Cylinder.Succeeded()) { BodyMesh->SetStaticMesh(Cylinder.Object); }

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetCapsuleComponent());
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 72.f));
	HeadMesh->SetRelativeScale3D(FVector(0.45f));
	if (Sphere.Succeeded()) { HeadMesh->SetStaticMesh(Sphere.Object); }

	BladePivot = CreateDefaultSubobject<USceneComponent>(TEXT("BladePivot"));
	BladePivot->SetupAttachment(GetCapsuleComponent());
	BladePivot->SetRelativeLocation(FVector(10.f, 0.f, 15.f));

	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(BladePivot);
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BladeMesh->SetRelativeLocation(FVector(75.f, 0.f, 0.f));
	BladeMesh->SetRelativeScale3D(FVector(1.1f, 0.06f, 0.12f));
	if (Cube.Succeeded()) { BladeMesh->SetStaticMesh(Cube.Object); }

	Lantern = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lantern"));
	Lantern->SetupAttachment(GetCapsuleComponent());
	Lantern->SetRelativeLocation(FVector(10.f, -45.f, 20.f));
	Lantern->SetIntensity(2500.f);
	Lantern->SetAttenuationRadius(800.f);
	Lantern->SetLightColor(FLinearColor(1.f, 0.7f, 0.4f));
	Lantern->SetCastShadows(false);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 60.f;
	Health->InvulnerabilityTime = 0.8f;

	LockOn = CreateDefaultSubobject<ULockOnComponent>(TEXT("LockOn"));
	Combo = CreateDefaultSubobject<UMeleeComboComponent>(TEXT("Combo"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	Melody = CreateDefaultSubobject<UMelodyComponent>(TEXT("Melody"));
}

void AEmberveilCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UEmberveilStatics::TintMesh(BodyMesh, FLinearColor(0.35f, 0.22f, 0.15f));
	UEmberveilStatics::TintMesh(HeadMesh, FLinearColor(0.8f, 0.65f, 0.5f));
	UEmberveilStatics::TintMesh(BladeMesh, FLinearColor(0.75f, 0.78f, 0.82f));
}

void AEmberveilCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health->OnDeath.AddDynamic(this, &AEmberveilCharacter::HandleDeath);
}

void AEmberveilCharacter::BuildInputMappings()
{
	if (DefaultContext)
	{
		return;
	}

	DefaultContext = NewObject<UInputMappingContext>(this, TEXT("IMC_EmberveilDefault"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	AttackAction = MakeAction(TEXT("IA_Attack"), EInputActionValueType::Boolean);
	LockOnAction = MakeAction(TEXT("IA_LockOn"), EInputActionValueType::Boolean);
	CycleRightAction = MakeAction(TEXT("IA_CycleRight"), EInputActionValueType::Boolean);
	CycleLeftAction = MakeAction(TEXT("IA_CycleLeft"), EInputActionValueType::Boolean);

	UInputMappingContext* Ctx = DefaultContext;
	auto Negate = [Ctx](bool bX, bool bY)
	{
		UInputModifierNegate* Mod = NewObject<UInputModifierNegate>(Ctx);
		Mod->bX = bX;
		Mod->bY = bY;
		Mod->bZ = false;
		return Mod;
	};

	// Move: WASD -> (X = right, Y = forward). Swizzle turns a 1D key press into the Y axis.
	{ FEnhancedActionKeyMapping& M = Ctx->MapKey(MoveAction, EKeys::W); M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Ctx)); }
	{ FEnhancedActionKeyMapping& M = Ctx->MapKey(MoveAction, EKeys::S); M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Ctx)); M.Modifiers.Add(Negate(true, true)); }
	{ FEnhancedActionKeyMapping& M = Ctx->MapKey(MoveAction, EKeys::A); M.Modifiers.Add(Negate(true, true)); }
	Ctx->MapKey(MoveAction, EKeys::D);
	{ FEnhancedActionKeyMapping& M = Ctx->MapKey(MoveAction, EKeys::Gamepad_Left2D); M.Modifiers.Add(NewObject<UInputModifierDeadZone>(Ctx)); }

	// Look: invert Y so pushing up looks up.
	{ FEnhancedActionKeyMapping& M = Ctx->MapKey(LookAction, EKeys::Mouse2D); M.Modifiers.Add(Negate(false, true)); }
	{ FEnhancedActionKeyMapping& M = Ctx->MapKey(LookAction, EKeys::Gamepad_Right2D); M.Modifiers.Add(NewObject<UInputModifierDeadZone>(Ctx)); M.Modifiers.Add(Negate(false, true)); }

	Ctx->MapKey(JumpAction, EKeys::SpaceBar);
	Ctx->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);

	Ctx->MapKey(AttackAction, EKeys::LeftMouseButton);
	Ctx->MapKey(AttackAction, EKeys::Gamepad_FaceButton_Left);

	Ctx->MapKey(LockOnAction, EKeys::Q);
	Ctx->MapKey(LockOnAction, EKeys::MiddleMouseButton);
	Ctx->MapKey(LockOnAction, EKeys::Gamepad_RightThumbstick);

	Ctx->MapKey(CycleRightAction, EKeys::E);
	Ctx->MapKey(CycleRightAction, EKeys::MouseScrollDown);
	Ctx->MapKey(CycleRightAction, EKeys::Gamepad_RightShoulder);
	Ctx->MapKey(CycleLeftAction, EKeys::MouseScrollUp);
	Ctx->MapKey(CycleLeftAction, EKeys::Gamepad_LeftShoulder);

	// Tidehorn notes 1-5: number keys, or D-pad + top face button.
	const FKey KeyboardNotes[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	const FKey PadNotes[] = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_FaceButton_Top };
	NoteActions.Reset();
	for (int32 i = 0; i < UMelodyComponent::NumNotes; ++i)
	{
		UInputAction* Note = NewObject<UInputAction>(this, *FString::Printf(TEXT("IA_Note%d"), i + 1));
		Note->ValueType = EInputActionValueType::Boolean;
		Ctx->MapKey(Note, KeyboardNotes[i]);
		Ctx->MapKey(Note, PadNotes[i]);
		NoteActions.Add(Note);
	}
}

void AEmberveilCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	BuildInputMappings();
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultContext, 0);
		}
	}
}

void AEmberveilCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildInputMappings();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogEmberveil, Error, TEXT("Enhanced Input is not the default input component. Check Config/DefaultInput.ini."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEmberveilCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEmberveilCharacter::Look);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	Input->BindAction(AttackAction, ETriggerEvent::Started, this, &AEmberveilCharacter::Attack);
	Input->BindAction(LockOnAction, ETriggerEvent::Started, this, &AEmberveilCharacter::ToggleLockOn);
	Input->BindAction(CycleRightAction, ETriggerEvent::Started, this, &AEmberveilCharacter::CycleTargetRight);
	Input->BindAction(CycleLeftAction, ETriggerEvent::Started, this, &AEmberveilCharacter::CycleTargetLeft);

	if (NoteActions.Num() == UMelodyComponent::NumNotes)
	{
		Input->BindAction(NoteActions[0], ETriggerEvent::Started, this, &AEmberveilCharacter::PlayNote1);
		Input->BindAction(NoteActions[1], ETriggerEvent::Started, this, &AEmberveilCharacter::PlayNote2);
		Input->BindAction(NoteActions[2], ETriggerEvent::Started, this, &AEmberveilCharacter::PlayNote3);
		Input->BindAction(NoteActions[3], ETriggerEvent::Started, this, &AEmberveilCharacter::PlayNote4);
		Input->BindAction(NoteActions[4], ETriggerEvent::Started, this, &AEmberveilCharacter::PlayNote5);
	}
}

void AEmberveilCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix Basis(YawRotation);
	AddMovementInput(Basis.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(Basis.GetUnitAxis(EAxis::Y), Axis.X);
}

void AEmberveilCharacter::Look(const FInputActionValue& Value)
{
	// The lock-on component owns the camera while a target is held.
	if (LockOn->IsLockedOn())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AEmberveilCharacter::Attack()
{
	if (!Health->IsDead())
	{
		Combo->RequestAttack();
	}
}

void AEmberveilCharacter::ToggleLockOn()
{
	LockOn->ToggleLockOn();
}

void AEmberveilCharacter::CycleTargetRight()
{
	LockOn->CycleTarget(true);
}

void AEmberveilCharacter::CycleTargetLeft()
{
	LockOn->CycleTarget(false);
}

void AEmberveilCharacter::PlayNote(int32 Note)
{
	if (!Health->IsDead())
	{
		Melody->PlayNote(Note);
	}
}

void AEmberveilCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Placeholder swing visual: sweep the blade around the body. Third hit is a full spin.
	float Yaw = 100.f;
	if (Combo->IsSwinging())
	{
		const float Alpha = FMath::InterpEaseOut(0.f, 1.f, Combo->GetSwingAlpha(), 2.f);
		switch (Combo->GetComboIndex())
		{
		case 0:  Yaw = FMath::Lerp(80.f, -80.f, Alpha); break;
		case 1:  Yaw = FMath::Lerp(-80.f, 80.f, Alpha); break;
		default: Yaw = FMath::Lerp(100.f, -260.f, Alpha); break;
		}
	}
	BladePivot->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
}

void AEmberveilCharacter::HandleDeath(AActor* Killer)
{
	LockOn->ClearLockOn();
	GetCharacterMovement()->DisableMovement();
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
	}

	UEmberveilStatics::ShowMessage(this, TEXT("Your lantern gutters out... restarting."), 3.f);
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AEmberveilCharacter::RestartLevel, 2.5f, false);
}

void AEmberveilCharacter::RestartLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
