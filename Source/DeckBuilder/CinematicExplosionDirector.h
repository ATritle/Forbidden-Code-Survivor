#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CinematicExplosionDirector.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UCurveFloat;

USTRUCT()
struct FFireFlightLight
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
    TWeakObjectPtr<AActor> Projectile;
};

USTRUCT()
struct FExplosionSlot
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> FX;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Sparks;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Aura;
    UPROPERTY(Transient) TObjectPtr<UPointLightComponent> Light;
    float Age = -1.f;
    float EnergyScale = 1.f;
    float Lifetime = 4.f;
    bool bShadowed = false;
    bool bFluid = false;
};

/** Bounded visual-only director. Damage must always resolve independently of admission/culling. */
UCLASS(Blueprintable)
class DECKBUILDER_API ACinematicExplosionDirector : public AActor
{
    GENERATED_BODY()
public:
    ACinematicExplosionDirector(const FObjectInitializer& ObjectInitializer);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable, Category="Explosion")
    bool TriggerExplosion(FVector WorldPosition, float Radius = 450.f, bool bCompact = false);
    void TrackFireball(AActor* Projectile, bool bTrack=true);
    UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="1",ClampMax="32")) int32 PoolSize = 8;
    UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="0",ClampMax="4")) int32 MaxShadowLights = 2;
    UPROPERTY(EditAnywhere, Category="Explosion") TSoftObjectPtr<UNiagaraSystem> ExplosionSystem;
    UPROPERTY(EditAnywhere, Category="Explosion") TSoftObjectPtr<UNiagaraSystem> SparkSystem;
    UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="0",ClampMax="8")) int32 MaxFluidGrids = 2;
    UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="32",ClampMax="128")) int32 GridResolution = 64;
    UPROPERTY(EditAnywhere, Category="Explosion", meta=(ClampMin="0",ClampMax="8000")) int32 SparksPerExplosion = 2000;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Metrics") int32 FluidAccepted = 0;
    bool HasAuthoredEffects() const;
    /** Optional normalized 0..1 curve over 0..0.2 seconds. Default is a quadratic decay. */
    UPROPERTY(EditAnywhere, Category="Explosion") TObjectPtr<UCurveFloat> FlashCurve;
    UPROPERTY(EditAnywhere, Category="Explosion") float PeakLumens = 4000.f;
    UPROPERTY(EditAnywhere, Category="Explosion") float CullDistance = 6000.f;
    UPROPERTY(EditAnywhere, Category="Explosion") float MaxEffectLifetime = 4.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Metrics") int32 Accepted = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Metrics") int32 Dropped = 0;
private:
    UPROPERTY(Transient) TArray<FExplosionSlot> Slots;
    UPROPERTY(Transient) TArray<FFireFlightLight> FlightLights;
};
