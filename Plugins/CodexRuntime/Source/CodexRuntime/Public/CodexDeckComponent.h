#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CodexDeckComponent.generated.h"

class UCardDataAsset;
class ASpellBase;
class AEnemySpawner;

UCLASS(ClassGroup=(Codex), meta=(BlueprintSpawnableComponent))
class CODEXRUNTIME_API UCodexDeckComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCodexDeckComponent();
    /** Persistent unlock inventory. Save CardIDs in your SaveGame, not actor pointers. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Codex")
    TArray<TObjectPtr<UCardDataAsset>> UnlockedCards;
    /** Pre-run selection; draws use only valid, unlocked cards in this deck. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Codex")
    TArray<TObjectPtr<UCardDataAsset>> EquippedDeck;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex")
    TObjectPtr<AEnemySpawner> EnemySpawner;
    /** Owned spells offer missing structural mutations, never flat stat upgrades. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex")
    bool bOfferOwnedSpellUpgrades = false;
    /** Zero means unlimited for generic integration and the dedicated test range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Codex") int32 MaximumActiveWeapons = 0;
    float GlobalCooldownScale = 1.f;
    UPROPERTY(BlueprintReadOnly, Category="Codex") int32 RerollsRemaining = 2;
    UPROPERTY(BlueprintReadOnly, Category="Codex") int32 BanishesRemaining = 1;
    UPROPERTY(BlueprintReadOnly, Category="Codex") TSet<FName> BanishedCards;
    UFUNCTION(BlueprintCallable, Category="Codex") bool RerollHand();
    UFUNCTION(BlueprintCallable, Category="Codex") bool BanishChoice(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Codex") bool SetOfferedEvolution(FName CardID, bool BranchB);
    UFUNCTION(BlueprintCallable, Category="Codex") bool ReplaceSelectedSpell(UCardDataAsset* Card, FName ReplacedID);
    UFUNCTION(BlueprintPure, Category="Codex")
    ASpellBase* FindOwnedSpell(FName CardID) const;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Codex")
    TArray<TObjectPtr<ASpellBase>> ActiveWeapons;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Codex")
    TArray<TObjectPtr<UCardDataAsset>> CurrentHand;
    /** Mutation selected when the hand is drawn; stable until selected or discarded.
     * Invalid tag means a new spell. Fully transformed spells are excluded.
     */
    UFUNCTION(BlueprintPure, Category="Codex")
    FGameplayTag GetOfferedMutation(FName CardID) const;

    /** Call once for each level-up. Returns the pending hand on repeated calls.
     * Draws up to three unique CardIDs; already-owned spells are excluded unless upgrades are enabled.
     */
    UFUNCTION(BlueprintCallable, Category="Codex")
    TArray<UCardDataAsset*> DrawLevelUpHand();
    /** Only accepts a card from CurrentHand. Success consumes the whole offer. */
    UFUNCTION(BlueprintCallable, Category="Codex")
    ASpellBase* AddSelectedCardSpell(UCardDataAsset* SelectedCard);
    UFUNCTION(BlueprintCallable, Category="Codex")
    void DiscardHand();
    /** Clears temporary weapons without touching unlocked cards or saved loadouts. */
    void ResetActiveSpells();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool OwnsCard(FName ID) const;
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<ASpellBase>> WeaponsByCard;
    bool bSelecting = false;
    UPROPERTY(Transient)
    TMap<FName, FGameplayTag> OfferedMutations;
    FName PendingReplacement;
};
