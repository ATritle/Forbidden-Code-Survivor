#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MortisProjectileBase.h"
#include "MortisTestingDirector.generated.h"

class AMVPPlayer;
class AMVPEnemy;
class AMVPController;
class AEnemySpawner;
class ANeonArsenalDirector;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UCardDataAsset;
class UActorComponent;
class AMVPProjectile;

struct FTrainingTarget
{
    TWeakObjectPtr<AMVPEnemy> Enemy;
    FVector Position=FVector::ZeroVector;
    int32 Archetype=0;
    float ResetAfter=0;
    bool bAlive=false;
};
struct FTrainingDamageNumber
{
    FVector Position=FVector::ZeroVector;
    TWeakObjectPtr<AMVPEnemy> Enemy;
    float Amount=0,Age=-1;
};

/** Isolated, bounded practice range. The original pawn, deck, waves and run clock are preserved. */
UCLASS()
class DECKBUILDER_API AMortisTestingDirector : public AActor
{
    GENERATED_BODY()
public:
    AMortisTestingDirector();
    virtual void Tick(float Dt) override;
    bool Configure(FName CardID,const TArray<int32>& Counts,bool bAutoReset,FString& Error,float Spacing=220.f);
    bool ConfigureCards(const TArray<FName>& Cards,const TArray<int32>& Counts,bool bAutoReset,FString& Error,float Spacing=220.f);
    bool Enter(AMVPController* Controller,bool bReturnPaused,FString& Error);
    void Exit(AMVPController* Controller);
    bool ApplySetup(FString& Error);
    void RecordDamage(AMVPEnemy* Enemy,float Amount,bool bDefeated);
    void EmitProjectile(FVector Origin,FVector Direction,float Damage,const FSpellModifiers& Modifiers,FLinearColor Color,EProjectileType Type,ENeonSpellStyle Style);
    bool IsActive() const {return bActive;}
    int32 GetTargetCount() const {return Targets.Num();}
    const TArray<FTrainingTarget>& GetTargets() const {return Targets;}
    const TArray<FTrainingDamageNumber>& GetDamageNumbers() const {return Numbers;}
    FName CardID=TEXT("PulseDart");
    TArray<FName> SelectedCards={FName(TEXT("PulseDart"))};
    TArray<int32> EnemyCounts;
    bool bAutoReset=true;
    float GridSpacing=220.f;
    int32 HitCount=0,Defeats=0;
    float TotalDamage=0,Elapsed=0;
    FString CardCaption;
private:
    struct FFrozenActor
    {
        TWeakObjectPtr<AActor> Actor;
        bool bTick=false,bHidden=false,bCollision=false;
        TArray<TWeakObjectPtr<UActorComponent>> EnabledTicks;
    };
    void Freeze(AActor* Actor);
    bool PrepareRange(FString& Error);
    void ActivateTarget(FTrainingTarget& Target);
    void ClearTestShots();
    UCardDataAsset* FindCard(FName ID) const;
    bool bActive=false,bReturnPaused=false,bSavedMenu=false,bSavedDraft=false,bSavedEnded=false;
    UPROPERTY() TObjectPtr<AMVPPlayer> OriginalPawn;
    UPROPERTY() TObjectPtr<AMVPPlayer> PracticePawn;
    UPROPERTY() TObjectPtr<AEnemySpawner> OriginalSpawner;
    UPROPERTY() TObjectPtr<AEnemySpawner> PracticeSpawner;
    UPROPERTY() TObjectPtr<ANeonArsenalDirector> OriginalArsenal;
    UPROPERTY() TObjectPtr<ANeonArsenalDirector> PracticeArsenal;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Floor;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Grid;
    UPROPERTY() TArray<TObjectPtr<AMVPProjectile>> PracticeProjectiles;
    UPROPERTY() TObjectPtr<UStaticMesh> ProjectileMesh;
    UPROPERTY() TObjectPtr<UNiagaraSystem> ProjectileFX;
    UPROPERTY() TObjectPtr<UNiagaraSystem> BeamFX;
    TArray<FFrozenActor> Frozen;
    TArray<TWeakObjectPtr<AActor>> OriginalShots;
    TArray<FTrainingTarget> Targets;
    TArray<FTrainingDamageNumber> Numbers;
    int32 NumberCursor=0;
};
