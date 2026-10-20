#include "ForbiddenWorldQueries.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
bool ForbiddenWorldQueries::FindDuelSite(UWorld* World,FVector Approach,FVector& Result)
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);FNavLocation Start;
    if(!Nav || !Nav->ProjectPointToNavigation(Approach,Start,FVector(100,100,200))) return false;
    for(int32 Candidate=0;Candidate<17;++Candidate)
    {
        const float Angle=Candidate*2.f*PI/8.f;const float Radius=Candidate==0 ? 0.f : Candidate<=8 ? 300.f : 550.f;
        FNavLocation Center;
        if(!Nav->ProjectPointToNavigation(Start.Location+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0),Center,FVector(80,80,80)) || FMath::Abs(Center.Location.Z-Start.Location.Z)>45.f) continue;
        int32 Reachable=0;
        for(int32 Sample=0;Sample<8;++Sample)
        {
            const float A=Sample*2.f*PI/8.f;FNavLocation Edge;
            if(!Nav->ProjectPointToNavigation(Center.Location+FVector(FMath::Cos(A)*580.f,FMath::Sin(A)*580.f,0),Edge,FVector(100,100,45)) || FMath::Abs(Edge.Location.Z-Center.Location.Z)>45.f) continue;
            auto* Path=Nav->FindPathToLocationSynchronously(World,Center.Location,Edge.Location);
            if(Path && Path->IsValid() && !Path->IsPartial() && Path->GetPathLength()<1250.f) ++Reachable;
        }
        if(Reachable>=6) {Result=Center.Location;return true;}
    }
    return false;
}
