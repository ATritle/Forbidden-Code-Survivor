#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SpellBase.generated.h"

class AEnemySpawner;
class APooledEnemy;
class UCardDataAsset;

USTRUCT(BlueprintType)
struct CODEXRUNTIME_API FSpellModifiers
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mutation")
    int32 ProjectileCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mutation")
    int32 PiercingCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mutation")
    float RadiusMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mutation")
    bool ExplodeOnImpact = false;
    /** Emit a full radial volley instead of the directed spread. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mutation")
    bool NovaPattern = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minions")
    bool BoneArcher = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minions")
    bool GraveGuard = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Minions")
    bool GraveBurst = false;
};

/** Timer and targeting framework. Override FireSpell to emit your actual attack. */
UCLASS(Abstract, Blueprintable)
class CODEXRUNTIME_API ASpellBase : public AActor
{
    GENERATED_BODY()
public:
    ASpellBase();
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Spell")
    float Damage = 0.f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Spell")
    float Cooldown = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spell", meta=(ClampMin="0"))
    float TargetRange = 3000.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mutation")
    FSpellModifiers Modifiers;
    UPROPERTY(EditDefaultsOnly, Category="Spell")
    bool bRequiresTarget = true;
    /** Specializations supply behavior-specific offers (e.g. summon spells). */
    virtual TArray<FGameplayTag> GetAvailableMutationTags() const;

    UFUNCTION(BlueprintCallable, Category="Spell")
    virtual void InitializeSpell(UCardDataAsset* Card, AEnemySpawner* Spawner);
    UFUNCTION(BlueprintCallable, Category="Spell")
    virtual void StartCasting();
    UFUNCTION(BlueprintCallable, Category="Spell")
    void StopCasting();
    void SetCastingPaused(bool bPaused);
    /** Exact supported tags only. Counts add rounded Value, radius multiplies by Value,
     * explosion/nova set true for Value > 0; enabling nova ensures at least eight shots.
     * Invalid tags/values return false without changes.
     */
    UFUNCTION(BlueprintCallable, Category="Mutation")
    virtual bool ApplyMutation(FGameplayTag ModifierTag, float Value);

protected:
    /** Snapshot is copied at fire time. Pass it into each projectile so impacts use it too. */
    UFUNCTION(BlueprintNativeEvent, Category="Spell")
    void FireSpell(APooledEnemy* Target, const FSpellModifiers& Snapshot);
    virtual void FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void TryFire();
    TWeakObjectPtr<AEnemySpawner> EnemyRegistry;
    FTimerHandle FireTimer;
    bool bInitialized = false;
};
