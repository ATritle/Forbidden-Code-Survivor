#include "MVPGame.h"
#include "ForbiddenCodeBranding.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "NeonAudioDirector.h"
#include "NeonMenuAudio.h"
#include "NeonMusicPlayerLayout.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "Engine/Font.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"

void AMVPHUD::PrepareNeonInterface()
{
    NeonMenuAudio::BeginPointerFrame(this);
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
    if(const auto* PC=Cast<AMVPController>(PlayerOwner); PC && (PC->IsSettingsOpen() || PC->HelpWidget || PC->ScoreWidget || PC->IsChallengeOpen())) Cursor=FVector2D(-100,-100);
    if(const auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));G && G->DecisionWidget) Cursor=FVector2D(-100,-100);
}
float AMVPHUD::Hover(FName Key,float X,float Y,float W,float H)
{
    const bool bCovered=bMusicPlaylistOpen && !Key.ToString().StartsWith(TEXT("Music")) && NeonMusicLayout::ContainsPlaylist(Cursor.X,Cursor.Y);
    const bool bOver=!bCovered && Cursor.X>=X && Cursor.X<=X+W && Cursor.Y>=Y && Cursor.Y<=Y+H;
    if(bOver) if(const auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));G && G->bInCodex) NeonMenuAudio::Hover(this,Key);
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
    DrawRiderSelection(Game);
}
