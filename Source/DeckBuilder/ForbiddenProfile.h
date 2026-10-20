#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForbiddenProfile.generated.h"

USTRUCT(BlueprintType)
struct FForbiddenDeckPreset
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name = TEXT("STREET MEDIUM");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> DraftPool;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> Starters;
};

/** Starting-card unlocks are rider-specific; the run draft always uses the full catalog. */
USTRUCT(BlueprintType)
struct FForbiddenRiderLoadouts
{
    GENERATED_BODY()
    UPROPERTY() TArray<FName> UnlockedStarters;
    UPROPERTY() TArray<FForbiddenDeckPreset> Decks;
    UPROPERTY() int32 SelectedDeck = 0;
};

USTRUCT(BlueprintType)
struct FForbiddenRunRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString ID;
    UPROPERTY(BlueprintReadOnly) FString DateUTC;
    UPROPERTY(BlueprintReadOnly) bool Victory = false;
    UPROPERTY(BlueprintReadOnly) float Seconds = 0;
    UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
    UPROPERTY(BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) int32 Contracts = 0;
    UPROPERTY(BlueprintReadOnly) int32 Currency = 0;
    UPROPERTY(BlueprintReadOnly) int32 Score = 0;
    UPROPERTY(BlueprintReadOnly) FString DeathCause;
    UPROPERTY(BlueprintReadOnly) TMap<FName, float> DamageByCard;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Programs;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Evolutions;
};

/** Versioned separately from the preserved 0.2 profile. No actor references or run state. */
UCLASS()
class DECKBUILDER_API UForbiddenProfileSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Version = 4;
    UPROPERTY() int32 Revision = 0;
    UPROPERTY() FString PlayerToken;
    UPROPERTY() FString Username;
    UPROPERTY() int32 CodeFragments = 0;
    UPROPERTY() int32 Victories = 0;
    UPROPERTY() TArray<FName> UnlockedPrograms;
    UPROPERTY() TArray<FName> UnlockedRecipes;
    UPROPERTY() TArray<FName> UnlockedBoards;
    UPROPERTY() TArray<FName> UnlockedDistrictModifiers;
    UPROPERTY() TMap<FName, int32> Research;
    // Mirrors of the selected rider, retained for existing menu/settings consumers.
    UPROPERTY() TArray<FForbiddenDeckPreset> Decks;
    UPROPERTY() int32 SelectedDeck = 0;
    UPROPERTY() TMap<FName, FForbiddenRiderLoadouts> RiderLoadouts;
    UPROPERTY() FName SelectedRider = TEXT("NeonExorcist");
    UPROPERTY() TArray<FForbiddenRunRecord> History;
    UPROPERTY() TMap<FName, float> Settings;
    UPROPERTY() TMap<FName, FKey> Bindings;
};

/** Validates all loadouts and banks a run only once. Practice never earns permanent rewards. */
UCLASS()
class DECKBUILDER_API UForbiddenProfile : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UForbiddenProfileSave> Data;
    UPROPERTY(BlueprintReadOnly) FString LastError;
    UFUNCTION(BlueprintCallable) bool Save();
    UFUNCTION(BlueprintCallable) bool SelectRider(FName ID);
    UFUNCTION(BlueprintCallable) bool UnlockProgram(FName ID);
    UFUNCTION(BlueprintCallable) bool SaveDeck(int32 Slot, const FForbiddenDeckPreset& Deck);
    UFUNCTION(BlueprintCallable) bool RecordRun(const FForbiddenRunRecord& Record, bool Practice);
    UFUNCTION(BlueprintPure) bool IsUnlocked(FName ID) const;
    UFUNCTION(BlueprintPure) FForbiddenDeckPreset GetDeck() const;
    static const TArray<FName>& ProgramIDs();
    static bool ValidateDeck(const FForbiddenDeckPreset& Deck, const TArray<FName>& Unlocked, FString& Error);
    FString SlotName() const;
private:
    void SetDefaults();
    void Sanitize();
    void StoreSelectedRider();
    void LoadSelectedRider();
};
