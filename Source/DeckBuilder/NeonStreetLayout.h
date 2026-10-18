#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeonStreetLayout.generated.h"
class UInstancedStaticMeshComponent;

/** A connected street grid layered over the original navigation and structural map. */
UCLASS()
class DECKBUILDER_API ANeonStreetLayout : public AActor
{
    GENERATED_BODY()
public:
    ANeonStreetLayout();
    virtual void BeginPlay() override;
    int32 GetSidewalkCount() const;
    int32 GetCurbCount() const;
private:
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Asphalt;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Sidewalks;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Curbs;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> WhitePaint;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> AmberPaint;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ironwork;
};
