#pragma once
#include "CoreMinimal.h"
#include "SpellBase.h"
#include "SkeletonMinion.generated.h"

class UMortisTerrain;
class AMVPEnemy;
class UNecromancyVisualComponent;
class UStaticMeshComponent;

/** Persistent summon; its timer replenishes the bounded prewarmed formation. */
UCLASS()
class DECKBUILDER_API ASkeletonSummonSpell : public ASpellBase
{
    GENERATED_BODY()
public:
    ASkeletonSummonSpell();
    virtual TArray<FGameplayTag> GetAvailableMutationTags() const override;
protected:
    virtual void FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot) override;
};

/** Visual-only ally; no physics or enemy-channel collision. Open-arena steering. */
UCLASS()
class DECKBUILDER_API ASkeletonMinion : public AActor
{
    GENERATED_BODY()
public:
    ASkeletonMinion();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Activate(ASkeletonSummonSpell* Spell, int32 FormationIndex);
    void Deactivate();
    bool IsActive() const { return Summoner.IsValid(); }
    ASkeletonSummonSpell* GetSummoner() const { return Summoner.Get(); }
    int32 AttackCount = 0;
private:
    UPROPERTY()
    TObjectPtr<UNecromancyVisualComponent> Art;
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> AllyMarker;
    UPROPERTY() TObjectPtr<UMortisTerrain> Terrain;
    TWeakObjectPtr<ASkeletonSummonSpell> Summoner;
    TWeakObjectPtr<AMVPEnemy> Target;
    uint32 TargetGeneration = 0;
    int32 Slot = 0;
    float SearchRemaining = 0.f;
    float AttackRemaining = 0.f;
    float AttackPoseRemaining = 0.f;
};
