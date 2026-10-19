#include "MVPGame.h"
#include "NeonAudioDirector.h"
#include "NeonMenuAudio.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    bool Within(float X,float Y,float L,float T,float W,float H) {return X>=L && X<=L+W && Y>=T && Y<=T+H;}
    FString Clock(float Seconds) {const int32 S=FMath::Max(0,FMath::FloorToInt(Seconds));return FString::Printf(TEXT("%d:%02d"),S/60,S%60);}
}

void AMVPHUD::DrawMusicPlayer(AMVPArenaGameMode* Game)
{
    auto* A=Game->AudioDirector.Get();if(!A || !A->IsReady()) return;
    const bool bCanInteract=Cursor.X>-99;
    if(!bCanInteract) {bMusicPlaylistOpen=false;bDraggingMusicVolume=bDraggingMusicScroll=false;}
    if(bDraggingMusicVolume)
    {
        if(!PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton)) bDraggingMusicVolume=false;
        else A->SetMusicVolume(FMath::Clamp((Cursor.X-1210)/135,0.f,1.f));
    }
    if(bDraggingMusicScroll)
    {
        if(!PlayerOwner->IsInputKeyDown(EKeys::LeftMouseButton)) bDraggingMusicScroll=false;
        else MusicScrollOffset=FMath::Clamp(FMath::RoundToInt((Cursor.Y-668)/136),0,1);
    }
    const FLinearColor Cyan=InterfaceAccent,Pink(1,.04f,.5f),White(.88f,.95f,1),Muted(.48f,.66f,.77f);
    Glass(TEXT("MusicDock"),80,850,1430,42,Cyan,0,false,2);
    Text(TEXT("SIGNAL /"),98,862,.61f,Cyan);
    const auto& Tracks=ANeonAudioDirector::MusicLibrary();const int32 Current=A->GetMusicTrackIndex();
    Text(Tracks[Current].Title.ToUpper(),181,855,.65f,White);
    Text(Tracks[Current].Artist,181,873,.51f,Muted);
    // A restrained decorative signal animation; no per-frame audio allocations.
    for(int32 I=0;I<14;++I)
    {
        const bool Active=!A->IsMusicPaused() && !A->IsMusicMuted() && A->GetMusicVolume()>0;
        const float Height=Active ? 3+FMath::Abs(FMath::Sin(float(FPlatformTime::Seconds())*(2+I%3)+I*1.7f))*16 : 3;
        Box(460+I*6,871-Height*.5f,3,Height,Pink*.8f);
    }
    auto Button=[&](const TCHAR* Key,float X,float W,const TCHAR* Caption,bool Selected=false)
    {
        const FName ID(Key);const float H=Hover(ID,X,855,W,32);Glass(ID,X,855,W,32,Cyan,H,Selected,2);
        if(*Caption) Text(Caption,X+10,864,.65f,Selected ? Pink : White);
    };
    Button(TEXT("MusicPrevious"),570,42,TEXT("|<"));
    Button(TEXT("MusicPlay"),620,62,TEXT("PLAY"),!A->IsMusicPaused());
    Button(TEXT("MusicPause"),690,72,TEXT("PAUSE"),A->IsMusicPaused());
    Button(TEXT("MusicNext"),770,42,TEXT(">|"));
    Text(Clock(A->GetMusicElapsed())+TEXT(" / ")+Clock(A->GetMusicDuration()),832,857,.57f,Muted);
    Box(832,880,245,2,FLinearColor(.08f,.2f,.27f));
    Box(832,880,245*A->GetMusicElapsed()/FMath::Max(1.f,A->GetMusicDuration()),2,Pink);
    Button(TEXT("MusicMute"),1095,100,A->IsMusicMuted() ? TEXT("UNMUTE") : TEXT("MUTE"),A->IsMusicMuted());
    const float VH=Hover(TEXT("MusicVolume"),1200,850,155,42);
    Box(1210,870,135,3,FLinearColor(.12f,.27f,.34f));Box(1210,870,135*A->GetMusicVolume(),3,A->IsMusicMuted() ? Muted : Cyan);
    Box(1207+135*A->GetMusicVolume(),864,6,15,VH>0 ? White : Cyan);
    Text(FString::Printf(TEXT("%d%%"),FMath::RoundToInt(A->GetMusicVolume()*100)),1354,863,.57f,Muted);
    Button(TEXT("MusicTracks"),1395,100,bMusicPlaylistOpen ? TEXT("TRACKS v") : TEXT("TRACKS ^"),bMusicPlaylistOpen);
    if(!bMusicPlaylistOpen) return;
    // Opaque enough to separate the popup from underlying deck controls.
    Box(880,610,630,234,FLinearColor(.004f,.01f,.024f,.98f));
    Glass(TEXT("MusicPlaylist"),880,610,630,234,Cyan,0,false,1);
    Text(TEXT("SIGNAL LIBRARY  /  4 TRACKS"),902,630,.78f,Cyan);
    Text(TEXT("SCROLL  /  SELECT TO PLAY"),902,656,.57f,Muted);
    for(int32 Row=0;Row<3;++Row)
    {
        const int32 Index=MusicScrollOffset+Row;if(!Tracks.IsValidIndex(Index)) continue;
        const float Y=682+Row*47;
        const FName Key(*FString::Printf(TEXT("MusicTrack_%d"),Index));
        const float H=Hover(Key,900,Y,575,43);Glass(Key,900,Y,575,43,Index==Current ? Pink : Cyan,H,Index==Current,2);
        Text(Index==Current ? (A->IsMusicPaused() ? TEXT("||") : TEXT(">")) : FString::Printf(TEXT("%02d"),Index+1),914,Y+14,.67f,Index==Current ? Pink : Muted);
        Text(Tracks[Index].Title,954,Y+7,.71f,White);Text(Tracks[Index].Artist,954,Y+26,.54f,Muted);
    }
    Box(1488,682,4,137,FLinearColor(.08f,.22f,.3f));Box(1487,682+MusicScrollOffset*35,6,102,Cyan);
    Text(FString::Printf(TEXT("%d-%d OF 4"),MusicScrollOffset+1,MusicScrollOffset+3),1377,635,.58f,Muted);
}

bool AMVPHUD::ClickMusicPlayer(float X,float Y,AMVPArenaGameMode* Game)
{
    auto* A=Game->AudioDirector.Get();if(!A) return false;
    if(Within(X,Y,80,850,1430,42))
    {
        if(Within(X,Y,570,855,42,32)) {NeonMenuAudio::Input(this);A->PreviousMusic();}
        else if(Within(X,Y,620,855,62,32)) {NeonMenuAudio::Input(this);A->PlayMusic();}
        else if(Within(X,Y,690,855,72,32)) {NeonMenuAudio::Input(this);A->PauseMusic();}
        else if(Within(X,Y,770,855,42,32)) {NeonMenuAudio::Input(this);A->NextMusic();}
        else if(Within(X,Y,1095,855,100,32)) {NeonMenuAudio::Input(this);A->SetMusicMuted(!A->IsMusicMuted());}
        else if(Within(X,Y,1200,850,155,42)) {A->SetMusicVolume(FMath::Clamp((X-1210)/135,0.f,1.f));bDraggingMusicVolume=true;}
        else if(Within(X,Y,1395,855,100,32))
        {
            if(bMusicPlaylistOpen) NeonMenuAudio::Exit(this);else NeonMenuAudio::Input(this);
            bMusicPlaylistOpen=!bMusicPlaylistOpen;MusicScrollOffset=FMath::Clamp(A->GetMusicTrackIndex()-1,0,1);
        }
        return true;
    }
    if(!bMusicPlaylistOpen) return false;
    if(Within(X,Y,1480,682,25,137)) {MusicScrollOffset=Y<750 ? 0 : 1;bDraggingMusicScroll=true;return true;}
    for(int32 Row=0;Row<3;++Row) if(Within(X,Y,900,682+Row*47,575,43))
    {NeonMenuAudio::Input(this);A->SelectMusicTrack(MusicScrollOffset+Row);return true;}
    if(!Within(X,Y,880,610,630,234)) {NeonMenuAudio::Exit(this);bMusicPlaylistOpen=false;}
    return true;
}

void AMVPHUD::ScrollMusicPlaylist(int32 Direction)
{
    const auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));const auto* PC=Cast<AMVPController>(PlayerOwner);
    if(!G || !G->bInCodex || !bMusicPlaylistOpen || !PC || PC->HelpWidget || PC->IsSettingsOpen() || PC->ScoreWidget || G->DecisionWidget || PC->IsChallengeOpen()) return;
    float X,Y;if(!PC->GetMousePosition(X,Y) || UIScale<=0 || !Within((X-OffsetX)/UIScale,(Y-OffsetY)/UIScale,880,610,630,282)) return;
    const int32 Next=FMath::Clamp(MusicScrollOffset+Direction,0,FMath::Max(0,ANeonAudioDirector::MusicLibrary().Num()-3));
    if(Next!=MusicScrollOffset) {MusicScrollOffset=Next;NeonMenuAudio::Navigate(this);}
}
