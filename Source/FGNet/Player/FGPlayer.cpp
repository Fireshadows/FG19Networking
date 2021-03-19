#include "FGPlayer.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerState.h"
#include "../Components/FGMovementComponent.h"
#include "../FGMovementStatics.h"
#include "Net/UnrealNetwork.h"
#include "FGPlayerSettings.h"
#include "../Debug/UI/FGNetDebugWidget.h"
#include "../FGRocket.h"
#include "../FGPickup.h"

AFGPlayer::AFGPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->SetupAttachment(CollisionComponent);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);

	MovementComponent = CreateDefaultSubobject<UFGMovementComponent>(TEXT("MovementComponent"));

	SetReplicateMovement(false);
}

void AFGPlayer::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent->SetUpdatedComponent(CollisionComponent);

	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	SpawnRockets();
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FireCooldownElapsed -= DeltaTime;

	if (!ensure(PlayerSettings != nullptr))
		return;

	FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();
		
	if (IsLocallyControlled())
	{
		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float Acceleration = PlayerSettings->Acceleration;

		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->Friction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (PlayerSettings->MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
		const float MovementDirection = MovementVelocity > 0.0f ? Turn : -Turn;

		Yaw += (MovementDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection);

		MovementVelocity += Forward * Acceleration * DeltaTime;
		MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);

		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		//Send information
		Server_SendLocation(GetActorLocation(), GetActorRotation());
		Server_SendYaw(MovementComponent->GetFacingRotation().Yaw);
	}
	else
	{
		MovementComponent->SetFacingRotation(FRotator(0.0f, ReplicatedYaw, 0.0f));
		SetActorRotation(MovementComponent->GetFacingRotation());

		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), ReplicatedLocation, DeltaTime, 10.0f);
		SetActorLocation(NewLocation);

		//float time = DeltaTime * 6;
		//SetActorLocation(FMath::Lerp<FVector>(GetActorLocation(), TargetLocation, time));
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);

	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFGPlayer::Handle_FirePressed);

	PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);

}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}
	return 0;
}

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (IsLocallyControlled())
	{
		Server_OnPickup(Pickup);
	}
}

void AFGPlayer::Client_OnPickupRockets_Implementation(int32 PickedUpRockets) {
	
	NumRockets += PickedUpRockets;
	BP_OnNumRocketsChanged(NumRockets);
}

void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	ServerNumRockets += Pickup->NumRockets;
	Client_OnPickupRockets(Pickup->NumRockets);
}

void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend, const FRotator& RotationToSend)
{
	Multicast_SendLocation(LocationToSend, RotationToSend);
}


void AFGPlayer::Multicast_SendLocation_Implementation(const FVector& LocationToSend, const FRotator& RotationToSend)
{
	ReplicatedLocation = LocationToSend;
	/*if (!IsLocallyControlled())
	{
		TargetLocation = LocationToSend;
		//SetActorLocation(LocationToSend);
		//SetActorRotation(RotationToSend);
	}*/
}


void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}

void AFGPlayer::ShowDebugMenu()
{
	CreateDebugWidget();

	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Visible);
	DebugMenuInstance->BP_OnShowWidget();
}

void AFGPlayer::HideDebugMenu()
{
	if (DebugMenuInstance == nullptr)
		return;


	DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	DebugMenuInstance->BP_OnHideWidget();
}

void AFGPlayer::Handle_Accelerate(float Value)
{
	Forward = Value;
}

void AFGPlayer::Handle_Turn(float Value)
{
	Turn = Value;
}

void AFGPlayer::Handle_BrakePressed()
{
	bBrake = true;
}

void AFGPlayer::Handle_BrakeReleased()
{
	bBrake = false;
}

int32 AFGPlayer::GetNumActiveRockets() const
{
	int32 NumActive = 0;
	for (AFGRocket* Rocket : RocketInstances)
	{
		if(!Rocket->IsFree())
			NumActive++;
	}

	return NumActive;
}

void AFGPlayer::Handle_FirePressed()
{
	FireRocket();
}

void AFGPlayer::FireRocket()
{
	if (FireCooldownElapsed > 0.0f)
		return;

	if (NumRockets <= 0 && !bUnlimitedRockets)
		return;

	if (GetNumActiveRockets() >= MaxActiveRockets)
		return;

	AFGRocket* NewRocket = GetFreeRocket();

	if (!ensure(NewRocket != nullptr))
		return;

	FireCooldownElapsed = PlayerSettings->FireCooldown;

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
		else
		{
			NumRockets--;
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
		}
	}
}

void AFGPlayer::SpawnRockets()
{
	if (HasAuthority() && RocketClass != nullptr)
	{
		const int32 RocketCache = 8;

		for (int32 Index = 0; Index <RocketCache; ++Index)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.ObjectFlags = RF_Transient;
			SpawnParams.Instigator = this;
			SpawnParams.Owner = this;
			AFGRocket* NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			RocketInstances.Add(NewRocketInstance);
		}
	}
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLoc;
}

AFGRocket* AFGPlayer::GetFreeRocket() const
{
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (Rocket == nullptr)
			continue;

		if (Rocket->IsFree())
			return Rocket;
	}

	return nullptr;
}

void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& FacingRotation)
{
	if ((ServerNumRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket);
	}
	else
	{
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(FacingRotation.Yaw, GetActorForwardVector().Rotation().Yaw) * 0.5f;
		const FRotator NewFacingRotation = FacingRotation + FRotator(0.0f, DeltaYaw, 0.0f);
		ServerNumRockets--;
		Multicast_FireRocket(NewRocket, RocketStartLocation, NewFacingRotation);
	}
}

void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove)
{
	RocketToRemove->MakeFree();
}

void AFGPlayer::Multicast_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation)
{
	if (!ensure(NewRocket != nullptr))
		return;

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		NewRocket->ApplyCorrection(RocketFacingRotation.Vector());
	}
	else
	{
		NumRockets--;
		NewRocket->StartMoving(RocketFacingRotation.Vector(), RocketStartLocation);
	}

}

void AFGPlayer::Cheat_IncreaseRockets(int32 InNumRockets)
{

}

void AFGPlayer::Handle_DebugMenuPressed()
{
	bShowDebugMenu = !bShowDebugMenu;

	if (bShowDebugMenu)
		ShowDebugMenu();
	else
		HideDebugMenu();
}

void AFGPlayer::CreateDebugWidget()
{
	if (DebugMenuClass == nullptr)
		return;

	if (!IsLocallyControlled())
		return;

	if (DebugMenuInstance == nullptr)
	{
		DebugMenuInstance = CreateWidget<UFGNetDebugWidget>(GetWorld(), DebugMenuClass);
		DebugMenuInstance->AddToViewport();
	}
}

void AFGPlayer::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
}