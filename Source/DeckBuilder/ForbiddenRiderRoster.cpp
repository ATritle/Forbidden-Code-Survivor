#include "ForbiddenRiderRoster.h"
#include "ForbiddenProfile.h"
#include "Kismet/GameplayStatics.h"

const TArray<ForbiddenRiders::FDefinition>& ForbiddenRiders::All()
{
    static const TArray<FDefinition> R={
        {"NeonExorcist",TEXT("NEON EXORCIST"),TEXT("STREET MEDIUM"),TEXT("The original rider. Forbidden energy programs, captured spirits and a custom neon board."),TEXT("OPEN SIGNAL / Flexible builds across every energy school."),TEXT("DECK FREEDOM / Choose any two starters from your saved draft pool."),TEXT("BASELINE / Standard health, movement and damage."),FLinearColor(1,.04f,.55f),170},
        {"Vex",TEXT("VEX"),TEXT("GHOST COURIER"),TEXT("An agile street courier who redirects the horde and extends energy networks."),TEXT("AFTERIMAGE / Boost leaves a 1.5s decoy. 6s cooldown; basic enemies only."),TEXT("GHOST ROUTING / Chain attacks gain one target and 15% more jump reach."),TEXT("TRADEOFF / 20% less maximum health."),FLinearColor(.02f,1,.65f),170},
        {"Rook",TEXT("ROOK"),TEXT("SIGNAL WARDEN"),TEXT("An armored street protector who turns collected essence into defensive energy."),TEXT("WARD RECHARGE / Every 25 essence restores 15% max-health shield; cap 30%."),TEXT("REPULSOR / Boost emits a 35-damage, 350cm push pulse. 6s cooldown."),TEXT("TRADEOFF / 10% lower movement speed."),FLinearColor(1,.5f,.035f),185},
        {"Zero",TEXT("ZERO"),TEXT("PRECISION ASSASSIN"),TEXT("A disciplined cyberpunk assassin. Lock a priority target and pierce the line."),TEXT("TARGET LOCK / Three direct hits in 2s mark one foe for 4s: +25% damage."),TEXT("PERFECT LINE / Every third projectile cast adds two pierces to its first shot."),TEXT("TRADEOFF / 20% smaller damage-area radius."),FLinearColor(.04f,.3f,1),178},
        {"Mira",TEXT("MIRA"),TEXT("NEON CONDUCTOR"),TEXT("An underground signal DJ who conducts captured digital spirits into combat."),TEXT("BOUND DAEMON / Permanent familiar fires every 1.2s; damage grows with level."),TEXT("POSSESSION / Every 20 summon hits converts a basic enemy for 8s. 10s cooldown."),TEXT("TRADEOFF / Own weapons deal 15% less damage; summons retain full power."),FLinearColor(.9f,.06f,1),170}
    };return R;
}
const ForbiddenRiders::FDefinition* ForbiddenRiders::Find(FName ID)
{return All().FindByPredicate([ID](const FDefinition& R){return R.ID==ID;});}
FName ForbiddenRiders::Selected(const UObject* Context)
{
    if(auto* GI=UGameplayStatics::GetGameInstance(Context))
        if(auto* P=GI->GetSubsystem<UForbiddenProfile>();P && P->Data && Find(P->Data->SelectedRider)) return P->Data->SelectedRider;
    return TEXT("NeonExorcist");
}
bool ForbiddenRiders::Select(const UObject* Context,FName ID)
{
    if(!Find(ID)) return false;
    if(auto* GI=UGameplayStatics::GetGameInstance(Context))
        if(auto* P=GI->GetSubsystem<UForbiddenProfile>();P && P->Data) {P->Data->SelectedRider=ID;return P->Save();}
    return false;
}
