#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KineticCardVFXDirector.generated.h"

class UNiagaraComponent;
class UPointLightComponent;

/** Independent gold impact pool; never recolors the pink energy explosions. */
UCLASS()
class DECKBUILDER_API AKineticCardVFXDirector : public AActor
{
    GENERATED_BODY()
public:
    AKineticCardVFXDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    bool PlayImpact(FVector Location);
    bool IsReady() const;
    int32 Accepted=0,Dropped=0;
private:
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Impacts;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    TArray<float> Ages;
};
