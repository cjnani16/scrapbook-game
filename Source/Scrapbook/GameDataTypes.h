#pragma once

#include "ProceduralMeshComponent.h"
#include "GameDataTypes.generated.h"

UENUM(BlueprintType)
enum class EEvidenceTraitType : uint8
{
	NONE,
	Wealth,
	Love,
	Elsewhere,
	Negligence,
	Threat,
	Loyalty,
	Provocation,
	Responsibility,
	Passion,
	Alibi,
	Accident,
	SelfDefense,
	Sacrifice,
	Debt,
	Suspicion,
	Betrayal,
	Violence,
	Hate,
	Anger
};

USTRUCT(BlueprintType)
struct FEvidenceTrait
{
	GENERATED_BODY()

public:
	FEvidenceTrait() : Type(EEvidenceTraitType::NONE), Magnitude(0) {};
	FEvidenceTrait(EEvidenceTraitType T, int M) : Type(T), Magnitude(M) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EEvidenceTraitType Type;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Magnitude;
};


USTRUCT(BlueprintType)
struct FScrapbookArea
{
	GENERATED_BODY()

public:
	FScrapbookArea() : Name(""), Position(0, 0), Size(0, 0) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Name;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector2D Position;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector2D Size;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FEvidenceTrait Trait;
};

USTRUCT(BlueprintType)
struct FScrapbookPage
{
	GENERATED_BODY()

public:
	FScrapbookPage() : Art(nullptr), Name(""), Areas({}) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UMaterialInterface> Art;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FScrapbookArea> Areas;
};

USTRUCT(BlueprintType)
struct FPlacedEvidenceData
{
	GENERATED_BODY()

public:
	FPlacedEvidenceData() : Mesh(nullptr), Traits({}) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UProceduralMeshComponent* Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FEvidenceTrait> Traits;
};

UENUM(BlueprintType)
enum class EEvidenceTraitInteractionType : uint8
{
	NONE,
	Conversion, // A converts X % of B to C
};

USTRUCT(BlueprintType)
struct FEvidenceTraitInteraction
{
	GENERATED_BODY()

public:
	FEvidenceTraitInteraction() : 
		InteractionType(EEvidenceTraitInteractionType::NONE), 
		InputTypeA(EEvidenceTraitType::NONE), 
		InputTypeB(EEvidenceTraitType::NONE), 
		OutputTypeC(EEvidenceTraitType::NONE) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EEvidenceTraitInteractionType InteractionType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EEvidenceTraitType InputTypeA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EEvidenceTraitType InputTypeB;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EEvidenceTraitType OutputTypeC;
};


USTRUCT(BlueprintType)
struct FEvidenceTraitInteractionResult
{
	GENERATED_BODY()

public:
	FEvidenceTraitInteractionResult() : Rule(), Values({}), Location() {};
	FEvidenceTraitInteractionResult(const FEvidenceTraitInteraction& R, const TArray<int>& V, const FVector& L) : Rule(R), Values(V), Location(L) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FEvidenceTraitInteraction Rule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int> Values;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector Location;
};