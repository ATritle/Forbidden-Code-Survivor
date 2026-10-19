#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "SpellBase.h"
#include "EnemySpawner.h"
#include "Engine/World.h"

UCodexDeckComponent::UCodexDeckComponent() { PrimaryComponentTick.bCanEverTick = false; }

namespace
{
    TArray<FGameplayTag> AvailableMutations(const ASpellBase* Spell)
    {
        return Spell->GetAvailableMutationTags();
    }
}

FGameplayTag UCodexDeckComponent::GetOfferedMutation(FName CardID) const
{
    const auto* Tag = OfferedMutations.Find(CardID);
    return Tag ? *Tag : FGameplayTag();
}

bool UCodexDeckComponent::OwnsCard(FName ID) const
{
    const TObjectPtr<ASpellBase>* Weapon = WeaponsByCard.Find(ID);
    return Weapon && IsValid(Weapon->Get());
}

ASpellBase* UCodexDeckComponent::FindOwnedSpell(FName CardID) const
{
    const TObjectPtr<ASpellBase>* Weapon = WeaponsByCard.Find(CardID);
    return Weapon && IsValid(Weapon->Get()) ? Weapon->Get() : nullptr;
}

TArray<UCardDataAsset*> UCodexDeckComponent::DrawLevelUpHand()
{
    TArray<UCardDataAsset*> Result;
    if (bSelecting) return Result;
    if (CurrentHand.IsEmpty())
    {
        OfferedMutations.Reset();
        TSet<FName> UnlockedIDs;
        for (const UCardDataAsset* Card : UnlockedCards)
            if (IsValid(Card) && !Card->CardID.IsNone()) UnlockedIDs.Add(Card->CardID);
        TSet<FName> Seen;
        TArray<UCardDataAsset*> Candidates;
        for (UCardDataAsset* Card : EquippedDeck)
        {
            if (!IsValid(Card) || Card->CardID.IsNone() || BanishedCards.Contains(Card->CardID) || !Card->SpellClass
                || Card->SpellClass->HasAnyClassFlags(CLASS_Abstract)
                || !UnlockedIDs.Contains(Card->CardID) || (!bOfferOwnedSpellUpgrades && OwnsCard(Card->CardID)) || Seen.Contains(Card->CardID)) continue;
            Seen.Add(Card->CardID);
            if (ASpellBase* Spell = FindOwnedSpell(Card->CardID))
            {
                const auto Mutations = AvailableMutations(Spell);
                if (Mutations.IsEmpty()) continue;
                OfferedMutations.Add(Card->CardID, Mutations[FMath::RandRange(0, Mutations.Num()-1)]);
            }
            Candidates.Add(Card);
        }
        // Sampling without replacement: duplicates by asset or CardID never enter the hand.
        const int32 Count = FMath::Min(3, Candidates.Num());
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const int32 Pick = FMath::RandRange(0, Candidates.Num() - 1);
            CurrentHand.Add(Candidates[Pick]);
            Candidates.RemoveAtSwap(Pick, 1, EAllowShrinking::No);
        }
    }
    for (UCardDataAsset* Card : CurrentHand) Result.Add(Card);
    return Result;
}

ASpellBase* UCodexDeckComponent::AddSelectedCardSpell(UCardDataAsset* SelectedCard)
{
    if (bSelecting || !IsValid(SelectedCard) || !CurrentHand.Contains(SelectedCard)
        || (!bOfferOwnedSpellUpgrades && OwnsCard(SelectedCard->CardID)) || !SelectedCard->SpellClass
        || SelectedCard->SpellClass->HasAnyClassFlags(CLASS_Abstract)
        || !IsValid(EnemySpawner) || !IsValid(GetOwner()) || !GetWorld()) return nullptr;
    TGuardValue<bool> Guard(bSelecting, true);
    if (ASpellBase* Existing = FindOwnedSpell(SelectedCard->CardID))
    {
        const FGameplayTag Tag = GetOfferedMutation(SelectedCard->CardID);
        if (!Tag.IsValid() || !AvailableMutations(Existing).Contains(Tag)) return nullptr;
        const float Value = Tag == FGameplayTag::RequestGameplayTag(TEXT("Mutation.RadiusMultiplier")) ? 1.25f
            : Tag == FGameplayTag::RequestGameplayTag(TEXT("Mutation.PiercingCount")) ? 2.f : 1.f;
        if (!Existing->ApplyMutation(Tag, Value)) return nullptr;
        CurrentHand.Reset();
        OfferedMutations.Reset();
        return Existing;
    }
    ActiveWeapons.RemoveAll([](const TObjectPtr<ASpellBase>& Entry){return !IsValid(Entry);});
    if(MaximumActiveWeapons>0 && ActiveWeapons.Num()>=MaximumActiveWeapons && !OwnsCard(PendingReplacement)) return nullptr;
    const FTransform Transform = GetOwner()->GetActorTransform();
    ASpellBase* Spell = GetWorld()->SpawnActorDeferred<ASpellBase>(SelectedCard->SpellClass, Transform,
        GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Spell) return nullptr;
    Spell->FinishSpawning(Transform);
    if (!IsValid(Spell)) return nullptr;
    Spell->InitializeSpell(SelectedCard, EnemySpawner);
    Spell->SetGlobalCooldownScale(GlobalCooldownScale);
    if(ASpellBase* Old=FindOwnedSpell(PendingReplacement))
    {
        Old->StopCasting(); ActiveWeapons.Remove(Old); WeaponsByCard.Remove(PendingReplacement); Old->Destroy();
    }
    PendingReplacement=NAME_None;
    Spell->AttachToActor(GetOwner(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    ActiveWeapons.RemoveAll([](const TObjectPtr<ASpellBase>& Entry) { return !IsValid(Entry); });
    ActiveWeapons.Add(Spell);
    WeaponsByCard.Add(SelectedCard->CardID, Spell);
    CurrentHand.Reset();
    OfferedMutations.Reset();
    Spell->StartCasting();
    return Spell;
}

bool UCodexDeckComponent::RerollHand()
{
    if(bSelecting || CurrentHand.IsEmpty() || RerollsRemaining<=0) return false;
    --RerollsRemaining;DiscardHand();DrawLevelUpHand();return true;
}
bool UCodexDeckComponent::BanishChoice(int32 Index)
{
    if(bSelecting || !CurrentHand.IsValidIndex(Index) || BanishesRemaining<=0) return false;
    BanishedCards.Add(CurrentHand[Index]->CardID);--BanishesRemaining;DiscardHand();DrawLevelUpHand();return true;
}
bool UCodexDeckComponent::SetOfferedEvolution(FName CardID,bool BranchB)
{
    auto* Spell=FindOwnedSpell(CardID);
    if(!Spell || !Spell->UsesProgramProgression() || !CurrentHand.Contains(Spell->CardDefinition)) return false;
    const auto Tag=FGameplayTag::RequestGameplayTag(BranchB ? TEXT("Mutation.Program.EvolutionB") : TEXT("Mutation.Program.EvolutionA"));
    if(!Spell->GetAvailableMutationTags().Contains(Tag)) return false;
    OfferedMutations.Add(CardID,Tag);return true;
}
bool UCodexDeckComponent::ReplaceSelectedSpell(UCardDataAsset* Card,FName ReplacedID)
{
    if(!Card || OwnsCard(Card->CardID) || !OwnsCard(ReplacedID)) return false;
    PendingReplacement=ReplacedID;
    const bool OK=AddSelectedCardSpell(Card)!=nullptr;PendingReplacement=NAME_None;return OK;
}

void UCodexDeckComponent::DiscardHand() { if (!bSelecting) { CurrentHand.Reset(); OfferedMutations.Reset(); } }
void UCodexDeckComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ResetActiveSpells();Super::EndPlay(Reason);
}
void UCodexDeckComponent::ResetActiveSpells()
{
    for (ASpellBase* Spell : ActiveWeapons) if (IsValid(Spell)) Spell->Destroy();
    ActiveWeapons.Reset(); WeaponsByCard.Reset(); CurrentHand.Reset();OfferedMutations.Reset();
}
