#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonStreetLighting.generated.h"

class UInstancedStaticMeshComponent;
class USpotLightComponent;

/** Street fixtures and bounded, local lighting for the lower city routes. */
UCLASS()
class DECKBUILDER_API ANeonStreetLighting : public AActor
{
    GENERATED_BODY()
public:
    ANeonStreetLighting();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Street Lighting", meta=(ClampMin="0", ClampMax="2"))
    float Brightness = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Street Lighting", meta=(ClampMin="0", ClampMax="16"))
    int32 MaxActiveLights = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City|Street Lighting", meta=(ClampMin="0", ClampMax="3"))
    int32 MaxShadowLights = 2;

    int32 GetFixtureCount() const { return Lamps.Num(); }
    int32 GetActiveLightCount() const;
    int32 GetShadowLightCount() const;

private:
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Metal;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Trim;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lenses;
    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Lamps;

    void AddLamp(const FVector& Foot, float StreetYaw, bool bUnderpass = false);
};
