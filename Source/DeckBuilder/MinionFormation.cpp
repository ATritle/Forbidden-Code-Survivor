#include "MVPGame.h"
#include "SkeletonMinion.h"

void AMVPArenaGameMode::ReplenishMinions(ASkeletonSummonSpell* Spell)
{
    if (bInCodex || bRunEnded || !IsValid(Spell)) return;
    int32 Count=0;
    for (ASkeletonMinion* M : Minions) if (IsValid(M) && M->GetSummoner()==Spell) ++Count;
    for (ASkeletonMinion* M : Minions)
        if (Count<2 && IsValid(M) && !M->IsActive()) M->Activate(Spell,Count++);
}
