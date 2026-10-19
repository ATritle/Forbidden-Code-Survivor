#include "MVPGame.h"
#include "ForbiddenCodeBranding.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "NeonAudioDirector.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "Engine/Font.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"

void AMVPHUD::PrepareNeonInterface()
{
    if(!bInterfacePrepared)
    {
        bInterfacePrepared=true;
        if(!GlassMaterial) GlassMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/NeonGlass/M_NeonGlass.M_NeonGlass"));
        if(!ArtMaterial) ArtMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/NeonGlass/M_NeonCardArt.M_NeonCardArt"));
        if(!BackdropMaterial) BackdropMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/NeonGlass/M_NeonBackdrop.M_NeonBackdrop"));
        NeonFont=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
        if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/NeonGlass/M_CriticalHealth.M_CriticalHealth"))) HealthAlertMaterial=UMaterialInstanceDynamic::Create(Base,this);
    }
    const double Now=FPlatformTime::Seconds();UIRealDt=FMath::Clamp(float(Now-LastUIFrame),0.f,0.05f);LastUIFrame=Now;
    float X,Y;Cursor=PlayerOwner && PlayerOwner->GetMousePosition(X,Y) ? FVector2D((X-OffsetX)/UIScale,(Y-OffsetY)/UIScale) : FVector2D(-100,-100);
    if(const auto* PC=Cast<AMVPController>(PlayerOwner); PC && PC->IsSettingsOpen()) Cursor=FVector2D(-100,-100);
}
float AMVPHUD::Hover(FName Key,float X,float Y,float W,float H)
{
    const bool bOver=Cursor.X>=X && Cursor.X<=X+W && Cursor.Y>=Y && Cursor.Y<=Y+H;
    float& Value=HoverStates.FindOrAdd(Key);Value=FMath::FInterpTo(Value,bOver ? 1.f : 0.f,UIRealDt,14.f);return Value;
}
void AMVPHUD::Glass(FName Key,float X,float Y,float W,float H,FLinearColor Accent,float HoverValue,bool bSelected,float Mode)
{
    if(!GlassMaterial) return;
    UMaterialInstanceDynamic* Material=UIInstances.FindRef(Key);
    if(!Material) {Material=UMaterialInstanceDynamic::Create(GlassMaterial,this);UIInstances.Add(Key,Material);}
    const auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    Material->SetVectorParameterValue(TEXT("Rect"),FLinearColor(X/1600.f,Y/900.f,W/1600.f,H/900.f));
    Material->SetVectorParameterValue(TEXT("PanelSize"),FLinearColor(W,H,0,0));
    Material->SetVectorParameterValue(TEXT("Accent"),Accent);
    Material->SetVectorParameterValue(TEXT("Control"),FLinearColor(HoverValue,bSelected ? 1.f : 0.f,Mode,Game && Game->bInCodex ? 1.f : 0.f));
    DrawMaterialSimple(Material,OffsetX+X*UIScale,OffsetY+Y*UIScale,W*UIScale,H*UIScale);
}
void AMVPHUD::SpellArt(FName Key,UTexture* Texture,float X,float Y,float W,float H,FLinearColor Accent,float HoverValue)
{
    if(!ArtMaterial || !Texture) return;
    UMaterialInstanceDynamic* Material=UIInstances.FindRef(Key);
    if(!Material) {Material=UMaterialInstanceDynamic::Create(ArtMaterial,this);UIInstances.Add(Key,Material);}
    Material->SetTextureParameterValue(TEXT("Backdrop"),Texture);Material->SetVectorParameterValue(TEXT("Accent"),Accent);
    Material->SetVectorParameterValue(TEXT("Control"),FLinearColor(HoverValue,0,0,0));
    // Contain the complete illustration; neither square nor future portrait art is stretched.
    const float Aspect=FMath::Max(1.f,Texture->GetSurfaceWidth())/FMath::Max(1.f,Texture->GetSurfaceHeight());
    if(W/H>Aspect) {const float Fit=H*Aspect;X+=(W-Fit)*0.5f;W=Fit;}
    else {const float Fit=W/Aspect;Y+=(H-Fit)*0.5f;H=Fit;}
    DrawMaterialSimple(Material,OffsetX+X*UIScale,OffsetY+Y*UIScale,W*UIScale,H*UIScale);
}
void AMVPHUD::NeonTitle(const FString& Value,float X,float Y,float Size,FLinearColor Color)
{
    const float Glitch=FMath::Pow(FMath::Max(0.f,FMath::Sin(float(FPlatformTime::Seconds())*2.7f)),28.f);
    Text(Value,X-1.4f-Glitch*1.5f,Y,Size,FLinearColor(0.03f,0.7f,1.f,0.3f));
    Text(Value,X+1.4f+Glitch*1.5f,Y,Size,FLinearColor(1.f,0.03f,0.4f,0.25f));
    Text(Value,X,Y,Size,Color);
}
void AMVPHUD::DrawHealthAlert(AMVPPlayer* Player)
{
    const auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!Player || !Game || Game->bInCodex || Game->bRunEnded || Player->Health<=0 || Player->Health>=30 || !HealthAlertMaterial) return;
    HealthAlertMaterial->SetVectorParameterValue(TEXT("Control"),FLinearColor(1.f-Player->Health/30.f,0,0,0));
    DrawMaterialSimple(HealthAlertMaterial,0,0,Canvas->SizeX,Canvas->SizeY);
    Text(TEXT("CRITICAL VITALITY  /  FIND ESSENCE"),590,696,0.83f,FLinearColor(1,0.1f,0.3f));
}

void AMVPHUD::DrawCodex(AMVPArenaGameMode* Game)
{
    const FLinearColor White(0.88f,0.95f,1),Muted(0.48f,0.66f,0.77f),Cyan=InterfaceAccent,Pink(1,0.04f,0.5f);
    DrawRect(FLinearColor(0.004f,0.008f,0.02f),0,0,Canvas->SizeX,Canvas->SizeY);
    if(BackdropMaterial) DrawMaterialSimple(BackdropMaterial,OffsetX,OffsetY,1600*UIScale,900*UIScale);
    Glass(TEXT("HeaderGlass"),50,40,1220,110,Cyan,0,false,0);
    NeonTitle(ForbiddenCode::DisplayTitle,80,58,1.55f,White);
    Text(TEXT("FORBIDDEN SOFTWARE. CAPTURED SPIRITS. LIMITLESS ENERGY."),83,113,0.72f,Muted);
    Text(TEXT("BUILD YOUR SIGNAL"),1090,68,0.75f,Cyan);
    Text(TEXT("PRE-RUN  /  SPELL LIBRARY"),1090,100,0.6f,Muted);
    Glass(TEXT("HowToPlay"),80,145,255,34,Cyan,Hover(TEXT("HowToPlay"),80,145,255,34),false,2);
    Text(TEXT("HOW TO PLAY  /  F1"),97,153,0.72f,Cyan);
    Glass(TEXT("CloseGame"),350,145,205,34,Pink,Hover(TEXT("CloseGame"),350,145,205,34),false,2);
    Text(TEXT("CLOSE GAME"),375,153,0.72f,White);
    const TCHAR* Schools[]={TEXT("ALL ENERGY"),TEXT("PULSE"),TEXT("PRISM"),TEXT("KINETIC"),TEXT("BARRIER")};
    for(int32 I=0;I<5;++I)
    {
        const bool bActive=Game->SchoolFilter==(I==0 ? -1 : I+5);
        const float HoverValue=Hover(*FString::Printf(TEXT("Tab_%d"),I),80+I*160,182,150,46);
        Text(Schools[I],90+I*160,197-HoverValue*2,0.78f,bActive ? Cyan : FLinearColor::LerpUsingHSV(Muted,White,HoverValue));
        if(I<4) DrawLine(OffsetX+(238+I*160)*UIScale,OffsetY+194*UIScale,OffsetX+(238+I*160)*UIScale,OffsetY+216*UIScale,FLinearColor(0.13f,0.28f,0.36f),UIScale);
        if(bActive || HoverValue>0.01f)
        {
            Box(87+I*160,225,125,2,bActive ? Cyan : Cyan*HoverValue*0.5f);
            Box(87+I*160,221,125,6,FLinearColor(0.03f,0.7f,1.f,bActive ? 0.08f : HoverValue*0.06f));
        }
    }
    auto Library=Game->GetLibraryCards();const int32 Pages=FMath::Max(1,(Library.Num()+2)/3);Game->LibraryPage=FMath::Clamp(Game->LibraryPage,0,Pages-1);
    Glass(TEXT("LeaderboardTab"),920,182,230,46,Cyan,Hover(TEXT("LeaderboardTab"),920,182,230,46));Text(TEXT("LEADERBOARD [L]"),942,197,0.72f,Cyan);
    for(int32 I=0;I<2;++I)
    {
        const float X=I==0 ? 1240.f : 1380.f;
        const FName Key=*FString::Printf(TEXT("Page_%d"),I);const float H=Hover(Key,X,182,120,46);
        Glass(Key,X,182,120,46,Cyan,H,false,2);Text(I==0 ? TEXT("< PREV") : TEXT("NEXT >"),X+20,197-H*2,0.75f,H>0.1f ? White : Muted);
    }
    Text(FString::Printf(TEXT("PAGE %02d / %02d"),Game->LibraryPage+1,Pages),1340,242,0.65f,Muted);
    for(int32 I=0;I<3 && Library.IsValidIndex(Game->LibraryPage*3+I);++I)
    {
        auto* Card=Library[Game->LibraryPage*3+I];const float X=80+I*485;
        const FName Key=*FString::Printf(TEXT("Library_%d"),I);const float H=Hover(Key,X,270,460,300);const float Y=270-H*4;
        const auto Style=NeonSpellPalette::ForCard(Card->CardID);const FLinearColor Accent=NeonSpellPalette::Color(Style);
        const bool Selected=Game->StartingCards.Contains(Card->CardID);
        Glass(Key,X,Y,460,300,Accent,H,Selected,1);
        Text(FString::Printf(TEXT("%02d   /   %s"),Game->LibraryPage*3+I+1,Schools[FMath::Clamp(int32(Card->MagicSchool)-5,1,4)]),X+24,Y+18,0.66f,Accent);
        Text(FString::Printf(TEXT("%.0f DMG   %.2fs"),Card->BaseDamage,Card->Cooldown),X+285,Y+18,0.62f,Muted);
        SpellArt(*FString::Printf(TEXT("Art_%d"),I),Card->Icon,X+24,Y+54,184,184,Accent,H);
        TArray<FString> Words;Card->Name.ToString().ParseIntoArray(Words,TEXT(" "),true);
        FString Heading;int32 Row=0;
        for(const FString& Word:Words) {if(Heading.Len()+Word.Len()>17 && !Heading.IsEmpty()) {NeonTitle(Heading,X+224,Y+58+Row++*25,0.92f,White);Heading.Empty();}Heading+=Heading.IsEmpty() ? Word : TEXT(" ")+Word;}
        if(!Heading.IsEmpty()) NeonTitle(Heading,X+224,Y+58+Row*25,0.92f,White);
        FString Copy=Card->Description.ToString().Replace(TEXT("\n"),TEXT(" "));Copy.ParseIntoArray(Words,TEXT(" "),true);
        FString Wrapped;Row=0;
        for(const FString& Word:Words) {if(Wrapped.Len()+Word.Len()>27 && !Wrapped.IsEmpty()) {if(Row<7) Text(Wrapped,X+224,Y+118+Row++*16,0.64f,Muted);Wrapped.Empty();}Wrapped+=Wrapped.IsEmpty() ? Word : TEXT(" ")+Word;}
        if(!Wrapped.IsEmpty() && Row<7) Text(Wrapped,X+224,Y+118+Row*16,0.64f,Muted);
        Glass(*FString::Printf(TEXT("Select_%d"),I),X+20,Y+260,420,31,Accent,H,Selected,2);
        Text(Selected ? TEXT("STARTER / EDIT PROGRAM") : Game->DraftPool.Contains(Card->CardID) ? TEXT("DRAFT POOL / EDIT PROGRAM") : TEXT("VIEW / UNLOCK / INSTALL"),X+46,Y+269,0.7f,Selected ? Accent : White);
    }
    if(Library.IsEmpty()) {Glass(TEXT("EmptyLibrary"),80,270,1430,300,Cyan);Text(TEXT("No unlocked spells in this school."),130,395,1.25f,White);}
    Text(FString::Printf(TEXT("STARTING LOADOUT  /  %d OF 2 SLOTS"),Game->StartingCards.Num()),80,606,0.8f,Cyan);
    for(int32 I=0;I<2;++I)
    {
        const FName Key=*FString::Printf(TEXT("Loadout_%d"),I);const float X=80+I*400;const float H=Hover(Key,X,640,380,56);
        FLinearColor Accent=Cyan;FString Name=TEXT("+ EMPTY SIGNAL SLOT");
        if(Game->StartingCards.IsValidIndex(I)) for(UCardDataAsset* Card:Game->GetArenaPlayer()->Deck->UnlockedCards)
            if(Card && Card->CardID==Game->StartingCards[I]) {Name=Card->Name.ToString()+TEXT("   [remove]");Accent=NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID));}
        Glass(Key,X,640-H*2,380,56,Accent,H,Game->StartingCards.IsValidIndex(I),2);Text(Name,X+22,659-H*2,0.78f,Accent);
    }
    Glass(TEXT("LoadoutHint"),900,634,610,68,Cyan);
    Text(TEXT("2 STARTERS / 8 DRAFT PROGRAMS / 6 ACTIVE SLOTS"),925,660,0.68f,Muted);
    Glass(TEXT("DeckMessage"),80,716,1430,43,Pink);Text(Game->DeckMessage,100,731,0.72f,White);
    const bool Valid=!Game->StartingCards.IsEmpty();
    float H=Hover(TEXT("SaveDeck"),80,776,290,64);Glass(TEXT("SaveDeck"),80,776,290,64,Cyan,H,false,2);Text(TEXT("SAVE DECK"),150,798-H*2,0.95f,Cyan);
    H=Hover(TEXT("Hideout"),410,776,330,64);Glass(TEXT("Hideout"),410,776,330,64,Pink,H,false,2);Text(TEXT("HIDEOUT / PRESETS [H]"),438,798-H*2,0.85f,White);
    H=Hover(TEXT("EnterArena"),1080,776,440,64);Glass(TEXT("EnterArena"),1080,776,440,64,Cyan,H,Valid,2);
    NeonTitle(TEXT("ENTER ARENA  >>"),1134,794-H*2,1.2f,Valid ? White : Muted);
    Text(TEXT("ENTER"),1433,822,0.54f,Cyan);
    Text(TEXT("RAIN-SOAKED STREETS. DIGITAL GHOSTS. YOUR RULES."),80,864,0.59f,Muted);
}
