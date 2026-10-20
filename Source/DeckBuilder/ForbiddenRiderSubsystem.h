#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SpellBase.h"
#include "ForbiddenRiderSubsystem.generated.h"
class AMVPPlayer;
class AMVPEnemy;
class AForbiddenSpirit;
class AMortisProjectileBase;
class UStaticMeshComponent;

/** Per-run rider mechanics. Companion actors are prewarmed once, never spawned by hits. */
UCLASS()
class DECKBUILDER_API UForbiddenRiderSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Tick(float Dt) override;
    virtual TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(UForbiddenRiderSubsystem,STATGROUP_Tickables);}
    virtual bool DoesSupportWorldType(EWorldType::Type T) const override {return T==EWorldType::Game || T==EWorldType::PIE;}
    void BeginRun(AMVPPlayer* Player);
    void EndRun();
    void OnBoost();
    void OnEssence(int32 Points);
    float AbsorbContact(float Damage);
    float ModifyDamage(AMVPEnemy* Enemy,float Damage,FName Program,bool Direct);
    void OnDamageConfirmed(AMVPEnemy* Enemy,FName Program,bool Direct,float Actual);
    FSpellModifiers ModifySnapshot(const FSpellModifiers& Snapshot,bool Chain=false) const;
    int32 ProjectileCast();
    bool DecoyGoal(const AMVPEnemy* Enemy,FVector& Goal) const;
    FName RiderID() const {return ID;}
    float GetShield() const {return Shield;}
    bool HasPossessedAlly() const {return PossessionRemaining>0;}
    bool IsLocked(const AMVPEnemy* Enemy) const;
    int32 ConfirmedSummonHits=0,Possessions=0,Decoys=0,Repulsors=0;
private:
    TWeakObjectPtr<AMVPPlayer> Rider;
    FName ID;
    UPROPERTY() TObjectPtr<AForbiddenSpirit> Decoy;
    UPROPERTY() TObjectPtr<AForbiddenSpirit> Familiar;
    UPROPERTY() TObjectPtr<AForbiddenSpirit> Possessed;
    UPROPERTY() TArray<TObjectPtr<AMortisProjectileBase>> Pulses;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShieldVisual;
    TArray<TWeakObjectPtr<AMVPEnemy>> PulseTargets;
    TArray<uint32> PulseLives;
    TWeakObjectPtr<AMVPEnemy> Marked,Candidate;
    uint32 MarkedLife=0,CandidateLife=0;
    float MarkRemaining=0,HitWindow=0,DecoyCooldown=0,DecoyRemaining=0,RepulsorCooldown=0;
    float Shield=0,FamiliarCooldown=0,PossessedCooldown=0,PossessionRemaining=0,PossessionCooldown=0;
    int32 Essence=0,Hits=0,Casts=0,SummonHits=0;
    bool Started=false;
    float MarkVisualAge=0;
    void TryPossession();
    void CompanionAttack(AForbiddenSpirit* Actor,float Damage,FName Program);
    static bool IsSummon(FName Program);
};
