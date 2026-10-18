#include "MVPGame.h"
#include "NeonChallenge.h"

void AMVPHUD::DrawChallengeHUD(AMVPPlayer* Player)
{
    if(!Player || !Player->Challenges) return;
    auto* Contracts=Player->Challenges.Get();
    const FString Tracker=Contracts->GetTrackerText();
    if(!Tracker.IsEmpty())
    {
        const FLinearColor Accent=Contracts->ActiveStation && Contracts->ActiveStation->Definition ? Contracts->ActiveStation->Definition->Accent : FLinearColor(0.05f,0.85f,1);
        Box(452,24,710,86,FLinearColor(0.006f,0.012f,0.028f,0.94f));Box(452,24,3,86,Accent);
        TArray<FString> Lines;Tracker.ParseIntoArrayLines(Lines);
        for(int32 I=0;I<Lines.Num();++I) Text(Lines[I],470,39+I*30,I==0 ? 0.8f : 0.7f,Accent);
    }
    if(auto* Station=Contracts->GetNearbyStation())
    {
        Box(465,604,670,83,FLinearColor(0.005f,0.009f,0.02f,0.94f));Box(465,604,670,2,FLinearColor(1,0.025f,0.45f));
        Text(TEXT("[E]  NO PAIN - NO GAIN"),490,617,1.1f,FLinearColor(1,0.08f,0.48f));
        Text(Station->Definition->Title.ToString()+TEXT(" / INTERACT TO REVIEW CONTRACT"),490,653,0.69f,FLinearColor(0.2f,0.85f,1));
    }
    if(Contracts->ActiveStation && Contracts->ActiveStation->Definition)
    {
        const auto Kind=Contracts->ActiveStation->Definition->Contract;
        if(Kind==ENeonChallenge::JackpotCourier || Kind==ENeonChallenge::LockdownDuel)
        {
            const FVector Goal=Kind==ENeonChallenge::LockdownDuel ? Contracts->DuelCenter : Contracts->bCarryingCache ? Contracts->ActiveStation->GetActorLocation() : Contracts->ObjectiveLocation;
            FVector2D Screen;FVector2D Point;
            if(PlayerOwner->ProjectWorldLocationToScreen(Goal+FVector(0,0,150),Screen)) Point=FVector2D((Screen.X-OffsetX)/UIScale,(Screen.Y-OffsetY)/UIScale);
            else {const FVector D=Goal-Player->GetActorLocation();Point=FVector2D(800+D.Y*.2f,450-D.X*.15f);}
            const float X=FMath::Clamp(float(Point.X)-120,40.f,1300.f),Y=FMath::Clamp(float(Point.Y),210.f,570.f);
            const FString Caption=Kind==ENeonChallenge::LockdownDuel ? TEXT("DUEL RING") : Contracts->bCarryingCache ? TEXT("RETURN TO SIGN") : TEXT("DATA CACHE");
            const FLinearColor Color=Contracts->OutsideTime>0 ? FLinearColor(1,0.4f,0.025f) : FLinearColor(0.1f,0.85f,1);
            Box(X,Y,260,42,FLinearColor(0.005f,0.01f,0.025f,0.9f));Box(X,Y,3,42,Color);
            Text(FString::Printf(TEXT("%s / %.0fm"),*Caption,FVector::Dist2D(Goal,Player->GetActorLocation())/100),X+12,Y+13,0.72f,Color);
        }
    }
}
