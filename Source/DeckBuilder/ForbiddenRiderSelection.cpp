#include "MVPGame.h"
#include "ForbiddenRiderRoster.h"
#include "ForbiddenRiderPreview.h"
#include "ForbiddenCodeBranding.h"
#include "NecromancyVisualComponent.h"
#include "NeonMenuAudio.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"

bool AMVPPlayer::ApplySelectedRider()
{
    const auto* D=ForbiddenRiders::Find(ForbiddenRiders::Selected(this));if(!D) return false;
    if(!CharacterArt->LoadCharacter(D->ID.ToString(),D->Height)) return false;
    Visual->SetVisibility(false);if(!NeonHoverLight) InitializeNeonVFX();
    if(NeonHoverLight) NeonHoverLight->SetLightColor(D->Color);
    return true;
}
void AMVPArenaGameMode::OpenRiderSelection()
{
    if(!bInCodex) return;
    if(!RiderPreview)
    {
        FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        RiderPreview=GetWorld()->SpawnActor<AForbiddenRiderPreview>(FVector(0,0,100000),FRotator::ZeroRotator,P);
    }
    if(RiderPreview)
    {RiderPreview->SetRider(ForbiddenRiders::Selected(this));RiderPreview->SetPreviewVisible(true);}
    bCharacterSelectionOpen=true;
}
bool AMVPArenaGameMode::SelectRider(FName ID)
{
    if(!bInCodex || !ForbiddenRiders::Find(ID)) return false;
    if(!RiderPreview) OpenRiderSelection();
    if(!RiderPreview || !RiderPreview->SetRider(ID)) {DeckMessage=TEXT("This rider's 3D assets are unavailable. Your current rider is unchanged.");return false;}
    const bool Saved=ForbiddenRiders::Select(this,ID);
    if(auto* Player=GetArenaPlayer()) Player->ApplySelectedRider();
    DeckMessage=Saved ? TEXT("Rider saved. Drag the model to inspect it, or preview an animation below.") : TEXT("Rider selected, but the profile could not be saved.");
    NeonMenuAudio::Input(this);return true;
}
void AMVPArenaGameMode::ContinueToDeck()
{
    if(!bInCodex) return;
    bCharacterSelectionOpen=false;if(RiderPreview) RiderPreview->SetPreviewVisible(false);
    NeonMenuAudio::Input(this);
}
void AMVPHUD::DrawRiderSelection(AMVPArenaGameMode* G)
{
    if(!G->RiderPreview) G->OpenRiderSelection();
    auto* Preview=G->RiderPreview.Get();
    if(Preview) Preview->SetPreviewVisible(true);
    const auto* D=ForbiddenRiders::Find(ForbiddenRiders::Selected(this));if(!D) return;
    const FLinearColor Cyan=InterfaceAccent,White(.88f,.95f,1),Muted(.48f,.66f,.77f),Pink(1,.04f,.5f);
    DrawRect(FLinearColor(.004f,.008f,.02f),0,0,Canvas->SizeX,Canvas->SizeY);
    if(BackdropMaterial) DrawMaterialSimple(BackdropMaterial,OffsetX,OffsetY,1600*UIScale,900*UIScale);
    Glass(TEXT("RiderHeader"),50,40,1500,105,Cyan);
    NeonTitle(ForbiddenCode::DisplayTitle,80,58,1.4f,White);Text(TEXT("CHOOSE YOUR RIDER / FORBIDDEN SIGNALS"),83,111,.72f,Muted);
    auto Button=[&](FName Key,const FString& Label,float X,float Y,float W,float H,FLinearColor Color,bool Selected=false)
    {const float Over=Hover(Key,X,Y,W,H);Glass(Key,X,Y,W,H,Color,Over,Selected,2);Text(Label,X+15,Y+H*.30f,.70f,Color);};
    Button(TEXT("HowToPlay"),TEXT("HOW TO PLAY / F1"),80,145,255,34,Cyan);
    Button(TEXT("MenuSettings"),TEXT("SETTINGS / ESC"),350,145,255,34,Cyan);
    Button(TEXT("CloseGame"),TEXT("CLOSE GAME"),620,145,205,34,Pink);
    Button(TEXT("RiderScores"),TEXT("LEADERBOARD [L]"),850,145,255,34,Cyan);
    Button(TEXT("RiderDeckTab"),TEXT("DECK & PROGRAMS >"),1190,145,320,34,Cyan);
    Text(TEXT("FULL 3D PREVIEW"),85,198,.7f,Muted);Text(TEXT("SELECT RIDER"),855,198,.7f,Cyan);
    Glass(TEXT("RiderStudio"),75,225,715,540,D->Color);
    if(Preview && Preview->IsReady() && Preview->GetImage())
        DrawTexture(Preview->GetImage(),OffsetX+90*UIScale,OffsetY+225*UIScale,675*UIScale,540*UIScale,0,0,1,1,FLinearColor::White,BLEND_Opaque);
    else Text(TEXT("Preparing the 3D turntable..."),180,455,.85f,Muted);
    if(Preview)
    {
        const bool Held=PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton);
        if(bDraggingRider) {if(!Held) bDraggingRider=false;else {Preview->RotateBy((Cursor.X-PreviousRiderMouseX)*.45f);PreviousRiderMouseX=Cursor.X;}}
        Text(FString::Printf(TEXT("DRAG TO ROTATE / %03d degrees"),FMath::RoundToInt(Preview->GetYaw())),110,742,.62f,Muted);
    }
    const auto& Riders=ForbiddenRiders::All();
    for(int32 I=0;I<Riders.Num();++I)
    {
        const auto& R=Riders[I];const float Y=225+I*59;
        const FName Key=*FString::Printf(TEXT("Rider_%d"),I);const float Over=Hover(Key,850,Y,660,51);
        Glass(Key,850,Y,660,51,R.Color,Over,R.ID==D->ID,1);
        Text(R.Name,873,Y+9,.9f,R.ID==D->ID ? R.Color : White);Text(R.Role,1160,Y+14,.63f,Muted);
    }
    NeonTitle(D->Name+TEXT(" / ")+D->Role,855,539,.88f,D->Color);
    auto Wrapped=[&](const FString& Copy,float Y,int32 Limit,float Size,FLinearColor Color)
    {
        TArray<FString> Words;Copy.ParseIntoArray(Words,TEXT(" "),true);FString Line;int32 Row=0;
        for(const auto& W:Words) {if(Line.Len()+W.Len()>Limit && !Line.IsEmpty()) {Text(Line,855,Y+Row++*18,Size,Color);Line.Empty();}Line+=Line.IsEmpty() ? W : TEXT(" ")+W;}
        if(!Line.IsEmpty()) Text(Line,855,Y+Row*18,Size,Color);
    };
    Wrapped(D->Description,570,72,.66f,Muted);Wrapped(D->PerkA,615,74,.65f,White);Wrapped(D->PerkB,661,74,.65f,White);Wrapped(D->Tradeoff,707,74,.64f,D->Color);
    const TCHAR* Clips[]={TEXT("IDLE A"),TEXT("IDLE B"),TEXT("ATTACK A"),TEXT("ATTACK B")};
    for(int32 I=0;I<4;++I) Button(*FString::Printf(TEXT("RiderClip_%d"),I),Clips[I],80+I*150,778,142,38,D->Color,Preview && Preview->GetClip()==I);
    Button(TEXT("RiderSpin"),Preview && Preview->AutoRotate ? TEXT("AUTO: ON") : TEXT("AUTO: OFF"),680,778,120,38,Cyan);
    Button(TEXT("RiderReset"),TEXT("RESET VIEW"),850,773,220,43,Cyan);
    Button(TEXT("RiderContinue"),TEXT("CONTINUE TO DECK >>"),1100,765,410,53,D->Color);
    DrawMusicPlayer(G);
}
bool AMVPHUD::ClickRiderSelection(float X,float Y,AMVPArenaGameMode* G)
{
    if(ClickMusicPlayer(X,Y,G)) return true;
    auto In=[&](float L,float T,float W,float H){return X>=L && X<=L+W && Y>=T && Y<=T+H;};
    auto* PC=Cast<AMVPController>(PlayerOwner);auto* P=G->RiderPreview.Get();
    if(In(80,145,255,34)) {if(PC) PC->OpenHelp();return true;}
    if(In(350,145,255,34)) {if(PC) PC->ToggleSettings();return true;}
    if(In(620,145,205,34)) {if(PC) PC->CloseGame();return true;}
    if(In(850,145,255,34)) {if(PC) PC->OpenLeaderboard();return true;}
    if(In(1190,145,320,34) || In(1100,765,410,53)) {G->ContinueToDeck();return true;}
    for(int32 I=0;I<ForbiddenRiders::All().Num();++I) if(In(850,225+I*59,660,51)) {G->SelectRider(ForbiddenRiders::All()[I].ID);return true;}
    if(P)
    {
        if(In(75,225,715,510)) {bDraggingRider=true;PreviousRiderMouseX=X;return true;}
        for(int32 I=0;I<4;++I) if(In(80+I*150,778,142,38)) {NeonMenuAudio::Input(this);P->SetClip(I);return true;}
        if(In(680,778,120,38)) {NeonMenuAudio::Input(this);P->AutoRotate=!P->AutoRotate;return true;}
        if(In(850,773,220,43)) {NeonMenuAudio::Input(this);P->ResetView();return true;}
    }
    return false;
}
