#include "SpellBase.h"
#include "CardDataAsset.h"
#include "EnemySpawner.h"
#include "PooledEnemy.h"
#include "Components/SceneComponent.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ProjectileCount, "Mutation.ProjectileCount");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PiercingCount, "Mutation.PiercingCount");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RadiusMultiplier, "Mutation.RadiusMultiplier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ExplodeOnImpact, "Mutation.ExplodeOnImpact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Nova, "Mutation.Nova");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Archer, "Mutation.Minion.BoneArcher");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Guard, "Mutation.Minion.GraveGuard");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Burst, "Mutation.Minion.GraveBurst");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Rank, "Mutation.Program.Rank");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EvolutionA, "Mutation.Program.EvolutionA");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EvolutionB, "Mutation.Program.EvolutionB");

TArray<FGameplayTag> ASpellBase::GetAvailableMutationTags() const
{
    TArray<FGameplayTag> Result;
    if(UsesProgramProgression())
    {
        if(Modifiers.Rank<3) Result.Add(TAG_Rank);
        else if(Modifiers.Evolution==0) {Result.Add(TAG_EvolutionA);Result.Add(TAG_EvolutionB);}
        return Result;
    }
    if (!Modifiers.NovaPattern) Result.Add(TAG_Nova);
    if (!Modifiers.ExplodeOnImpact) Result.Add(TAG_ExplodeOnImpact);
    if (Modifiers.PiercingCount == 0) Result.Add(TAG_PiercingCount);
    return Result;
}

ASpellBase::ASpellBase()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASpellBase::InitializeSpell(UCardDataAsset* Card, AEnemySpawner* Spawner)
{
    StopCasting();
    bInitialized = false;
    EnemyRegistry = Spawner;
    if (!IsValid(Card) || !IsValid(Spawner)) return;
    Damage = FMath::IsFinite(Card->BaseDamage) ? FMath::Max(0.f, Card->BaseDamage) : 0.f;
    Cooldown = FMath::IsFinite(Card->Cooldown) ? FMath::Max(0.05f, Card->Cooldown) : 1.f;
    Modifiers = GetClass()->GetDefaultObject<ASpellBase>()->Modifiers;
    CardDefinition=Card; Modifiers.ProgramID=Card->CardID;
    bInitialized = true;
}

void ASpellBase::StartCasting()
{
    if (!bInitialized || !EnemyRegistry.IsValid() || !IsValid(GetOwner())) return;
    // First shot after one cooldown; avoids activation callbacks firing during deck mutation.
    GetWorldTimerManager().SetTimer(FireTimer, this, &ASpellBase::TryFire, Cooldown*GlobalCooldownScale, true);
}
void ASpellBase::StopCasting() { GetWorldTimerManager().ClearTimer(FireTimer); }
void ASpellBase::SetCastingPaused(bool bPaused)
{
    if(bPaused) GetWorldTimerManager().PauseTimer(FireTimer);else GetWorldTimerManager().UnPauseTimer(FireTimer);
}

void ASpellBase::TryFire()
{
    if (!IsValid(GetOwner()) || !EnemyRegistry.IsValid()) { StopCasting(); return; }
    APooledEnemy* Target = ResolveTarget();
    if (Target || !bRequiresTarget)
    {
        const FSpellModifiers Snapshot = Modifiers;
        FireSpell(Target, Snapshot);
    }
}

APooledEnemy* ASpellBase::ResolveTarget() const
{
    return EnemyRegistry.IsValid() && IsValid(GetOwner()) ? EnemyRegistry->FindNearestEnemy(GetOwner()->GetActorLocation(),TargetRange,true) : nullptr;
}

bool ASpellBase::ApplyMutation(FGameplayTag ModifierTag, float Value)
{
    if (!FMath::IsFinite(Value)) return false;
    if(UsesProgramProgression())
    {
        if(Value<=0 || !ASpellBase::GetAvailableMutationTags().Contains(ModifierTag)) return false;
        if(ModifierTag==TAG_Rank)
        {
            ++Modifiers.Rank;
            Damage=CardDefinition->BaseDamage*(1.f+0.12f*(Modifiers.Rank-1));
            Cooldown=CardDefinition->Cooldown*(1.f-0.04f*(Modifiers.Rank-1));
            if(Modifiers.ProgramID=="PulseDart" || Modifiers.ProgramID=="PhotonLance" || Modifiers.ProgramID=="HaloNova" || Modifiers.ProgramID=="KineticCards" || Modifiers.ProgramID=="ShatterSigil") Modifiers.PiercingCount=FMath::Min(5,Modifiers.PiercingCount+1);
            else if(Modifiers.ProgramID=="PrismBurst" || Modifiers.ProgramID=="EchoPetals" || Modifiers.ProgramID=="SatelliteChoir" || Modifiers.ProgramID=="OrbitRazor") Modifiers.ProjectileCount=FMath::Min(Modifiers.ProgramID=="PrismBurst" ? 5 : 3,Modifiers.ProjectileCount+1);
            else Modifiers.RadiusMultiplier=FMath::Min(1.3f,Modifiers.RadiusMultiplier+0.15f);
            StartCasting();
        }
        else
        {
            Modifiers.Evolution=ModifierTag==TAG_EvolutionA ? 1 : 2;
            Modifiers.RadiusMultiplier=FMath::Min(1.5f,Modifiers.RadiusMultiplier+0.2f);
        }
        return true;
    }
    if (ModifierTag == TAG_ProjectileCount)
        Modifiers.ProjectileCount = FMath::Clamp(Modifiers.ProjectileCount + FMath::RoundToInt(FMath::Clamp(Value, -128.f, 128.f)), 1, 128);
    else if (ModifierTag == TAG_PiercingCount)
        Modifiers.PiercingCount = FMath::Clamp(Modifiers.PiercingCount + FMath::RoundToInt(FMath::Clamp(Value, -128.f, 128.f)), 0, 128);
    else if (ModifierTag == TAG_RadiusMultiplier)
    {
        if (Value <= 0.f) return false;
        Modifiers.RadiusMultiplier = FMath::Clamp(Modifiers.RadiusMultiplier * FMath::Min(Value, 100.f), 0.1f, 20.f);
    }
    else if (ModifierTag == TAG_ExplodeOnImpact) Modifiers.ExplodeOnImpact = Value > 0.f;
    else if (ModifierTag == TAG_Nova)
    {
        Modifiers.NovaPattern = Value > 0.f;
        if (Modifiers.NovaPattern) Modifiers.ProjectileCount = FMath::Max(8, Modifiers.ProjectileCount);
    }
    else if (ModifierTag == TAG_Archer) Modifiers.BoneArcher = Value > 0.f;
    else if (ModifierTag == TAG_Guard) Modifiers.GraveGuard = Value > 0.f;
    else if (ModifierTag == TAG_Burst) Modifiers.GraveBurst = Value > 0.f;
    else return false;
    return true;
}

bool ASpellBase::UsesProgramProgression() const {return CardDefinition && CardDefinition->bUseProgramProgression;}
float ASpellBase::GetCooldownFraction() const
{
    if(!GetWorld() || !GetWorldTimerManager().TimerExists(FireTimer)) return 1.f;
    return FMath::Clamp(1.f-GetWorldTimerManager().GetTimerRemaining(FireTimer)/FMath::Max(0.05f,Cooldown*GlobalCooldownScale),0.f,1.f);
}
void ASpellBase::SetGlobalCooldownScale(float Scale)
{
    GlobalCooldownScale=FMath::Clamp(Scale,0.5f,2.f);
    if(GetWorldTimerManager().TimerExists(FireTimer)) StartCasting();
}
FText ASpellBase::DescribeMutation(FGameplayTag Tag) const
{
    if(!UsesProgramProgression()) return FText();
    if(Tag==TAG_EvolutionA) return FText::Format(FText::FromString(TEXT("{0}\n{1}\nExclusive evolution; locks the other branch.")),CardDefinition->EvolutionA,CardDefinition->EvolutionDescriptionA);
    if(Tag==TAG_EvolutionB) return FText::Format(FText::FromString(TEXT("{0}\n{1}\nExclusive evolution; locks the other branch.")),CardDefinition->EvolutionB,CardDefinition->EvolutionDescriptionB);
    if(Tag==TAG_Rank)
    {
        const int32 Next=Modifiers.Rank+1;
        FString Structure=Modifiers.ProgramID=="PulseDart" || Modifiers.ProgramID=="PhotonLance" || Modifiers.ProgramID=="HaloNova" || Modifiers.ProgramID=="KineticCards" || Modifiers.ProgramID=="ShatterSigil" ? TEXT("+1 pierce (maximum 5)")
            : Modifiers.ProgramID=="PrismBurst" || Modifiers.ProgramID=="EchoPetals" || Modifiers.ProgramID=="SatelliteChoir" || Modifiers.ProgramID=="OrbitRazor" ? TEXT("+1 projectile per volley") : TEXT("+15% reach/field scale (capped at 1.3x)");
        if(Modifiers.ProgramID=="TalismanGuard") Structure=TEXT("+2 consumable charges (maximum 8). Basic: 1 charge; medium: 2; elite: 3; boss: 4 for 8% max HP.");
        return FText::FromString(FString::Printf(TEXT("RANK %d -> %d / 3\nDamage %.1f -> %.1f\nCooldown %.2fs -> %.2fs\n%s\nRank 3 unlocks two exclusive evolutions."),Modifiers.Rank,Next,Damage,CardDefinition->BaseDamage*(1.f+0.12f*(Next-1)),Cooldown,CardDefinition->Cooldown*(1.f-0.04f*(Next-1)),*Structure));
    }
    return FText();
}

void ASpellBase::FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot)
{
    // Intentionally empty: projectile, nova, aura, and summon spells implement different outputs.
}
void ASpellBase::EndPlay(const EEndPlayReason::Type Reason)
{
    StopCasting();
    Super::EndPlay(Reason);
}
