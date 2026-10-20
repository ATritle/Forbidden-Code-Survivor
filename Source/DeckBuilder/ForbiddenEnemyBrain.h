#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForbiddenEnemyBrain.generated.h"
class AMVPEnemy;
class AMVPPlayer;
class UStaticMeshComponent;
class UAnimSequence;

/** Pooled combat state, independent of visual simulation and animation tick budgets. */
UCLASS()
class DECKBUILDER_API UForbiddenEnemyBrain : public UActorComponent
{
    GENERATED_BODY()
public:
    UForbiddenEnemyBrain();
    virtual void BeginPlay() override;
    bool Advance(float Dt,AMVPPlayer* Player,FVector& Goal,float& Speed);
    void ResetLife();
    void ApplySlow(float Amount,float Seconds);
    float ModifyIncoming(float Damage,FVector Velocity,FName ProgramID);
    void AddMark(float Damage);
    void CancelChannel();
    float ShieldRemaining=0;
    float SlowRemaining=0;
    float SlowMultiplier=1;
    float ExposedRemaining=0;
    UPROPERTY(BlueprintReadOnly) int32 State=0;
    UPROPERTY(BlueprintReadOnly) float Windup=0;
    UPROPERTY(BlueprintReadOnly) FVector AttackPoint=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) int32 BossPhase=0;
    void UpdateBroadcastLane(float Progress);
private:
    TWeakObjectPtr<AMVPEnemy> Enemy;
    uint32 Life=0;
    int32 Archetype=INDEX_NONE;
    float Cooldown=0,StateTime=0,LastHit=-10,ZoneTick=0;
    int32 Marks=0;
    int32 PulseStacks=0;
    float LastPulseTime=-1000;
    float MarkPower=0;
    FVector DashDirection=FVector::ZeroVector;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Telegraph;
    UPROPERTY() TObjectPtr<UAnimSequence> AttackClip;
    TArray<TPair<TWeakObjectPtr<AMVPEnemy>,uint32>> Nodes;
    TWeakObjectPtr<AMVPEnemy> ShieldSource;
    uint32 ShieldLife=0;
    void GrantShield();
    void BeginAttack(AMVPPlayer* Player,float Delay,float Radius);
    void Strike(AMVPPlayer* Player,float Damage,float Radius);
    void SpawnBossNodes();
};
