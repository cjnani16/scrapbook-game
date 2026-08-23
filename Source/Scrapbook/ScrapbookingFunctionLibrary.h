// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScrapbookingFunctionLibrary.generated.h"

class UDynamicMesh;
class UStaticMeshComponent;
class ULineSetComponent;
class UScrapbookSaveGame;
struct FScrapbookPage;
struct FGameProgressionData;

/**
 * 
 */
UCLASS(BlueprintType)
class SCRAPBOOK_API UScrapbookingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Progression stuff
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool GetSaveData(FGameProgressionData& OutData);

	UFUNCTION(BlueprintCallable)
	static void SetSaveData(const FGameProgressionData& NewData);

	UFUNCTION(BlueprintCallable)
	static void ClearSaveData();

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void SetProgressionFactValue(const UObject* WorldContextObject, const FName Fact, const FString& Value);

	UFUNCTION(BlueprintCallable)
	static FString GetProgressionFactValue(const FName Fact, bool& Found);

	// Debug stuff
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDrawPath(const UObject* WorldContextObject, const TArray<FVector>& PathPoints, ULineSetComponent* LineSetComponent, const int DotLength = 3, const int GapLength = 2);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDrawPageAreas(const UObject* WorldContextObject, const FTransform& PageComponentTransform, const FVector& PageLocalMin, const FVector& PageLocalMax, const FScrapbookPage& Page);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDrawPageNormalizationResult(
		const UObject* WorldContextObject,
		UDynamicMesh* EvidenceMesh,
		const FTransform& EvidenceMeshTransform,
		const FTransform& PageTransform,
		FBox PageBox,
		const FScrapbookPage& Page,
		float Duration = 5.0f);

	// Geometry stuff for gameplay
	UFUNCTION(BlueprintCallable)
	static void TriangulatePathPoints(const FTransform& SourceMeshComponentTransform, const FTransform& ProcMeshComponentTransform, const FVector& SourceLocalMin, const FVector& SourceLocalMax, const TArray<FVector>& PathPoints, TArray<FVector>& Vertices, TArray<int>& Triangles, TArray<FVector>& Normals, TArray<FVector2D>& UV0);

	UFUNCTION(BlueprintCallable)
	static bool CheckForPathIntersection(const TArray<FVector>& PathPoints, TArray<FVector>& LoopedPoints);

	// Helper to get area
	static float CalculateSignedArea(const TArray<FVector>& PathPoints);

	UFUNCTION(BlueprintCallable)
	static float CalculateArea(const TArray<FVector>& PathPoints);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void GetClippedTraitsFromMeshAndPage(const UObject* WorldContextObject, const FTransform& EvidenceMeshTransform, const FTransform& PageTransform, FBox PageSize, const FScrapbookPage& Page, UDynamicMesh* EvidenceMesh, bool DrawDebug, TArray<FEvidenceTrait>& ClippedTraits);

	UFUNCTION(BlueprintCallable)
	static void LayoutActorsInTightSpiral(TArray<AActor*> TargetActors, float Padding, FVector CenterLocation);

	UFUNCTION(BlueprintCallable)
	static UDynamicMesh* AppendSimpleExtrudeLoop(UDynamicMesh* TargetMesh, const TArray<FVector2D>& PolygonVertices, float Height);

	UFUNCTION(BlueprintCallable)
	static bool DoPolygonsIntersect(const TArray<FVector2D>& A, const TArray<FVector2D>& B );

	UFUNCTION(BlueprintCallable)
	static TArray<AActor*> ActorLocationSort(const TArray<AActor*>& Array);
	
	UFUNCTION(BlueprintCallable)
	static TArray<FEvidenceTrait> GetTraitProductsFromInteractions( const TArray<FEvidenceTrait>& A, const TArray<FEvidenceTrait>& B, AActor* ActorA, AActor* ActorB, const TArray<FEvidenceTraitInteraction>& Rules, const FVector& Location, UPARAM(ref) TArray<FEvidenceSet>& EvidenceSets, TArray<FEvidenceTraitInteractionResult>& RuleApplicationResults );

	UFUNCTION(BlueprintCallable)
	static TArray<FEvidenceTrait> SumTraits( const TArray<FEvidenceTrait>& Input );

	UFUNCTION(BlueprintCallable)
	static void DoJurorScoring( const int Threshold, const TArray<FJurorData>& Jurors, const TArray<FEvidenceTrait>& Traits, TArray<float>& Scores, TArray<float>& ReactionScales, bool& AllPassed );

	// Editor stuff
	UFUNCTION(BlueprintCallable)
	static void CopyToClipboard(const FString& TextToCopy);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FString ReadFromClipboard();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FString ConvertPageStructToUnrealText(const FScrapbookPage& PageData);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static FString ConvertInteractionsToUnrealText(const TArray<FEvidenceTraitInteraction>& Rules);

	UFUNCTION(BlueprintCallable)
	static bool ConvertUnrealTextToPageStruct(const FString& UnrealText, FScrapbookPage& OutPage);

	UFUNCTION(BlueprintCallable)
	static bool ConvertUnrealTextToInteractionList(const FString& UnrealText, TArray<FEvidenceTraitInteraction>& Rules);

	UFUNCTION(BlueprintCallable)
	static UTexture2D* LoadPNGViaFileDialog( FString DialogTitle = TEXT("Select PNG Image"));

private:
	static UScrapbookSaveGame* CurrentSave;
};
