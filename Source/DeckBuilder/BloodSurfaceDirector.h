#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BloodSurfaceDirector.generated.h"
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UTexture2D;
class UNiagaraComponent;
class UPrimitiveComponent;
USTRUCT()
struct FBloodSurface
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UPrimitiveComponent> Surface;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Mask;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    FVector2D Origin=FVector2D::ZeroVector;
    FVector2D Size=FVector2D(19000,19000);
};
/** Per-surface render-target paint prevents splatter leaking between elevations. */
UCLASS()
class DECKBUILDER_API ABloodSurfaceDirector : public AActor
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    ABloodSurfaceDirector();
    UFUNCTION(BlueprintCallable) void Splatter(FVector Impact);
    UPROPERTY(BlueprintReadOnly) int32 PaintedImpacts=0;
private:
    UPROPERTY() TArray<FBloodSurface> Surfaces;
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Bursts;
    UPROPERTY() TObjectPtr<UTexture2D> MaskTexture;
    UPROPERTY() TObjectPtr<UTexture2D> PoolTexture;
    TArray<FVector> Pending;
    TArray<float> Ages;
    void Paint(FVector Impact);
};
