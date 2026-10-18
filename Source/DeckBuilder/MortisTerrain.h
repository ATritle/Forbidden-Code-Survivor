#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MortisTerrain.generated.h"

/** Bounded, staggered path queries; movement still sweeps against physical geometry. */
UCLASS(ClassGroup=Movement)
class DECKBUILDER_API UMortisTerrain : public UActorComponent
{
    GENERATED_BODY()
public:
    void MoveTowards(FVector Destination, float Speed, float Dt);
    void ResetPath();
    static bool Ground(AActor* Actor, float Clearance=55.f);
    static bool ClearSight(const AActor* Actor, FVector End);
private:
    TArray<FVector> Points;
    int32 PointIndex=0;
    float Repath=0.f;
};
