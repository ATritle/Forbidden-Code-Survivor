#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForbiddenSpirit.generated.h"
class UNecromancyVisualComponent;
class UMortisTerrain;
class AMVPPlayer;

/** The approved Meshy digital ancestor, used for street life, decoys and stolen-code support. */
UCLASS()
class DECKBUILDER_API AForbiddenSpirit : public AActor
{
    GENERATED_BODY()
public:
    AForbiddenSpirit();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    void ActivateSupport(AMVPPlayer* Player,float Seconds);
    void ActivateDecoy(FVector Position,float Seconds);
    void SetPedestrianRoute(FVector A,FVector B);
    bool IsDecoyActive() const {return Decoy && Remaining>0;}
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNecromancyVisualComponent> Art;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UMortisTerrain> Terrain;
private:
    TWeakObjectPtr<AMVPPlayer> Rider;
    FVector RouteA=FVector::ZeroVector,RouteB=FVector::ZeroVector;
    float Remaining=0,AttackCooldown=0;
    bool Pedestrian=false,Decoy=false,RouteForward=true;
};
