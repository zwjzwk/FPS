
#include "FPSTeachBaseCharacter.h"

#include "Blueprint/UserWidget.h"
#include "Components/DecalComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


AFPSTeachBaseCharacter::AFPSTeachBaseCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

#pragma region Component
	PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PlayerCamera"));
	if(PlayerCamera)
	{
		PlayerCamera->SetupAttachment(RootComponent);
		PlayerCamera->bUsePawnControlRotation = true;
	}
	
	FPArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPArmsMesh"));
	if(FPArmsMesh)
	{
		FPArmsMesh->SetupAttachment(PlayerCamera);
		FPArmsMesh->SetOnlyOwnerSee(true);
	}

	Mesh->SetOwnerNoSee(true);

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
#pragma endregion
}

void AFPSTeachBaseCharacter::DelayBeginPlayCallBack()
{
	FPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
	if(FPSPlayerController)
	{
		FPSPlayerController->CreatePlayerUI();
	}
	else
	{
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelayBeginPlayCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0;
		UKismetSystemLibrary::Delay(this, 0.5, ActionInfo);
	}
}

// Called when the game starts or when spawned
void AFPSTeachBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health = 100;
	IsFiring = false;
	IsReloading = false;
	IsAiming = false;
	
	OnTakePointDamage.AddDynamic(this, &AFPSTeachBaseCharacter::OnHit);
	
	ClientArmsAnimBP = FPArmsMesh->GetAnimInstance();
	ServerBodysAnimBP = Mesh->GetAnimInstance();
	
	FPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
	if(FPSPlayerController)
	{
		FPSPlayerController->CreatePlayerUI();
	}
	else
	{
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelayBeginPlayCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0;
		UKismetSystemLibrary::Delay(this, 0.5, ActionInfo);
	}

	StartWithKindOfWeapon();
}

void AFPSTeachBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSTeachBaseCharacter, IsFiring, COND_None);
	DOREPLIFETIME_CONDITION(AFPSTeachBaseCharacter, IsReloading, COND_None);
	DOREPLIFETIME_CONDITION(AFPSTeachBaseCharacter, ActiveWeapon, COND_None);
	DOREPLIFETIME_CONDITION(AFPSTeachBaseCharacter, IsAiming, COND_None);
}



// Called every frame
void AFPSTeachBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AFPSTeachBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	InputComponent->BindAction(TEXT("LowSpeedWalk"), IE_Pressed, this, &AFPSTeachBaseCharacter::LowSpeedWalkAction);
	InputComponent->BindAction(TEXT("LowSpeedWalk"), IE_Released, this, &AFPSTeachBaseCharacter::NormalSpeedWalkAction);
	
	InputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AFPSTeachBaseCharacter::JumpAction);
	InputComponent->BindAction(TEXT("Jump"), IE_Released, this, &AFPSTeachBaseCharacter::StopJumpAction);

	InputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFPSTeachBaseCharacter::InputFirePressed);
	InputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AFPSTeachBaseCharacter::InputFireReleased);

	InputComponent->BindAction(TEXT("Aiming"), IE_Pressed, this, &AFPSTeachBaseCharacter::InputAimingPressed);
	InputComponent->BindAction(TEXT("Aiming"), IE_Released, this, &AFPSTeachBaseCharacter::InputAimingReleased);

	InputComponent->BindAxis(TEXT("MoveRight"), this, &AFPSTeachBaseCharacter::MoveRight);
	InputComponent->BindAxis(TEXT("MoveForward"), this, &AFPSTeachBaseCharacter::MoveForward);

	InputComponent->BindAxis(TEXT("Turn"), this, &AFPSTeachBaseCharacter::AddControllerYawInput);
	InputComponent->BindAxis(TEXT("LookUp"), this, &AFPSTeachBaseCharacter::AddControllerPitchInput);

	InputComponent->BindAction(TEXT("Reload"), IE_Pressed, this, &AFPSTeachBaseCharacter::InputReload);
}




#pragma region NetWorking

void AFPSTeachBaseCharacter::ServerLowSpeedWalkAction_Implementation()
{
	CharacterMovement->MaxWalkSpeed = 300;
}

bool AFPSTeachBaseCharacter::ServerLowSpeedWalkAction_Validate()
{
	return true;
}

void AFPSTeachBaseCharacter::ServerNormalSpeedWalkAction_Implementation()
{
	CharacterMovement->MaxWalkSpeed = 600;
}

bool AFPSTeachBaseCharacter::ServerNormalSpeedWalkAction_Validate()
{
	return true;
}

//?????????????????????
void AFPSTeachBaseCharacter::ServerFireRifleWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	if(ServerPrimaryWeapon)
	{
		//??????????????????????????????????????????????????????
		ServerPrimaryWeapon->MultiShootingEffect();
		ServerPrimaryWeapon->ClipCurrentAmmo -= 1;

		//??????????????????????????????????????????
		MultiShooting();

		//???????????????UI
		ClientUpdateAmmoUI(ServerPrimaryWeapon->ClipCurrentAmmo, ServerPrimaryWeapon->GunCurrentAmmo);
	}

	IsFiring = true;
	RifleLineTrace(CameraLocation, CameraRotation, IsMoving);
	
	
	//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("ServerPrimaryWeapon->ClipCurrentAmmo : %d"), ServerPrimaryWeapon->ClipCurrentAmmo));
}

bool AFPSTeachBaseCharacter::ServerFireRifleWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	return true;
}

void AFPSTeachBaseCharacter::ServerFireSniperWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	if(ServerPrimaryWeapon)
	{
		//??????????????????????????????????????????????????????
		ServerPrimaryWeapon->MultiShootingEffect();
		ServerPrimaryWeapon->ClipCurrentAmmo -= 1;

		//??????????????????????????????????????????
		MultiShooting();

		//???????????????UI
		ClientUpdateAmmoUI(ServerPrimaryWeapon->ClipCurrentAmmo, ServerPrimaryWeapon->GunCurrentAmmo);
	}

	if(ClientPrimaryWeapon)
	{
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelaySniperShootCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0;
		UKismetSystemLibrary::Delay(this, ClientPrimaryWeapon->ClientArmsFireAnimMontage->GetPlayLength(),
								ActionInfo);
	}

	IsFiring = true;
	SniperLineTrace(CameraLocation, CameraRotation, IsMoving);
	
}

bool AFPSTeachBaseCharacter::ServerFireSniperWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	return true;
}

void AFPSTeachBaseCharacter::ServerFirePistolWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,
                                                                   bool IsMoving)
{
	if(ServerSecondaryWeapon)
	{
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelaySpreadWeaponShootCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0;
		UKismetSystemLibrary::Delay(this, ServerSecondaryWeapon->SpreadWeaponCallBackRate, ActionInfo);
		
		//??????????????????????????????????????????????????????
		ServerSecondaryWeapon->MultiShootingEffect();
		ServerSecondaryWeapon->ClipCurrentAmmo -= 1;

		//??????????????????????????????????????????
		MultiShooting();

		//???????????????UI
		ClientUpdateAmmoUI(ServerSecondaryWeapon->ClipCurrentAmmo, ServerSecondaryWeapon->GunCurrentAmmo);
	}

	IsFiring = true;
	PistolLineTrace(CameraLocation, CameraRotation, IsMoving);
}

bool AFPSTeachBaseCharacter::ServerFirePistolWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	return true;
}

void AFPSTeachBaseCharacter::ServerReloadPrimary_Implementation()
{
	if(ServerPrimaryWeapon)
	{
		if(ServerPrimaryWeapon->GunCurrentAmmo > 0 && ServerPrimaryWeapon->ClipCurrentAmmo < ServerPrimaryWeapon->MaxClipAmmo)
		{
			//???????????????????????????????????????????????????????????????????????????UI??????
			ClientReload();
			MultiReloadAnimation();
			IsReloading = true;
			if(ClientPrimaryWeapon)
			{
				FLatentActionInfo ActionInfo;
				ActionInfo.CallbackTarget = this;
				ActionInfo.ExecutionFunction = TEXT("DelayPlayArmReloadCallBack");
				ActionInfo.UUID = FMath::Rand();
				ActionInfo.Linkage = 0;
				UKismetSystemLibrary::Delay(this, ClientPrimaryWeapon->ClientArmsReloadAnimMontage->GetPlayLength(), ActionInfo);
			}
		}
	}
	
}

bool AFPSTeachBaseCharacter::ServerReloadPrimary_Validate()
{
	return true;
}

void AFPSTeachBaseCharacter::ServerReloadSecondary_Implementation()
{
	if(ServerSecondaryWeapon)
	{
		if(ServerSecondaryWeapon->GunCurrentAmmo > 0 && ServerSecondaryWeapon->ClipCurrentAmmo < ServerSecondaryWeapon->MaxClipAmmo)
		{
			//???????????????????????????????????????????????????????????????????????????UI??????
			ClientReload();
			MultiReloadAnimation();
			IsReloading = true;
			if(ClientSecondaryWeapon)
			{
				FLatentActionInfo ActionInfo;
				ActionInfo.CallbackTarget = this;
				ActionInfo.ExecutionFunction = TEXT("DelayPlayArmReloadCallBack");
				ActionInfo.UUID = FMath::Rand();
				ActionInfo.Linkage = 0;
				UKismetSystemLibrary::Delay(this, ClientSecondaryWeapon->ClientArmsReloadAnimMontage->GetPlayLength(), ActionInfo);
			}
		}
	}
}

bool AFPSTeachBaseCharacter::ServerReloadSecondary_Validate()
{
	return true;
}

void AFPSTeachBaseCharacter::ServerStopFiring_Implementation()
{
	IsFiring = false;
}

bool AFPSTeachBaseCharacter::ServerStopFiring_Validate()
{
	return true;
}

void AFPSTeachBaseCharacter::ServerSetAiming_Implementation(bool AimingState)
{
	IsAiming = AimingState;
}

bool AFPSTeachBaseCharacter::ServerSetAiming_Validate(bool AimingState)
{
	return true;
}

void AFPSTeachBaseCharacter::MultiShooting_Implementation()
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTPBodysWeaponActor();
	if(ServerBodysAnimBP)
	{
		if(CurrentServerWeapon)
		{
			ServerBodysAnimBP->Montage_Play(CurrentServerWeapon->ServerTPBodysShootAnimMontage);
		}
	}
}

bool AFPSTeachBaseCharacter::MultiShooting_Validate()
{
	return true;
}

void AFPSTeachBaseCharacter::MultiReloadAnimation_Implementation()
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTPBodysWeaponActor();
	if(ServerBodysAnimBP)
	{
		if(CurrentServerWeapon)
		{
			ServerBodysAnimBP->Montage_Play(CurrentServerWeapon->ServerTPBodysReloadAnimMontage);
		}
	}

}

bool AFPSTeachBaseCharacter::MultiReloadAnimation_Validate()
{
	return true;
}


void AFPSTeachBaseCharacter::MultiSpawnBulletDecal_Implementation(FVector Location, FRotator Rotation)
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTPBodysWeaponActor();
	if(CurrentServerWeapon)
	{
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), CurrentServerWeapon->BulletDecalMaterial, FVector(8,8,8),
			Location, Rotation, 10);
		if(Decal)
		{
			Decal->SetFadeScreenSize(0.001);
		}
	}
}

bool AFPSTeachBaseCharacter::MultiSpawnBulletDecal_Validate(FVector Location, FRotator Rotation)
{
	return true;
}

void AFPSTeachBaseCharacter::ClientDeathMatchDeath_Implementation()
{
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();

	if(CurrentClientWeapon)
	{
		CurrentClientWeapon->Destroy();
	}
	if(FPArmsMesh)
	{
		FPArmsMesh->SetVisibility(false);
	}
}

void AFPSTeachBaseCharacter::ClientEndAiming_Implementation()
{
	if(FPArmsMesh)
	{
		FPArmsMesh->SetHiddenInGame(false);
	}

	if(FPSPlayerController)
	{
		FPSPlayerController->SetPlayerUIHidden(false);
	}
	
	if(ClientPrimaryWeapon)
	{
		ClientPrimaryWeapon->SetActorHiddenInGame(false);
		if(PlayerCamera)
		{
			PlayerCamera->SetFieldOfView(90);
		}
	}

	if(WidgetScope)
	{
		WidgetScope->RemoveFromParent();
	}
	
}

void AFPSTeachBaseCharacter::ClientAiming_Implementation()
{
	if(FPArmsMesh)
	{
		FPArmsMesh->SetHiddenInGame(true);
	}

	if(FPSPlayerController)
	{
		FPSPlayerController->SetPlayerUIHidden(true);
	}
	
	if(ClientPrimaryWeapon)
	{
		ClientPrimaryWeapon->SetActorHiddenInGame(true);
		if(PlayerCamera)
		{
			PlayerCamera->SetFieldOfView(ClientPrimaryWeapon->FieldOfAimingView);
		}
	}

	WidgetScope = CreateWidget<UUserWidget>(GetWorld(), SniperScopeBPClass);
	WidgetScope->AddToViewport();
}

void AFPSTeachBaseCharacter::ClientEquipFPArmsSecondary_Implementation()
{
	if(ServerSecondaryWeapon)
	{
		if(ClientSecondaryWeapon)
		{
			
		}
		else
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ClientSecondaryWeapon = GetWorld()->SpawnActor<AWeaponBaseClient>(ServerSecondaryWeapon->ClientWeaponBaseBPClass,
				GetActorTransform(), SpawnInfo);

			//????????????
			FName WeaponSocketName = TEXT("WeaponSocket");
			
			ClientSecondaryWeapon->K2_AttachToComponent(FPArmsMesh, WeaponSocketName, EAttachmentRule::SnapToTarget,
				EAttachmentRule::SnapToTarget,
				EAttachmentRule::SnapToTarget, true);

			ClientUpdateAmmoUI(ServerSecondaryWeapon->ClipCurrentAmmo, ServerSecondaryWeapon->GunCurrentAmmo);
			
			//??????????????????
			if(ClientSecondaryWeapon)
			{
				UpdateFPArmsBlendPose(ClientSecondaryWeapon->FPArmsBlendPose);
			}
			
		}
	}
}

//???????????????
void AFPSTeachBaseCharacter::ClientReload_Implementation()
{
	//???????????????????????????
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	if(CurrentClientWeapon)
	{
		UAnimMontage* ClientArmsReloadMontage = CurrentClientWeapon->ClientArmsReloadAnimMontage;
	
		ClientArmsAnimBP->Montage_Play(ClientArmsReloadMontage);

		CurrentClientWeapon->PlayReloadAnimation();
	}
	
}

//?????????
void AFPSTeachBaseCharacter::ClientRecoil_Implementation()
{
	UCurveFloat* VerticalRecoilCurve = nullptr;
	UCurveFloat* HorizontalRecoilCurve = nullptr;
	if(ServerPrimaryWeapon)
	{
		VerticalRecoilCurve = ServerPrimaryWeapon->VerticalRecoilCurve;
		HorizontalRecoilCurve = ServerPrimaryWeapon->HorizontalRecoilCurve;
	}
	RecoilXCoordPerShoot += 0.1;

	if(VerticalRecoilCurve)
	{
		NewVerticalRecoilAmount = VerticalRecoilCurve->GetFloatValue(RecoilXCoordPerShoot);
	}

	if(HorizontalRecoilCurve)
	{
		NewHorizontalRecoilAmount = HorizontalRecoilCurve->GetFloatValue(RecoilXCoordPerShoot);
	}
	
	VerticalRecoilAmount = NewVerticalRecoilAmount - OldVerticalRecoilAmount;
	HorizontalRecoilAmount = NewHorizontalRecoilAmount - OldHorizontalRecoilAmount;
	
	if(FPSPlayerController)
	{
		FRotator ControllerRotator = FPSPlayerController->GetControlRotation();
		FPSPlayerController->SetControlRotation(FRotator(ControllerRotator.Pitch + VerticalRecoilAmount,
			ControllerRotator.Yaw + HorizontalRecoilAmount,
			ControllerRotator.Roll));
	}
	
	OldVerticalRecoilAmount = NewVerticalRecoilAmount;
	OldHorizontalRecoilAmount = NewHorizontalRecoilAmount;
}

void AFPSTeachBaseCharacter::ClientUpdateHealthUI_Implementation(float NewHealth)
{
	if(FPSPlayerController)
	{
		FPSPlayerController->UpdateHealthUI(NewHealth);
	}
}

void AFPSTeachBaseCharacter::ClientUpdateAmmoUI_Implementation(int32 ClipCurrentAmmo, int32 GunCurrentAmmo)
{
	if(FPSPlayerController)
	{
		FPSPlayerController->UpdateAmmoUI(ClipCurrentAmmo, GunCurrentAmmo);
	}
}


//?????????????????????
void AFPSTeachBaseCharacter::ClientFire_Implementation()
{
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	if(CurrentClientWeapon)
	{
		//??????????????????
		CurrentClientWeapon->PlayShootAnimation();
		
		//???????????????????????????
		UAnimMontage* ClientArmsFireMontage = CurrentClientWeapon->ClientArmsFireAnimMontage;
		ClientArmsAnimBP->Montage_SetPlayRate(ClientArmsFireMontage, 1);
		ClientArmsAnimBP->Montage_Play(ClientArmsFireMontage);

		//???????????????????????????
		CurrentClientWeapon->DisplayWeaponEffect();

		//????????????
		AMultiFPSPlayerController* MultiFPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
		if(MultiFPSPlayerController)
		{
			MultiFPSPlayerController->PlayerCameraShake(CurrentClientWeapon->CameraShakeClass);
		}
		
		//??????????????????????????????
		FPSPlayerController->DoCrosshairRecoil();
	}
}

void AFPSTeachBaseCharacter::ClientEquipFPArmsPrimary_Implementation()
{
	if(ServerPrimaryWeapon)
	{
		if(ClientPrimaryWeapon)
		{
			
		}
		else
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ClientPrimaryWeapon = GetWorld()->SpawnActor<AWeaponBaseClient>(ServerPrimaryWeapon->ClientWeaponBaseBPClass,
				GetActorTransform(), SpawnInfo);

			//??????????????????????????????
			FName WeaponSocketName = TEXT("WeaponSocket");
			if(ActiveWeapon == EWeaponType::M4A1)
			{
				WeaponSocketName = TEXT("M4A1_Socket");
			}
			if(ActiveWeapon == EWeaponType::Sniper)
			{
				WeaponSocketName = TEXT("AWP_Socket");
			}
			ClientPrimaryWeapon->K2_AttachToComponent(FPArmsMesh, WeaponSocketName, EAttachmentRule::SnapToTarget,
				EAttachmentRule::SnapToTarget,
				EAttachmentRule::SnapToTarget, true);

			ClientUpdateAmmoUI(ServerPrimaryWeapon->ClipCurrentAmmo, ServerPrimaryWeapon->GunCurrentAmmo);
			
			//??????????????????
			if(ClientPrimaryWeapon)
			{
				UpdateFPArmsBlendPose(ClientPrimaryWeapon->FPArmsBlendPose);
			}
			
		}
	}
}


#pragma endregion 


#pragma region InputEvent
void AFPSTeachBaseCharacter::MoveRight(float AxisValue)
{
	AddMovementInput(GetActorRightVector(), AxisValue, false);
}

void AFPSTeachBaseCharacter::MoveForward(float AxisValue)
{
	AddMovementInput(GetActorForwardVector(), AxisValue, false);
}

void AFPSTeachBaseCharacter::JumpAction()
{
	Jump();
}

void AFPSTeachBaseCharacter::StopJumpAction()
{
	StopJumping();
}

void AFPSTeachBaseCharacter::InputFirePressed()
{
	switch (ActiveWeapon)
	{
		case EWeaponType::AK47:
			{
				FireWeaponPrimary();
			}
			break;
		case EWeaponType::M4A1:
			{
				FireWeaponPrimary();
			}
			break;
		case EWeaponType::MP7:
			{
				FireWeaponPrimary();
			}
			break;
		case EWeaponType::DesertEagle:
			{
				FireWeaponSecondary();
			}
			break;
		case EWeaponType::Sniper:
			{
				FireWeaponSniper();
			}
			break;
	}
}

void AFPSTeachBaseCharacter::InputFireReleased()
{
	switch (ActiveWeapon)
	{
		case EWeaponType::AK47:
			{
				StopFirePrimary();
			}
			break;
		case EWeaponType::M4A1:
			{
				StopFirePrimary();
			}
			break;
		case EWeaponType::MP7:
			{
				StopFirePrimary();
			}
			break;
		case EWeaponType::DesertEagle:
			{
				StopFireSecondary();
			}
			break;
		case EWeaponType::Sniper:
			{
				StopFireSniper();
			}
			break;
	}
}

void AFPSTeachBaseCharacter::InputAimingPressed()
{
	//???????????????UI?????????????????????????????????????????????????????? ?????????RPC
	//??????IsAiming?????? ?????????RPC
	if(ActiveWeapon == EWeaponType::Sniper && !IsReloading && !IsFiring)
	{
		ServerSetAiming(true);
		ClientAiming();
	}
}

void AFPSTeachBaseCharacter::InputAimingReleased()
{
	//???????????????UI????????????????????????????????????????????????????????????
	if(ActiveWeapon == EWeaponType::Sniper)
	{
		ServerSetAiming(false);
		ClientEndAiming();
	}
}

void AFPSTeachBaseCharacter::LowSpeedWalkAction()
{
	CharacterMovement->MaxWalkSpeed = 300;
	ServerLowSpeedWalkAction();
}

void AFPSTeachBaseCharacter::NormalSpeedWalkAction()
{
	CharacterMovement->MaxWalkSpeed = 600;
	ServerNormalSpeedWalkAction();
}

void AFPSTeachBaseCharacter::InputReload()
{
	if(!IsReloading)
	{
		if(!IsFiring)
		{
			switch (ActiveWeapon)
			{
				case EWeaponType::AK47:
					{
						ServerReloadPrimary();
					}
					break;
				case EWeaponType::M4A1:
					{
						ServerReloadPrimary();
					}
					break;
				case EWeaponType::MP7:
					{
						ServerReloadPrimary();
					}
					break;
				case EWeaponType::DesertEagle:
					{
						ServerReloadSecondary();
					}
					break;
				case EWeaponType::Sniper:
					{
						ServerReloadPrimary();
					}
					break;
			}
		}
	}
}

#pragma endregion

#pragma region Weapon
void AFPSTeachBaseCharacter::EquipPrimary(AWeaponBaseServer* WeaponBaseServer)
{
	if(ServerPrimaryWeapon)
	{
		
	}
	else
	{
		ServerPrimaryWeapon = WeaponBaseServer;
		ServerPrimaryWeapon->SetOwner(this);
		ServerPrimaryWeapon->K2_AttachToComponent(Mesh, TEXT("Weapon_Rifle"), EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget, true);
		ActiveWeapon = ServerPrimaryWeapon->KindOfWeapon;
		ClientEquipFPArmsPrimary();
	}
}

void AFPSTeachBaseCharacter::EquipSecondary(AWeaponBaseServer* WeaponBaseServer)
{
	if(ServerSecondaryWeapon)
	{
		
	}
	else
	{
		ServerSecondaryWeapon = WeaponBaseServer;
		ServerSecondaryWeapon->SetOwner(this);
		ServerSecondaryWeapon->K2_AttachToComponent(Mesh, TEXT("Weapon_Rifle"), EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget, true);
		ActiveWeapon = ServerSecondaryWeapon->KindOfWeapon;
		ClientEquipFPArmsSecondary();
	}
}


void AFPSTeachBaseCharacter::StartWithKindOfWeapon()
{
	if(HasAuthority())
	{
		PurchaseWeapon(static_cast<EWeaponType>(UKismetMathLibrary::RandomIntegerInRange(0, static_cast<int8>(EWeaponType::EEND) - 1)));
	}
	
}

void AFPSTeachBaseCharacter::PurchaseWeapon(EWeaponType WeaponType)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	switch (WeaponType)
	{
		case EWeaponType::AK47:
			{
				//????????????AK47 Server???
				UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr, TEXT("Blueprint'/Game/Blueprint/Weapon/AK47/ServerBP_AK47.ServerBP_AK47_C'"));
				AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(BlueprintVar,
				GetActorTransform(), SpawnInfo);
				ServerWeapon->EquipWeapon();
				//ActiveWeapon = EWeaponType::AK47;
				EquipPrimary(ServerWeapon);
			}
			break;
		case EWeaponType::M4A1:
			{
				//????????????M4A1 Server???
				UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr, TEXT("Blueprint'/Game/Blueprint/Weapon/M4A1/ServerBP_M4A1.ServerBP_M4A1_C'"));
				AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(BlueprintVar,
				GetActorTransform(), SpawnInfo);
				ServerWeapon->EquipWeapon();
				//ActiveWeapon = EWeaponType::M4A1;
				EquipPrimary(ServerWeapon);
			}
			break;
		case EWeaponType::MP7:
			{
				//????????????MP7 Server???
				UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr, TEXT("Blueprint'/Game/Blueprint/Weapon/MP7/ServerBP_MP7.ServerBP_MP7_C'"));
				AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(BlueprintVar,
				GetActorTransform(), SpawnInfo);
				ServerWeapon->EquipWeapon();
				//ActiveWeapon = EWeaponType::MP7;
				EquipPrimary(ServerWeapon);
			}
			break;
		case EWeaponType::DesertEagle:
			{
				//????????????DesertEagle Server???
				UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr, TEXT("Blueprint'/Game/Blueprint/Weapon/DesertEagle/ServerBP_DesertEagle.ServerBP_DesertEagle_C'"));
				AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(BlueprintVar,
				GetActorTransform(), SpawnInfo);
				ServerWeapon->EquipWeapon();
				//ActiveWeapon = EWeaponType::DesertEagle;
				EquipSecondary(ServerWeapon);
			}
			break;
		case EWeaponType::Sniper:
			{
				//????????????Sniper Server???
				UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr, TEXT("Blueprint'/Game/Blueprint/Weapon/Sniper/ServerBP_Sniper.ServerBP_Sniper_C'"));
				AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(BlueprintVar,
				GetActorTransform(), SpawnInfo);
				ServerWeapon->EquipWeapon();
				//ActiveWeapon = EWeaponType::Sniper;
				EquipPrimary(ServerWeapon);
			}
			break;
		default:
			{
				
			}
			break;
	}
}

AWeaponBaseClient* AFPSTeachBaseCharacter::GetCurrentClientFPArmsWeaponActor()
{
	switch (ActiveWeapon)
	{
		case EWeaponType::AK47:
			{
				return ClientPrimaryWeapon;
			}
			break;
		case EWeaponType::M4A1:
			{
				return ClientPrimaryWeapon;
			}
			break;
		case EWeaponType::MP7:
			{
				return ClientPrimaryWeapon;
			}
			break;
		case EWeaponType::DesertEagle:
			{
				return ClientSecondaryWeapon;
			}
			break;
		case EWeaponType::Sniper:
			{
				return ClientPrimaryWeapon;
			}
			break;
	}

	return nullptr;
}

AWeaponBaseServer* AFPSTeachBaseCharacter::GetCurrentServerTPBodysWeaponActor()
{
	switch (ActiveWeapon)
	{
		case EWeaponType::AK47:
			{
				return ServerPrimaryWeapon;
			}
			break;
		case EWeaponType::M4A1:
			{
				return ServerPrimaryWeapon;
			}
			break;
		case EWeaponType::MP7:
			{
				return ServerPrimaryWeapon;
			}
			break;
		case EWeaponType::DesertEagle:
			{
				return ServerSecondaryWeapon;
			}
			break;
		case EWeaponType::Sniper:
			{
				return ServerPrimaryWeapon;
			}
			break;
	}

	return nullptr;
}
#pragma endregion


#pragma region Fire
void AFPSTeachBaseCharacter::AutomaticFire()
{
	//????????????????????????
	if(ServerPrimaryWeapon->ClipCurrentAmmo > 0)
	{
		//??????????????????????????????(ok)???????????????3D(ok)???????????????(ok)???????????????UI(ok)??????UI??????(ok)????????????????????????????????????????????????????????????????????????????????????????????????
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}
		

		//????????????????????????(ok)???????????????(ok)???????????????2D(ok)???????????????(ok)???????????????????????????????????????(ok)???
		//???????????????????????????UI????????????UI?????????????????????????????????(ok)
		ClientFire();
		//?????????
		ClientRecoil();
	}
	else
	{
		StopFirePrimary();
	}
}

void AFPSTeachBaseCharacter::ResetRecoil()
{
	NewVerticalRecoilAmount = 0;
	OldVerticalRecoilAmount = 0;
	VerticalRecoilAmount = 0;
	RecoilXCoordPerShoot = 0;
}

//???????????????
void AFPSTeachBaseCharacter::FireWeaponPrimary()
{
	//????????????????????????
	if(ServerPrimaryWeapon->ClipCurrentAmmo > 0 && !IsReloading)
	{
		//??????????????????????????????(ok)???????????????3D(ok)???????????????(ok)???????????????UI(ok)??????UI??????(ok)????????????????????????????????????????????????????????????????????????????????????????????????
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}

		//????????????????????????(ok)???????????????(ok)???????????????2D(ok)???????????????(ok)???????????????????????????????????????(ok)???
		//???????????????????????????UI????????????UI?????????????????????????????????(ok)
		ClientFire();
		//?????????
		ClientRecoil();
		

		//????????????????????????
		//????????????????????????????????????????????????
		if(ServerPrimaryWeapon->IsAutomatic) //????????????????????????
		{
			GetWorldTimerManager().SetTimer(AutomaticFireTimerHandle, this, &AFPSTeachBaseCharacter::AutomaticFire,
				ServerPrimaryWeapon->AutomaticFireRate, true);
		}
		


		//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("ServerPrimaryWeapon->ClipCurrentAmmo : %d"), ServerPrimaryWeapon->ClipCurrentAmmo));
	}
	
}

//?????????????????????
void AFPSTeachBaseCharacter::StopFirePrimary()
{
	//??????IsFiring??????
	ServerStopFiring();
	
	//????????????????????????????????????????????????
	GetWorldTimerManager().ClearTimer(AutomaticFireTimerHandle);
	
	//???????????????????????????
	ResetRecoil();
}

//??????????????????
void AFPSTeachBaseCharacter::RifleLineTrace(FVector CameraLocation, FRotator CameraRotation, bool IsMoving)
{
	FVector EndLocation;
	FVector CameraForwardVector = UKismetMathLibrary::GetForwardVector(CameraRotation);
	TArray<AActor*> IgnoreArray;
	IgnoreArray.Add(this);
	FHitResult HitResult;

	if(ServerPrimaryWeapon)
	{
		//???????????????????????????????????????????????????EndLocation??????
		if(IsMoving)
		{
			//x???y???z???????????????????????????
			FVector Vector = CameraLocation + CameraForwardVector * ServerPrimaryWeapon->BulletDistance;
			float RandomX = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
			float RandomY = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
			float RandomZ = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
			EndLocation = FVector(Vector.X + RandomX, Vector.Y + RandomY, Vector.Z + RandomZ);
		}
		else
		{
			EndLocation = CameraLocation + CameraForwardVector * ServerPrimaryWeapon->BulletDistance;
		}
	}
	
	bool HitSuccess = UKismetSystemLibrary::LineTraceSingle(GetWorld(), CameraLocation, EndLocation, ETraceTypeQuery::TraceTypeQuery1,
		false, IgnoreArray, EDrawDebugTrace::None, HitResult, true, FLinearColor::Red,
		FLinearColor::Green, 3.f);
	
	if(HitSuccess)
	{
		//UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Hit Actor Name : %s"), *HitResult.Actor->GetName()));
		AFPSTeachBaseCharacter* FPSCharacter = Cast<AFPSTeachBaseCharacter>(HitResult.Actor);
		if(FPSCharacter)
		{
			//???????????????????????????
			DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
		}
		else
		{
			FRotator XRotator = UKismetMathLibrary::MakeRotFromX(HitResult.Normal);
			//?????????????????????????????????,??????????????????????????????????????????????????????
			MultiSpawnBulletDecal(HitResult.Location, XRotator);
		}
	}
}

void AFPSTeachBaseCharacter::FireWeaponSecondary()
{
	//????????????????????????
	if(ServerSecondaryWeapon->ClipCurrentAmmo > 0 && !IsReloading)
	{
		//??????????????????????????????(ok)???????????????3D(ok)???????????????(ok)???????????????UI(ok)??????UI??????(ok)????????????????????????????????????????????????????????????????????????????????????????????????
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFirePistolWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFirePistolWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}

		//????????????????????????(ok)???????????????(ok)???????????????2D(ok)???????????????(ok)???????????????????????????????????????(ok)???
		//???????????????????????????UI????????????UI?????????????????????????????????(ok)
		ClientFire();
		
	}
}

void AFPSTeachBaseCharacter::StopFireSecondary()
{
	//??????IsFiring??????
	ServerStopFiring();
}

void AFPSTeachBaseCharacter::PistolLineTrace(FVector CameraLocation, FRotator CameraRotation, bool IsMoving)
{
	FVector EndLocation;
	FVector CameraForwardVector = UKismetMathLibrary::GetForwardVector(CameraRotation);
	TArray<AActor*> IgnoreArray;
	IgnoreArray.Add(this);
	FHitResult HitResult;

	if(ServerSecondaryWeapon)
	{
		//???????????????????????????????????????????????????EndLocation??????
		if(IsMoving)
		{
			//x???y???z???????????????????????????
			FRotator Rotator;
			Rotator.Roll = CameraRotation.Roll;
			Rotator.Pitch = CameraRotation.Pitch + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin, PistolSpreadMax);
			Rotator.Yaw = CameraRotation.Yaw + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin, PistolSpreadMax);
			CameraForwardVector = UKismetMathLibrary::GetForwardVector(Rotator);
			
			FVector Vector = CameraLocation + CameraForwardVector * ServerSecondaryWeapon->BulletDistance;
			float RandomX = UKismetMathLibrary::RandomFloatInRange(-ServerSecondaryWeapon->MovingFireRandomRange, ServerSecondaryWeapon->MovingFireRandomRange);
			float RandomY = UKismetMathLibrary::RandomFloatInRange(-ServerSecondaryWeapon->MovingFireRandomRange, ServerSecondaryWeapon->MovingFireRandomRange);
			float RandomZ = UKismetMathLibrary::RandomFloatInRange(-ServerSecondaryWeapon->MovingFireRandomRange, ServerSecondaryWeapon->MovingFireRandomRange);
			EndLocation = FVector(Vector.X + RandomX, Vector.Y + RandomY, Vector.Z + RandomZ);
		}
		else
		{
			//?????????????????????????????????????????????????????????????????????
			FRotator Rotator;
			Rotator.Roll = CameraRotation.Roll;
			Rotator.Pitch = CameraRotation.Pitch + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin, PistolSpreadMax);
			Rotator.Yaw = CameraRotation.Yaw + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin, PistolSpreadMax);
			CameraForwardVector = UKismetMathLibrary::GetForwardVector(Rotator);
			EndLocation = CameraLocation + CameraForwardVector * ServerSecondaryWeapon->BulletDistance;
		}
	}
	
	bool HitSuccess = UKismetSystemLibrary::LineTraceSingle(GetWorld(), CameraLocation, EndLocation, ETraceTypeQuery::TraceTypeQuery1,
		false, IgnoreArray, EDrawDebugTrace::None, HitResult, true, FLinearColor::Red,
		FLinearColor::Green, 3.f);

	PistolSpreadMax += ServerSecondaryWeapon->SpreadWeaponMaxIndex;
	PistolSpreadMin -= ServerSecondaryWeapon->SpreadWeaponMinIndex;
	
	if(HitSuccess)
	{
		//UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Hit Actor Name : %s"), *HitResult.Actor->GetName()));
		AFPSTeachBaseCharacter* FPSCharacter = Cast<AFPSTeachBaseCharacter>(HitResult.Actor);
		if(FPSCharacter)
		{
			//UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Hit Actor Name : %s"), *HitResult.Actor->GetName()));
			//???????????????????????????
			DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
		}
		else
		{
			//UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Hit")));
			FRotator XRotator = UKismetMathLibrary::MakeRotFromX(HitResult.Normal);
			//?????????????????????????????????,??????????????????????????????????????????????????????
			MultiSpawnBulletDecal(HitResult.Location, XRotator);
		}
	}
}

void AFPSTeachBaseCharacter::FireWeaponSniper()
{
	//????????????????????????
	if(ServerPrimaryWeapon->ClipCurrentAmmo > 0 && !IsReloading && !IsFiring)
	{
		//??????????????????????????????(ok)???????????????3D(ok)???????????????(ok)???????????????UI(ok)??????UI??????(ok)????????????????????????????????????????????????????????????????????????????????????????????????
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFireSniperWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFireSniperWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}

		//????????????????????????(ok)???????????????(ok)???????????????2D(ok)???????????????(ok)???????????????????????????????????????(ok)???
		//???????????????????????????UI????????????UI?????????????????????????????????(ok)
		ClientFire();
		
	}
}

void AFPSTeachBaseCharacter::StopFireSniper()
{
	
}

void AFPSTeachBaseCharacter::SniperLineTrace(FVector CameraLocation, FRotator CameraRotation, bool IsMoving)
{
	FVector EndLocation;
	FVector CameraForwardVector = UKismetMathLibrary::GetForwardVector(CameraRotation);
	TArray<AActor*> IgnoreArray;
	IgnoreArray.Add(this);
	FHitResult HitResult;

	if(ServerPrimaryWeapon)
	{
		if(IsAiming) //???????????????????????????????????????
		{
			//???????????????????????????????????????????????????EndLocation??????
			if(IsMoving)
			{
				//x???y???z???????????????????????????
				FVector Vector = CameraLocation + CameraForwardVector * ServerPrimaryWeapon->BulletDistance;
				float RandomX = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
				float RandomY = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
				float RandomZ = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
				EndLocation = FVector(Vector.X + RandomX, Vector.Y + RandomY, Vector.Z + RandomZ);
			}
			else
			{
				EndLocation = CameraLocation + CameraForwardVector * ServerPrimaryWeapon->BulletDistance;
			}
			ClientEndAiming();
		}
		else
		{
			//x???y???z???????????????????????????
			FVector Vector = CameraLocation + CameraForwardVector * ServerPrimaryWeapon->BulletDistance;
			float RandomX = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
			float RandomY = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
			float RandomZ = UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange, ServerPrimaryWeapon->MovingFireRandomRange);
			EndLocation = FVector(Vector.X + RandomX, Vector.Y + RandomY, Vector.Z + RandomZ);
		}
		
	}
	
	bool HitSuccess = UKismetSystemLibrary::LineTraceSingle(GetWorld(), CameraLocation, EndLocation, ETraceTypeQuery::TraceTypeQuery1,
		false, IgnoreArray, EDrawDebugTrace::None, HitResult, true, FLinearColor::Red,
		FLinearColor::Green, 3.f);
	
	if(HitSuccess)
	{
		//UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Hit Actor Name : %s"), *HitResult.Actor->GetName()));
		AFPSTeachBaseCharacter* FPSCharacter = Cast<AFPSTeachBaseCharacter>(HitResult.Actor);
		if(FPSCharacter)
		{
			//???????????????????????????
			DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
		}
		else
		{
			FRotator XRotator = UKismetMathLibrary::MakeRotFromX(HitResult.Normal);
			//?????????????????????????????????,??????????????????????????????????????????????????????
			MultiSpawnBulletDecal(HitResult.Location, XRotator);
		}
	}
}


void AFPSTeachBaseCharacter::DelaySpreadWeaponShootCallBack()
{
	PistolSpreadMin = 0;
	PistolSpreadMax = 0;
}

void AFPSTeachBaseCharacter::DelaySniperShootCallBack()
{
	IsFiring = false;
}

void AFPSTeachBaseCharacter::DelayPlayArmReloadCallBack()
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTPBodysWeaponActor();
	if(CurrentServerWeapon)
	{
		int32 GunCurrentAmmo  = CurrentServerWeapon->GunCurrentAmmo;
		int32 ClipCurrentAmmo = CurrentServerWeapon->ClipCurrentAmmo;
		int32 const MaxClipAmmo = CurrentServerWeapon->MaxClipAmmo;

		IsReloading = false;
		//??????????????????????????????
		if(MaxClipAmmo - ClipCurrentAmmo >= GunCurrentAmmo)
		{
			ClipCurrentAmmo += GunCurrentAmmo;
			GunCurrentAmmo = 0;
		}
		else
		{
			GunCurrentAmmo -= MaxClipAmmo - ClipCurrentAmmo;
			ClipCurrentAmmo = MaxClipAmmo;
		}
		CurrentServerWeapon->GunCurrentAmmo = GunCurrentAmmo;
		CurrentServerWeapon->ClipCurrentAmmo = ClipCurrentAmmo;

		ClientUpdateAmmoUI(ClipCurrentAmmo, GunCurrentAmmo);
	}
	
}

void AFPSTeachBaseCharacter::DamagePlayer(UPhysicalMaterial* PhysicalMaterial, AActor* DamagedActor, FVector& HitFromDirection, FHitResult& HitInfo)
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTPBodysWeaponActor();
	if(CurrentServerWeapon)
	{
		//??????????????????????????????
		switch (PhysicalMaterial->SurfaceType)
		{
		case EPhysicalSurface::SurfaceType1:
			{
				//Head
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 4, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//????????????????????????????????????
			}
			break;
		case EPhysicalSurface::SurfaceType2:
			{
				//Body
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 1, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//????????????????????????????????????
			}
			break;
		case EPhysicalSurface::SurfaceType3:
			{
				//Arm
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 0.8, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//????????????????????????????????????
			}
			break;
		case EPhysicalSurface::SurfaceType4:
			{
				//Leg
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 0.7, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//????????????????????????????????????
			}
			break;
		}
	}
	
}

void AFPSTeachBaseCharacter::OnHit(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation,
	UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType,
	AActor* DamageCauser)
{
	Health -= Damage;
	
	//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("PlayerName %s Health : %f"), *GetName(), Health));
	
	//1 ?????????RPC  2 ???????????????PlayerController?????????????????????????????????  3 ??????PlayerUI??????????????????????????????
	ClientUpdateHealthUI(Health);
	//????????????
	if(Health <= 0)
	{
		DeathMatchDeath(DamageCauser);
	}
}

void AFPSTeachBaseCharacter::DeathMatchDeath(AActor* DamageActor)
{
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTPBodysWeaponActor();
	if(CurrentClientWeapon)
	{
		CurrentClientWeapon->Destroy();
	}
	if(CurrentServerWeapon)
	{
		CurrentServerWeapon->Destroy();
	}
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClientDeathMatchDeath();
	
	AMultiFPSPlayerController* MultiFPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
	if(MultiFPSPlayerController)
	{
		MultiFPSPlayerController->DeathMatchDeath(DamageActor);
	}

	TPBodysDeath();
}
#pragma endregion
