// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScrapbookingFunctionLibrary.generated.h"

class UDynamicMesh;
class UStaticMeshComponent;
struct FScrapbookPage;

/**
 * 
 */
UCLASS(BlueprintType)
class SCRAPBOOK_API UScrapbookingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Debug stuff
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
	static void DebugDrawPath(const UObject* WorldContextObject, const TArray<FVector>& PathPoints);

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
};
