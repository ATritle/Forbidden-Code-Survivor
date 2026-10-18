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

TArray<FGameplayTag> ASpellBase::GetAvailableMutationTags() const
{
    TArray<FGameplayTag> Result;
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
    bInitialized = true;
}

void ASpellBase::StartCasting()
{
    if (!bInitialized || !EnemyRegistry.IsValid() || !IsValid(GetOwner())) return;
    // First shot after one cooldown; avoids activation callbacks firing during deck mutation.
    GetWorldTimerManager().SetTimer(FireTimer, this, &ASpellBase::TryFire, Cooldown, true);
}
void ASpellBase::StopCasting() { GetWorldTimerManager().ClearTimer(FireTimer); }
void ASpellBase::SetCastingPaused(bool bPaused)
{
    if(bPaused) GetWorldTimerManager().PauseTimer(FireTimer);else GetWorldTimerManager().UnPauseTimer(FireTimer);
}

void ASpellBase::TryFire()
{
    if (!IsValid(GetOwner()) || !EnemyRegistry.IsValid()) { StopCasting(); return; }
    APooledEnemy* Target = EnemyRegistry->FindNearestEnemy(GetOwner()->GetActorLocation(), TargetRange,true);
    if (Target || !bRequiresTarget)
    {
        const FSpellModifiers Snapshot = Modifiers;
        FireSpell(Target, Snapshot);
    }
}

bool ASpellBase::ApplyMutation(FGameplayTag ModifierTag, float Value)
{
    if (!FMath::IsFinite(Value)) return false;
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

void ASpellBase::FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot)
{
    // Intentionally empty: projectile, nova, aura, and summon spells implement different outputs.
}
void ASpellBase::EndPlay(const EEndPlayReason::Type Reason)
{
    StopCasting();
    Super::EndPlay(Reason);
}
