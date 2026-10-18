#pragma once
#include "MVPGame.h"
#include "MortisCharacter.generated.h"

/** Blueprint extension point for the playable necromancer. Keeps the tested flat-arena
 * pawn movement, imported Meshy animations, deck and pool integration from AMVPPlayer. */
UCLASS(Blueprintable)
class DECKBUILDER_API AMortisCharacter : public AMVPPlayer
{
    GENERATED_BODY()
};
