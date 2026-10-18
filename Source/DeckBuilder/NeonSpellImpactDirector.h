#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonSpellStyle.h"
#include "NeonSpellImpactDirector.generated.h"
class UNiagaraComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class AMVPEnemy;

/** Shared original explosion physics, reserved per-spell bursts, and a guaranteed lightweight hit cue. */
UCLASS()
class DECKBUILDER_API ANeonSpellImpactDirector : public AActor
{
    GENERATED_BODY()
public:
    ANeonSpellImpactDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    bool PlayImpact(ENeonSpellStyle Style,FVector Location);
    void ConfirmDamage(ENeonSpellStyle Style,AMVPEnemy* Enemy,float ActualDamage);
    void PlayFlightTrail(ENeonSpellStyle Style,FVector Position);
    bool IsReady() const;
    int32 Accepted=0,Dropped=0;
    int32 ConfirmedHits[16]={},FlightBursts[16]={},ImpactBursts[16]={};
    float ConfirmedDamage[16]={};
    int32 CoveredHits=0,VisiblePulses=0;
    float ReadabilityScale=1.f;
    UFUNCTION(BlueprintCallable,Category="VFX Settings") void SetImpactIntensity(float Value);
    UFUNCTION(BlueprintPure,Category="VFX Settings") float GetImpactIntensity() const {return ImpactIntensity;}
    bool SaveVFXSettings();
    void LoadVFXSettings();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    float ImpactIntensity=1.f;
    bool bSettingsDirty=false;
    double SaveAfter=0;
    FString SettingsSlot() const;
    static constexpr int32 Reserved=4;
    static constexpr int32 Shared=32;
    static constexpr int32 RingCapacity=256;
    struct FHitPulse {float Age=-1;FVector Position=FVector::ZeroVector;FLinearColor Color=FLinearColor::White;};
    struct FHitFlash {TWeakObjectPtr<AMVPEnemy> Enemy;uint32 Generation=0;float Age=0;FLinearColor Color=FLinearColor::White;};
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Effects;
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Flights;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> HitRings;
    TArray<float> Ages;
    TArray<float> FlightAges;
    TArray<FHitPulse> Pulses;
    TArray<FHitFlash> Flashes;
    TArray<int32> SlotStyles;
    TArray<FVector> SlotPositions;
    float LightAges[4]={};
    int32 PulseCursor=0,LightCursor=0;
};
