#pragma once

#include "ProceduralMeshComponent.h"
#include "GameDataTypes.generated.h"

/**
** Gameplay Stuff
**/

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
	FEvidenceTrait() : TypeName("[None]"), Magnitude(0) {};
	FEvidenceTrait(FName T, int M) : TypeName(T), Magnitude(M) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName TypeName;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int Magnitude;
};


USTRUCT(BlueprintType)
struct FScrapbookArea
{
	GENERATED_BODY()

public:
	FScrapbookArea() : Name(), Position(0, 0), Size(0, 0) {};

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
	FScrapbookPage() : Art(nullptr), Name(), Areas({}) {};

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
	Combination, // A X and B Y become C X+Y
};

USTRUCT(BlueprintType)
struct FEvidenceTraitInteraction
{
	GENERATED_BODY()

public:
	FEvidenceTraitInteraction() : 
		InteractionType(EEvidenceTraitInteractionType::NONE), 
		InputTypeNameA("[None]"),
		InputTypeNameB("[None]"),
		OutputTypeNameC("[None]") {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EEvidenceTraitInteractionType InteractionType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName InputTypeNameA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName InputTypeNameB;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName OutputTypeNameC;
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

USTRUCT(BlueprintType)
struct FEvidenceSet
{
	GENERATED_BODY()

public:
	FEvidenceSet() : Evidence(), Traits() {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<AActor*> Evidence;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FEvidenceTrait> Traits;
};

USTRUCT(BlueprintType)
struct FJurorData : public FTableRowBase
{
	GENERATED_BODY()

public:
	FJurorData() : Name(), TraitMultipliers() {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, float> TraitMultipliers;
};

//// Types below are imported from Book of Bark!

/**
** Sound stuff
**/

USTRUCT(BlueprintType)
struct FSoundReference : public FTableRowBase
{
	GENERATED_BODY()

	// The metasound asset to play
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USoundBase* Sound;
};


/**
** Character / Narrative Stuff
**/

UENUM(BlueprintType)
enum class ECharacterEmotion : uint8
{
	NONE,
	Neutral,
	Happy,
	Sad,
	Surprise,
	Question,
	Greeting
};

// Character name + each portrait with an emotion
USTRUCT(BlueprintType)
struct FCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	// We don't have skeletal meshes for chars yet so I'm using static meshes -- use SKM when we can!
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UStaticMesh* StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USkeletalMesh* SkeletalMesh;

	// Emotions used in Dialogue Data should match the ones here
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<ECharacterEmotion, USoundBase*> EmotionsToSounds;

	// Bio entries that are locked behind progression facts (Exist check the fact, ignore value)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> ProgressionFactsToBios;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* Texture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* TextureProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* TextureProfileColorless;
};

// Writing
USTRUCT(BlueprintType)
struct FDialogueLineData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Speaker;

	// There should be a matching texture for this in the Speaker's Character Data
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECharacterEmotion Emotion;

	// There should be a matching sound for this in the Speaker's Character Data
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ECharacterEmotion SoundEmotion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Text;

	// When this line plays, the following progression facts will be set tot he given values.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FName, FString> ProgressionFactsToSet;
};

USTRUCT(BlueprintType)
struct FDialogueData : public FTableRowBase
{
	GENERATED_BODY()

	// There should be a matching texture for this name in the Speaker's Character Data
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FDialogueLineData> Lines;

	// After this dialogue plays its last line, choices here will be shown to the player as buttons that will then play their respective named dialogue when clicked
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FString, FName> ChoicesToDialogueNames;
};