#pragma once
#include "SpellBase.h"
#include "CodexTestSpell.generated.h"

/** Concrete fixture for native automation; not exposed as an authored spell. */
UCLASS(NotBlueprintable, Transient)
class ACodexTestSpell : public ASpellBase
{
    GENERATED_BODY()
public:
    int32 Shots = 0;
    FSpellModifiers LastShot;
    TWeakObjectPtr<APooledEnemy> LastTarget;
protected:
    virtual void FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot) override;
};
