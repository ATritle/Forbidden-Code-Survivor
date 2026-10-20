#include "CinematicExplosionDirector.h"
#include "ForbiddenSettingsControls.h"
#include "MVPGame.h"
#include "NeonSpellImpactDirector.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Components/PointLightComponent.h"
#include "Curves/CurveFloat.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"

ACinematicExplosionDirector::ACinematicExplosionDirector(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    ExplosionSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Cinematic/NS_CinematicExplosion.NS_CinematicExplosion")));
    SparkSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Cinematic/NS_ExplosionSparks.NS_ExplosionSparks")));
}
void ACinematicExplosionDirector::BeginPlay()
{
    Super::BeginPlay();
    FParse::Value(FCommandLine::Get(),TEXT("FluidGrids="),MaxFluidGrids);
    FParse::Value(FCommandLine::Get(),TEXT("FluidResolution="),GridResolution);
    MaxFluidGrids=FMath::Clamp(MaxFluidGrids,0,8); GridResolution=FMath::Clamp(GridResolution,32,128);
    // Resolve once during pre-run prewarm, never on an impact callback.
    UNiagaraSystem* Asset=nullptr;
    if (FPackageName::DoesPackageExist(ExplosionSystem.ToSoftObjectPath().GetLongPackageName())) Asset=ExplosionSystem.LoadSynchronous();
    UNiagaraSystem* SparkAsset=SparkSystem.LoadSynchronous();
    auto* AuraAsset=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_EnergyAura.NS_EnergyAura"));
    if (!Asset || !SparkAsset) UE_LOG(LogTemp,Error,TEXT("Cinematic Niagara assets missing. Run ExplosionAuthoring commandlet."));
    FlightLights.SetNum(4);
    for(auto& S:FlightLights)
    {
        S.Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(S.Light);
        S.Light->SetupAttachment(RootComponent);S.Light->SetMobility(EComponentMobility::Movable);
        S.Light->SetIntensityUnits(ELightUnits::Lumens);S.Light->SetLightColor(FLinearColor(1.f,0.02f,0.5f));
        S.Light->ComponentTags.Add(TEXT("FireFlight"));S.Light->SetAttenuationRadius(180.f);S.Light->SetSourceRadius(10.f);
        S.Light->SetCastShadows(false);S.Light->SetVisibility(false);S.Light->RegisterComponent();
    }
    Slots.SetNum(FMath::Clamp(PoolSize,1,32));
    for (auto& S : Slots)
    {
        S.FX=NewObject<UNiagaraComponent>(this);
        AddInstanceComponent(S.FX);
        S.FX->SetupAttachment(RootComponent);
        S.FX->SetAutoActivate(false); S.FX->SetAutoDestroy(false);
        S.FX->SetAsset(Asset); S.FX->RegisterComponent();
        S.Sparks=NewObject<UNiagaraComponent>(this); AddInstanceComponent(S.Sparks);
        S.Sparks->SetupAttachment(RootComponent); S.Sparks->SetAutoActivate(false); S.Sparks->SetAutoDestroy(false);
        S.Sparks->SetAsset(SparkAsset); S.Sparks->RegisterComponent();
        S.Aura=NewObject<UNiagaraComponent>(this);AddInstanceComponent(S.Aura);
        S.Aura->SetupAttachment(RootComponent);S.Aura->SetAutoActivate(false);S.Aura->SetAutoDestroy(false);
        S.Aura->SetAsset(AuraAsset);S.Aura->RegisterComponent();
        S.Light=NewObject<UPointLightComponent>(this);
        AddInstanceComponent(S.Light); S.Light->SetupAttachment(RootComponent);
        S.Light->SetMobility(EComponentMobility::Movable);
        S.Light->SetIntensityUnits(ELightUnits::Lumens);
        S.Light->SetLightColor(FLinearColor(1.f,0.025f,0.55f));
        S.Light->SetIntensity(0); S.Light->SetVisibility(false); S.Light->RegisterComponent();
    }
}
bool ACinematicExplosionDirector::TriggerExplosion(FVector Position,float Radius,bool bCompact)
{
    if (Position.ContainsNaN() || !FMath::IsFinite(Radius) || Radius<=0) return false;
    FVector Camera; FRotator Rotation;
    if (auto* PC=UGameplayStatics::GetPlayerController(this,0))
    {
        PC->GetPlayerViewPoint(Camera,Rotation);
        if (FVector::DistSquared(Position,Camera)>FMath::Square(FMath::Max(0.f,CullDistance))) { ++Dropped; return false; }
    }
    int32 Shadows=0, Fluids=0;
    for (const auto& S : Slots) { if (S.Age>=0 && S.Age<0.2f && S.bShadowed) ++Shadows; if(S.Age>=0 && S.bFluid) ++Fluids; }
    for (auto& S : Slots)
        if (S.Age<0)
        {
            S.Age=0; S.EnergyScale=bCompact ? 0.22f : 1.f; S.Lifetime=bCompact ? 0.85f : MaxEffectLifetime; S.bShadowed=Shadows<FMath::Clamp(MaxShadowLights,0,4);
            S.Light->SetWorldLocation(Position+FVector(0,0,80));
            S.Light->SetAttenuationRadius(FMath::Clamp(Radius*3,250.f,1800.f));
            S.Light->SetCastShadows(S.bShadowed);
            S.Light->SetIntensity(0); S.Light->SetVisibility(true);
            S.bFluid=!bCompact && Fluids<MaxFluidGrids && S.FX->GetAsset();
            if (S.bFluid)
            {
                S.FX->SetWorldLocation(Position);
                S.FX->SetVariableInt(TEXT("User.ResolutionMaxAxis"),GridResolution);
                S.FX->SetVariableBool(TEXT("User.DrawBounds"),false);
                S.FX->SetVariableVec3(TEXT("User.WorldSpaceSize"),FVector(FMath::Clamp(Radius*2.f,300.f,1000.f)));
                S.FX->SetVariableVec3(TEXT("User.SourceOffset"),FVector(0,0,60));
                S.FX->Activate(true);
                ++FluidAccepted;
            }
            const auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
            const float Intensity=Game && Game->SpellImpactDirector ? Game->SpellImpactDirector->GetImpactIntensity() : 1.f;
            if(S.Sparks->GetAsset() && Intensity>0) { S.Sparks->SetWorldLocation(Position+FVector(0,0,35)); S.Sparks->SetVariableInt(TEXT("User.SparkCount"),FMath::RoundToInt((bCompact ? 120 : FMath::Clamp(SparksPerExplosion,0,8000))*Intensity)); S.Sparks->Activate(true); }
            if(S.Aura->GetAsset())
            {
                S.Aura->SetWorldLocation(Position);S.Aura->SetWorldScale3D(FVector(FMath::Clamp(Radius/100.f,0.5f,4.f)));
                S.Aura->Activate(true);
            }
            // Bound summed peak energy too: overlapping flashes must not erase combat silhouettes.
            int32 LiveFlashes=0;
            for (const auto& Other : Slots) if (Other.Age>=0 && Other.Age<0.2f) ++LiveFlashes;
            for (auto& Other : Slots)
                if (Other.Age>=0 && Other.Age<0.2f)
                    Other.Light->SetIntensity(ForbiddenSettings::Get(this,"Flash",1)*(ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f ? 0.15f : 1.f)*FMath::Max(0.f,PeakLumens)*Other.EnergyScale/FMath::Max(1,LiveFlashes));
            ++Accepted; return true;
        }
    ++Dropped; return false;
}
void ACinematicExplosionDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    for(auto& S:FlightLights)
    {
        AActor* Projectile=S.Projectile.Get();
        if(!Projectile || Projectile->IsHidden())
        { S.Projectile.Reset();S.Light->SetVisibility(false);continue; }
        S.Light->SetWorldLocation(Projectile->GetActorLocation()+FVector(0,0,18));
        S.Light->SetIntensity(140.f+30.f*FMath::Sin(GetWorld()->GetTimeSeconds()*23.f));
        S.Light->SetVisibility(true);
    }
    int32 LiveFlashes=0;
    for (const auto& S : Slots) if (S.Age>=0 && S.Age<0.2f) ++LiveFlashes;
    for (auto& S : Slots)
    {
        if (S.Age<0) continue;
        S.Age+=Dt;
        if(S.Age>=0.5f && S.Age-Dt<0.5f && FParse::Param(FCommandLine::Get(),TEXT("VFXDebug")) && HasAuthoredEffects())
        {
            UE_LOG(LogTemp,Display,TEXT("VFX_STATE fluid=%d fxActive=%d fxState=%d sparkActive=%d sparkState=%d valid=%d/%d ready=%d/%d"),S.bFluid,S.FX->IsActive(),int32(S.FX->GetExecutionState()),S.Sparks->IsActive(),int32(S.Sparks->GetExecutionState()),S.FX->GetAsset()->IsValid(),S.Sparks->GetAsset()->IsValid(),S.FX->GetAsset()->IsReadyToRun(),S.Sparks->GetAsset()->IsReadyToRun());
            if(auto Controller=S.Sparks->GetSystemInstanceController())
            {
                Controller->WaitForConcurrentTickAndFinalize();
                if(auto* Instance=Controller->GetSystemInstance_Unsafe())
                    for(const auto& Emitter:Instance->GetEmitters()) UE_LOG(LogTemp,Display,TEXT("VFX_SPARK_PARTICLES %d"),Emitter->GetNumParticles());
            }
        }
        const float Alpha=FlashCurve ? FMath::Clamp(FlashCurve->GetFloatValue(S.Age),0.f,1.f) : FMath::Square(1.f-FMath::Clamp(S.Age/0.2f,0.f,1.f));
        const float Gain=ForbiddenSettings::Get(this,"Flash",1)*(ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f ? 0.15f : 1.f);
        S.Light->SetIntensity(S.Age<0.2f ? Gain*FMath::Max(0.f,PeakLumens)*S.EnergyScale*Alpha/FMath::Max(1,LiveFlashes) : 0.f);
        if (S.Age>=0.2f) { S.Light->SetVisibility(false); S.Light->SetCastShadows(false); }
        if (S.Age>=FMath::Max(0.2f,S.Lifetime))
        { S.FX->DeactivateImmediate(); S.Sparks->DeactivateImmediate(); S.Aura->DeactivateImmediate();S.Age=-1; S.bShadowed=false; S.bFluid=false; }
    }
}
void ACinematicExplosionDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    for (auto& S : Slots) { if (S.FX) S.FX->DeactivateImmediate(); if(S.Sparks) S.Sparks->DeactivateImmediate();if(S.Aura) S.Aura->DeactivateImmediate(); if (S.Light) S.Light->SetVisibility(false); }
    for(auto& S:FlightLights) {S.Projectile.Reset();S.Light->SetVisibility(false);}
    Super::EndPlay(Reason);
}
bool ACinematicExplosionDirector::HasAuthoredEffects() const { return !Slots.IsEmpty() && Slots[0].FX->GetAsset() && Slots[0].Sparks->GetAsset(); }

void ACinematicExplosionDirector::TrackFireball(AActor* Projectile,bool bTrack)
{
    if(!IsValid(Projectile)) return;
    for(auto& S:FlightLights) if(S.Projectile==Projectile) {if(!bTrack) {S.Projectile.Reset();S.Light->SetVisibility(false);} return;}
    if(!bTrack) return;
    for(auto& S:FlightLights)
        if(!S.Projectile.IsValid() || S.Projectile->IsHidden()) {S.Projectile=Projectile;return;}
}
