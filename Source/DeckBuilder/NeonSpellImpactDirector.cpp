#include "NeonSpellImpactDirector.h"
#include "ForbiddenSettingsControls.h"
#include "ForbiddenCombatSubsystem.h"
#include "MVPGame.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CodexVFXSettingsSave.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ANeonSpellImpactDirector::ANeonSpellImpactDirector()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    HitRings=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ConfirmedHitPulses"));HitRings->SetupAttachment(RootComponent);
    HitRings->SetCollisionEnabled(ECollisionEnabled::NoCollision);HitRings->SetCastShadow(false);HitRings->NumCustomDataFloats=4;
}
void ANeonSpellImpactDirector::BeginPlay()
{
    Super::BeginPlay();
    LoadVFXSettings();
    auto* Asset=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Feedback/NS_PhysicalHitSparks.NS_PhysicalHitSparks"));
    auto Make=[&](const TCHAR* Name)
    {
        auto* FX=NewObject<UNiagaraComponent>(this,FName(Name));AddInstanceComponent(FX);FX->SetupAttachment(RootComponent);
        FX->SetAutoActivate(false);FX->SetAutoDestroy(false);FX->SetAsset(Asset);FX->RegisterComponent();return FX;
    };
    for(int32 I=0;I<16*Reserved+Shared;++I)
    {Effects.Add(Make(*FString::Printf(TEXT("Impact%d"),I)));Ages.Add(-1.f);SlotStyles.Add(-1);SlotPositions.Add(FVector::ZeroVector);}
    for(int32 I=0;I<16*Reserved;++I) {Flights.Add(Make(*FString::Printf(TEXT("Flight%d"),I)));FlightAges.Add(-1.f);}
    for(int32 I=0;I<4;++I)
    {
        auto* Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(Light);Light->SetupAttachment(RootComponent);
        Light->SetMobility(EComponentMobility::Movable);Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(0);
        Light->SetAttenuationRadius(320);Light->SetSourceRadius(18);Light->SetCastShadows(false);Light->SetAffectGlobalIllumination(false);
        Light->SetVisibility(false);Light->RegisterComponent();Lights.Add(Light);LightAges[I]=-1;
    }
    HitRings->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    HitRings->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Feedback/M_ConfirmedHit.M_ConfirmedHit")));
    Pulses.SetNum(RingCapacity);Flashes.Reserve(240);
    for(int32 I=0;I<RingCapacity;++I) HitRings->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));
}
void ANeonSpellImpactDirector::ConfirmDamage(ENeonSpellStyle Style,AMVPEnemy* Enemy,float ActualDamage)
{
    const int32 School=int32(Style);
    if(!IsValid(Enemy) || !FMath::IsFinite(ActualDamage) || ActualDamage<=0 || School<0 || School>=16) return;
    ++ConfirmedHits[School];ConfirmedDamage[School]+=ActualDamage;
    if(auto* Combat=GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()) Combat->RecordDamage(Enemy->GetCombatVFXLocation(),ActualDamage,Style);
    PlayImpact(Style,Enemy->GetCombatVFXLocation());
    if(!Enemy->IsPoolActive()) return;
    const FLinearColor Color=NeonSpellPalette::Color(Style);
    FHitFlash* Flash=Flashes.FindByPredicate([Enemy](const FHitFlash& F){return F.Enemy==Enemy;});
    if(!Flash) Flash=&Flashes.AddDefaulted_GetRef();
    Flash->Enemy=Enemy;Flash->Generation=Enemy->Generation;Flash->Age=0;Flash->Color=Color;
    Enemy->SetCombatHitFlash(Color,ForbiddenSettings::Get(this,"Flash",1)*(ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f ? 0.2f : 1.f));
}
bool ANeonSpellImpactDirector::PlayImpact(ENeonSpellStyle Style,FVector Location)
{
    const int32 School=int32(Style);const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if(School<0 || School>=16 || Location.ContainsNaN() || !Player || FVector::DistSquared(Location,Player->GetActorLocation())>FMath::Square(4500.f)) {++Dropped;return false;}
    const FLinearColor Color=NeonSpellPalette::Color(Style);
    // Damage always receives a pulse, even when dense GPU bursts are already at capacity.
    auto& Pulse=Pulses[PulseCursor++%RingCapacity];Pulse.Age=0;Pulse.Position=Location;Pulse.Color=Color;++VisiblePulses;
    if(ImpactIntensity<=0.f) return true; // Keep confirmed-hit cues even when particles are disabled.
    int32 Slot=INDEX_NONE;
    for(int32 I=0;I<Effects.Num();++I)
    {
        if(Ages[I]>=0 && Ages[I]<0.055f && SlotStyles[I]==School && FVector::DistSquared(SlotPositions[I],Location)<FMath::Square(65.f)) {++CoveredHits;return true;}
    }
    for(int32 I=School*Reserved;I<School*Reserved+Reserved;++I) if(Ages[I]<0) {Slot=I;break;}
    if(Slot==INDEX_NONE) for(int32 I=16*Reserved;I<Effects.Num();++I) if(Ages[I]<0) {Slot=I;break;}
    if(Slot==INDEX_NONE) {++Dropped;return false;}
    UNiagaraComponent* FX=Effects[Slot];if(!FX->GetAsset()) {++Dropped;return false;}
    const bool Major=Style==ENeonSpellStyle::Prism || Style==ENeonSpellStyle::NeonCataclysm;
    const bool Compact=Slot>=16*Reserved;
    FX->SetWorldLocation(Location+FVector(0,0,12));FX->SetVariableInt(TEXT("User.SparkCount"),FMath::RoundToInt((Compact ? 108 : Major ? 576 : 324)*ImpactIntensity));
    FX->SetVariableLinearColor(TEXT("User.EnergyColor"),Color*8.f);
    FX->SetVariableFloat(TEXT("User.Speed"),Major ? 850.f : 750.f);
    FX->SetVariableFloat(TEXT("User.LifeMin"),Compact ? 0.4f : 0.75f);FX->SetVariableFloat(TEXT("User.LifeMax"),Compact ? 0.85f : 1.5f);
    FX->SetVariableFloat(TEXT("User.SizeMin"),3.5f*ReadabilityScale);FX->SetVariableFloat(TEXT("User.SizeMax"),8.f*ReadabilityScale);
    FX->SetVariableFloat(TEXT("User.SourceRadius"),14.f);FX->Activate(true);
    Ages[Slot]=0;SlotStyles[Slot]=School;SlotPositions[Slot]=Location;++Accepted;++ImpactBursts[School];
    const int32 LightIndex=LightCursor++%4;
    Lights[LightIndex]->SetWorldLocation(Location+FVector(0,0,35));Lights[LightIndex]->SetLightColor(Color);
    Lights[LightIndex]->SetIntensity(650*ForbiddenSettings::Get(this,"Flash",1));Lights[LightIndex]->SetVisibility(true);LightAges[LightIndex]=0;
    return true;
}
void ANeonSpellImpactDirector::PlayFlightTrail(ENeonSpellStyle Style,FVector Position)
{
    const int32 School=int32(Style);if(School<0 || School>=16 || Position.ContainsNaN()) return;
    for(int32 I=School*Reserved;I<School*Reserved+Reserved;++I) if(FlightAges[I]<0 && Flights[I]->GetAsset())
    {
        auto* FX=Flights[I].Get();FX->SetWorldLocation(Position);FX->SetVariableInt(TEXT("User.SparkCount"),28);
        FX->SetVariableLinearColor(TEXT("User.EnergyColor"),NeonSpellPalette::Color(Style)*5.f);
        FX->SetVariableFloat(TEXT("User.Speed"),110);FX->SetVariableFloat(TEXT("User.LifeMin"),0.2f);FX->SetVariableFloat(TEXT("User.LifeMax"),0.42f);
        FX->SetVariableFloat(TEXT("User.SizeMin"),2.8f*ReadabilityScale);FX->SetVariableFloat(TEXT("User.SizeMax"),6.f*ReadabilityScale);FX->SetVariableFloat(TEXT("User.SourceRadius"),6);
        FX->Activate(true);FlightAges[I]=0;++FlightBursts[School];break;
    }
}
void ANeonSpellImpactDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bSettingsDirty && FPlatformTime::Seconds()>=SaveAfter) SaveVFXSettings();
    if(auto* P=Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this,0))) ReadabilityScale=FMath::Clamp(P->PreferredCameraDistance/2100.f,1.f,1.5f);
    for(int32 I=0;I<Ages.Num();++I) if(Ages[I]>=0)
    {Ages[I]+=Dt;const float Lifetime=I>=16*Reserved ? 0.95f : 1.6f;if(Ages[I]>=Lifetime) {Ages[I]=-1;Effects[I]->DeactivateImmediate();}}
    for(int32 I=0;I<FlightAges.Num();++I) if(FlightAges[I]>=0) {FlightAges[I]+=Dt;if(FlightAges[I]>=0.48f) {FlightAges[I]=-1;Flights[I]->DeactivateImmediate();}}
    const float FlashGain=ForbiddenSettings::Get(this,"Flash",1)*(ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f ? 0.15f : 1.f);
    for(int32 I=0;I<4;++I) if(LightAges[I]>=0) {LightAges[I]+=Dt;Lights[I]->SetIntensity(650*FlashGain*FMath::Square(FMath::Max(0.f,1.f-LightAges[I]/0.16f)));if(LightAges[I]>=0.16f) {Lights[I]->SetVisibility(false);LightAges[I]=-1;}}
    FVector Camera=FVector(0,0,1000);FRotator View;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->GetPlayerViewPoint(Camera,View);
    bool Dirty=false;
    for(int32 I=0;I<Pulses.Num();++I) if(Pulses[I].Age>=0)
    {
        auto& Pulse=Pulses[I];Pulse.Age+=Dt;Dirty=true;
        FTransform Transform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector);
        if(Pulse.Age<0.32f)
        {
            const FVector Position=Pulse.Position+(Camera-Pulse.Position).GetSafeNormal()*45;
            Transform=FTransform(FRotationMatrix::MakeFromZ(Camera-Position).ToQuat(),Position,FVector(1.45f*ReadabilityScale));
            HitRings->SetCustomDataValue(I,0,Pulse.Color.R);HitRings->SetCustomDataValue(I,1,Pulse.Color.G);HitRings->SetCustomDataValue(I,2,Pulse.Color.B);HitRings->SetCustomDataValue(I,3,Pulse.Age/0.32f);
        }
        else Pulse.Age=-1;
        HitRings->UpdateInstanceTransform(I,Transform,true,false,true);
    }
    if(Dirty) HitRings->MarkRenderStateDirty();
    for(int32 I=Flashes.Num()-1;I>=0;--I)
    {
        auto& F=Flashes[I];F.Age+=Dt;auto* Enemy=F.Enemy.Get();
        if(!Enemy || !Enemy->IsPoolActive() || Enemy->Generation!=F.Generation) {Flashes.RemoveAtSwap(I);continue;}
        Enemy->SetCombatHitFlash(F.Color,FMath::Max(0.f,1.f-F.Age/0.13f)*FlashGain);
        if(F.Age>=0.13f) Flashes.RemoveAtSwap(I);
    }
}
bool ANeonSpellImpactDirector::IsReady() const
{
    return Effects.Num()==96 && Flights.Num()==64 && Effects[0]->GetAsset() && HitRings->GetMaterial(0) && Pulses.Num()==RingCapacity;
}

FString ANeonSpellImpactDirector::SettingsSlot() const
{
    return FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) ? TEXT("ForbiddenVFX_v3_Automation") : TEXT("ForbiddenVFX_v3");
}
void ANeonSpellImpactDirector::SetImpactIntensity(float Value)
{
    if(!FMath::IsFinite(Value)) return;
    ImpactIntensity=FMath::Clamp(Value,0.f,1.5f);bSettingsDirty=true;SaveAfter=FPlatformTime::Seconds()+0.35;
}
bool ANeonSpellImpactDirector::SaveVFXSettings()
{
    if(!bSettingsDirty) return true;
    if(FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark"))) {bSettingsDirty=false;return true;}
    auto* Save=Cast<UCodexVFXSettingsSave>(UGameplayStatics::CreateSaveGameObject(UCodexVFXSettingsSave::StaticClass()));Save->ImpactIntensity=ImpactIntensity;
    if(!UGameplayStatics::SaveGameToSlot(Save,SettingsSlot(),0)) {SaveAfter=FPlatformTime::Seconds()+5;return false;}
    bSettingsDirty=false;return true;
}
void ANeonSpellImpactDirector::LoadVFXSettings()
{
    ImpactIntensity=1;
    const FString LoadSlot=UGameplayStatics::DoesSaveGameExist(SettingsSlot(),0) || SettingsSlot().EndsWith("Automation") ? SettingsSlot() : TEXT("CodexVFX");
    if(!FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) && UGameplayStatics::DoesSaveGameExist(LoadSlot,0))
        if(const auto* Save=Cast<UCodexVFXSettingsSave>(UGameplayStatics::LoadGameFromSlot(LoadSlot,0));Save && Save->Version==1 && FMath::IsFinite(Save->ImpactIntensity)) ImpactIntensity=FMath::Clamp(Save->ImpactIntensity,0.f,1.5f);
    bSettingsDirty=false;
}
void ANeonSpellImpactDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(bSettingsDirty) SaveVFXSettings();Super::EndPlay(Reason);
}
