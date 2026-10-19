#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForbiddenDistrictDirector.generated.h"

class ANeonChallengeEffect;
class AMVPPlayer;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EForbiddenObjective : uint8 { Relay, Cache, Shop, Ward, Subway };

/** Bounded world objectives. Markers are gameplay information, never culled by the VFX slider. */
UCLASS()
class DECKBUILDER_API AForbiddenDistrictDirector : public AActor
{
    GENERATED_BODY()
public:
    AForbiddenDistrictDirector();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    bool Interact();
    FString GetPrompt() const;
    FString GetTracker() const;
    UPROPERTY(BlueprintReadOnly) int32 HackedRelays=0;
    UPROPERTY(BlueprintReadOnly) int32 FoundCaches=0;
    UPROPERTY(BlueprintReadOnly) bool WardComplete=false;
    UPROPERTY(BlueprintReadOnly) float HackProgress=0;
    UPROPERTY(BlueprintReadOnly) float WardProgress=0;
    UPROPERTY(BlueprintReadOnly) int32 BroadcastPhase=0;
    bool IsSubwayLive() const;
    void OfferCapture(int32 Archetype,FVector Position);
    int32 GetNearbyIndex() const;
private:
    struct FObjective
    {
        EForbiddenObjective Type;
        FVector Position;
        FString Name;
        bool Complete=false;
    };
    TArray<FObjective> Objectives;
    UPROPERTY() TArray<TObjectPtr<ANeonChallengeEffect>> Markers;
    UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> Labels;
    TWeakObjectPtr<AMVPPlayer> Player;
    int32 Hacking=INDEX_NONE;
    float BossRetry=0, HazardDamageCooldown=0, WardAbsence=0, WardReinforcements=0;
    int32 CaptureArchetype=INDEX_NONE;
    FVector CapturePosition=FVector::ZeroVector;
    float CaptureRemaining=0;
    UPROPERTY() TObjectPtr<ANeonChallengeEffect> CaptureMarker;
};
