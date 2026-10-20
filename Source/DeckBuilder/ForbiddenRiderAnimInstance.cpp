#include "ForbiddenRiderAnimInstance.h"
#include "NecromancyVisualComponent.h"
void UForbiddenRiderAnimInstance::NativeUpdateAnimation(float Dt)
{
    Super::NativeUpdateAnimation(Dt);
    const auto* Visual=Cast<UNecromancyVisualComponent>(GetSkelMeshComponent());if(!Visual) return;
    static const TArray<FName> States={"HoverIdle","Cruise","Accelerate","Brake","TurnLeft","TurnRight","Cast","Hit","Fall","Land","Defeat","CastBlood","CastStorm","HoverIdleAlt"};
    PoseIndex=FMath::Max(0,States.Find(Visual->GetRiderState()));SequenceRate=Visual->GetRiderPlayRate();
}
