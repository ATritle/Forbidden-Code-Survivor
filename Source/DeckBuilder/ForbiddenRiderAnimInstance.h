#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ForbiddenRiderAnimInstance.generated.h"

/** The saved AnimBP blends board-safe Meshy clips; gameplay never depends on montage timing. */
UCLASS(Transient,Blueprintable)
class DECKBUILDER_API UForbiddenRiderAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Rider") int32 PoseIndex=0;
    UPROPERTY(Transient,BlueprintReadOnly,Category="Rider") float SequenceRate=1;
};
