#include "NeonStreetLighting.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

ANeonStreetLighting::ANeonStreetLighting()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .2f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StreetLightingRoot")));
    Metal = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LampHousings"));
    Trim = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LampTrim"));
    Lenses = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WarmDiffusers"));
    for (UInstancedStaticMeshComponent* Group : {Metal.Get(), Trim.Get(), Lenses.Get()})
    {
        Group->SetupAttachment(RootComponent);
        Group->SetMobility(EComponentMobility::Movable);
        // Small curbside fixtures must not invalidate the baked navigation or snag hordes.
        Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Group->SetCanEverAffectNavigation(false);
        Group->SetCastShadow(false);
    }
    Tags.Add(TEXT("CityStreetLighting"));
}

void ANeonStreetLighting::BeginPlay()
{
    Super::BeginPlay();
    UStaticMesh* Panel = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Art/City/SM_CityPanel.SM_CityPanel"));
    Metal->SetStaticMesh(Panel); Trim->SetStaticMesh(Panel); Lenses->SetStaticMesh(Panel);
    Metal->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/City/MI_Panel.MI_Panel")));
    Trim->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/City/MI_Trim.MI_Trim")));
    Lenses->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Art/City/MI_Ivory.MI_Ivory")));

    // Each yaw aims into the street. Poles sit beside routes, never on their ramps.
    AddLamp(FVector(-1360,-1020,-15), 0);
    AddLamp(FVector(-1360,  260,-15), 0);
    AddLamp(FVector( 1360,-1020,-15), 180);
    AddLamp(FVector( 1360, -100,-15), 180);
    AddLamp(FVector(-1000, 1160,-15), -90);
    AddLamp(FVector(  200,-1330,-15), 90);
    AddLamp(FVector(-3930,-2130,-15), 0);
    AddLamp(FVector(-3930,-1130,-15), 0);
    AddLamp(FVector(-3930,  -70,-15), 0);
    AddLamp(FVector(-2630,-2130,-15), 180);
    AddLamp(FVector(-2630,-1130,-15), 180);
    AddLamp(FVector(-2630,  -70,-15), 180);
    AddLamp(FVector( 2540,-2460,-15), 135);
    AddLamp(FVector( 2540,-1200,-15), 180);
    AddLamp(FVector( 3840, -600,-15), 180);
    AddLamp(FVector( 3630, 1630,-15), 180);
    AddLamp(FVector( 1750, 2460,-15), 180);
    AddLamp(FVector(-2080, 3500,-15), 0);
    AddLamp(FVector( -800,-3900,-15), 0);
    AddLamp(FVector( 1040,-3900,-15), 180);
    // Short wall brackets put light BELOW the bridge deck, not on top of it.
    AddLamp(FVector(-1080,-3000,290), 0, true);
    AddLamp(FVector( 1080,-3000,290), 180, true);
    AddLamp(FVector( 2860,  170,320), 180);
    AddLamp(FVector( 2860, 1090,320), 180);
    AddLamp(FVector(-3650, 1100,560), 0);
    AddLamp(FVector( -850, 3450,700), 0);
    AddLamp(FVector(  850, 4300,700), 180);

    // Gentle environment-only bounce reveals shaded facades without bleaching neon.
    // This costs no additional shadow map; the stronger character fill stays separate.
    FActorSpawnParameters Params; Params.Owner = this;
    ADirectionalLight* Bounce = GetWorld()->SpawnActor<ADirectionalLight>(
        ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-28,155,0), Params);
    if (Bounce)
    {
        ULightComponent* Light = Bounce->GetLightComponent();
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightingChannels(true,false,false);
        Light->SetCastShadows(false);
        Light->SetAffectGlobalIllumination(false);
        Light->SetIndirectLightingIntensity(0.f);
        Light->SetIntensity(.85f);
        Light->SetLightColor(FLinearColor(.56f,.68f,1.f));
        Bounce->Tags.Add(TEXT("StreetFacadeFill"));
    }
    Tick(0.f);
    UE_LOG(LogTemp, Display, TEXT("STREET_LIGHTING_READY fixtures=%d activeCap=%d shadowCap=%d"), Lamps.Num(), MaxActiveLights, MaxShadowLights);
}

void ANeonStreetLighting::AddLamp(const FVector& Foot, float StreetYaw, bool bUnderpass)
{
    const FRotator Frame(0, StreetYaw, 0);
    auto Part = [&](UInstancedStaticMeshComponent* Group, const FVector& Offset, const FVector& Size)
    {
        Group->AddInstance(FTransform(Frame, Foot + Frame.RotateVector(Offset), Size / 100.f));
    };
    const float Height = bUnderpass ? 100.f : 390.f;
    if (!bUnderpass)
    {
        Part(Metal, FVector(0,0,12), FVector(48,42,24));
        Part(Trim, FVector(0,0,Height/2), FVector(12,15,Height));
        Part(Metal, FVector(0,0,Height-48), FVector(22,24,86));
        Part(Lenses, FVector(12,0,Height-50), FVector(3,9,44));
        // Curb footing and a pale safety stripe help separate walkway from building.
        Part(Trim, FVector(0,0,3), FVector(65,180,6));
    }
    else Part(Trim, FVector(0,0,Height/2), FVector(18,32,Height));
    Part(Trim, FVector(65,0,Height), FVector(145,14,12));
    Part(Metal, FVector(120,0,Height-6), FVector(72,40,18));
    Part(Lenses, FVector(120,0,Height-17), FVector(60,30,3));
    USpotLightComponent* Light = NewObject<USpotLightComponent>(this);
    AddInstanceComponent(Light);
    Light->SetupAttachment(RootComponent);
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetRelativeLocation(Foot + Frame.RotateVector(FVector(120,0,Height-24)));
    Light->SetRelativeRotation(FRotator(-77,StreetYaw,0));
    Light->SetIntensityUnits(ELightUnits::Lumens);
    Light->SetIntensity(0.f);
    Light->SetLightColor(bUnderpass ? FLinearColor(.48f,.8f,1.f) : FLinearColor(1.f,.79f,.58f));
    Light->SetAttenuationRadius(1350.f);
    Light->SetInnerConeAngle(42.f); Light->SetOuterConeAngle(68.f);
    Light->SetSourceRadius(14.f); Light->SetSourceLength(35.f);
    Light->SetVolumetricScatteringIntensity(.08f);
    Light->SetIndirectLightingIntensity(.35f);
    Light->SetCastShadows(false);
    Light->SetVisibility(false);
    Light->RegisterComponent();
    Lamps.Add(Light);
}

void ANeonStreetLighting::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const APawn* Viewer = UGameplayStatics::GetPlayerPawn(this,0);
    if (!Viewer) return;
    const FVector Position = Viewer->GetActorLocation();
    TArray<int32, TInlineAllocator<32>> Order;
    for (int32 Index=0; Index<Lamps.Num(); ++Index) Order.Add(Index);
    Order.Sort([&](int32 A,int32 B)
    {
        return FVector::DistSquared(Position,Lamps[A]->GetComponentLocation()) < FVector::DistSquared(Position,Lamps[B]->GetComponentLocation());
    });
    int32 Active = 0, Shadows = 0;
    for (const int32 Index : Order)
    {
        USpotLightComponent* Light = Lamps[Index];
        const float Distance = FVector::Distance(Position,Light->GetComponentLocation());
        const float Fade = 1.f-FMath::SmoothStep(2300.f,3900.f,Distance);
        const bool bVisible = Active < FMath::Clamp(MaxActiveLights,0,16) && Fade > .001f && Brightness > 0.f;
        // The near lights retain shadows; remote pools use cheaper direct lighting.
        const bool bShadow = bVisible && Shadows < FMath::Clamp(MaxShadowLights,0,3) && Distance < 1700.f;
        Light->SetVisibility(bVisible);
        if (Light->CastShadows != bShadow) Light->SetCastShadows(bShadow);
        Light->SetIntensity(bVisible ? 1800.f*Fade*FMath::Clamp(Brightness,0.f,2.f) : 0.f);
        if (bVisible) ++Active;
        if (bShadow) ++Shadows;
    }
}

int32 ANeonStreetLighting::GetActiveLightCount() const
{
    int32 Count=0; for (const USpotLightComponent* Light : Lamps) if (Light->IsVisible()) ++Count; return Count;
}
int32 ANeonStreetLighting::GetShadowLightCount() const
{
    int32 Count=0; for (const USpotLightComponent* Light : Lamps) if (Light->IsVisible() && Light->CastShadows) ++Count; return Count;
}
