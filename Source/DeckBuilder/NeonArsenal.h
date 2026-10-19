#pragma once
#include "CoreMinimal.h"
#include "SpellBase.h"
#include "CardDataAsset.h"
#include "NeonArsenal.generated.h"

class AMVPEnemy;
class UInstancedStaticMeshComponent;
class UNiagaraComponent;
class UPointLightComponent;

/** Immutable spell tuning shared by the deck, casting, UI and validation. Units are centimetres. */
struct FNeonArsenalProfile
{
    FName ID;
    const TCHAR* Name;
    const TCHAR* Description;
    EMagicSchool School;
    float Damage, Cooldown, Range, Radius;
    int32 Count;
    FLinearColor Color;
};
namespace NeonArsenal
{
    DECKBUILDER_API const TArray<FNeonArsenalProfile>& Profiles();
    DECKBUILDER_API int32 Find(FName ID);
    DECKBUILDER_API FString MutationDescription(FName ID,const FGameplayTag& Tag);
}

/** One reusable weapon implementation, ten authored attack patterns. Card ID selects the profile. */
UCLASS()
class DECKBUILDER_API ANeonArsenalSpell : public ASpellBase
{
    GENERATED_BODY()
public:
    virtual void InitializeSpell(UCardDataAsset* Card,AEnemySpawner* Spawner) override;
    virtual TArray<FGameplayTag> GetAvailableMutationTags() const override;
protected:
    virtual APooledEnemy* ResolveTarget() const override;
    virtual void FireSpell_Implementation(APooledEnemy* Target,const FSpellModifiers& Snapshot) override;
private:
    int32 ProfileIndex=INDEX_NONE;
};

/** Per spell: 32 gameplay slots, 32 visual streak slots, four GPU bursts. No spawned shot actors. */
UCLASS()
class DECKBUILDER_API ANeonArsenalDirector : public AActor
{
    GENERATED_BODY()
public:
    ANeonArsenalDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    void FirePattern(int32 Profile,AActor* Caster,AMVPEnemy* Target,float Damage,const FSpellModifiers& Snapshot);
    bool IsReady() const;
    int32 Casts[10]={};
    int32 Hits[10]={};
    int32 ChainLinks=0,AreaHits=0,DroppedVisuals=0,DroppedGameplay=0,WallBounces=0;
    int32 ActiveCount() const;
private:
    static constexpr int32 Capacity=64;
    struct FShot
    {
        bool Active=false,Fragment=false;
        int32 Profile=0,Mode=0,Remaining=0,Pulses=0;
        float Age=0,Life=1.5f,Power=0,Radius=0,TrailAge=0;
        FVector Position=FVector::ZeroVector,Start=FVector::ZeroVector,End=FVector::ZeroVector,Direction=FVector::ForwardVector;
        TWeakObjectPtr<AActor> Owner;
        TMap<TWeakObjectPtr<AMVPEnemy>,uint32> HitLives;
        FSpellModifiers Modifiers;
    };
    TArray<FShot> Shots;
    FShot* Rent(int32 Profile,int32 Mode);
    void Streak(int32 Profile,FVector A,FVector B,float Lifetime=0.18f);
    void Burst(int32 Profile,FVector Location);
    void Area(int32 Profile,FVector Location,float Radius,float Damage);
    void DamageEnemy(int32 Profile,AMVPEnemy* Enemy,float Damage,FVector Direction,FName Source=NAME_None);
    AMVPEnemy* Nearest(FVector Origin,float Range,const TMap<TWeakObjectPtr<AMVPEnemy>,uint32>& Excluded) const;
    void Chain(int32 Profile,FVector Origin,AMVPEnemy* First,float Damage,int32 Count,float Jump,float Falloff,const FSpellModifiers& Snapshot);
    bool Clear(FVector A,FVector B) const;
    void ProjectileStep(FShot& Shot,float Dt);
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Bodies;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Streaks;
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Sparks;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Flashes;
    float SparkBusy[40]={};
    float FlashLife[2]={};
};
