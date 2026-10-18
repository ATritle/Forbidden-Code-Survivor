#include "ArenaSetupCommandlet.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/BlockingVolume.h"
#include "Misc/Paths.h"

UArenaSetupCommandlet::UArenaSetupCommandlet() { IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
int32 UArenaSetupCommandlet::Main(const FString& Params)
{
    const FString MapFile = FPaths::ProjectContentDir()/TEXT("MVP/Arena.umap");
    if (!FEditorFileUtils::LoadMap(MapFile,false,false)) return 1;
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return 2;
    ANavMeshBoundsVolume* Bounds = nullptr;
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("MortisArenaNavigation"))) { Bounds=*It; break; }
    if (!Bounds)
    {
        Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
        Bounds->Tags.Add(TEXT("MortisArenaNavigation"));
        Bounds->SetActorLabel(TEXT("NavMeshBounds_EntireArena"));
    }
    auto* Builder = NewObject<UCubeBuilder>(Bounds);
    Builder->X=18000.f; Builder->Y=18000.f; Builder->Z=4000.f;
    UActorFactory::CreateBrushForVolumeActor(Bounds,Builder);
    Bounds->SetActorLocation(FVector(0,0,1000));
    Bounds->PostEditChange();
    // The visible floor is generated at runtime. This saved, invisible static proxy gives
    // editor navigation a stable walkable surface without waiting for procedural geometry.
    AStaticMeshActor* Proxy=nullptr;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("MortisNavFloor"))) { Proxy=*It; break; }
    if (!Proxy) { Proxy=World->SpawnActor<AStaticMeshActor>(); Proxy->Tags.Add(TEXT("MortisNavFloor")); }
    Proxy->SetActorLabel(TEXT("NavigationFloor_CollisionOnly"));
    Proxy->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Proxy->SetActorLocation(FVector(0,0,-65));
    Proxy->SetActorScale3D(FVector(180,180,1)); // top -15cm, matches existing floor
    Proxy->SetActorHiddenInGame(true);
    Proxy->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Proxy->GetStaticMeshComponent()->SetVisibility(false);
    Proxy->GetStaticMeshComponent()->SetCanEverAffectNavigation(true);
    for (int32 I=0; I<4; ++I)
    {
        const FName Tag(*FString::Printf(TEXT("MortisBoundary%d"),I));
        ABlockingVolume* Wall=nullptr;
        for (TActorIterator<ABlockingVolume> It(World); It; ++It)
            if (It->ActorHasTag(Tag)) { Wall=*It; break; }
        if (!Wall) { Wall=World->SpawnActor<ABlockingVolume>(); Wall->Tags.Add(Tag); }
        Wall->SetActorLabel(FString::Printf(TEXT("ArenaBoundary_%d"),I));
        auto* Box=NewObject<UCubeBuilder>(Wall);
        Box->X=I<2 ? 200.f : 14400.f;
        Box->Y=I<2 ? 14400.f : 200.f;
        Box->Z=2000.f;
        UActorFactory::CreateBrushForVolumeActor(Wall,Box);
        Wall->SetActorLocation(I<2 ? FVector(I==0?-7100:7100,0,800) : FVector(0,I==2?-7100:7100,800));
        Wall->PostEditChange();
        Wall->SetActorHiddenInGame(true);
    }
    auto Kit=[&](const TCHAR* Name,FVector Position,FVector Scale,FRotator Rotation=FRotator::ZeroRotator)
    {
        AStaticMeshActor* Piece=nullptr;
        for(TActorIterator<AStaticMeshActor> It(World);It;++It) if(It->ActorHasTag(FName(Name))) { Piece=*It; break; }
        if(!Piece) { Piece=World->SpawnActor<AStaticMeshActor>(); Piece->Tags.Add(FName(Name)); }
        Piece->Tags.AddUnique(TEXT("MortisTerrainSurface")); Piece->SetActorLabel(Name);
        auto* Mesh=Piece->GetStaticMeshComponent();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetCollisionProfileName(TEXT("BlockAll")); Mesh->SetCanEverAffectNavigation(true);
        if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cinematic/M_StoneTest.M_StoneTest"))) Mesh->SetMaterial(0,Material);
        Piece->SetActorTransform(FTransform(Rotation,Position,Scale)); Piece->PostEditChange();
        return Piece;
    };
    Kit(TEXT("EastRaisedSanctum"),FVector(2200,0,135),FVector(12,14,3));
    Kit(TEXT("EastRamp"),FVector(1000,0,115.6),FVector(12.37,7,0.4),FRotator(14.04,0,0));
    Kit(TEXT("WestRaisedCrypt"),FVector(-2400,-1200,210),FVector(14,16,4.5));
    Kit(TEXT("WestRamp"),FVector(-1000,-1200,190.6),FVector(14.7,7,0.4),FRotator(-17.82,0,0));
    Kit(TEXT("NorthBrokenWall"),FVector(300,1800,135),FVector(15,1.5,3));
    Kit(TEXT("SouthBrokenWall"),FVector(-400,-2300,135),FVector(1.5,14,3));
    // Circular ritual dais with opposed ramps and a ring of ruined pillars.
    auto* Dais=Kit(TEXT("MoonDais"),FVector(0,4200,110),FVector(17,17,2.5));
    Dais->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    Kit(TEXT("MoonSouthRamp"),FVector(0,2900,90.5),FVector(11.3,6,0.4),FRotator(12.8,90,0));
    Kit(TEXT("MoonNorthRamp"),FVector(0,5500,90.5),FVector(11.3,6,0.4),FRotator(12.8,-90,0));
    for(int32 I=0;I<8;++I)
    {
        const float A=I*PI/4.f;
        if(I==2 || I==6) continue;
        Kit(*FString::Printf(TEXT("MoonPillar%d"),I),FVector(FMath::Cos(A)*700,4200+FMath::Sin(A)*700,385),FVector(0.8,0.8,I%2 ? 3.f : 2.f));
    }
    // Elevated bridge: a narrow crossing, end towers, and long sloping approaches.
    Kit(TEXT("ProcessionalBridge"),FVector(-4300,1600,530),FVector(7,18,0.8));
    Kit(TEXT("BridgeSouthTower"),FVector(-4300,450,270),FVector(11,6,5.7));
    Kit(TEXT("BridgeNorthTower"),FVector(-4300,2750,270),FVector(11,6,5.7));
    Kit(TEXT("BridgeSouthApproach"),FVector(-4300,-750,250),FVector(18.92,6,0.4),FRotator(18.43,90,0));
    Kit(TEXT("BridgeNorthApproach"),FVector(-4300,3950,250),FVector(18.92,6,0.4),FRotator(18.43,-90,0));
    // Stepped cruciform crypt; generous stair treads support the same crowd nav agent.
    Kit(TEXT("CryptLowerTerrace"),FVector(3800,-3700,60),FVector(18,18,1.5));
    Kit(TEXT("CryptUpperTerrace"),FVector(3800,-3700,210),FVector(10,10,1.5));
    Kit(TEXT("CryptEastWing"),FVector(4550,-3700,60),FVector(6,7,1.5));
    Kit(TEXT("CryptWestWing"),FVector(3050,-3700,60),FVector(6,7,1.5));
    for(int32 I=0;I<12;++I)
    {
        const float Height=(I+1)*25.f;
        Kit(*FString::Printf(TEXT("CryptStair%d"),I),FVector(3800,-2200-I*85,-15+Height*0.5f),FVector(5,I==11 ? 1.4f : 0.9f,Height/100.f));
    }
    if (auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        FlushAsyncLoading();
        Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock);
        Nav->OnNavigationBoundsUpdated(Bounds); Nav->Build();
        FNavLocation Point;
        if (!Nav->ProjectPointToNavigation(FVector(0,0,55),Point,FVector(100,100,250))) return 7;
        UE_LOG(LogTemp,Display,TEXT("MORTIS_NAV_PROJECT_OK %s"),*Point.Location.ToString());
    }
    Bounds->SetActorHiddenInGame(true);
    if (Bounds->GetComponentsBoundingBox(true).GetExtent().X < 8500.f) return 6;
    if (!FEditorFileUtils::SaveLevel(World->PersistentLevel,MapFile)) return 4;
    UE_LOG(LogTemp,Display,TEXT("MORTIS_NAV_BOUNDS_READY extent=%s"),*Bounds->GetComponentsBoundingBox(true).GetExtent().ToString());
    return 0;
}
