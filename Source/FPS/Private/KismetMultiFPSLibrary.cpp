// Fill out your copyright notice in the Description page of Project Settings.


#include "KismetMultiFPSLibrary.h"

void UKismetMultiFPSLibrary::SortValues(TArray<FDeathMatchPlayerData>& Values)
{
	Values.Sort([](const FDeathMatchPlayerData& a, const FDeathMatchPlayerData& b){return a.PlayerScore > b.PlayerScore;});
}


