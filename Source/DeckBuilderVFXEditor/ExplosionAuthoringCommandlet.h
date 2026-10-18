#pragma once
#include "Commandlets/Commandlet.h"
#include "ExplosionAuthoringCommandlet.generated.h"

/** Reproducible Niagara authoring tool; never linked into the packaged game. */
UCLASS()
class UExplosionAuthoringCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UExplosionAuthoringCommandlet();
    virtual int32 Main(const FString& Params) override;
};
