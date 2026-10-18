#include "TestingCardInfo.h"
#include "CardDataAsset.h"
#include "SpellBase.h"
#include "NeonArsenal.h"

float TestingCardInfo::ChainReach(FName ID)
{
    const int32 P=NeonArsenal::Find(ID);return P==0 || P==1 || P==4 ? NeonArsenal::Profiles()[P].Radius : 0.f;
}
FString TestingCardInfo::SpacingWarning(const TArray<FName>& Cards,float Spacing)
{
    FString Warning;
    for(FName ID:Cards)
    {
        const float Reach=ChainReach(ID);
        // Nearest() uses a strict distance comparison; spacing equal to reach cannot jump either.
        if(Reach>0 && Spacing>=Reach)
        {
            const auto& P=NeonArsenal::Profiles()[NeonArsenal::Find(ID)];
            Warning+=FString::Printf(TEXT("%s: %.0f cm jump range\n"),P.Name,Reach);
        }
    }
    if(Warning.IsEmpty()) return Warning;
    return FString::Printf(TEXT("GRID SPACING: %.0f cm\n\n%s\nThese attacks cannot jump between adjacent targets at this spacing. They can still hit their first target.\n\nReduce spacing or continue with this setup."),Spacing,*Warning);
}
FString TestingCardInfo::Describe(const UCardDataAsset* Card)
{
    if(!Card) return FString();
    FString Text=Card->Name.ToString()+TEXT("\n\n")+Card->Description.ToString()+TEXT("\n\n");
    if(Card->CardID==TEXT("TalismanGuard"))
        return Text+TEXT("3 orbiting charges. Each contact instantly defeats\none enemy and consumes one charge, including bosses.\nNo automatic recharge; reapply the setup to refill.");
    const int32 P=NeonArsenal::Find(Card->CardID);
    const auto* Defaults=Card->SpellClass ? Card->SpellClass->GetDefaultObject<ASpellBase>() : nullptr;
    const float Range=P>=0 ? NeonArsenal::Profiles()[P].Range : Defaults ? Defaults->TargetRange : 0;
    Text+=FString::Printf(TEXT("Base damage: %.1f     Cooldown: %.2f s\nTarget range: %.0f cm"),Card->BaseDamage,Card->Cooldown,Range);
    if(P>=0)
    {
        const auto& Profile=NeonArsenal::Profiles()[P];
        if(ChainReach(Card->CardID)>0) Text+=FString::Printf(TEXT("\n%s: %.0f cm     Total targets: %d"),P==4 ? TEXT("Rebound range") : TEXT("Chain jump"),Profile.Radius,P==4 ? 4 : Profile.Count);
        else if(P==2 || P==7) Text+=FString::Printf(TEXT("\nArea radius: %.0f cm"),Profile.Radius);
        else if(P==5) Text+=FString::Printf(TEXT("\nBeam width: %.0f cm     Pierces the full line"),Profile.Radius);
        else Text+=FString::Printf(TEXT("\nProjectiles per cast: %d"),P==8 ? 1 : Profile.Count);
    }
    else if(Defaults)
    {
        const auto& M=Defaults->Modifiers;
        Text+=FString::Printf(TEXT("\nProjectiles per cast: %d     Extra pierces: %d"),M.ProjectileCount,M.PiercingCount);
        if(M.NovaPattern) Text+=TEXT("\nPattern: 360-degree nova");
        if(M.ExplodeOnImpact) Text+=TEXT("\nImpact blast: 120 cm radius; secondary damage 60%");
    }
    return Text+TEXT("\n\nPractice uses base stats without run mutations.");
}
