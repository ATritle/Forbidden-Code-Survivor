#include "MVPGame.h"
#include "ForbiddenWorkbenchLayout.h"
#include "ForbiddenRiderRoster.h"
#include "ForbiddenRiderPreview.h"
#include "ForbiddenCodeBranding.h"
#include "NecromancyVisualComponent.h"
#include "NeonMenuAudio.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    FString SchoolName(EMagicSchool School)
    {switch(School) {case EMagicSchool::Pulse:return TEXT("PULSE");case EMagicSchool::Prism:return TEXT("PRISM");case EMagicSchool::Kinetic:return TEXT("KINETIC");case EMagicSchool::Barrier:return TEXT("BARRIER");default:return TEXT("ENERGY");}}
    TArray<FString> PerkCopy(FName ID)
    {
        if(ID=="Vex") return {TEXT("AFTERIMAGE"),TEXT("Boost decoy / 1.5s / 6s cooldown"),TEXT("GHOST ROUTING"),TEXT("+1 chain target / +15% jump reach"),TEXT("TRADEOFF"),TEXT("-20% maximum health")};
        if(ID=="Rook") return {TEXT("WARD RECHARGE"),TEXT("25 essence: +15% shield / 30% cap"),TEXT("REPULSOR"),TEXT("35 damage / 350cm pulse / 6s cooldown"),TEXT("TRADEOFF"),TEXT("-10% movement speed")};
        if(ID=="Zero") return {TEXT("TARGET LOCK"),TEXT("3 direct hits: +25% marked damage"),TEXT("PERFECT LINE"),TEXT("Every third cast: first shot +2 pierces"),TEXT("TRADEOFF"),TEXT("-20% damage-area radius")};
        if(ID=="Mira") return {TEXT("BOUND DAEMON"),TEXT("Familiar / 1.2s shots / level scaling"),TEXT("POSSESSION"),TEXT("20 summon hits: 8s ally / 10s cooldown"),TEXT("TRADEOFF"),TEXT("-15% own-weapon damage")};
        return {TEXT("OPEN SIGNAL"),TEXT("Flexible builds across energy schools"),TEXT("DECK FREEDOM"),TEXT("Choose any two starting programs"),TEXT("BASELINE"),TEXT("Standard health, speed and damage")};
    }
}

bool AMVPPlayer::ApplySelectedRider()
{
    const auto* D=ForbiddenRiders::Find(ForbiddenRiders::Selected(this));if(!D) return false;
    if(!CharacterArt->LoadCharacter(D->ID.ToString(),D->Height)) return false;
    Visual->SetVisibility(false);if(!NeonHoverLight) InitializeNeonVFX();
    if(NeonHoverLight) NeonHoverLight->SetLightColor(D->Color);return true;
}
void AMVPArenaGameMode::OpenRiderSelection()
{
    if(!bInCodex) return;
    // Both controls now live on the same screen; never allocate a menu scene capture.
    bCharacterSelectionOpen=false;if(RiderPreview) RiderPreview->SetPreviewVisible(false);
}
bool AMVPArenaGameMode::SelectRider(FName ID)
{
    if(!bInCodex || !ForbiddenRiders::Find(ID)) return false;
    auto* PC=UGameplayStatics::GetPlayerController(this,0);auto* H=PC ? Cast<AMVPHUD>(PC->GetHUD()) : nullptr;
    if(!H || !H->GetWorkbenchPortrait(ID)) {DeckMessage=TEXT("Rider artwork is unavailable. Your current rider is unchanged.");return false;}
    const FName Previous=ForbiddenRiders::Selected(this);
    if(!ForbiddenRiders::Select(this,ID)) {DeckMessage=TEXT("Could not save the rider selection. Please retry.");return false;}
    if(auto* Player=GetArenaPlayer();!Player || !Player->ApplySelectedRider())
    {ForbiddenRiders::Select(this,Previous);if(Player) Player->ApplySelectedRider();DeckMessage=TEXT("Rider gameplay assets are unavailable.");return false;}
    DeckMessage=TEXT("Rider saved. Choose your programs and enter the arena.");NeonMenuAudio::Input(this);return true;
}
void AMVPArenaGameMode::ContinueToDeck() {OpenRiderSelection();}
bool AMVPArenaGameMode::SelectDeckPreset(int32 Index)
{
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();
    if(!bInCodex || !Profile || !Profile->Data || !Profile->Data->Decks.IsValidIndex(Index)) return false;
    if(!SaveStartingDeck()) return false;
    const int32 Previous=Profile->Data->SelectedDeck;Profile->Data->SelectedDeck=Index;
    if(!Profile->Save()) {Profile->Data->SelectedDeck=Previous;DeckMessage=TEXT("Could not save the preset selection.");return false;}
    LoadStartingDeck();LibraryPage=0;DeckMessage=FString::Printf(TEXT("Deck %d loaded. Card edits autosave to this deck only."),Index+1);
    NeonMenuAudio::Input(this);return true;
}
UTexture* AMVPHUD::GetWorkbenchPortrait(FName ID)
{
    if(WorkbenchPortraits.Contains(ID)) return WorkbenchPortraits.FindRef(ID);
    const auto* Definition=ForbiddenRiders::Find(ID);if(!Definition) return nullptr;
    const FString Name=TEXT("T_Rider_")+Definition->ID.ToString();
    UTexture* Image=LoadObject<UTexture>(nullptr,*(TEXT("/Game/UI/LoadoutWorkbench/")+Name+TEXT(".")+Name));
    WorkbenchPortraits.Add(ID,Image);return Image;
}
TArray<UCardDataAsset*> AMVPHUD::GetWorkbenchCards(AMVPArenaGameMode* G) const
{
    TArray<UCardDataAsset*> Result;if(!G) return Result;
    const auto Library=G->GetLibraryCards();
    if(!bWorkbenchDraftView) return Library;
    for(FName ID:G->DraftPool) for(UCardDataAsset* Card:Library) if(Card && Card->CardID==ID) {Result.Add(Card);break;}
    return Result;
}
void AMVPHUD::WorkbenchPanel(FName Key,float X,float Y,float W,float H,FLinearColor Accent,float Over,bool Selected,bool Chamfer)
{
    Glass(Key,X,Y,W,H,Accent,Over,Selected,Chamfer ? 2.f : 1.f);
    const float Cut=Chamfer ? 13.f : 5.f;
    const FVector2D Points[]={{X+Cut,Y},{X+W-Cut,Y},{X+W,Y+Cut},{X+W,Y+H-Cut},{X+W-Cut,Y+H},{X+Cut,Y+H},{X,Y+H-Cut},{X,Y+Cut}};
    // A filled tint makes hover visible even over the busy city backdrop.
    const float Strength=FMath::Clamp(Over,0.f,1.f);
    if(Strength>.001f) DrawRect(FLinearColor(Accent.R,Accent.G,Accent.B,Strength*.24f),OffsetX+(X+9)*UIScale,OffsetY+(Y+9)*UIScale,(W-18)*UIScale,(H-18)*UIScale);
    const FLinearColor Border=Accent*(.66f+Strength*.34f+(Selected ? .12f : 0.f));
    for(int32 I=0;I<8;++I)
    {
        const auto A=Points[I],B=Points[(I+1)%8];
        FCanvasLineItem Line(FVector2D(OffsetX+A.X*UIScale,OffsetY+A.Y*UIScale),FVector2D(OffsetX+B.X*UIScale,OffsetY+B.Y*UIScale));
        Line.SetColor(Border);Line.LineThickness=(1.f+Strength*.8f)*UIScale;Line.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Line);
    }
}
void AMVPHUD::WorkbenchImage(UTexture* Image,float X,float Y,float W,float H,bool Cover,FVector4 UV,FLinearColor Tint)
{
    if(!Image) return;
    const float SourceAspect=FMath::Max(1.f,Image->GetSurfaceWidth())*UV.Z/(FMath::Max(1.f,Image->GetSurfaceHeight())*UV.W),TargetAspect=W/H;
    if(Cover)
    {
        if(SourceAspect>TargetAspect) {const float Crop=UV.Z*TargetAspect/SourceAspect;UV.X+=(UV.Z-Crop)*.5f;UV.Z=Crop;}
        else {const float Crop=UV.W*SourceAspect/TargetAspect;UV.Y+=(UV.W-Crop)*.5f;UV.W=Crop;}
    }
    else if(TargetAspect>SourceAspect) {const float Fit=H*SourceAspect;X+=(W-Fit)*.5f;W=Fit;}
    else {const float Fit=W/SourceAspect;Y+=(H-Fit)*.5f;H=Fit;}
    DrawTexture(Image,OffsetX+X*UIScale,OffsetY+Y*UIScale,W*UIScale,H*UIScale,UV.X,UV.Y,UV.Z,UV.W,Tint,BLEND_Translucent);
}
void AMVPHUD::WorkbenchCopy(const FString& Copy,float X,float Y,float W,float Size,FLinearColor Color,int32 MaxLines)
{
    UFont* Font=NeonFont ? NeonFont.Get() : GEngine->GetMediumFont();
    const float Ratio=GEngine->GetMediumFont()->GetMaxCharHeight()/FMath::Max(1.f,Font->GetMaxCharHeight());
    auto Fits=[&](const FString& Line){float Width,Height;Canvas->StrLen(Font,Line,Width,Height);return Width*Size*1.5f*Ratio<=W;};
    TArray<FString> Words,Lines;Copy.Replace(TEXT("\n"),TEXT(" ")).ParseIntoArray(Words,TEXT(" "),true);FString Line;
    for(const auto& Word:Words) {const FString Candidate=Line.IsEmpty() ? Word : Line+TEXT(" ")+Word;if(!Line.IsEmpty() && !Fits(Candidate)) {Lines.Add(Line);Line=Word;}else Line=Candidate;}
    if(!Line.IsEmpty()) Lines.Add(Line);
    if(Lines.Num()>MaxLines) {Lines.SetNum(MaxLines);auto& Last=Lines.Last();while(!Last.IsEmpty() && !Fits(Last+TEXT("..."))) Last.LeftChopInline(1);Last+=TEXT("...");}
    for(int32 I=0;I<Lines.Num();++I) Text(Lines[I],X,Y+I*(Size*21.f+3.f),Size,Color);
}
void AMVPHUD::DrawRiderSelection(AMVPArenaGameMode* G)
{
    using namespace ForbiddenWorkbench;
    const auto* D=ForbiddenRiders::Find(ForbiddenRiders::Selected(this));if(!D) return;
    const FLinearColor Cyan=InterfaceAccent,White(.91f,.96f,1),Muted(.57f,.72f,.82f),Pink(1,.025f,.54f),Gold(1,.68f,.06f);
    auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();
    DrawRect(FLinearColor(.004f,.008f,.02f),0,0,Canvas->SizeX,Canvas->SizeY);
    if(BackdropMaterial) DrawMaterialSimple(BackdropMaterial,OffsetX,OffsetY,1600*UIScale,900*UIScale);
    auto Button=[&](FName Key,const FString& Copy,float X,float Y,float W,float H,FLinearColor Color,bool Selected=false)
    {
        const float Over=Hover(Key,X,Y,W,H);WorkbenchPanel(Key,X,Y,W,H,Color,Over,Selected,true);
        UFont* Font=NeonFont ? NeonFont.Get() : GEngine->GetMediumFont();float Width,Height;Canvas->StrLen(Font,Copy,Width,Height);
        const float TextScale=.86f*1.5f*GEngine->GetMediumFont()->GetMaxCharHeight()/FMath::Max(1.f,Font->GetMaxCharHeight());
        Text(Copy,X+FMath::Max(8.f,(W-Width*TextScale)*.5f),Y+(H-Height*TextScale)*.5f,.86f,Selected ? White : Color);
    };
    auto Icon=[&](int32 Kind,float X,float Y,FLinearColor Color)
    {
        auto Line=[&](FVector2D A,FVector2D B){DrawLine(OffsetX+(X+A.X)*UIScale,OffsetY+(Y+A.Y)*UIScale,OffsetX+(X+B.X)*UIScale,OffsetY+(Y+B.Y)*UIScale,Color,1.4f*UIScale);};
        if(Kind==0) {for(int32 Ring=0;Ring<2;++Ring) for(int32 I=0;I<24;++I){const float R=Ring==0 ? 10.f : 6.f,A=I*2*PI/24,B=(I+1)*2*PI/24;Line(FVector2D(FMath::Cos(A)*R,FMath::Sin(A)*R),FVector2D(FMath::Cos(B)*R,FMath::Sin(B)*R));}Line({-14,0},{14,0});Line({0,-14},{0,14});}
        else if(Kind==1) {for(int32 I=0;I<2;++I){const float DX=I*8-9;Line({DX-4,-10},{DX+6,0});Line({DX+6,0},{DX-4,10});}}
        else if(Kind==2) {Line({0,-12},{13,11});Line({13,11},{-13,11});Line({-13,11},{0,-12});Line({0,-4},{0,3});Line({0,6},{0,8});}
        else if(Kind==3) {for(int32 I=0;I<3;++I){const float DY=I*5-6;Line({-11,DY},{0,DY-5});Line({0,DY-5},{11,DY});Line({11,DY},{0,DY+5});Line({0,DY+5},{-11,DY});}}
        else {for(int32 I=0;I<10;++I){const float A=I*PI/5-PI/2,B=(I+1)*PI/5-PI/2,R=I%2 ? 5.f : 12.f,T=(I+1)%2 ? 5.f : 12.f;Line({FMath::Cos(A)*R,FMath::Sin(A)*R},{FMath::Cos(B)*T,FMath::Sin(B)*T});}}
    };
    WorkbenchPanel(TEXT("WorkbenchHeader"),52,20,1496,65,Cyan);
    NeonTitle(ForbiddenCode::DisplayTitle,76,29,1.35f,White);Text(TEXT("LOADOUT WORKBENCH"),77,66,.61f,Cyan);
    Button(TEXT("HowToPlay"),TEXT("HOW TO PLAY / F1"),HowToX,NavY,NavW,NavH,Cyan);
    Button(TEXT("MenuSettings"),TEXT("SETTINGS / ESC"),SettingsX,NavY,NavW,NavH,Cyan);
    Button(TEXT("RiderScores"),TEXT("LEADERBOARD [L]"),ScoresX,NavY,NavW,NavH,Cyan);
    Button(TEXT("CloseGame"),TEXT("CLOSE GAME"),CloseX,NavY,NavW,NavH,Pink);
    WorkbenchPanel(TEXT("WorkbenchRider"),53,98,394,514,Cyan);
    WorkbenchCopy(D->Name,77,117,177,1.17f,White,1);WorkbenchCopy(D->Role,78,150,171,.65f,Muted,2);
    const float PortraitOver=Hover(TEXT("WorkbenchPortrait"),76,175,174,207);
    const float PortraitShade=1.f-PortraitOver*.22f;
    WorkbenchImage(GetWorkbenchPortrait(D->ID),76,175,174,207,false,FVector4(0,0,1,1),FLinearColor(PortraitShade,PortraitShade,PortraitShade));
    const auto& Riders=ForbiddenRiders::All();
    for(int32 I=0;I<Riders.Num();++I)
    {
        const auto& R=Riders[I];const float Y=RiderY+I*RiderPitch;const FName Key=*FString::Printf(TEXT("WorkbenchRider%d"),I);
        const float Over=Hover(Key,RiderX,Y,RiderW,RiderH);WorkbenchPanel(Key,RiderX,Y,RiderW,RiderH,R.ID==D->ID ? Cyan : R.Color,Over,R.ID==D->ID);
        WorkbenchImage(GetWorkbenchPortrait(R.ID),RiderX+5,Y+5,38,38,true,FVector4(.28,.035,.44,.26),FLinearColor(1.f-Over*.25f,1.f-Over*.25f,1.f-Over*.25f));
        Text(I==0 ? TEXT("NEON") : R.Name,RiderX+51,Y+17,.85f,R.ID==D->ID ? Cyan : White);
    }
    WorkbenchCopy(D->Name+TEXT(" / ")+D->Role,77,408,350,.82f,Cyan,1);
    WorkbenchCopy(D->Description,77,433,348,.68f,Muted,2);
    const auto Perks=PerkCopy(D->ID);
    for(int32 I=0;I<3;++I)
    {
        const float Y=466+I*45;const FLinearColor C=I==2 ? Pink : Cyan;
        WorkbenchPanel(*FString::Printf(TEXT("WorkbenchPerk%d"),I),76,Y,351,39,C);
        Icon(I,98,Y+20,C);
        Text(Perks[I*2],121,Y+5,.7f,C);WorkbenchCopy(Perks[I*2+1],121,Y+22,294,.64f,White,1);
    }
    Button(TEXT("WorkbenchHideout"),TEXT("HIDEOUT / RUN HISTORY [H]"),76,631,351,31,Cyan);
    WorkbenchCopy(G->DeckMessage,78,678,343,.63f,Muted,2);
    Text(TEXT("HOW YOUR DECK WORKS"),77,722,.75f,Cyan);
    const TCHAR* Steps[]={TEXT("1. Pick Deck 1, 2 or 3; each saves separately."),TEXT("2. Browse All Programs to replace a card."),TEXT("3. Click a deck card to set a starting attack."),TEXT("4. Start with 2. Equip up to 6 during a run.")};
    for(int32 I=0;I<4;++I) Text(Steps[I],77,744+I*18,.68f,White);
    WorkbenchPanel(TEXT("WorkbenchDeck"),462,98,1086,724,Cyan);
    Text(FString::Printf(TEXT("DECK %d / PROGRAMS"),Profile && Profile->Data ? Profile->Data->SelectedDeck+1 : 1),491,127,1.03f,Cyan);
    for(int32 I=0;I<3;++I) Button(*FString::Printf(TEXT("WorkbenchPreset%d"),I),FString::Printf(TEXT("DECK %d"),I+1),PresetX+I*PresetPitch,PresetY,PresetW,PresetH,Cyan,Profile && Profile->Data && Profile->Data->SelectedDeck==I);
    Button(TEXT("WorkbenchSave"),TEXT("SAVE DECK"),SaveX,ActionY,168,ActionH,Cyan);
    Button(TEXT("WorkbenchEnter"),TEXT("ENTER ARENA >>"),EnterX,ActionY,244,ActionH,Pink);
    const TCHAR* Schools[]={TEXT("ALL"),TEXT("PULSE"),TEXT("PRISM"),TEXT("KINETIC"),TEXT("BARRIER")};
    for(int32 I=0;I<5;++I) Button(*FString::Printf(TEXT("WorkbenchSchool%d"),I),Schools[I],TabsX+I*TabPitch,TabsY,TabW,TabH,I==2 ? Pink : I==3 ? Gold : Cyan,G->SchoolFilter==(I==0 ? -1 : I+5));
    Text(TEXT("FILTER BY SCHOOL"),1380,185,.67f,Muted);
    auto Library=GetWorkbenchCards(G);const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Library.Num(),CardsPerPage));G->LibraryPage=FMath::Clamp(G->LibraryPage,0,Pages-1);
    for(int32 I=0;I<CardsPerPage && Library.IsValidIndex(G->LibraryPage*CardsPerPage+I);++I)
    {
        auto* Card=Library[G->LibraryPage*CardsPerPage+I];const float X=CardX(I),Y=CardY(I);const FName Key=*FString::Printf(TEXT("WorkbenchCard%d"),I);
        const bool Starter=G->StartingCards.Contains(Card->CardID),Draft=G->DraftPool.Contains(Card->CardID),Unlocked=!Profile || Profile->IsUnlocked(Card->CardID);
        const FLinearColor Accent=NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID));const float Over=Hover(Key,X,Y,CardW,CardH);
        WorkbenchPanel(Key,X,Y,CardW,CardH,Accent,Over,Draft);
        const float Shade=1.f-Over*.34f;
        WorkbenchImage(Card->Icon,X+5,Y+5,CardW-10,143,true,FVector4(0,0,1,1),Unlocked ? FLinearColor(Shade,Shade,Shade) : FLinearColor(.38f*Shade,.38f*Shade,.45f*Shade));
        if(Starter || !Unlocked)
        {Box(X+3,Y+4,Unlocked ? 85 : 124,23,Unlocked ? Cyan : FLinearColor(.5f,.1f,.3f));Text(Unlocked ? TEXT("STARTING") : TEXT("LOCKED / 30"),X+12,Y+9,.59f,Unlocked ? FLinearColor(.005f,.015f,.03f) : White);}
        WorkbenchCopy(Card->Name.ToString(),X+14,Y+155,CardW-28,1.02f,White,1);
        Text(SchoolName(Card->MagicSchool),X+14,Y+180,.76f,Accent);
        WorkbenchCopy(Card->Description.ToString(),X+14,Y+201,CardW-28,.73f,Muted,2);
        Text(FString::Printf(TEXT("%.0f DMG / %.2fs"),Card->BaseDamage,Card->Cooldown),X+113,Y+181,.63f,White);
        if(Draft) Text(TEXT("+"),X+CardW-20,Y+156,.68f,Cyan);
    }
    if(Library.IsEmpty()) WorkbenchCopy(bWorkbenchDraftView ? TEXT("No draft programs in this school. Browse the library to choose programs.") : TEXT("No programs in this school."),515,350,900,.9f,White,3);
    WorkbenchPanel(TEXT("WorkbenchDraftCount"),491,739,324,49,Cyan);Icon(3,514,763,Cyan);Text(FString::Printf(TEXT("%d / 8"),G->DraftPool.Num()),538,752,1.f,Cyan);Text(TEXT("LEVEL-UP CARD POOL"),620,758,.7f,White);
    WorkbenchPanel(TEXT("WorkbenchStarterCount"),830,739,700,49,Gold);Icon(4,855,763,Gold);Text(FString::Printf(TEXT("%d / 2"),G->StartingCards.Num()),882,752,1.f,Gold);Text(TEXT("ATTACKS EQUIPPED AT RUN START"),964,746,.65f,White);
    FString StarterNames;for(FName ID:G->StartingCards) for(UCardDataAsset* Card:G->GetArenaPlayer()->Deck->UnlockedCards) if(Card && Card->CardID==ID) {if(!StarterNames.IsEmpty()) StarterNames+=TEXT(" + ");StarterNames+=Card->Name.ToString();break;}
    WorkbenchCopy(StarterNames,964,765,540,.72f,Gold,1);
    Button(TEXT("WorkbenchView"),bWorkbenchDraftView ? TEXT("BROWSE ALL PROGRAMS >") : TEXT("< SHOW MY 8-CARD DECK"),ViewX,ViewY,ViewW,ViewH,Cyan);
    Text(FString::Printf(TEXT("%s / %d PROGRAMS / PAGE %d OF %d"),bWorkbenchDraftView ? TEXT("DECK") : TEXT("LIBRARY"),Library.Num(),G->LibraryPage+1,Pages),784,801,.55f,Muted);
    Button(TEXT("WorkbenchPrev"),TEXT("< PREV"),PrevX,PageY,PageW,ViewH,Cyan);Button(TEXT("WorkbenchNext"),TEXT("NEXT >"),NextX,PageY,PageW,ViewH,Cyan);
    DrawMusicPlayer(G);
}
bool AMVPHUD::ClickRiderSelection(float X,float Y,AMVPArenaGameMode* G)
{
    using namespace ForbiddenWorkbench;
    if(ClickMusicPlayer(X,Y,G)) return true;
    auto In=[&](float L,float T,float W,float H){return Within(X,Y,L,T,W,H);};auto* PC=Cast<AMVPController>(PlayerOwner);
    if(In(HowToX,NavY,NavW,NavH)) {if(PC) PC->OpenHelp();return true;}
    if(In(SettingsX,NavY,NavW,NavH)) {if(PC) PC->ToggleSettings();return true;}
    if(In(ScoresX,NavY,NavW,NavH)) {if(PC) PC->OpenLeaderboard();return true;}
    if(In(CloseX,NavY,NavW,NavH)) {if(PC) PC->CloseGame();return true;}
    for(int32 I=0;I<ForbiddenRiders::All().Num();++I) if(In(RiderX,RiderY+I*RiderPitch,RiderW,RiderH)) {G->SelectRider(ForbiddenRiders::All()[I].ID);return true;}
    for(int32 I=0;I<3;++I) if(In(PresetX+I*PresetPitch,PresetY,PresetW,PresetH)) {G->SelectDeckPreset(I);return true;}
    if(In(SaveX,ActionY,168,ActionH)) {if(G->SaveStartingDeck()) NeonMenuAudio::Input(this);return true;}
    if(In(EnterX,ActionY,244,ActionH)) {if(G->StartSelectedRun()) NeonMenuAudio::Input(this);return true;}
    if(In(76,631,351,31)) {if(PC) PC->OpenHideout();return true;}
    if(In(ViewX,ViewY,ViewW,ViewH)) {bWorkbenchDraftView=!bWorkbenchDraftView;G->LibraryPage=0;NeonMenuAudio::Input(this);return true;}
    for(int32 I=0;I<5;++I) if(In(TabsX+I*TabPitch,TabsY,TabW,TabH)) {G->SchoolFilter=I==0 ? -1 : I+5;G->LibraryPage=0;NeonMenuAudio::Input(this);return true;}
    const auto Library=GetWorkbenchCards(G);const int32 LastPage=FMath::Max(0,(Library.Num()-1)/CardsPerPage);
    if(In(PrevX,PageY,PageW,ViewH)) {G->LibraryPage=FMath::Max(0,G->LibraryPage-1);NeonMenuAudio::Input(this);return true;}
    if(In(NextX,PageY,PageW,ViewH)) {G->LibraryPage=FMath::Min(LastPage,G->LibraryPage+1);NeonMenuAudio::Input(this);return true;}
    for(int32 I=0;I<CardsPerPage && Library.IsValidIndex(G->LibraryPage*CardsPerPage+I);++I) if(In(CardX(I),CardY(I),CardW,CardH))
    {NeonMenuAudio::Input(this);G->bProgramEditorFromHideout=false;G->OpenProgramEditor(Library[G->LibraryPage*CardsPerPage+I]->CardID);return true;}
    return false;
}
