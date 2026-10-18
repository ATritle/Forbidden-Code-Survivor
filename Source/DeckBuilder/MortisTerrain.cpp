#include "MortisTerrain.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"

bool UMortisTerrain::Ground(AActor* Actor,float Clearance)
{
    if (!IsValid(Actor)) return false;
    if(const auto* Shape=Cast<USphereComponent>(Actor->GetRootComponent())) Clearance=FMath::Max(Clearance,Shape->GetScaledSphereRadius()+21.f);
    const FVector P=Actor->GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(MortisGround),false,Actor);
    if (!Actor->GetWorld()->LineTraceSingleByObjectType(Hit,P+FVector(0,0,30),P-FVector(0,0,2000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)
        || Hit.ImpactNormal.Z<0.65f) return false;
    Actor->SetActorLocation(FVector(P.X,P.Y,Hit.ImpactPoint.Z+Clearance),true);
    return true;
}
bool UMortisTerrain::ClearSight(const AActor* Actor,FVector End)
{
    FHitResult Hit;
    return Actor && !Actor->GetWorld()->LineTraceSingleByObjectType(Hit,Actor->GetActorLocation(),End,
        FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(MortisSight),false,Actor));
}
void UMortisTerrain::ResetPath() { Points.Reset(); PointIndex=0; Repath=FMath::FRandRange(0.f,0.25f); }
void UMortisTerrain::MoveTowards(FVector Destination,float Speed,float Dt)
{
    AActor* Actor=GetOwner();
    if (!Actor || Dt<=0.f) return;
    Ground(Actor);
    Repath-=Dt;
    // At most four synchronous path solves per frame, across the entire crowd.
    static uint64 BudgetFrame=0;
    static int32 Queries=0;
    if (BudgetFrame!=GFrameCounter) { BudgetFrame=GFrameCounter; Queries=0; }
    if (Repath<=0.f && Queries<4)
    {
        ++Queries; Repath=0.6f+FMath::FRandRange(0.f,0.2f);
        if (auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Actor->GetWorld()))
        {
            FNavLocation Goal,Start;
            if (Nav->ProjectPointToNavigation(Destination,Goal,FVector(160,160,220)) &&
                Nav->ProjectPointToNavigation(Actor->GetActorLocation(),Start,FVector(100,100,150)))
            {
                if (auto* Path=Nav->FindPathToLocationSynchronously(Actor->GetWorld(),Start.Location,Goal.Location))
                { Points=Path->PathPoints; PointIndex=Points.Num()>1 ? 1 : 0; }
            }
        }
    }
    if (!Points.IsValidIndex(PointIndex)) return;
    const auto* Shape=Cast<USphereComponent>(Actor->GetRootComponent());
    const float Height=Shape ? FMath::Max(55.f,Shape->GetScaledSphereRadius()+21.f) : 55.f;
    FVector Goal=Points[PointIndex]+FVector(0,0,Height);
    const float Acceptance=Shape ? FMath::Max(45.f,Shape->GetScaledSphereRadius()+20.f) : 45.f;
    if (FVector::DistSquared2D(Actor->GetActorLocation(),Goal)<FMath::Square(Acceptance) && PointIndex+1<Points.Num())
        Goal=Points[++PointIndex]+FVector(0,0,Height);
    const FVector Delta=Goal-Actor->GetActorLocation();
    const FVector Step=Delta.GetSafeNormal()*FMath::Min(Speed*Dt,float(Delta.Size()));
    FHitResult Block;Actor->AddActorWorldOffset(Step,true,&Block);
    if(Block.bBlockingHit && FMath::Abs(Block.Normal.Z)<0.65f)
        Actor->AddActorWorldOffset(FVector::VectorPlaneProject(Step*(1.f-Block.Time),Block.Normal),true);
    Ground(Actor);
}
