// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiFPSPlayerController.h"

void AMultiFPSPlayerController::PlayerCameraShake(TSubclassOf<UCameraShakeBase> CameraShake)
{
	ClientStartCameraShake(CameraShake, 1, ECameraShakePlaySpace::CameraLocal, FRotator::ZeroRotator);
}
