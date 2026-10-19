#include "ForbiddenPrograms.h"
#include "CardDataAsset.h"

void ForbiddenPrograms::Configure(UCardDataAsset* Card)
{
    if(!Card) return;
    struct FBranch {const TCHAR* ID;const TCHAR* A;const TCHAR* AD;const TCHAR* B;const TCHAR* BD;ECardRarity Rarity;};
    static const FBranch Branches[]={
        {TEXT("PulseDart"),TEXT("WEAKPOINT COMPILER"),TEXT("Repeated hits on the same living target build up to 3 precision stacks, +15% damage each."),TEXT("PACKET NOVA"),TEXT("Eight radial darts share a 1.5x volley damage budget. Each dart deals 18.75% damage; maximum 6 pierces."),ECardRarity::Common},
        {TEXT("PrismBurst"),TEXT("AURA CASCADE"),TEXT("Pink impact bursts leave a short damaging aura. Secondary damage cannot trigger itself."),TEXT("PRISM NOVA"),TEXT("Eight compact charges form a circle. Each deals 50% volley damage; area remains capped."),ECardRarity::Common},
        {TEXT("HaloNova"),TEXT("RETURNING HALO"),TEXT("Shards return to the rider and can hit once again on their return leg."),TEXT("SANCTUARY RING"),TEXT("An expanding ring briefly slows enemies it reaches. Boss slow is reduced."),ECardRarity::Rare},
        {TEXT("KineticCards"),TEXT("LOADED DECK"),TEXT("Cards lodge energy marks. The third mark detonates for 90% card damage in 150cm."),TEXT("HOUSE EDGE"),TEXT("Three cards ricochet between nearby targets. Each card uses 60% damage; 3 hits maximum."),ECardRarity::Common},
        {TEXT("PhotonLance"),TEXT("ARMOR DECODER"),TEXT("Lances deal 50% bonus damage against elites and bosses and bypass directional armor."),TEXT("CROSS COMPILER"),TEXT("Fire a perpendicular cross through the target. Each lance deals 60% damage."),ECardRarity::Rare},
        {TEXT("TalismanGuard"),TEXT("RECOVERY SIGILS"),TEXT("Gain 4 charges. Recover one charge every 18s while at least one charge remains. Maximum 8."),TEXT("REACTIVE WARD"),TEXT("Gain 4 charges. A consumed charge emits a 100cm pulse for 45 damage. No recursive echoes."),ECardRarity::Epic},
        {TEXT("ArcRelay"),TEXT("LONG NETWORK"),TEXT("Chain reach grows to 500cm; damage falls 18% each hop. Maximum 8 targets."),TEXT("RETURN SIGNAL"),TEXT("A final bolt returns to the first surviving target for 60% initial damage."),ECardRarity::Common},
        {TEXT("GhostLink"),TEXT("SOUL CACHE"),TEXT("Linked enemies are slowed 25% for 1.5s and yield research toward a stolen routine."),TEXT("GHOST DECOY"),TEXT("Create a digital decoy at the final link for 2s. Basic enemies investigate it."),ECardRarity::Rare},
        {TEXT("NeonCataclysm"),TEXT("AFTERSHOCK"),TEXT("A second 50% damage burst follows the clearly marked primary blast after 0.35s."),TEXT("DENSE CORE"),TEXT("Shrink the blast to 230cm and deal 80% extra damage. Maximum two simultaneous charges."),ECardRarity::Epic},
        {TEXT("OrbitRazor"),TEXT("RECALL EDGE"),TEXT("Returning blades may hit each enemy once per leg. Return damage is 70%."),TEXT("CLOSE ORBIT"),TEXT("Blades orbit the rider before release, covering nearby enemies. Maximum 4 blades."),ECardRarity::Rare},
        {TEXT("RicochetSeal"),TEXT("BANK SHOT"),TEXT("Rebound off one wall and gain 35% damage after the wall bounce."),TEXT("JADE NETWORK"),TEXT("Ricochet to 6 distinct enemies within 420cm; damage falls 12% each hop."),ECardRarity::Common},
        {TEXT("StrobeRail"),TEXT("PERFECT ALIGNMENT"),TEXT("Each successive enemy on the rail takes 12% more damage, capped at +60%."),TEXT("TWIN TRACKS"),TEXT("Fire two parallel 24cm rails, each dealing 70% damage. Walls stop both rails."),ECardRarity::Rare},
        {TEXT("EchoPetals"),TEXT("BLOOM ORBIT"),TEXT("Petals orbit for 0.5s before curving outward. Maximum 8 petals."),TEXT("PETAL REPRISE"),TEXT("Petals curve back toward the rider and may hit once on each leg at 65% return damage."),ECardRarity::Common},
        {TEXT("EventHorizon"),TEXT("GRAVITY WELL"),TEXT("Four pulses pull basic enemies toward the center. Elite pull is reduced; bosses resist it."),TEXT("COLLAPSE"),TEXT("The final pulse implodes for 3x pulse damage within the same capped field."),ECardRarity::Epic},
        {TEXT("ShatterSigil"),TEXT("SHAPED CHARGE"),TEXT("Seven fragments fan forward from impact. Each deals 14 damage and pierces one enemy."),TEXT("AMBER RING"),TEXT("Eight fragments radiate from impact for 10 damage each. Fragments cannot split again."),ECardRarity::Rare},
        {TEXT("SatelliteChoir"),TEXT("HUNTER CHOIR"),TEXT("Orbs prioritize the marked target, then the strongest visible enemy; +35% elite damage."),TEXT("FINISHER CHOIR"),TEXT("Orbs seek the lowest health fraction and gain +60% damage against targets below 30% HP."),ECardRarity::Rare}
    };
    for(const auto& Row:Branches) if(Card->CardID==Row.ID)
    {
        Card->bUseProgramProgression=true;
        if(Card->CardID=="TalismanGuard") Card->Description=FText::FromString(TEXT("Three consumable wards. Basic: 1 charge; medium: 2; elite: 3. Bosses cost 4 for 8% max HP. Upgrades replenish charges; cap 8."));
        Card->EvolutionA=FText::FromString(Row.A);Card->EvolutionB=FText::FromString(Row.B);
        Card->EvolutionDescriptionA=FText::FromString(Row.AD);Card->EvolutionDescriptionB=FText::FromString(Row.BD);
        Card->Rarity=Row.Rarity;
        Card->RarityColor=Row.Rarity==ECardRarity::Epic ? FLinearColor(0.78f,0.2f,1.f) : Row.Rarity==ECardRarity::Rare ? FLinearColor(0.15f,0.6f,1.f) : FLinearColor(0.5f,0.9f,0.8f);
        break;
    }
}
