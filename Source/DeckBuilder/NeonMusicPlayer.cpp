#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "NeonMenuAudio.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "Kismet/GameplayStatics.h"
namespace
{
    constexpr int32 VisibleRows=4;
    bool Within(float X,float Y,float L,float T,float W,float H) {return X>=L && X<=L+W && Y>=T && Y<=T+H;}
    FString Clock(float Seconds) {const int32 S=FMath::Max(0,FMath::FloorToInt(Seconds));return FString::Printf(TEXT("%d:%02d"),S/60,S%60);}
    FVector4 Pixels(float X,float Y,float W,float H) {return FVector4(X/1774.f,Y/887.f,W/1774.f,H/887.f);}
    const FVector4 Sprites[]={Pixels(148,185,194,190),Pixels(523,135,285,285),Pixels(967,135,288,284),Pixels(1436,187,196,186),
        Pixels(125,571,240,200),Pixels(548,541,246,248),Pixels(847,552,519,228),Pixels(1448,583,199,162)};
    const FName Covers[]={TEXT("CoverAlex"),TEXT("CoverHuman"),TEXT("CoverMonume"),TEXT("CoverMountain")};
}
void AMVPHUD::MusicGraphic(FName Name,float X,float Y,float W,float H,FLinearColor Tint,FVector4 UV)
{
    UTexture* Texture=MusicGraphics.FindRef(Name);
    if(!Texture)
    {
        Texture=LoadObject<UTexture>(nullptr,*FString::Printf(TEXT("/Game/UI/SignalDock/T_%s.T_%s"),*Name.ToString(),*Name.ToString()));
        if(Texture) MusicGraphics.Add(Name,Texture);
    }
    if(Texture) DrawTexture(Texture,OffsetX+X*UIScale,OffsetY+Y*UIScale,W*UIScale,H*UIScale,UV.X,UV.Y,UV.Z,UV.W,Tint,BLEND_Translucent);
}
void AMVPHUD::DrawMusicPlayer(AMVPArenaGameMode* Game)
{
    auto* A=Game->AudioDirector.Get();if(!A || !A->IsReady()) return;
    if(Cursor.X<=-99) {bMusicPlaylistOpen=false;bDraggingMusicVolume=bDraggingMusicScroll=bDraggingMusicSeek=false;}
    const bool Held=PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton);
    if(bDraggingMusicVolume) {if(!Held) bDraggingMusicVolume=false;else A->SetMusicVolume(FMath::Clamp((Cursor.X-1082)/132,0.f,1.f));}
    if(bDraggingMusicSeek)
    {
        if(Held) MusicSeekPreview=FMath::Clamp((Cursor.X-794)/210,0.f,1.f)*A->GetMusicDuration();
        else {A->SeekMusic(MusicSeekPreview);bDraggingMusicSeek=false;}
    }
    const auto& Tracks=ANeonAudioDirector::MusicLibrary();const int32 Current=A->GetMusicTrackIndex();
    const int32 MaxScroll=FMath::Max(0,Tracks.Num()-VisibleRows);
    if(bDraggingMusicScroll)
    {if(!Held) bDraggingMusicScroll=false;else MusicScrollOffset=FMath::Clamp(FMath::RoundToInt((Cursor.Y-660)/145*MaxScroll),0,MaxScroll);}
    const FLinearColor Cyan=InterfaceAccent,Pink(1,.04f,.6f),White(.9f,.96f,1),Muted(.55f,.7f,.8f);
    MusicGraphic(TEXT("SignalFrame"),70,822,1460,78,FLinearColor::White,FVector4(26.f/2172,238.f/724,2121.f/2172,215.f/724));
    // Keep square cover art inside the frame's illuminated rim.
    MusicGraphic(Covers[Current],111,848,38,38);
    Text(Tracks[Current].Title,164,848,.78f,White);Text(Tracks[Current].Artist,164,873,.59f,Muted);
    const auto& Spectrum=A->GetMusicSpectrum();
    for(int32 I=0;I<Spectrum.Num();++I)
    {
        const float H=Spectrum[I]*32.f,X=415+I*4.7f;
        Box(X-1,884-H-2,4,H+4,FLinearColor(1,.03f,.55f,.10f));Box(X,884-H,2,FMath::Max(1.f,H),Pink);
    }
    auto Icon=[&](const TCHAR* Key,int32 Sprite,float X,float Y,float W,float H,bool Active=false)
    {
        const float Over=Hover(FName(Key),X,Y,W,H),Grow=1.f+Over*.08f;
        MusicGraphic(TEXT("SignalControls"),X-(Grow-1)*W*.5f,Y-(Grow-1)*H*.5f,W*Grow,H*Grow,
            Active ? FLinearColor(1,.7f,1) : FLinearColor::White,Sprites[Sprite]);
    };
    Icon(TEXT("MusicPrevious"),0,590,850,32,32);
    Icon(TEXT("MusicPlayPause"),A->IsMusicPaused() ? 1 : 2,648,839,54,54);
    Icon(TEXT("MusicNext"),3,728,850,32,32);
    const float Position=bDraggingMusicSeek ? MusicSeekPreview : A->GetMusicElapsed();
    Text(Clock(Position)+TEXT(" / ")+Clock(A->GetMusicDuration()),794,848,.65f,White);
    Hover(TEXT("MusicSeek"),784,846,230,43);
    Box(794,877,210,3,FLinearColor(.15f,.3f,.4f));Box(794,877,210*Position/FMath::Max(1.f,A->GetMusicDuration()),3,Pink);
    auto Knob=[&](float X,float Y,FLinearColor Color)
    {Box(X-7,Y-7,14,14,FLinearColor(Color.R,Color.G,Color.B,.10f));for(int32 J=-4;J<=4;++J){const float W=FMath::Sqrt(25.f-J*J);Box(X-W,Y+J,W*2,1,Color);}};
    Knob(794+210*Position/FMath::Max(1.f,A->GetMusicDuration()),878,Pink);
    MusicGraphic(TEXT("SignalControls"),1039,852,29,27,FLinearColor::White,Sprites[4]);
    Hover(TEXT("MusicVolume"),1072,844,152,45);
    Box(1082,865,132,4,FLinearColor(.12f,.3f,.4f));Box(1082,865,132*A->GetMusicVolume(),4,Cyan);
    Knob(1082+132*A->GetMusicVolume(),867,A->IsMusicMuted() ? Muted : Cyan);
    Icon(TEXT("MusicMute"),A->IsMusicMuted() ? 5 : 4,1240,845,42,42,A->IsMusicMuted());
    Icon(TEXT("MusicTracks"),6,1320,842,170,49,bMusicPlaylistOpen);
    if(!bMusicPlaylistOpen) return;
    Box(930,618,580,200,FLinearColor(.004f,.01f,.025f,.98f));
    Glass(TEXT("MusicPlaylist"),930,618,580,200,Cyan,0,false,1);
    Text(TEXT("PLAYLIST"),951,635,.78f,Cyan);
    Text(FString::Printf(TEXT("%d TRACKS"),Tracks.Num()),1390,639,.59f,Muted);
    for(int32 Row=0;Row<VisibleRows;++Row)
    {
        const int32 Index=MusicScrollOffset+Row;if(!Tracks.IsValidIndex(Index)) continue;
        const float Y=660+Row*37;const FName Key(*FString::Printf(TEXT("MusicTrack_%d"),Index));
        const float Over=Hover(Key,950,Y,526,34);Glass(Key,950,Y,526,34,Cyan,Over,Index==Current,0);
        MusicGraphic(Covers[Index],979,Y+3,28,28);
        if(Index==Current) MusicGraphic(TEXT("SignalControls"),955,Y+8,18,18,FLinearColor::White,Sprites[A->IsMusicPaused() ? 2 : 1]);
        Text(Tracks[Index].Title,1020,Y+10,.65f,White);Text(Tracks[Index].Artist,1300,Y+12,.54f,Muted);
    }
    Box(1488,660,3,145,FLinearColor(.12f,.3f,.4f));
    const float Thumb=145.f*FMath::Min(1.f,VisibleRows/float(Tracks.Num()));
    Box(1487,660+(145-Thumb)*(MaxScroll>0 ? float(MusicScrollOffset)/MaxScroll : 0),5,Thumb,Cyan);
}
bool AMVPHUD::ClickMusicPlayer(float X,float Y,AMVPArenaGameMode* Game)
{
    auto* A=Game->AudioDirector.Get();if(!A) return false;
    if(Within(X,Y,80,826,1430,66))
    {
        if(Within(X,Y,580,840,50,52)) {NeonMenuAudio::Input(this);A->PreviousMusic();}
        else if(Within(X,Y,640,836,70,56)) {NeonMenuAudio::Input(this);if(A->IsMusicPaused()) A->PlayMusic();else A->PauseMusic();}
        else if(Within(X,Y,718,840,50,52)) {NeonMenuAudio::Input(this);A->NextMusic();}
        else if(Within(X,Y,784,846,230,43))
        {
            MusicSeekPreview=FMath::Clamp((X-794)/210,0.f,1.f)*A->GetMusicDuration();
            bDraggingMusicSeek=PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton);
            if(!bDraggingMusicSeek) A->SeekMusic(MusicSeekPreview);
        }
        else if(Within(X,Y,1072,844,152,45)) {A->SetMusicVolume(FMath::Clamp((X-1082)/132,0.f,1.f));bDraggingMusicVolume=true;}
        else if(Within(X,Y,1234,840,54,52)) {NeonMenuAudio::Input(this);A->SetMusicMuted(!A->IsMusicMuted());}
        else if(Within(X,Y,1310,837,190,55))
        {if(bMusicPlaylistOpen) NeonMenuAudio::Exit(this);else NeonMenuAudio::Input(this);bMusicPlaylistOpen=!bMusicPlaylistOpen;MusicScrollOffset=0;}
        return true;
    }
    if(!bMusicPlaylistOpen) return false;
    const int32 MaxScroll=FMath::Max(0,ANeonAudioDirector::MusicLibrary().Num()-VisibleRows);
    if(Within(X,Y,1480,660,25,145)) {MusicScrollOffset=FMath::Clamp(FMath::RoundToInt((Y-660)/145*MaxScroll),0,MaxScroll);bDraggingMusicScroll=true;return true;}
    for(int32 Row=0;Row<VisibleRows;++Row) if(Within(X,Y,950,660+Row*37,526,34))
    {NeonMenuAudio::Input(this);A->SelectMusicTrack(MusicScrollOffset+Row);return true;}
    if(!Within(X,Y,930,618,580,200)) {NeonMenuAudio::Exit(this);bMusicPlaylistOpen=false;}
    return true;
}
void AMVPHUD::ScrollMusicPlaylist(int32 Direction)
{
    const auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));const auto* PC=Cast<AMVPController>(PlayerOwner);
    if(!G || !G->bInCodex || !bMusicPlaylistOpen || !PC || PC->HelpWidget || PC->IsSettingsOpen() || PC->ScoreWidget || G->DecisionWidget || PC->IsChallengeOpen()) return;
    float X,Y;if(!PC->GetMousePosition(X,Y) || UIScale<=0 || !Within((X-OffsetX)/UIScale,(Y-OffsetY)/UIScale,930,618,580,274)) return;
    MusicScrollOffset=FMath::Clamp(MusicScrollOffset+Direction,0,FMath::Max(0,ANeonAudioDirector::MusicLibrary().Num()-VisibleRows));
}
