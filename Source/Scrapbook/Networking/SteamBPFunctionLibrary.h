#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OnlineSessionSettings.h"
#include "SteamBPFunctionLibrary.generated.h"

// Searching, use a wrapper on results and an async action for execution in BP
USTRUCT(BlueprintType)
struct FSteamSessionResult
{
	GENERATED_BODY()

	FOnlineSessionSearchResult SessionResult;

	UPROPERTY(BlueprintReadOnly, Category = "Steam Multiplayer")
	FString HostName = "Unknown Friend";

	UPROPERTY(BlueprintReadOnly, Category = "Steam Multiplayer")
	int32 OpenSlots = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamSearchOutputPin, const TArray<FSteamSessionResult>&, Results);

UCLASS()
class SCRAPBOOK_API USteamAction_Search : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FSteamSearchOutputPin OnComplete;

    UFUNCTION(BlueprintCallable, Category = "Steam Multiplayer", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
    static USteamAction_Search* SearchSteamGamesAsync(const UObject* WorldContextObject, int32 MaxResults = 50);

    virtual void Activate() override;

private:
    void HandleSearchComplete(bool bWasSuccessful);

    const UObject* WorldContext;
    int32 MaxSearchResults;
    TSharedPtr<FOnlineSessionSearch> SessionSearch;
    FDelegateHandle SearchHandle;
};

// Join, also an async action with success/fail pins
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamJoinOutputPin);

UCLASS()
class SCRAPBOOK_API USteamAction_Join : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FSteamJoinOutputPin OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FSteamJoinOutputPin OnFailure;

    UFUNCTION(BlueprintCallable, Category = "Steam Multiplayer", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
    static USteamAction_Join* JoinSteamGameAsync(const UObject* WorldContextObject, FSteamSessionResult TargetSession);

    virtual void Activate() override;

private:
    void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    const UObject* WorldContext;
    FSteamSessionResult CachedTargetSession;
    FDelegateHandle JoinHandle;
};


// Host session, an async action with success/fail pins
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamHostOutputPin);

UCLASS()
class SCRAPBOOK_API USteamAction_Host : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FSteamHostOutputPin OnSuccess;

    UPROPERTY(BlueprintAssignable)
    FSteamHostOutputPin OnFailure;

    UFUNCTION(BlueprintCallable, Category = "Steam Multiplayer", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
    static USteamAction_Host* HostSteamGameAsync( const UObject* WorldContextObject, int32 MaxPlayers, TSoftObjectPtr<UWorld> LevelToOpen );

    virtual void Activate() override;

private:
    void HandleHostComplete(FName SessionName, bool bWasSuccessful);

    const UObject* WorldContext;
    TSoftObjectPtr<UWorld> CachedLevelToOpen;
    FDelegateHandle HostHandle;
    int CachedMaxPlayers;
};

