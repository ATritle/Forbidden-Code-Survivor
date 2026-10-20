#include "KineticCardVFXDirector.h"
#include "ForbiddenSettingsControls.h"
#include "MVPGame.h"
#include "NeonSpellImpactDirector.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"

AKineticCardVFXDirector::AKineticCardVFXDirector()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void AKineticCardVFXDirector::BeginPlay()
{
    Super::BeginPlay();
    auto* Asset=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_KineticImpact.NS_KineticImpact"));
    for(int32 I=0;I<12;++I)
    {
        auto* FX=NewObject<UNiagaraComponent>(this);AddInstanceComponent(FX);FX->SetupAttachment(RootComponent);
        FX->SetAutoActivate(false);FX->SetAutoDestroy(false);FX->SetAsset(Asset);FX->RegisterComponent();Impacts.Add(FX);Ages.Add(-1.f);
        if(I<4)
        {
            auto* Light=NewObject<UPointLightComponent>(this);AddInstanceComponent(Light);Light->SetupAttachment(RootComponent);
            Light->SetMobility(EComponentMobility::Movable);Light->SetIntensityUnits(ELightUnits::Lumens);
            Light->SetLightColor(FLinearColor(1.f,0.62f,0.025f));Light->SetAttenuationRadius(220.f);Light->SetSourceRadius(12.f);
            Light->SetCastShadows(false);Light->SetVisibility(false);Light->RegisterComponent();Lights.Add(Light);
        }
    }
}
bool AKineticCardVFXDirector::PlayImpact(FVector Location)
{
    const auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    const float Intensity=Game && Game->SpellImpactDirector ? Game->SpellImpactDirector->GetImpactIntensity() : 1.f;
    if(Intensity<=0) return true;
    const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if(Location.ContainsNaN() || !Player || FVector::DistSquared(Player->GetActorLocation(),Location)>FMath::Square(2400.f)) {++Dropped;return false;}
    for(int32 I=0;I<Impacts.Num();++I)
        if(Ages[I]<0 && Impacts[I]->GetAsset())
        {
            Ages[I]=0;Impacts[I]->SetWorldLocation(Location);Impacts[I]->SetVariableInt(TEXT("User.SparkCount"),FMath::RoundToInt(36*Intensity));Impacts[I]->Activate(true);
            if(Lights.IsValidIndex(I)) {Lights[I]->SetWorldLocation(Location+FVector(0,0,25));Lights[I]->SetIntensity(220.f*ForbiddenSettings::Get(this,"Flash",1)*(ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f ? 0.15f : 1.f));Lights[I]->SetVisibility(true);}
            ++Accepted;return true;
        }
    ++Dropped;return false;
}
void AKineticCardVFXDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    for(int32 I=0;I<Ages.Num();++I)
        if(Ages[I]>=0)
        {
            Ages[I]+=Dt;
            if(Lights.IsValidIndex(I)) {Lights[I]->SetIntensity(220.f*ForbiddenSettings::Get(this,"Flash",1)*(ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f ? 0.15f : 1.f)*FMath::Square(FMath::Max(0.f,1.f-Ages[I]/0.14f)));if(Ages[I]>=0.14f) Lights[I]->SetVisibility(false);}
            if(Ages[I]>=0.8f) {Ages[I]=-1;Impacts[I]->DeactivateImmediate();}
        }
}
bool AKineticCardVFXDirector::IsReady() const {return Impacts.Num()==12 && Impacts[0]->GetAsset()!=nullptr && Lights.Num()==4;}
