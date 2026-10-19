#include "NeonStreetLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "NeonCityDirector.h"

namespace StreetLayout
{
    struct FRoute { FVector2D Center; FVector2D Size; };
    const FRoute Routes[] = {
        {FVector2D(0,-450),FVector2D(10300,900)},
        {FVector2D(0,-1200),FVector2D(900,7800)},
        {FVector2D(-3250,-1050),FVector2D(900,3100)},
        {FVector2D(2350,-2250),FVector2D(620,3100)}
    };
    bool InRoad(const FVector2D& P,int32 Exclude=-1,float Margin=0)
    {
        for(int32 I=0;I<UE_ARRAY_COUNT(Routes);++I)
            if(I!=Exclude && FMath::Abs(P.X-Routes[I].Center.X)<Routes[I].Size.X*.5f+Margin && FMath::Abs(P.Y-Routes[I].Center.Y)<Routes[I].Size.Y*.5f+Margin) return true;
        return false;
    }
    bool RampEntry(const FVector2D& P)
    {
        return (P.X>225 && P.X<1000 && P.Y>800 && P.Y<2800)
            || (P.X>500 && P.X<1950 && P.Y>270 && P.Y<1030)
            || (P.X>-2700 && P.X<-1400 && P.Y>1150 && P.Y<1850)
            || (P.X>2000 && P.X<2800 && P.Y>-1800 && P.Y<100)
            || (P.X>-2400 && P.X<-900 && P.Y>3200 && P.Y<4000)
            || (P.X>-4800 && P.X<-3700 && P.Y>1100 && P.Y<1900)
            || (FMath::Abs(FMath::Abs(P.X)-1500)<380 && P.Y>-2700 && P.Y<-1300);
    }
}

ANeonStreetLayout::ANeonStreetLayout()
{
    PrimaryActorTick.bCanEverTick=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("StreetRoot")));
    Asphalt=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WetAsphalt"));
    Sidewalks=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RaisedSidewalks"));
    Curbs=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SegmentedCurbs"));
    WhitePaint=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WornWhiteMarkings"));
    AmberPaint=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WornAmberMarkings"));
    Ironwork=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GutterGrates"));
    for(UInstancedStaticMeshComponent* Group:{Asphalt.Get(),Sidewalks.Get(),Curbs.Get(),WhitePaint.Get(),AmberPaint.Get(),Ironwork.Get()})
    {
        Group->SetupAttachment(RootComponent);Group->SetMobility(EComponentMobility::Movable);
        Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);Group->SetCanEverAffectNavigation(false);
        Group->SetGenerateOverlapEvents(false);Group->SetCastShadow(Group==Curbs);
    }
    // Floor probes see the 8cm sidewalk lift. Horizontal movement ignores the
    // decorative edges, so small steps cannot catch hoverboards or crowd spheres.
    Sidewalks->SetCollisionObjectType(ECC_WorldStatic);
    Sidewalks->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Sidewalks->SetCollisionResponseToAllChannels(ECR_Ignore);
    Sidewalks->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}

void ANeonStreetLayout::BeginPlay()
{
    Super::BeginPlay();
    UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/City/SM_CityPanel.SM_CityPanel"));
    const TCHAR* Names[]={TEXT("MI_Asphalt"),TEXT("MI_Sidewalk"),TEXT("MI_Curb"),TEXT("MI_RoadWhite"),TEXT("MI_RoadAmber"),TEXT("MI_Trim")};
    UInstancedStaticMeshComponent* Groups[]={Asphalt.Get(),Sidewalks.Get(),Curbs.Get(),WhitePaint.Get(),AmberPaint.Get(),Ironwork.Get()};
    for(int32 I=0;I<6;++I)
    {
        Groups[I]->SetStaticMesh(Mesh);
        Groups[I]->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/City/%s.%s"),Names[I],Names[I])));
    }
    auto Part=[](UInstancedStaticMeshComponent* Group,FVector P,FVector Size,float Yaw=0.f)
    {Group->AddInstance(FTransform(FRotator(0,Yaw,0),P,Size/100.f));};

    // Replace the prototype's huge tile grid on existing floors with irregular wet stone.
    UMaterialInterface* Court=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/City/MI_Courtyard.MI_Courtyard"));
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        if(*It==this) continue;
        TInlineComponentArray<UInstancedStaticMeshComponent*> Existing(*It);
        for(UInstancedStaticMeshComponent* Group:Existing)
        {
            UMaterialInterface* Material=Group->GetMaterial(0);
            if(Court && Material && Material->GetName()==TEXT("M_WetConcrete")) Group->SetMaterial(0,Court);
            // Remove the old floating, emissive crossing; new paint follows actual roads.
            if(Material && Material->GetName()==TEXT("MI_Ivory"))
                for(int32 Index=Group->GetInstanceCount()-1;Index>=0;--Index)
                {
                    FTransform T;Group->GetInstanceTransform(Index,T,true);
                    if(FMath::IsNearlyEqual(T.GetLocation().Y,-950.f,1.f) && FMath::IsNearlyEqual(T.GetLocation().Z,-9.f,1.f) && FMath::IsNearlyEqual(T.GetScale3D().Y,2.6f,.02f)) Group->RemoveInstance(Index);
                }
        }
    }

    for(int32 Index=0;Index<UE_ARRAY_COUNT(StreetLayout::Routes);++Index)
    {
        const auto& Route=StreetLayout::Routes[Index];
        Part(Asphalt,FVector(Route.Center,-13.9f),FVector(Route.Size,1.8f));
        const bool bEastWest=Route.Size.X>Route.Size.Y;
        const FVector2D Forward=bEastWest?FVector2D(1,0):FVector2D(0,1),Right(-Forward.Y,Forward.X);
        const float Length=bEastWest?Route.Size.X:Route.Size.Y,Width=bEastWest?Route.Size.Y:Route.Size.X;
        auto Position=[&](float Along,float Across){return Route.Center+Forward*Along+Right*Across;};
        auto Oriented=[&](float Along,float Across,float Z,FVector Size,UInstancedStaticMeshComponent* Group)
        {Part(Group,FVector(Position(Along,Across),Z),Size,bEastWest?0:90);};
        for(float Along=-Length*.5f+50;Along<Length*.5f;Along+=100)
            for(int32 Side:{-1,1})
            {
                const FVector2D P=Position(Along,Side*(Width*.5f+120));
                if(StreetLayout::InRoad(P) || StreetLayout::RampEntry(P)) continue;
                Oriented(Along,Side*(Width*.5f+120),-11,FVector(100,240,8),Sidewalks);
                const FVector2D Edge=Position(Along,Side*(Width*.5f+7));
                // Crossing gaps are 3m wide, generous enough for player and elite crowds.
                const bool bCrossing=FMath::Abs(Along)<160 || (!bEastWest && FMath::Abs(Position(Along,0).Y+450)<650);
                if(!bCrossing && !StreetLayout::InRoad(Edge,Index,80))
                    Oriented(Along,Side*(Width*.5f+7),-9,FVector(96,14,12),Curbs);
                if(FMath::Fmod(Along+Length*.5f,600)<100 && !StreetLayout::InRoad(Edge,Index,80))
                    for(int32 Slat=0;Slat<7;++Slat) Oriented(Along-24+Slat*8,Side*(Width*.5f-20),-12.3f,FVector(4,35,1.2f),Ironwork);
            }
        for(float Along=-Length*.5f+180;Along<Length*.5f-120;Along+=360)
        {
            if(StreetLayout::InRoad(Position(Along,0),Index,160)) continue;
            Oriented(Along,-8,-12.7f,FVector(145,6,.5f),AmberPaint);
            Oriented(Along, 8,-12.7f,FVector(145,6,.5f),AmberPaint);
        }
        // Edge lines stop at junctions; they are reflective paint, not emissive neon.
        for(float Along=-Length*.5f+50;Along<Length*.5f;Along+=100)
            if(!StreetLayout::InRoad(Position(Along,0),Index,160))
                for(int32 Side:{-1,1}) Oriented(Along,Side*(Width*.5f-42),-12.6f,FVector(92,8,.5f),WhitePaint);
    }
    // Zebra crossings at the plaza and market junction, with separate stop bars.
    for(const FVector2D Junction:{FVector2D(0,-450),FVector2D(-3250,-450)})
    {
        for(int32 Side:{-1,1})
        {
            for(int32 Stripe=0;Stripe<7;++Stripe)
                Part(WhitePaint,FVector(Junction.X-330+Stripe*110,Junction.Y+Side*620,-12.5f),FVector(58,250,.5f));
            Part(WhitePaint,FVector(Junction.X,Junction.Y+Side*830,-12.5f),FVector(770,15,.5f));
        }
    }
    // Direction arrows in the main approach lanes.
    for(float X:{-4450.f,-1900.f,1300.f,4100.f})
    {
        const float Direction=X<0?1.f:-1.f,Y=X<0?-670.f:-230.f;
        Part(WhitePaint,FVector(X,Y,-12.45f),FVector(150,17,.5f));
        for(int32 Side:{-1,1}) Part(WhitePaint,FVector(X+Direction*60,Y+Side*24,-12.45f),FVector(65,17,.5f),-Direction*Side*45.f);
    }
    UE_LOG(LogTemp,Display,TEXT("STREET_LAYOUT_READY roads=4 sidewalks=%d curbs=%d markings=%d"),Sidewalks->GetInstanceCount(),Curbs->GetInstanceCount(),WhitePaint->GetInstanceCount()+AmberPaint->GetInstanceCount());
}
int32 ANeonStreetLayout::GetSidewalkCount() const {return Sidewalks->GetInstanceCount();}
int32 ANeonStreetLayout::GetCurbCount() const {return Curbs->GetInstanceCount();}
