#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExplosionBenchmark.generated.h"
class AMVPEnemy;
/** Opt-in, deterministic GPU benchmark. No player save writes and no gameplay damage. */
UCLASS()
class AExplosionBenchmark : public AActor
{
    GENERATED_BODY()
public:
    AExplosionBenchmark();
    virtual void Tick(float Dt) override;
private:
    bool bStarted=false, bFinished=false;
    int32 CapturePhase=0;
    double Start=0, Previous=0, NextBlast=0;
    float Rate=2, Warmup=10, Duration=20;
    int32 Burst=1, Serial=0;
    FString OutputName=TEXT("ExplosionBenchmark");
    FString Rows=TEXT("elapsed_s,frame_ms,game_ms,render_ms,gpu_ms,active_enemies,accepted,dropped,fluid_accepted\n");
    TArray<TWeakObjectPtr<AMVPEnemy>> Enemies;
};
