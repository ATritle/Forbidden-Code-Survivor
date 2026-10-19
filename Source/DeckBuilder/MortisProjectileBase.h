#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpellBase.h"
#include "NeonSpellStyle.h"
#include "MortisProjectileBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class AMVPEnemy;

UENUM(BlueprintType)
enum class EProjectileType : uint8 { PhysicalMesh, ConcentratedElemental, ContinuousBeam };

/** Pooled projectile: each launch snapshots mutations and resets hit-generation tracking. */
UCLASS(Blueprintable)
class DECKBUILDER_API AMortisProjectileBase : public AActor
{
    GENERATED_BODY()
public:
    AMortisProjectileBase();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UFUNCTION(BlueprintCallable) void SetupProjectileVisuals(EProjectileType Type,UStaticMesh* MeshAsset,UNiagaraSystem* ParticleAsset);
    UFUNCTION(BlueprintCallable) void SetSpellStyle(ENeonSpellStyle Style) { SpellStyle=Style; }
    UPROPERTY(BlueprintReadOnly) ENeonSpellStyle SpellStyle=ENeonSpellStyle::Prism;
    UFUNCTION(BlueprintCallable) void Launch(FVector Origin,FVector Direction,float Damage,const FSpellModifiers& Snapshot,FLinearColor Color);
    UFUNCTION(BlueprintCallable) void Deactivate();
    UFUNCTION(BlueprintCallable) void FireContinuousBeam(FVector Origin,FVector Target,float Damage,const FSpellModifiers& Snapshot);
    UFUNCTION(BlueprintImplementableEvent) void PlayCinematicImpactVFX(FVector Location,FVector Direction);
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UNiagaraComponent> Trail;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
    UPROPERTY(BlueprintReadOnly) bool bActive=false;
    UPROPERTY(BlueprintReadOnly) int32 PiercingCount=0;
    UPROPERTY(BlueprintReadOnly) EProjectileType ProjectileType=EProjectileType::PhysicalMesh;
    UPROPERTY(BlueprintReadWrite) bool bBloodSpell=false;
private:
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Branches;
    UPROPERTY() TObjectPtr<UNiagaraSystem> FireTailAsset;
    UPROPERTY() TObjectPtr<UNiagaraSystem> CardTrailAsset;
    UPROPERTY() TObjectPtr<UMaterialInterface> ChargedCardMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> CoreMaterials;
    UPROPERTY() TArray<TObjectPtr<UNiagaraSystem>> StyleTrails;
    FVector BaseVisualScale=FVector::OneVector;
    FVector LaunchOrigin=FVector::ZeroVector;
    FVector BeamEnd=FVector::ZeroVector;
    float BranchUpdate=0.f;
    void UpdateBranches();
    void FireImpact(FVector Position);
    UFUNCTION() void Stopped(const FHitResult& Hit);
    void OnProjectileImpact(const FHitResult& Hit);
    void TraceDamage(FVector Start,FVector End);
    float RemainingLife=0.f;
    float FlightAge=0.f;
    float SparkTrailAge=0.f;
    float SpinPhase=0.f;
    float ShotDamage=0.f;
    FSpellModifiers ShotModifiers;
    bool Returning=false;
    TMap<TWeakObjectPtr<AMVPEnemy>,uint32> HitLives;
};
