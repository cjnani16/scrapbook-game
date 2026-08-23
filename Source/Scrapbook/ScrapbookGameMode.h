// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ScrapbookGameMode.generated.h"

// Event that broadcasts when progression facts are updated, so triggers can occur
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnProgressionFactsChanged, FName, Key, FString, Value, bool, IsPartOfLoading);

/**
 * 
 */
UCLASS()
class SCRAPBOOK_API AScrapbookGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FOnProgressionFactsChanged OnProgressionFactsChanged;
};
