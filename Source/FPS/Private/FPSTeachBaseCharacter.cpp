
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

//服务端射击逻辑
void AFPSTeachBaseCharacter::ServerFireRifleWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	if(ServerPrimaryWeapon)
	{
		//多播（必须在服务器调用，谁调谁多播）
		ServerPrimaryWeapon->MultiShootingEffect();
		ServerPrimaryWeapon->ClipCurrentAmmo -= 1;

		//多播，播放身体射击动画蒙太奇
		MultiShooting();

		//客户端更新UI
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
		//多播（必须在服务器调用，谁调谁多播）
		ServerPrimaryWeapon->MultiShootingEffect();
		ServerPrimaryWeapon->ClipCurrentAmmo -= 1;

		//多播，播放身体射击动画蒙太奇
		MultiShooting();

		//客户端更新UI
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
		
		//多播（必须在服务器调用，谁调谁多播）
		ServerSecondaryWeapon->MultiShootingEffect();
		ServerSecondaryWeapon->ClipCurrentAmmo -= 1;

		//多播，播放身体射击动画蒙太奇
		MultiShooting();

		//客户端更新UI
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
			//客户端手臂播放动画，服务器身体多播动画，数据更新，UI更改
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
			//客户端手臂播放动画，服务器身体多播动画，数据更新，UI更改
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

			//手枪插槽
			FName WeaponSocketName = TEXT("WeaponSocket");
			
			ClientSecondaryWeapon->K2_AttachToComponent(FPArmsMesh, WeaponSocketName, EAttachmentRule::SnapToTarget,
				EAttachmentRule::SnapToTarget,
				EAttachmentRule::SnapToTarget, true);

			ClientUpdateAmmoUI(ServerSecondaryWeapon->ClipCurrentAmmo, ServerSecondaryWeapon->GunCurrentAmmo);
			
			//手臂动画混合
			if(ClientSecondaryWeapon)
			{
				UpdateFPArmsBlendPose(ClientSecondaryWeapon->FPArmsBlendPose);
			}
			
		}
	}
}

//客户端换弹
void AFPSTeachBaseCharacter::ClientReload_Implementation()
{
	//手臂播放动画蒙太奇
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	if(CurrentClientWeapon)
	{
		UAnimMontage* ClientArmsReloadMontage = CurrentClientWeapon->ClientArmsReloadAnimMontage;
	
		ClientArmsAnimBP->Montage_Play(ClientArmsReloadMontage);

		CurrentClientWeapon->PlayReloadAnimation();
	}
	
}

//后坐力
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


//客户端射击相关
void AFPSTeachBaseCharacter::ClientFire_Implementation()
{
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	if(CurrentClientWeapon)
	{
		//枪体播放动画
		CurrentClientWeapon->PlayShootAnimation();
		
		//手臂播放动画蒙太奇
		UAnimMontage* ClientArmsFireMontage = CurrentClientWeapon->ClientArmsFireAnimMontage;
		ClientArmsAnimBP->Montage_SetPlayRate(ClientArmsFireMontage, 1);
		ClientArmsAnimBP->Montage_Play(ClientArmsFireMontage);

		//射击声音及粒子播放
		CurrentClientWeapon->DisplayWeaponEffect();

		//屏幕抖动
		AMultiFPSPlayerController* MultiFPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
		if(MultiFPSPlayerController)
		{
			MultiFPSPlayerController->PlayerCameraShake(CurrentClientWeapon->CameraShakeClass);
		}
		
		//播放十字准星扩散动画
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

			//不同武器的插槽不一样
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
			
			//手臂动画混合
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
	//贴瞄准镜的UI，关闭枪体可见性，摄像头可见距离拉远 客户端RPC
	//更改IsAiming属性 服务器RPC
	if(ActiveWeapon == EWeaponType::Sniper && !IsReloading && !IsFiring)
	{
		ServerSetAiming(true);
		ClientAiming();
	}
}

void AFPSTeachBaseCharacter::InputAimingReleased()
{
	//贴瞄准镜的UI删除，开启枪体可见性，摄像头可见距离恢复
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
				//动态拿到AK47 Server类
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
				//动态拿到M4A1 Server类
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
				//动态拿到MP7 Server类
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
				//动态拿到DesertEagle Server类
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
				//动态拿到Sniper Server类
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
	//判断子弹是否足够
	if(ServerPrimaryWeapon->ClipCurrentAmmo > 0)
	{
		//服务端（枪口闪光效果(ok)，射击声音3D(ok)，减少弹药(ok)，创建子弹UI(ok)，让UI更新(ok)，射线检测（三种，步枪，手枪，狙击枪），伤害应用，墙体弹孔生成）
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}
		

		//客户端（枪体动画(ok)，手臂动画(ok)，射击声音2D(ok)，屏幕抖动(ok)，后坐力，枪口闪光粒子效果(ok)）
		//客户端（十字线瞄准UI，初始化UI，播放十字线扩散动画）(ok)
		ClientFire();
		//后坐力
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

//主武器开火
void AFPSTeachBaseCharacter::FireWeaponPrimary()
{
	//判断子弹是否足够
	if(ServerPrimaryWeapon->ClipCurrentAmmo > 0 && !IsReloading)
	{
		//服务端（枪口闪光效果(ok)，射击声音3D(ok)，减少弹药(ok)，创建子弹UI(ok)，让UI更新(ok)，射线检测（三种，步枪，手枪，狙击枪），伤害应用，墙体弹孔生成）
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}

		//客户端（枪体动画(ok)，手臂动画(ok)，射击声音2D(ok)，屏幕抖动(ok)，后坐力，枪口闪光粒子效果(ok)）
		//客户端（十字线瞄准UI，初始化UI，播放十字线扩散动画）(ok)
		ClientFire();
		//后坐力
		ClientRecoil();
		

		//连续射击系统开发
		//开启计时器，每隔一段时间重新射击
		if(ServerPrimaryWeapon->IsAutomatic) //是自动步枪才计时
		{
			GetWorldTimerManager().SetTimer(AutomaticFireTimerHandle, this, &AFPSTeachBaseCharacter::AutomaticFire,
				ServerPrimaryWeapon->AutomaticFireRate, true);
		}
		


		//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("ServerPrimaryWeapon->ClipCurrentAmmo : %d"), ServerPrimaryWeapon->ClipCurrentAmmo));
	}
	
}

//主武器停止开火
void AFPSTeachBaseCharacter::StopFirePrimary()
{
	//更改IsFiring状态
	ServerStopFiring();
	
	//关闭计时器，每隔一段时间重新射击
	GetWorldTimerManager().ClearTimer(AutomaticFireTimerHandle);
	
	//重置后坐力相关变量
	ResetRecoil();
}

//步枪射线检测
void AFPSTeachBaseCharacter::RifleLineTrace(FVector CameraLocation, FRotator CameraRotation, bool IsMoving)
{
	FVector EndLocation;
	FVector CameraForwardVector = UKismetMathLibrary::GetForwardVector(CameraRotation);
	TArray<AActor*> IgnoreArray;
	IgnoreArray.Add(this);
	FHitResult HitResult;

	if(ServerPrimaryWeapon)
	{
		//是否移动会导致不同的射线检测，影响EndLocation计算
		if(IsMoving)
		{
			//x，y，z全部加一个随机偏移
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
			//打到玩家，应用伤害
			DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
		}
		else
		{
			FRotator XRotator = UKismetMathLibrary::MakeRotFromX(HitResult.Normal);
			//打到场景物体，生成弹孔,以广播方式生成，因为其他人都要能看见
			MultiSpawnBulletDecal(HitResult.Location, XRotator);
		}
	}
}

void AFPSTeachBaseCharacter::FireWeaponSecondary()
{
	//判断子弹是否足够
	if(ServerSecondaryWeapon->ClipCurrentAmmo > 0 && !IsReloading)
	{
		//服务端（枪口闪光效果(ok)，射击声音3D(ok)，减少弹药(ok)，创建子弹UI(ok)，让UI更新(ok)，射线检测（三种，步枪，手枪，狙击枪），伤害应用，墙体弹孔生成）
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFirePistolWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFirePistolWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}

		//客户端（枪体动画(ok)，手臂动画(ok)，射击声音2D(ok)，屏幕抖动(ok)，后坐力，枪口闪光粒子效果(ok)）
		//客户端（十字线瞄准UI，初始化UI，播放十字线扩散动画）(ok)
		ClientFire();
		
	}
}

void AFPSTeachBaseCharacter::StopFireSecondary()
{
	//更改IsFiring状态
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
		//是否移动会导致不同的射线检测，影响EndLocation计算
		if(IsMoving)
		{
			//x，y，z全部加一个随机偏移
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
			//旋转加一个随机偏移，范围根据连续射击的快慢决定
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
			//打到玩家，应用伤害
			DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
		}
		else
		{
			//UKismetSystemLibrary::PrintString(GetWorld(), FString::Printf(TEXT("Hit")));
			FRotator XRotator = UKismetMathLibrary::MakeRotFromX(HitResult.Normal);
			//打到场景物体，生成弹孔,以广播方式生成，因为其他人都要能看见
			MultiSpawnBulletDecal(HitResult.Location, XRotator);
		}
	}
}

void AFPSTeachBaseCharacter::FireWeaponSniper()
{
	//判断子弹是否足够
	if(ServerPrimaryWeapon->ClipCurrentAmmo > 0 && !IsReloading && !IsFiring)
	{
		//服务端（枪口闪光效果(ok)，射击声音3D(ok)，减少弹药(ok)，创建子弹UI(ok)，让UI更新(ok)，射线检测（三种，步枪，手枪，狙击枪），伤害应用，墙体弹孔生成）
		if(UKismetMathLibrary::VSize(GetVelocity()) > 0.1f)
		{
			ServerFireSniperWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), true);
		}
		else
		{
			ServerFireSniperWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(), false);
		}

		//客户端（枪体动画(ok)，手臂动画(ok)，射击声音2D(ok)，屏幕抖动(ok)，后坐力，枪口闪光粒子效果(ok)）
		//客户端（十字线瞄准UI，初始化UI，播放十字线扩散动画）(ok)
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
		if(IsAiming) //是否开镜导致不同的射线检测
		{
			//是否移动会导致不同的射线检测，影响EndLocation计算
			if(IsMoving)
			{
				//x，y，z全部加一个随机偏移
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
			//x，y，z全部加一个随机偏移
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
			//打到玩家，应用伤害
			DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
		}
		else
		{
			FRotator XRotator = UKismetMathLibrary::MakeRotFromX(HitResult.Normal);
			//打到场景物体，生成弹孔,以广播方式生成，因为其他人都要能看见
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
		//是否装填全部枪体子弹
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
		//五个位置应用不同伤害
		switch (PhysicalMaterial->SurfaceType)
		{
		case EPhysicalSurface::SurfaceType1:
			{
				//Head
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 4, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//打了别人调用这个，发通知
			}
			break;
		case EPhysicalSurface::SurfaceType2:
			{
				//Body
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 1, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//打了别人调用这个，发通知
			}
			break;
		case EPhysicalSurface::SurfaceType3:
			{
				//Arm
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 0.8, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//打了别人调用这个，发通知
			}
			break;
		case EPhysicalSurface::SurfaceType4:
			{
				//Leg
				UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentServerWeapon->BaseDamage * 0.7, HitFromDirection, HitInfo,
		GetController(), this, UDamageType::StaticClass());//打了别人调用这个，发通知
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
	
	//1 客户端RPC  2 调用客户端PlayerController的一个方法（留给蓝图）  3 实现PlayerUI里面的血量减少的接口
	ClientUpdateHealthUI(Health);
	//死亡逻辑
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
}
#pragma endregion
