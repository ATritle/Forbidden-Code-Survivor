#include "NeonCitySetupCommandlet.h"
#include "NeonCityDirector.h"
#include "MVPGame.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/BlockingVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierVolume.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationSystem.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "PhysicsEngine/BodySetup.h"

UNeonCitySetupCommandlet::UNeonCitySetupCommandlet() {IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}

int32 UNeonCitySetupCommandlet::Main(const FString& Params)
{
    UWorld* World=GEditor->NewMap(false);
    if(!World) return 1;
    World->GetWorldSettings()->DefaultGameMode=AMVPArenaGameMode::StaticClass();
    auto* Director=World->SpawnActor<ANeonCityDirector>();Director->SetActorLabel(TEXT("Neon District | Weather and Camera Cutaway"));
    Director->WalkableDestinations={FVector(2400,650,320),FVector(0,3550,700),FVector(-3200,1500,560),FVector(-3200,2700,560),FVector(0,-3600,-15),FVector(0,-3000,620)};
    auto* Scene=World->SpawnActor<AActor>();Scene->SetActorLabel(TEXT("Neon District | Instanced Architecture"));
    auto* Root=NewObject<USceneComponent>(Scene,TEXT("ArchitectureRoot"));Scene->SetRootComponent(Root);Root->SetMobility(EComponentMobility::Static);Root->RegisterComponent();
    TMap<FString,UInstancedStaticMeshComponent*> Groups;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/City/SM_CityPanel.SM_CityPanel"));
    auto* PipeMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/City/SM_CityPipe.SM_CityPipe"));
    auto* RampMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/City/SM_CityRamp.SM_CityRamp"));
    auto* CanopyMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/City/SM_CityCanopy.SM_CityCanopy"));
    auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
    if(!Cube || !RampMesh || !PipeMesh || !CanopyMesh) {UE_LOG(LogTemp,Error,TEXT("Run author_city.py before city setup"));return 2;}
    auto Material=[](const FString& Name){return LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/Art/City/"))+Name+TEXT(".")+Name));};
    auto Group=[&](const FString& Name,UStaticMesh* Mesh,const FString& Mat,bool Solid)
    {
        const FString Key=Name+TEXT("_")+Mat+(Solid ? TEXT("_Solid") : TEXT("_Visual"));
        if(auto** Existing=Groups.Find(Key)) return *Existing;
        auto* Component=NewObject<UInstancedStaticMeshComponent>(Scene,*Key);Scene->AddInstanceComponent(Component);
        Component->SetupAttachment(Root);Component->SetMobility(EComponentMobility::Static);Component->SetStaticMesh(Mesh);
        if(!Mat.IsEmpty()) Component->SetMaterial(0,Material(Mat));
        Component->SetCollisionProfileName(Solid ? TEXT("BlockAll") : TEXT("NoCollision"));
        Component->SetCollisionObjectType(ECC_WorldStatic);Component->SetCanEverAffectNavigation(Solid);Component->SetGenerateOverlapEvents(false);
        Component->SetCastShadow(!Mat.Contains(TEXT("Sign")) && !Mat.Contains(TEXT("Ghost")));
        Component->RegisterComponent();Groups.Add(Key,Component);return Component;
    };
    auto Part=[&](const FString& Mat,FVector P,FVector Size,bool Solid=false,FRotator R=FRotator::ZeroRotator)
    {return Group(TEXT("Panels"),Cube,Mat,Solid)->AddInstance(FTransform(R,P,Size/100.f));};
    auto Pipe=[&](FVector A,FVector B,float Diameter,const FString& Mat=TEXT("MI_Trim"))
    {
        FVector Delta=B-A;
        Group(TEXT("Pipes"),PipeMesh,Mat,false)->AddInstance(FTransform(FRotationMatrix::MakeFromZ(Delta).Rotator(),(A+B)*0.5f,FVector(Diameter/100.f,Diameter/100.f,Delta.Size()/100.f)));
    };
    auto Proxy=[&](FVector P,FVector Size,FRotator R=FRotator::ZeroRotator)
    {
        auto* Colliders=Group(TEXT("ArtistGameplayProxies"),Cube,TEXT("MI_Panel"),true);
        Colliders->SetVisibility(false);Colliders->SetHiddenInGame(true);Colliders->SetCastShadow(false);
        Colliders->AddInstance(FTransform(R,P,Size/100.f));
    };
    auto Line=[&](FVector A,FVector B,float Width,const FString& Mat)
    {
        FVector Delta=B-A;Part(Mat,(A+B)*0.5f,FVector(Delta.Size(),Width,Width),false,Delta.Rotation());
    };
    auto Light=[&](FVector P,FLinearColor Color,float Lumens,float Radius,bool Shadow=false)
    {
        auto* Component=NewObject<UPointLightComponent>(Scene);Scene->AddInstanceComponent(Component);Component->SetupAttachment(Root);
        Component->SetMobility(EComponentMobility::Movable);Component->SetRelativeLocation(P);Component->SetIntensityUnits(ELightUnits::Lumens);
        Component->SetIntensity(Lumens);Component->SetLightColor(Color);Component->SetAttenuationRadius(Radius);Component->SetSourceRadius(25);
        Component->SetCastShadows(Shadow);Component->SetVolumetricScatteringIntensity(0.25f);Component->RegisterComponent();
    };
    auto NullRoof=[&](FVector Center,FVector Size)
    {
        auto* Volume=World->SpawnActor<ANavModifierVolume>();Volume->SetActorLabel(TEXT("No navigation on inaccessible tower roof"));
        auto* Builder=NewObject<UCubeBuilder>(Volume);Builder->X=Size.X+40;Builder->Y=Size.Y+40;Builder->Z=4500;
        UActorFactory::CreateBrushForVolumeActor(Volume,Builder);Volume->SetActorLocation(FVector(Center.X,Center.Y,1800));Volume->SetAreaClass(UNavArea_Null::StaticClass());Volume->PostEditChange();
    };
    auto Sign=[&](FVector P,FVector Normal,float Width,float Height,int32 Tile)
    {
        const FVector Right=FVector::CrossProduct(FVector::UpVector,Normal).GetSafeNormal();
        const FRotator Rotation=FRotationMatrix::MakeFromXZ(Right,Normal).Rotator();
        Group(TEXT("Signs"),Plane,FString::Printf(TEXT("MI_Sign%02d"),Tile%16),false)->AddInstance(FTransform(Rotation,P,FVector(Width/100.f,Height/100.f,1)));
        Part(TEXT("MI_Panel"),P-Normal*6,FVector(Width+16,12,Height+16),false,FRotator(0,Normal.Rotation().Yaw-90,0));
        Line(P-Right*(Width*0.5f)+FVector(0,0,-Height*0.5f),P+Right*(Width*0.5f)+FVector(0,0,-Height*0.5f),3,TEXT("MI_Trim"));
    };
    auto Rail=[&](FVector A,FVector B,float Z)
    {
        const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/180.f));
        for(int32 I=0;I<=Count;++I){const FVector P=FMath::Lerp(A,B,float(I)/Count);Pipe(FVector(P.X,P.Y,Z),FVector(P.X,P.Y,Z+105),6);}
        for(float Height:{48.f,105.f}) Pipe(FVector(A.X,A.Y,Z+Height),FVector(B.X,B.Y,Z+Height),5);
        // Collision is independent of decorative rail geometry.
        const FVector D=B-A;Proxy(FVector((A.X+B.X)*0.5f,(A.Y+B.Y)*0.5f,Z+52),FVector(D.Size2D(),10,104),D.Rotation());
    };
    auto Ramp=[&](FVector Start,FVector End,float Width)
    {
        const FVector D=End-Start;const FVector Center=(Start+End)*0.5f;
        Group(TEXT("WalkableRamps"),RampMesh,TEXT("M_WetConcrete"),true)->AddInstance(FTransform(FRotator(0,D.Rotation().Yaw,0),FVector(Center.X,Center.Y,Start.Z),FVector(D.Size2D()/100.f,Width/100.f,(End.Z-Start.Z)/100.f)));
        const FVector Side=FVector::CrossProduct(D.GetSafeNormal2D(),FVector::UpVector)*(Width*0.5f-10);
        for(float S:{-1.f,1.f})
        {
            const FVector A=Start+Side*S,B=End+Side*S;
            Pipe(A+FVector(0,0,85),B+FVector(0,0,85),5);
            Line(A+FVector(0,0,6),B+FVector(0,0,6),5,TEXT("MI_Cyan"));
            for(int32 I=0;I<=8;++I){const FVector P=FMath::Lerp(A,B,I/8.f);Pipe(P,P+FVector(0,0,85),5);}
        }
    };
    auto Vent=[&](FVector P,float Scale=1.f)
    {
        Part(TEXT("MI_Trim"),P+FVector(0,0,45*Scale),FVector(150,105,90)*Scale);
        Part(TEXT("MI_Panel"),P+FVector(0,-54*Scale,45*Scale),FVector(118,5,60)*Scale);
        for(int32 I=0;I<7;++I) Part(TEXT("MI_Trim"),P+FVector(0,-58*Scale,(20+I*8)*Scale),FVector(120,3,2)*Scale);
        Pipe(P+FVector(0,0,90*Scale),P+FVector(0,0,108*Scale),72*Scale,TEXT("MI_Panel"));
        for(int32 I=0;I<4;++I) Part(TEXT("MI_Trim"),P+FVector(0,0,110*Scale),FVector(65,9,3)*Scale,false,FRotator(0,I*45.f,0));
    };
    auto Tower=[&](FVector XY,FVector Footprint,float Height,int32 Style)
    {
        const FVector Base(XY.X,XY.Y,-15);
        const FVector StreetFootprint=Footprint;
        const FVector UpperFootprint(Footprint.X*(Style%2 ? 0.78f : 0.87f),Footprint.Y*(Style%3 ? 0.82f : 0.72f),0);
        const FVector UpperOffset(Footprint.X*0.055f,Footprint.Y*0.055f,0);
        const int32 Floors=FMath::Max(2,FMath::RoundToInt(Height/480.f));
        const int32 Split=FMath::Max(1,Floors/2);const float PodiumHeight=Split*Height/Floors;
        Part(TEXT("MI_Wall"),Base+FVector(0,0,PodiumHeight*0.5f),FVector(Footprint.X,Footprint.Y,PodiumHeight),true);
        Part(TEXT("MI_Wall"),Base+UpperOffset+FVector(0,0,(Height+PodiumHeight)*0.5f),FVector(UpperFootprint.X,UpperFootprint.Y,Height-PodiumHeight),true);
        NullRoof(XY,Footprint);
        for(int32 F=0;F<=Floors;++F)
        {
            Footprint=F>=Split ? UpperFootprint : StreetFootprint;
            const FVector FaceBase=Base+(F>=Split ? UpperOffset : FVector::ZeroVector);
            const float Z=-15+F*(Height/Floors);
            for(float S:{-1.f,1.f})
            {
                Part(TEXT("MI_Trim"),FVector(FaceBase.X,FaceBase.Y+S*Footprint.Y*0.5f,Z),FVector(Footprint.X+24,18,16));
                Part(TEXT("MI_Trim"),FVector(FaceBase.X+S*Footprint.X*0.5f,FaceBase.Y,Z),FVector(18,Footprint.Y+24,16));
            }
            if(F==Floors) continue;
            for(int32 Face=0;Face<4;++Face)
            {
                const FVector Normals[]={FVector(0,-1,0),FVector(1,0,0),FVector(0,1,0),FVector(-1,0,0)};
                const FVector N=Normals[Face];
                const FVector Right=FVector::CrossProduct(FVector::UpVector,N);
                const float Length=Face%2==0 ? Footprint.X : Footprint.Y;
                const int32 Windows=FMath::Max(3,FMath::FloorToInt(Length/190));
                for(int32 W=0;W<Windows;++W)
                {
                    const FVector P=FaceBase+N*((Face%2==0 ? Footprint.Y : Footprint.X)*0.5f+7)+Right*((W+0.5f)*Length/Windows-Length*0.5f)+FVector(0,0,(F+0.5f)*Height/Floors);
                    const FRotator R(0,Face%2==0 ? 0 : 90,0);
                    Part(TEXT("MI_Panel"),P,FVector(144,12,190),false,R);
                    const FString Glow=(W+F+Style)%5==0 ? TEXT("MI_Pink") : (W+F)%3==0 ? TEXT("MI_Cyan") : TEXT("MI_Ivory");
                    for(int32 Slit=0;Slit<3;++Slit)
                        if((W*7+F*11+Slit*3+Face*5+Style)%5<3) Part(Glow,P+N*8+Right*((Slit-1)*39),FVector(27,4,142),false,R);
                    Part(TEXT("MI_Trim"),P+FVector(0,0,-91),FVector(152,35,12),false,R);
                }
                if(F==0 || F==Floors-2)
                    Sign(FaceBase+N*((Face%2==0 ? Footprint.Y : Footprint.X)*0.5f+45)+FVector(0,0,(F+0.7f)*Height/Floors),N,F==0 ? 280 : 350,F==0 ? 160 : 230,(Style*3+F+Face)%16);
                if(F%2==0)
                {
                    const FVector Unit=FaceBase+N*((Face%2==0 ? Footprint.Y : Footprint.X)*0.5f+85)+Right*(Length*0.28f)+FVector(0,0,(F+0.35f)*Height/Floors);
                    Part(TEXT("MI_Trim"),Unit,FVector(130,105,100),false,FRotator(0,Face%2 ? 90 : 0,0));
                    Pipe(Unit+N*52,Unit+N*61,75,TEXT("MI_Panel"));
                    for(int32 G=0;G<6;++G) Line(Unit+N*65+Right*(-48+G*19)+FVector(0,0,-40),Unit+N*65+Right*(-48+G*19)+FVector(0,0,40),3,TEXT("MI_Trim"));
                }
            }
        }
        // Panel seams, raised corner columns and external plumbing create actual depth.
        for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f})
        {
            Part(TEXT("MI_Trim"),Base+FVector(SX*StreetFootprint.X*0.5f,SY*StreetFootprint.Y*0.5f,PodiumHeight*0.5f),FVector(22,22,PodiumHeight+35));
            Part(TEXT("MI_Trim"),Base+UpperOffset+FVector(SX*UpperFootprint.X*0.5f,SY*UpperFootprint.Y*0.5f,(Height+PodiumHeight)*0.5f),FVector(22,22,Height-PodiumHeight+35));
        }
        Part(TEXT("MI_Roof"),Base+FVector(0,0,PodiumHeight+5),FVector(StreetFootprint.X+18,StreetFootprint.Y+18,10));
        Line(Base+FVector(-StreetFootprint.X*0.5f,-StreetFootprint.Y*0.5f,PodiumHeight+12),Base+FVector(StreetFootprint.X*0.5f,-StreetFootprint.Y*0.5f,PodiumHeight+12),5,TEXT("MI_Amber"));
        for(int32 I=0;I<3;++I)
        {
            const FVector P=Base+FVector(-Footprint.X*0.5f-25,(I-1)*110,0);
            Pipe(P+FVector(0,0,80),P+FVector(0,0,Height+120),18,I%2 ? TEXT("MI_Rust") : TEXT("MI_Trim"));
            for(int32 C=0;C<4;++C) Part(TEXT("MI_Panel"),P+FVector(0,0,180+C*Height/4),FVector(35,35,15));
        }
        const FVector Roof=Base+UpperOffset;
        Part(TEXT("MI_Roof"),Roof+FVector(0,0,Height+7),FVector(Footprint.X+35,Footprint.Y+35,22));
        for(float S:{-1.f,1.f})
        {
            Part(TEXT("MI_Trim"),Roof+FVector(0,S*Footprint.Y*0.5f,Height+65),FVector(Footprint.X,18,110));
            Part(TEXT("MI_Trim"),Roof+FVector(S*Footprint.X*0.5f,0,Height+65),FVector(18,Footprint.Y,110));
        }
        for(int32 I=0;I<3;++I) Vent(Roof+FVector(-Footprint.X*0.28f+I*220,Footprint.Y*0.15f,Height+20),1.4f);
        Part(TEXT("MI_Panel"),Roof+FVector(-Footprint.X*0.25f,-Footprint.Y*0.12f,Height+135),FVector(280,240,250));
        const FVector Antenna=Roof+FVector(Footprint.X*0.25f,Footprint.Y*0.23f,Height+20);
        Part(TEXT("MI_Panel"),Antenna+FVector(0,0,150),FVector(150,150,300));
        for(int32 I=0;I<4;++I) Pipe(Antenna+FVector(I%2 ? -55 : 55,I/2 ? -55 : 55,260),Antenna+FVector(I%2 ? -55 : 55,I/2 ? -55 : 55,650-I*60),4);
        Line(Antenna+FVector(0,0,350),Antenna+FVector(0,0,650),5,TEXT("MI_Cyan"));
        const FVector Billboard=Roof+FVector(0,-Footprint.Y*0.3f,Height+240);
        Sign(Billboard,FVector(0,-1,0),520,260,(Style+1)%16);
        Sign(Billboard+FVector(0,20,0),FVector(0,1,0),520,260,(Style+1)%16);
        Sign(Base+FVector(-Footprint.X*0.5f-45,0,Height-520),FVector(-1,0,0),160,520,(Style+9)%16);
        for(float S:{-1.f,1.f}) Pipe(Billboard+FVector(S*210,0,-130),Billboard+FVector(S*210,0,-240),12);
        Light(Base+FVector(0,-StreetFootprint.Y*0.5f-130,260),Style%2 ? FLinearColor(1,0.18f,0.03f) : FLinearColor(0.015f,0.3f,1),900,1050);
    };
    auto ArtistProp=[&](const TCHAR* Name,FVector P,float MaxWidth,float Yaw,bool Collision)
    {
        const FString Asset=FString(TEXT("/Game/Art/City/Meshy/"))+Name+TEXT("/SM_")+Name;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Asset+TEXT(".SM_")+Name));
        if(!Mesh) return false;
        const auto Bounds=Mesh->GetBounds();const float Scale=MaxWidth/FMath::Max(1.f,float(FMath::Max(Bounds.BoxExtent.X,Bounds.BoxExtent.Y)*2));
        const FVector Offset(0,0,-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale);
        Group(FString(TEXT("Meshy_"))+Name,Mesh,TEXT(""),false)->AddInstance(FTransform(FRotator(0,Yaw,0),P+Offset,FVector(Scale)));
        if(Collision)
        {
            const FVector Size(Bounds.BoxExtent.X*2*Scale*0.8f,Bounds.BoxExtent.Y*2*Scale*0.8f,FMath::Min(250.f,float(Bounds.BoxExtent.Z*2*Scale)));
            Proxy(P+FVector(0,0,Size.Z*0.5f),Size,FRotator(0,Yaw,0));
        }
        return true;
    };
    auto StreetDetail=[&](const TCHAR* Name,FVector P,float Width,float Yaw)
    {
        const FString Asset=FString(TEXT("/Game/Art/City/Overhaul/"))+Name+TEXT("/SM_")+Name;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Asset+TEXT(".SM_")+Name));if(!Mesh) return;
        const auto B=Mesh->GetBounds();const float Scale=Width/FMath::Max(1.f,float(B.BoxExtent.X*2));
        const FRotator Rotation(0,Yaw,0);const FVector Bottom=B.Origin-FVector(0,0,B.BoxExtent.Z);
        const FVector Offset=Rotation.RotateVector(FVector(-B.Origin.X,-B.Origin.Y,-Bottom.Z)*Scale);
        Group(FString(TEXT("Overhaul_"))+Name,Mesh,TEXT(""),false)->AddInstance(FTransform(Rotation,P+Offset,FVector(Scale)));
        const FVector Size(B.BoxExtent.X*2*Scale*0.86f,B.BoxExtent.Y*2*Scale*0.86f,B.BoxExtent.Z*2*Scale);
        Proxy(P+FVector(0,0,Size.Z*0.5f),Size,Rotation);
    };

    // A visible, genuinely walkable static floor supplies both PBR reflections and baked navigation.
    Part(TEXT("MI_Panel"),FVector(0,0,-115),FVector(11000,11000,200),true);
    Part(TEXT("M_WetConcrete"),FVector(0,0,-32.5f),FVector(10800,10800,35),true);
    for(int32 I=-24;I<=24;++I)
    {
        const float P=I*200.f;
        for(float Side:{-1.f,1.f}) Part(TEXT("MI_Trim"),FVector(P,Side*5200,-8),FVector(150,22,10));
    }
    // Four districts surround a broad combat plaza. Tall towers have nav-null roof volumes.
    Tower(FVector(-3500,-3300,0),FVector(1400,1300,0),1500,0);
    Tower(FVector(3600,-1650,0),FVector(1700,1600,0),2100,1);
    Tower(FVector(4450,1250,0),FVector(1300,1400,0),1750,2);
    Tower(FVector(-4650,2550,0),FVector(1000,1800,0),950,3);
    Tower(FVector(-2600,4350,0),FVector(1600,1000,0),850,4);
    Tower(FVector(2650,-4400,0),FVector(1600,1000,0),1850,5);
    Tower(FVector(-1250,-4650,0),FVector(950,850,0),1250,6);
    for(int32 I=0;I<3;++I)
    {
        StreetDetail(TEXT("Shopfront"),FVector(-3950+I*430,-2600,-15),400.f,180.f);
        StreetDetail(TEXT("Shopfront"),FVector(2690,-2150+I*430,-15),400.f,90.f);
    }
    for(const FVector P:{FVector(-1700,-800,-15),FVector(900,-1050,-15),FVector(2650,1700,-15),FVector(-1850,2150,-15),FVector(-4000,-2200,-15),FVector(4100,-3400,-15)}) StreetDetail(TEXT("ServiceCabinet"),P,180.f,0);
    if(ArtistProp(TEXT("ShoppingBlock"),FVector(3350,3600,-15),2400,145,false))
    {
        Proxy(FVector(3350,3600,735),FVector(2150,2100,1500),FRotator(0,-35,0));NullRoof(FVector(3350,3600,0),FVector(3400,3400,0));
        for(int32 I=0;I<3;++I) {Sign(FVector(2600+I*480,2450,350+I*160),FVector(0,-1,0),200,350,9+I);}
        const FRotator Orientation(0,-35,0);const FVector Origin(3350,3600,-15);
        for(int32 I=0;I<3;++I)
        {
            Sign(Origin+Orientation.RotateVector(FVector(-1020,-450+I*430,700+I*420)),Orientation.RotateVector(FVector(-1,0,0)),200,330,9+I);
            Sign(Origin+Orientation.RotateVector(FVector(-450+I*450,1020,500+I*350)),Orientation.RotateVector(FVector(0,1,0)),280,160,I==1 ? 0 : 13);
        }
        Light(FVector(2800,2350,260),FLinearColor(0.1f,0.25f,1.f),1300,1100);
        Light(FVector(2050,3500,650),FLinearColor(1,0.24f,0.035f),1800,1250);
    }
    else
    {
        Tower(FVector(3000,3550,0),FVector(1650,1600,0),2600,7);
        Tower(FVector(4350,3800,0),FVector(850,1050,0),1700,8);
    }
    // Raised shopping promenade, approached from the combat plaza.
    Part(TEXT("MI_Wall"),FVector(2400,650,152.5f),FVector(1100,1500,335),true);
    Part(TEXT("M_WetConcrete"),FVector(2400,650,312),FVector(1100,1500,16));
    Ramp(FVector(650,650,-15),FVector(1850,650,320),600);
    Ramp(FVector(2400,-1700,-15),FVector(2400,-100,320),650);
    Rail(FVector(1850,1400,0),FVector(2950,1400,0),320);
    Rail(FVector(2950,-100,0),FVector(2950,1400,0),320);
    for(int32 I=0;I<3;++I)
    {
        const FVector P(2750,-50+I*550,320);Part(TEXT("MI_Panel"),P+FVector(0,0,100),FVector(160,100,200),true);
        Sign(P+FVector(-85,0,120),FVector(-1,0,0),75,130,3+I*4);
    }
    // Reachable rooftop shrine with a wide ramp and an open arrival edge.
    Part(TEXT("MI_Wall"),FVector(0,3700,342.5f),FVector(2000,1800,715),true);
    Part(TEXT("M_WetConcrete"),FVector(0,3700,692),FVector(2000,1800,16));
    Ramp(FVector(600,1000,-15),FVector(600,2800,700),650);
    Ramp(FVector(-2250,3600,-15),FVector(-1000,3600,700),650);
    Rail(FVector(-1000,2800,0),FVector(-1000,3250,0),700);
    Rail(FVector(-1000,3950,0),FVector(-1000,4600,0),700);
    Rail(FVector(-1000,4600,0),FVector(1000,4600,0),700);
    Rail(FVector(1000,4600,0),FVector(1000,2800,0),700);
    Rail(FVector(-1000,2800,0),FVector(180,2800,0),700);
    for(float S:{-1.f,1.f})
    {
        Part(TEXT("MI_Shrine"),FVector(S*400,3450,1010),FVector(50,70,620),true);
        Part(TEXT("MI_Trim"),FVector(S*300,3450,710),FVector(100,110,25));
    }
    Part(TEXT("MI_Shrine"),FVector(0,3450,1320),FVector(960,100,55));
    Group(TEXT("Canopies"),CanopyMesh,TEXT("MI_Panel"),false)->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,3450,1320),FVector(10,2,2)));
    Sign(FVector(0,3390,1300),FVector(0,-1,0),160,140,14);
    Sign(FVector(0,3510,1300),FVector(0,1,0),160,140,14);
    if(!ArtistProp(TEXT("RooftopShrine"),FVector(0,4170,700),460,0,true))
    {
        Part(TEXT("MI_Shrine"),FVector(0,4160,800),FVector(380,230,200),true);
        Sign(FVector(0,4030,820),FVector(0,-1,0),240,160,14);
        Group(TEXT("Canopies"),CanopyMesh,TEXT("MI_Panel"),false)->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,4160,940),FVector(5,3,2)));
    }
    for(int32 I=0;I<64;++I)
    {
        const float A=I*2*PI/64,B=(I+1)*2*PI/64;
        Line(FVector(FMath::Cos(A)*350,3650+FMath::Sin(A)*350,703),FVector(FMath::Cos(B)*350,3650+FMath::Sin(B)*350,703),3,TEXT("MI_Pink"));
    }
    Light(FVector(0,3800,1000),FLinearColor(1,0.02f,0.3f),1400,1100,true);
    // Night market: low, richly layered stalls, broad central lanes and a raised service deck.
    Part(TEXT("MI_Wall"),FVector(-3200,1500,272.5f),FVector(1200,1600,575),true);
    Part(TEXT("M_WetConcrete"),FVector(-3200,1500,552),FVector(1200,1600,16));
    Ramp(FVector(-1500,1500,-15),FVector(-2600,1500,560),600);
    Ramp(FVector(-4700,1500,-15),FVector(-3800,1500,560),650);
    Part(TEXT("MI_Trim"),FVector(-3200,2750,530),FVector(550,900,60),true);
    for(float Side:{-1.f,1.f}) Rail(FVector(-3200+Side*275,2300,0),FVector(-3200+Side*275,3200,0),560);
    Rail(FVector(-3475,3200,0),FVector(-2925,3200,0),560);
    for(int32 I=0;I<3;++I)
    {
        Vent(FVector(-3550+I*280,1880,560));
        for(float S:{-1.f,1.f}) Pipe(FVector(-3200+S*220,2450+I*240,-15),FVector(-3200+S*220,2450+I*240,500),22);
    }
    for(int32 I=0;I<6;++I)
    {
        const FVector P(I<3 ? -4300 : -2250,-1850+(I%3)*600,-15);
        const bool Imported=ArtistProp(TEXT("MarketKiosk"),P,420,I<3 ? 90 : -90,true);
        if(!Imported)
        {
            Part(TEXT("MI_Rust"),P+FVector(0,0,85),FVector(330,240,170),true);
            Group(TEXT("Canopies"),CanopyMesh,TEXT("MI_Canvas"),false)->AddInstance(FTransform(FRotator::ZeroRotator,P+FVector(0,0,235),FVector(4.5f,3.2f,2)));
            for(float SX:{-1.f,1.f}) for(float SY:{-1.f,1.f}) Pipe(P+FVector(SX*160,SY*105,0),P+FVector(SX*160,SY*105,260),7);
        }
        Sign(P+FVector(0,-145,210),FVector(0,-1,0),250,130,(I*3)%16);
        Sign(P+FVector(0,145,210),FVector(0,1,0),250,130,(I*3)%16);
        for(int32 C=0;C<3;++C) Part(TEXT("MI_Rust"),P+FVector(200+(C%2)*70,(C/2)*90,30+C*12),FVector(60,60,60),true);
        if(I%2==0) Light(P+FVector(0,-250,180),I==0 ? FLinearColor(1,0.25f,0.04f) : FLinearColor(1,0.025f,0.3f),600,650);
    }
    // Cable bundles and strung lanterns are real 3D details above the market street.
    for(int32 W=0;W<3;++W)
    {
        FVector Previous(-4300,-1500+W*500,450);
        for(int32 I=1;I<=16;++I)
        {
            const float T=I/16.f;FVector P(-4300+2050*T,-1500+W*500,450-FMath::Sin(T*PI)*110);
            Pipe(Previous,P,2.5f,TEXT("MI_Panel"));Previous=P;
            if(I%3==0)
            {
                Group(TEXT("Lanterns"),Sphere,I%2 ? TEXT("MI_Amber") : TEXT("MI_Pink"),false)->AddInstance(FTransform(FRotator::ZeroRotator,P-FVector(0,0,40),FVector(0.22f,0.22f,0.4f)));
                Pipe(P,P-FVector(0,0,30),3,TEXT("MI_Panel"));
            }
        }
    }
    // Abandoned subway interchange: parallel rails beneath a traversable overpass.
    for(float X:{-1500.f,1500.f})
    {
        Part(TEXT("MI_Wall"),FVector(X,-3000,302.5f),FVector(600,800,635),true);
        Ramp(FVector(X,-1400,-15),FVector(X,-2600,620),550);
    }
    Part(TEXT("MI_Trim"),FVector(0,-3000,590),FVector(3000,450,60),true);
    Part(TEXT("M_WetConcrete"),FVector(0,-3000,614),FVector(3000,450,12));
    for(float Y:{-3225.f,-2775.f}) Rail(FVector(-1200,Y,0),FVector(1200,Y,0),620);
    for(float X:{-1100.f,1100.f}) for(float Y:{-3150.f,-2850.f}) Pipe(FVector(X,Y,-15),FVector(X,Y,560),30);
    for(float X:{-620.f,-400.f,500.f,720.f}) Part(TEXT("MI_Trim"),FVector(X,-4050,-5),FVector(9,2100,12));
    for(int32 I=0;I<24;++I) for(float X:{-510.f,610.f}) Part(TEXT("MI_Rust"),FVector(X,-5000+I*85,-3),FVector(330,25,10));
    Part(TEXT("MI_Train"),FVector(-510,-4150,180),FVector(370,1500,380),true);
    Part(TEXT("MI_Panel"),FVector(-510,-4150,375),FVector(345,1450,40));
    for(int32 I=0;I<6;++I)
    {
        Part(TEXT("MI_Dim"),FVector(-320,-4750+I*220,230),FVector(4,150,110));
        Part(TEXT("MI_Trim"),FVector(-315,-4750+I*220,160),FVector(10,165,8));
    }
    for(int32 I=0;I<3;++I)
    {
        const FVector P(780,-3700-I*470,-15);Part(TEXT("MI_Panel"),P+FVector(0,0,100),FVector(60,190,200),true);
        Sign(P+FVector(-35,0,140),FVector(-1,0,0),130,170,I==0 ? 10 : 11);
    }
    Sign(FVector(0,-3260,720),FVector(0,-1,0),420,160,10);
    Light(FVector(0,-3300,350),FLinearColor(0.02f,0.35f,1),1400,1300,true);
    // Street furniture, safety paint and slim lights keep the plaza readable.
    for(int32 I=0;I<16;++I)
    {
        const float A=I*2*PI/16;const FVector P(FMath::Cos(A)*1150,FMath::Sin(A)*1150,-15);
        if(P.X>550 && P.Y>350) continue;
        Part(TEXT("MI_Panel"),P+FVector(0,0,35),FVector(32,32,70),true);
        Part(TEXT("MI_Cyan"),P+FVector(0,0,72),FVector(24,24,9));
        if(I%4==0) Light(P+FVector(0,0,90),FLinearColor(0.01f,0.45f,1),250,450);
    }
    for(int32 I=0;I<6;++I) Part(TEXT("MI_Ivory"),FVector(-450+I*180,-950,-9),FVector(90,260,1));
    for(int32 I=0;I<18;++I)
    {
        const FVector P(-1150+(I%6)*460,-700+(I/6)*700,-10);
        for(int32 G=0;G<7;++G) Part(TEXT("MI_Panel"),P+FVector(G*6,0,0),FVector(3,70,2));
    }
    // Animated Meshy digital pedestrians are prewarmed by the run manager.
    // Perimeter and explicit collision bounds.
    for(int32 I=0;I<4;++I)
    {
        auto* Wall=World->SpawnActor<ABlockingVolume>();Wall->SetActorLabel(FString::Printf(TEXT("City perimeter %d"),I));
        auto* Builder=NewObject<UCubeBuilder>(Wall);Builder->X=I<2 ? 120 : 11000;Builder->Y=I<2 ? 11000 : 120;Builder->Z=4000;
        UActorFactory::CreateBrushForVolumeActor(Wall,Builder);Wall->SetActorLocation(I<2 ? FVector(I==0 ? -5400 : 5400,0,1400) : FVector(0,I==2 ? -5400 : 5400,1400));Wall->PostEditChange();Wall->SetActorHiddenInGame(true);
    }
    auto* Moon=World->SpawnActor<ADirectionalLight>();Moon->SetActorLabel(TEXT("Cool moon | soft industrial silhouettes"));Moon->SetActorRotation(FRotator(-55,-40,0));
    Moon->GetLightComponent()->SetMobility(EComponentMobility::Movable);Moon->GetLightComponent()->SetIntensity(3.f);Moon->GetLightComponent()->SetLightColor(FLinearColor(0.27f,0.35f,0.65f));Moon->GetLightComponent()->SetCastShadows(true);
    Moon->GetComponent()->ForwardShadingPriority=1;
    auto* Fill=World->SpawnActor<ADirectionalLight>();Fill->SetActorLabel(TEXT("Violet night fill | silhouette readability"));Fill->SetActorRotation(FRotator(-35,135,0));
    Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);Fill->GetLightComponent()->SetIntensity(1.2f);Fill->GetLightComponent()->SetLightColor(FLinearColor(0.4f,0.2f,0.65f));Fill->GetLightComponent()->SetCastShadows(false);
    auto* Sky=World->SpawnActor<ASkyLight>();Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);Sky->GetLightComponent()->SetIntensity(0.25f);Sky->GetLightComponent()->SetLightColor(FLinearColor(0.18f,0.14f,0.4f));
    auto* Fog=World->SpawnActor<AExponentialHeightFog>();Fog->GetComponent()->SetFogDensity(0.012f);Fog->GetComponent()->SetFogHeightFalloff(0.22f);Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.012f,0.008f,0.035f));Fog->GetComponent()->SetVolumetricFog(true);
    auto* Dome=Group(TEXT("SkyDome"),Sphere,TEXT("M_NightSky"),false);Dome->SetCastShadow(false);Dome->bAffectDistanceFieldLighting=false;
    Dome->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,0,0),FVector(1500)));
    auto* Post=World->SpawnActor<APostProcessVolume>();Post->bUnbound=true;
    Post->Settings.bOverride_BloomIntensity=true;Post->Settings.BloomIntensity=0.45f;
    Post->Settings.bOverride_VignetteIntensity=true;Post->Settings.VignetteIntensity=0.2f;
    Post->Settings.bOverride_AutoExposureMethod=true;Post->Settings.AutoExposureMethod=AEM_Manual;
    Post->Settings.bOverride_AutoExposureBias=true;Post->Settings.AutoExposureBias=0.7f;
    Post->Settings.bOverride_MotionBlurAmount=true;Post->Settings.MotionBlurAmount=0;
    auto* Bounds=World->SpawnActor<ANavMeshBoundsVolume>();Bounds->SetActorLabel(TEXT("NavMesh | Entire Neon District"));
    auto* Builder=NewObject<UCubeBuilder>(Bounds);Builder->X=11200;Builder->Y=11200;Builder->Z=5000;UActorFactory::CreateBrushForVolumeActor(Bounds,Builder);Bounds->SetActorLocation(FVector(0,0,1800));Bounds->PostEditChange();
    FlushAsyncLoading();FAssetCompilingManager::Get().FinishAllCompilation();World->UpdateWorldComponents(true,false);
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if(!Nav){FNavigationSystem::AddNavigationSystemToWorld(*World,FNavigationSystemRunMode::EditorMode);Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);}
    if(!Nav) return 4;
    Nav->OnWorldInitDone(FNavigationSystemRunMode::EditorMode);
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock);Nav->OnNavigationBoundsUpdated(Bounds);
    Nav->GetDefaultNavDataInstance(FNavigationSystem::Create);
    UE_LOG(LogTemp,Display,TEXT("CITY_NAV_INPUT mesh=%s collisionShapes=%d bounds=%s nav=%s"),*Cube->GetBoundingBox().ToString(),Cube->GetBodySetup() ? Cube->GetBodySetup()->AggGeom.GetElementCount() : 0,*Bounds->GetComponentsBoundingBox(true).ToString(),*GetNameSafe(Nav->GetDefaultNavDataInstance()));
    Nav->Build();
    FNavLocation Point;
    if(!Nav->ProjectPointToNavigation(FVector(0,0,40),Point,FVector(100,100,150))) {UE_LOG(LogTemp,Error,TEXT("CITY_NAV_MISSING_ORIGIN"));return 5;}
    for(const FVector Goal:Director->WalkableDestinations)
        if(!Nav->ProjectPointToNavigation(Goal,Point,FVector(100,100,150))) {UE_LOG(LogTemp,Error,TEXT("CITY_NAV_MISSING_DESTINATION %s"),*Goal.ToString());return 6;}
    if(!FEditorFileUtils::SaveLevel(World->PersistentLevel,FPaths::ProjectContentDir()/TEXT("Maps/NeonDistrict.umap"))) return 7;
    int32 Instances=0;for(const auto& Pair:Groups) Instances+=Pair.Value->GetInstanceCount();
    UE_LOG(LogTemp,Display,TEXT("NEON_CITY_READY: %d geometry/material groups, %d 3D instances, six connected elevation destinations, full-arena nav bounds"),Groups.Num(),Instances);
    return 0;
}
