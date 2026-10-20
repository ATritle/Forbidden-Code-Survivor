#include "ForbiddenCardInfo.h"
#include "ForbiddenPrograms.h"
#include "MVPGame.h"
#include "CardDataAsset.h"
#include "TestingCardInfo.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "NeonMenuAudio.h"

FString ForbiddenCardInfo::Describe(const UCardDataAsset* Card)
{
    return TestingCardInfo::Describe(Card).Replace(TEXT("Practice uses base stats without run mutations."),TEXT("Base values before rider bonuses and run mutations."))+(Card ? FString::Printf(TEXT("\n%s / STARTER UNLOCK: %d CODE FRAGMENTS\nAll 16 cards remain available in arena drafts."),*ForbiddenPrograms::RarityName(Card->CardID),ForbiddenPrograms::UnlockCost(Card->CardID)) : FString());
}
UWidget* ForbiddenCardInfo::MakeTooltip(UUserWidget* Owner,const FString& Copy)
{
    auto* Rim=Owner->WidgetTree->ConstructWidget<UBorder>();Rim->SetBrushColor(FLinearColor(.03f,.85f,1));Rim->SetPadding(FMargin(1));
    auto* Body=Owner->WidgetTree->ConstructWidget<UBorder>();Body->SetBrushColor(FLinearColor(.005f,.012f,.025f));Body->SetPadding(FMargin(18));Rim->SetContent(Body);
    auto* Label=Owner->WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Copy));Label->SetColorAndOpacity(FLinearColor(.9f,.96f,1));
    Label->SetAutoWrapText(false);Label->SetWrapTextAt(440);auto Font=Label->GetFont();Font.Size=14;Label->SetFont(Font);Body->SetContent(Label);return Rim;
}
void AMVPHUD::DrawWorkbenchTooltip(const UCardDataAsset* Card,float CardX,float CardY)
{
    if(!Card || !Canvas) return;
    HoveredWorkbenchCard=Card->CardID;WorkbenchTooltipText=ForbiddenCardInfo::Describe(Card);
    const float Width=458,Size=.78f,LineHeight=19;
    UFont* Font=NeonFont ? NeonFont.Get() : GEngine->GetMediumFont();
    const float FontScale=Size*1.5f*GEngine->GetMediumFont()->GetMaxCharHeight()/FMath::Max(1.f,Font->GetMaxCharHeight());
    auto Fits=[&](const FString& Line){float W,H;Canvas->StrLen(Font,Line,W,H);return W*FontScale<=Width-36;};
    TArray<FString> Paragraphs,Lines;WorkbenchTooltipText.ParseIntoArrayLines(Paragraphs,false);
    for(const auto& Paragraph:Paragraphs)
    {
        TArray<FString> Words;Paragraph.ParseIntoArray(Words,TEXT(" "),true);FString Line;
        for(const auto& Word:Words) {const FString Next=Line.IsEmpty() ? Word : Line+TEXT(" ")+Word;if(!Line.IsEmpty() && !Fits(Next)) {Lines.Add(Line);Line=Word;}else Line=Next;}
        Lines.Add(Line);
    }
    const float Height=36+Lines.Num()*LineHeight;
    const float X=CardX+210+12+Width<1580 ? CardX+210+12 : CardX-Width-12;
    const float Y=FMath::Clamp(CardY,20.f,FMath::Max(20.f,810-Height));
    DrawRect(FLinearColor(.001f,.003f,.008f,.99f),OffsetX+X*UIScale,OffsetY+Y*UIScale,Width*UIScale,Height*UIScale);
    const FLinearColor Accent=NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID));
    WorkbenchPanel(TEXT("WorkbenchTooltip"),X,Y,Width,Height,Accent);
    for(int32 I=0;I<Lines.Num();++I) Text(Lines[I],X+18,Y+18+I*LineHeight,Size,I==0 ? Accent : FLinearColor(.9f,.96f,1));
}
bool AMVPHUD::BackFromWorkbench()
{
    auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));if(!G || !G->bInCodex) return false;
    if(bMusicPlaylistOpen) {bMusicPlaylistOpen=false;NeonMenuAudio::Exit(this);return true;}
    if(G->SchoolFilter!=-1 || G->LibraryPage!=0)
    {
        bWorkbenchDraftView=true;G->SchoolFilter=-1;G->LibraryPage=0;HoveredWorkbenchCard=NAME_None;WorkbenchTooltipText.Reset();NeonMenuAudio::Exit(this);return true;
    }
    return false; // Root workbench has no parent. Escape never quits the application.
}
