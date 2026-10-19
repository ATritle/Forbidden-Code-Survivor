#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CardDataAsset.generated.h"

class ASpellBase;
class UStaticMesh;
class UTexture;

UENUM(BlueprintType)
enum class EMagicSchool : uint8 { Necromancy, Fire, Blood, Curses, Frost, Lightning, Pulse, Prism, Kinetic, Barrier };
UENUM(BlueprintType)
enum class ECardRarity : uint8 { Common, Rare, Epic, Legendary };

/** Immutable authored definition; mutable run state belongs to the spawned spell. */
UCLASS(BlueprintType)
class CODEXRUNTIME_API UCardDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** Finite program ranks and one mutually exclusive evolution, enabled by the Windows game. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Program") bool bUseProgramProgression = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Program") FText EvolutionA;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Program") FText EvolutionB;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Program", meta=(MultiLine=true)) FText EvolutionDescriptionA;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Program", meta=(MultiLine=true)) FText EvolutionDescriptionB;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card")
    FName CardID;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card")
    FText Name;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card", meta=(MultiLine=true))
    FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card|UI")
    FLinearColor RarityColor = FLinearColor(0.34f, 0.95f, 0.72f);
    /** Texture2D icon or a pre-created render target. No per-card scene capture required. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card|UI")
    TObjectPtr<UTexture> Icon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card|UI")
    TObjectPtr<UTexture> SchoolIcon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card|UI")
    ECardRarity Rarity = ECardRarity::Common;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Card")
    EMagicSchool MagicSchool = EMagicSchool::Necromancy;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spell", meta=(ClampMin="0"))
    float BaseDamage = 10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spell", meta=(ClampMin="0.05"))
    float Cooldown = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Art")
    TSoftObjectPtr<UStaticMesh> MeshyModel;
    /** Generic static or skeletal art path. Resolve asynchronously during pre-run loading. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Art", meta=(AllowedClasses="/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
    FSoftObjectPath MeshyVisual;
    /** Hard class reference deliberately preloads spell classes with the deck. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spell")
    TSubclassOf<ASpellBase> SpellClass;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(FPrimaryAssetType(TEXT("Card")), CardID.IsNone() ? GetFName() : CardID);
    }
};
