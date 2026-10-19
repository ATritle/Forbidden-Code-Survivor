#include "NeonMenuAudio.h"
#include "NeonAudioDirector.h"
#include "MVPGame.h"
#include "Kismet/GameplayStatics.h"
namespace
{
    ANeonAudioDirector* FindMenuAudio(const UObject* Context)
    {auto* G=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(Context));return G ? G->AudioDirector.Get() : nullptr;}
    TWeakObjectPtr<const UObject> PointerOwner;
    FName PreviousHover,CurrentHover;
}
void NeonMenuAudio::Input(const UObject* Context) {if(auto* A=FindMenuAudio(Context)) A->PlayMenuFeedback(false,false);}
void NeonMenuAudio::Exit(const UObject* Context) {if(auto* A=FindMenuAudio(Context)) A->PlayMenuFeedback(true,false);}
void NeonMenuAudio::Navigate(const UObject* Context) {if(auto* A=FindMenuAudio(Context)) A->PlayMenuFeedback(false,true);}
void NeonMenuAudio::BeginPointerFrame(const UObject* HUD)
{
    if(PointerOwner.Get()!=HUD) {PointerOwner=HUD;CurrentHover=NAME_None;}
    PreviousHover=CurrentHover;CurrentHover=NAME_None;
}
void NeonMenuAudio::Hover(const UObject* HUD,FName Key)
{CurrentHover=Key;if(Key!=PreviousHover) Navigate(HUD);}
