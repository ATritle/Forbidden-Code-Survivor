#pragma once
#include "Commandlets/Commandlet.h"
#include "SpellAuthoringCommandlet.generated.h"

/** Reproducible Niagara authoring tool; never linked into the packaged game. */
UCLASS()
class USpellAuthoringCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USpellAuthoringCommandlet();
    virtual int32 Main(const FString& Params) override;
};
