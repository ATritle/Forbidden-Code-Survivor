#include "MVPGame.h"
#include "NecromancyVisualComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "ForbiddenSettingsControls.h"

void AMVPPlayer::UpdateGroundPhysics(float Dt)
{
    Movement->Velocity.Z=0.f;
    float Left=FMath::Min(Dt,0.25f);
    while(Left>0.f)
    {
        const float Step=FMath::Min(Left,1.f/120.f); Left-=Step;
        const FVector P=GetActorLocation();
        FHitResult Floor;
        const bool Found=GetWorld()->LineTraceSingleByObjectType(Floor,P+FVector(0,0,30),P-FVector(0,0,3000),
            FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(PlayerFloor),false,this)) && Floor.ImpactNormal.Z>0.65f;
        const float FloorZ=Found ? Floor.ImpactPoint.Z+55.f : -3000.f;
        if(!bFalling && Found && P.Z-FloorZ<=24.f)
        { SetActorLocation(FVector(P.X,P.Y,FloorZ),true); VerticalSpeed=0.f; continue; }
        if(!bFalling) { bFalling=true;FallStartZ=P.Z;VerticalSpeed=0.f; }
        VerticalSpeed=FMath::Max(-1800.f,VerticalSpeed-1500.f*Step);
        const float NextZ=P.Z+VerticalSpeed*Step;
        if(Found && NextZ<=FloorZ)
        {
            const float Speed=-VerticalSpeed, Height=FMath::Max(0.f,FallStartZ-FloorZ);
            SetActorLocation(FVector(P.X,P.Y,FloorZ),true);
            bFalling=false;VerticalSpeed=0.f;
            if(Height>55.f)
            {
                ++LandingCount;LandingPulse=0.34f;LandingStrength=FMath::Clamp(Speed/1050.f,0.2f,1.f);
                LandingDust->SetWorldLocation(Floor.ImpactPoint+FVector(0,0,5));LandingDust->Activate(true);
                OnLandingImpact(Speed,Height);
                if(Height>=180.f && LandingRewardCooldown<=0 && Deck && Deck->EnemySpawner)
                {
                    LandingRewardCooldown=8.f;LandingCharge=2.5f;
                    for(APooledEnemy* A:Deck->EnemySpawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive() && FVector::DistSquared(E->GetActorLocation(),GetActorLocation())<FMath::Square(260.f)) E->StaggerRemaining=E->EnemyTier>=3 ? 0.15f : 0.8f;
                }
            }
        }
        else SetActorLocation(FVector(P.X,P.Y,NextZ),true);
    }
    LandingPulse=FMath::Max(0.f,LandingPulse-Dt);
    const float T=1.f-LandingPulse/0.34f;
    const float Offset=LandingPulse>0 ? -14.f*LandingStrength*FMath::Sin(T*PI)*FMath::Exp(-T*2.f) : 0.f;
    CharacterArt->AddLocalOffset(FVector(0,0,Offset-LastLandingOffset));LastLandingOffset=Offset;
    CameraBoom->SocketOffset.Z=LandingPulse>0 ? LandingStrength*10.f*FMath::Sin(T*PI*4.f)*(1.f-T)*ForbiddenSettings::Get(this,"Shake",0.6f) : 0.f;
}
