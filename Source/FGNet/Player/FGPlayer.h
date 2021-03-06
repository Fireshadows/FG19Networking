#pragma once

#include "GameFrameWork/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFGPlayerSettings;
class UFGNetDebugWidget;
class AFGRocket;
class AFGPickup;

UCLASS()
class FGNET_API AFGPlayer : public APawn
{
	GENERATED_BODY()

public:
	AFGPlayer();

protected:

	virtual void BeginPlay() override;

public:

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = Settings)
	UFGPlayerSettings* PlayerSettings = nullptr;
	
	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }

	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	UPROPERTY(EditAnywhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;
	

	void OnPickup(AFGPickup* Pickup);

	UFUNCTION(Server, reliable)
	void Server_OnPickup(AFGPickup* Pickup);

	UFUNCTION(Client, Reliable)
	void Client_OnPickupRockets(int32 PickedUpRockets);


	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend, const FRotator& RotationToSend);

	UFUNCTION(Server, Unreliable)
	void Server_SendYaw(float NewYaw);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendLocation(const FVector& LocationToSend, const FRotator& RotationToSend);
	
	FVector TargetLocation;

	
	void ShowDebugMenu();
	void HideDebugMenu();
	
	UFUNCTION(BlueprintPure)
	int32 GetNumRockets() const { return NumRockets; };

	UFUNCTION(BlueprintImplementableEvent, Category = Player, meta = (DisplayName = "On Num Rockets Changed"))
	void BP_OnNumRocketsChanged(int32 NewNumRockets);

	int32 GetNumActiveRockets() const;
	
	void FireRocket();

	void SpawnRockets();

private:
	void AddMovementVelocity(float DeltaTime);

	UPROPERTY(Replicated)
	int32 ServerNumRockets = 0;

	//UPROPERTY(Replicated)
	int32 NumRockets = 0;

	FVector GetRocketStartLocation() const;

	AFGRocket* GetFreeRocket() const;

	UFUNCTION(Server, Unreliable)
	void Server_SendMovement(const FVector& ClientLocation, float TimeStamp, float ClientForward, float ClientYaw);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendMovement(const FVector& InClientLocation, float TimeStamp, float ClientForward, float ClientYaw);

	UFUNCTION(Server, Reliable)
	void Server_FireRocket(AFGRocket* NewRocket, const FVector& RocketSTartLocation, const FRotator& RocketFacignRotation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(Client, Reliable)
	void Client_RemoveRocket(AFGRocket* RocketToRemove);

	UFUNCTION(BlueprintCallable)
	void Cheat_IncreaseRockets(int32 InNumRockets);

	UPROPERTY(Replicated, Transient)
	TArray<AFGRocket*> RocketInstances;

	UPROPERTY(EditAnywhere, Category = Weapon)
	TSubclassOf<AFGRocket> RocketClass;

	int32 MaxActiveRockets = 3;

	float FireCooldownElapsed = 0.0f;

	UPROPERTY(EditAnywhere, Category = Weapon)
		bool bUnlimitedRockets = false;

	void Handle_Accelerate(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();
	void Handle_FirePressed();
	
	void Handle_DebugMenuPressed();

	void CreateDebugWidget();
	
	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;
	
	bool bShowDebugMenu = false;

	UPROPERTY(Replicated)
	float ReplicatedYaw = 0.0f;

	UPROPERTY(Replicated)
	FVector ReplicatedLocation;

	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;
	
	bool bBrake = false;

	float ClientTimeStamp = 0.0f;
	float ServerTimeStamp = 0.0f;
	float LastCorrectionDelta = 0.0f;

	UPROPERTY(EditAnywhere)
	bool bPerformNetworkSmoothing = true;

	FVector OriginalMeshOffset = FVector::ZeroVector;

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	USpringArmComponent* SpringArmComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Movement)
	UFGMovementComponent* MovementComponent;

};
