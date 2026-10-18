#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonCityDirector.generated.h"

class UMaterialParameterCollection;
class UInstancedStaticMeshComponent;

/** Saved city marker and bounded weather/camera-cutaway controller. */
UCLASS()
class DECKBUILDER_API ANeonCityDirector : public AActor
{
    GENERATED_BODY()
public:
    ANeonCityDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    UPROPERTY(EditAnywhere,Category="City|Weather",meta=(ClampMin="0",ClampMax="2400")) int32 RainDrops=1000;
    UPROPERTY(EditAnywhere,Category="City|Camera") bool bEnableBuildingCutaway=true;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="City") TObjectPtr<UInstancedStaticMeshComponent> Rain;
    UPROPERTY(EditAnywhere,Category="City|Navigation") TArray<FVector> WalkableDestinations;
private:
    UPROPERTY() TObjectPtr<UMaterialParameterCollection> CityParameters;
};
