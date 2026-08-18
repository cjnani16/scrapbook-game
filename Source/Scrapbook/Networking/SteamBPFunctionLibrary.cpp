#include "SteamBPFunctionLibrary.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
//#include "steam/steam_api.h"

#define SCREEN_MSG(Format, ...) \
    if (GEngine) { \
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, \
        FString::Printf(TEXT(Format), ##__VA_ARGS__)); \
    } \
    UE_LOG( LogTemp, Verbose, TEXT(Format), ##__VA_ARGS__ );

#define UNIQUE_GAME_KEY FString(TEXT("SSJ26_v1.0"))

// Searching
USteamAction_Search* USteamAction_Search::SearchSteamGamesAsync(const UObject* WorldContextObject, int32 MaxResults)
{
    USteamAction_Search* Action = NewObject<USteamAction_Search>();
    Action->WorldContext = WorldContextObject;
    Action->MaxSearchResults = MaxResults;
    return Action;
}

void USteamAction_Search::Activate()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();

    // If Steam isn't available (or we are in PIE), fallback to NULL subsystem for local testing
    if (!Subsystem || GIsEditor)
    {
        SCREEN_MSG("Search - Bypassing Steam Subsystem, using localhost due to PIE");
        Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
    }

    if (!Subsystem || !WorldContext) 
    { 
        OnComplete.Broadcast(TArray<FSteamSessionResult>()); 
        return; 
    }

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid()) 
    { 
        OnComplete.Broadcast(TArray<FSteamSessionResult>()); 
        return; 
    }

    SessionSearch = MakeShareable(new FOnlineSessionSearch());
    SessionSearch->MaxSearchResults = MaxSearchResults;
    SessionSearch->bIsLanQuery = GIsEditor;
    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, !GIsEditor, EOnlineComparisonOp::Equals);

    if (!GIsEditor)
    {
        SessionSearch->QuerySettings.Set(SETTING_GAMEMODE, UNIQUE_GAME_KEY, EOnlineComparisonOp::Equals);
    }

    FOnFindSessionsCompleteDelegate Delegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &USteamAction_Search::HandleSearchComplete);
    SearchHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);

    const ULocalPlayer* LocalPlayer = WorldContext->GetWorld()->GetFirstLocalPlayerFromController();
    if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(SearchHandle);
        OnComplete.Broadcast(TArray<FSteamSessionResult>());
        SetReadyToDestroy();
    }
}

void USteamAction_Search::HandleSearchComplete(bool bWasSuccessful)
{
    TArray<FSteamSessionResult> FormattedResults;

    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem || GIsEditor)
    {
        Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
    }

    if (!Subsystem)
    {
        OnComplete.Broadcast(FormattedResults);
        SetReadyToDestroy();
    }

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnComplete.Broadcast(FormattedResults);
        SetReadyToDestroy();
    }

    SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(SearchHandle);

    if (bWasSuccessful && SessionSearch.IsValid())
    {
        for (const FOnlineSessionSearchResult& RawResult : SessionSearch->SearchResults)
        {
            FSteamSessionResult CustomResult;
            CustomResult.SessionResult = RawResult;
            CustomResult.HostName = RawResult.Session.OwningUserName;
            CustomResult.OpenSlots = RawResult.Session.NumOpenPublicConnections;
            FormattedResults.Add(CustomResult);
        }
    }

    SCREEN_MSG("Searching %s", *FString(bWasSuccessful ? "SUCCESS" : "FAILED"));
    SCREEN_MSG("%d Results Found - Response %s", SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0, *FString(SessionSearch.IsValid() ? "VALID" : "INVALID"));

    OnComplete.Broadcast(FormattedResults);
    SetReadyToDestroy();
}

//Joining
USteamAction_Join* USteamAction_Join::JoinSteamGameAsync(const UObject* WorldContextObject, FSteamSessionResult TargetSession)
{
    USteamAction_Join* Action = NewObject<USteamAction_Join>();
    Action->WorldContext = WorldContextObject;
    Action->CachedTargetSession = TargetSession;
    return Action;
}

void USteamAction_Join::Activate()
{
    SCREEN_MSG("Joining...");
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem || GIsEditor)
    {
        SCREEN_MSG("Join - Bypassing Steam Subsystem, using localhost due to PIE");
        Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
    }

    if (!Subsystem || !WorldContext || !WorldContext->GetWorld())
    {
        OnFailure.Broadcast();
        SetReadyToDestroy();
        return;
    }

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnFailure.Broadcast();
        SetReadyToDestroy();
        return;
    }

    FOnJoinSessionCompleteDelegate Delegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &USteamAction_Join::HandleJoinSessionComplete);
    JoinHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(Delegate);

    const ULocalPlayer* LocalPlayer = WorldContext->GetWorld()->GetFirstLocalPlayerFromController();
    if (!LocalPlayer || !SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, CachedTargetSession.SessionResult))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
        OnFailure.Broadcast();
        SetReadyToDestroy();
    }
}

void USteamAction_Join::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem || GIsEditor)
    {
        Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
    }

    if (!Subsystem)
    {
        OnFailure.Broadcast();
        SetReadyToDestroy();
    }

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnFailure.Broadcast();
        SetReadyToDestroy();
    }

    SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);

    const bool bWasSuccessful = (Result == EOnJoinSessionCompleteResult::Success);
    SCREEN_MSG("Joining %s", *FString(bWasSuccessful ? "SUCCESS" : "FAILED"));

    if (bWasSuccessful && WorldContext && WorldContext->GetWorld())
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext->GetWorld(), 0);
        if (PC)
        {
            FString ConnectionInfo;
            if (SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectionInfo))
            {
                SCREEN_MSG("Join - Resolved Connect String: %s", *ConnectionInfo);

                OnSuccess.Broadcast();
                PC->ClientTravel(ConnectionInfo, ETravelType::TRAVEL_Absolute);
                SetReadyToDestroy();
                return;
            }
        }
    }

    OnFailure.Broadcast();
    SetReadyToDestroy();
}


// Hosting
USteamAction_Host* USteamAction_Host::HostSteamGameAsync( const UObject* WorldContextObject, int32 MaxPlayers, TSoftObjectPtr<UWorld> LevelToOpen )
{
    USteamAction_Host* Action = NewObject<USteamAction_Host>();
    Action->WorldContext = WorldContextObject;
    Action->CachedMaxPlayers = MaxPlayers;
    Action->CachedLevelToOpen = LevelToOpen;
    return Action;
}

void USteamAction_Host::Activate()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem || GIsEditor)
    {
        SCREEN_MSG("Host - Bypassing Steam Subsystem, using localhost due to PIE");
        Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
    }

    if (!Subsystem) return;

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid()) return;

    // Clear old session names
    FNamedOnlineSession* ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        SessionInterface->DestroySession(NAME_GameSession);
    }

    FOnlineSessionSettings SessionSettings;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bUseLobbiesIfAvailable = !GIsEditor;
    SessionSettings.bIsLANMatch = GIsEditor;
    SessionSettings.NumPublicConnections = CachedMaxPlayers;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bAllowJoinViaPresence = true;

    SessionSettings.Set(SETTING_MAPNAME, CachedLevelToOpen.GetAssetName(), EOnlineDataAdvertisementType::ViaOnlineService);

    if (!GIsEditor)
    {
        SessionSettings.Set(SETTING_GAMEMODE, UNIQUE_GAME_KEY, EOnlineDataAdvertisementType::ViaOnlineService);
    }

    SCREEN_MSG("Target Level To Host: %s", *CachedLevelToOpen.ToSoftObjectPath().GetAssetName() );

    FOnCreateSessionCompleteDelegate Delegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &USteamAction_Host::HandleHostComplete);
    HostHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(Delegate);

    const ULocalPlayer* LocalPlayer = WorldContext->GetWorld()->GetFirstLocalPlayerFromController();
    if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSettings))
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(HostHandle);
        OnFailure.Broadcast();
        SetReadyToDestroy();
    }
}

void USteamAction_Host::HandleHostComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem || GIsEditor)
    {
        Subsystem = IOnlineSubsystem::Get(NULL_SUBSYSTEM);
    }

    if (!Subsystem)
    {
        OnFailure.Broadcast();
        SetReadyToDestroy();
    }

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        OnFailure.Broadcast();
        SetReadyToDestroy();
    }

    SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(HostHandle);

    SCREEN_MSG("Hosting %s", *FString(bWasSuccessful ? "SUCCESS" : "FAILED"));

    if (bWasSuccessful)
    {
        // After session setup, move immediately to the lobby level and listen for joiners. 
        // Running OnSuccess here is probably pretty useless
        OnSuccess.Broadcast();
        UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContext, CachedLevelToOpen, true, "listen");
    }
}


