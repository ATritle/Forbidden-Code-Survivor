#include "MVPGame.h"
#include "NativeGameplayTags.h"
#include "CardDataAsset.h"
#include "EnemySpawner.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TalismanReinforce,"Mutation.Talisman.Add");

void ATalismanGuardSpell::InitializeSpell(UCardDataAsset* Card,AEnemySpawner* Registry)
{
    Super::InitializeSpell(Card,Registry);
    if(!IsValid(Card) || !IsValid(Registry)) return;
    // No casting timer is needed: collision and rendering use the player's prewarmed pool.
    bRequiresTarget=true;
    if(auto* Player=Cast<AMVPPlayer>(GetOwner())) Player->AddTalismans(3);
}
TArray<FGameplayTag> ATalismanGuardSpell::GetAvailableMutationTags() const
{
    const auto* Player=Cast<AMVPPlayer>(GetOwner());
    return Player && Player->GetTalismanCount()<AMVPPlayer::MaxTalismans ? TArray<FGameplayTag>{TalismanReinforce} : TArray<FGameplayTag>{};
}
bool ATalismanGuardSpell::ApplyMutation(FGameplayTag Tag,float Value)
{
    if(Tag!=TalismanReinforce || !FMath::IsFinite(Value) || Value<=0.f) return false;
    auto* Player=Cast<AMVPPlayer>(GetOwner());
    return Player && Player->AddTalismans(2)>0;
}
