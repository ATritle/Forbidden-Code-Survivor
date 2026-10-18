#include "NeonCityDirector.h"
#include "NeonStreetLighting.h"
#include "NeonStreetLayout.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"

ANeonCityDirector::ANeonCityDirector()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=0.05f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CityRoot")));
    Rain=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RainStreaks"));Rain->SetupAttachment(RootComponent);
    Rain->SetMobility(EComponentMobility::Movable);Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Rain->SetCanEverAffectNavigation(false);Rain->SetCastShadow(false);
    Rain->bReceivesDecals=false;Rain->BoundsScale=3.f;
    Tags.Add(TEXT("NeonCity"));
}
void ANeonCityDirector::BeginPlay()
{
    Super::BeginPlay();
    if(!FParse::Param(FCommandLine::Get(),TEXT("StreetLayoutBefore")))
    {
        FActorSpawnParameters LayoutParams;LayoutParams.Owner=this;
        GetWorld()->SpawnActor<ANeonStreetLayout>(ANeonStreetLayout::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,LayoutParams);
    }
    // The comparison switch is for standalone visual/performance validation only.
    if(!FParse::Param(FCommandLine::Get(),TEXT("StreetLightingBefore")))
    {
        FActorSpawnParameters StreetParams;StreetParams.Owner=this;
        GetWorld()->SpawnActor<ANeonStreetLighting>(ANeonStreetLighting::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,StreetParams);
    }
    // Shared character-only fills: readable hordes without one light per enemy
    // or lifting the wet street's carefully controlled night exposure.
    for(int32 I=0;I<2;++I)
    {
        FActorSpawnParameters Params;Params.Owner=this;
        auto* Fill=GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),FVector::ZeroVector,FRotator(-45.f,I==0 ? -35.f : 145.f,0),Params);
        auto* Light=Fill->GetLightComponent();Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightingChannels(false,true,false);Light->SetCastShadows(false);
        Light->SetAffectGlobalIllumination(false);Light->SetIndirectLightingIntensity(0.f);
        Light->SetIntensity(I==0 ? 4.f : 1.2f);
        Light->SetLightColor(I==0 ? FLinearColor(0.7f,0.82f,1.f) : FLinearColor(0.65f,0.55f,1.f));
        Fill->Tags.Add(TEXT("CharacterReadability"));
    }
    CityParameters=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/Art/City/MPC_City.MPC_City"));
    Rain->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Rain->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/City/M_Rain.M_Rain")));
    FRandomStream Random(9832);TArray<FTransform> Drops;Drops.Reserve(FMath::Clamp(RainDrops,0,2400));
    for(int32 I=0;I<FMath::Clamp(RainDrops,0,2400);++I)
        Drops.Emplace(FRotator(-12,0,0),FVector(Random.FRandRange(-2200,2200),Random.FRandRange(-2200,2200),Random.FRandRange(0,1600)),FVector(0.004f,0.004f,Random.FRandRange(0.12f,0.28f)));
    Rain->AddInstances(Drops,false,false,false);
    Tick(0.f);
}
void ANeonCityDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    auto* Controller=UGameplayStatics::GetPlayerController(this,0);
    if(!Player || !Controller) return;
    FVector Camera;FRotator Rotation;Controller->GetPlayerViewPoint(Camera,Rotation);
    const FVector Position=Player->GetActorLocation();
    Rain->SetWorldLocation(FVector(Position.X,Position.Y,Position.Z-120.f));
    if(CityParameters)
        if(auto* Parameters=GetWorld()->GetParameterCollectionInstance(CityParameters))
        {
            Parameters->SetVectorParameterValue(TEXT("PlayerPosition"),FLinearColor(Position.X,Position.Y,Position.Z+60.f,bEnableBuildingCutaway ? 1.f : 0.f));
            Parameters->SetVectorParameterValue(TEXT("CameraPosition"),FLinearColor(Camera.X,Camera.Y,Camera.Z,1));
        }
}
