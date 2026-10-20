#include "MVPGame.h"
#include "ForbiddenProfile.h"
#include "ForbiddenRiderRoster.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

/** Isolated save fixtures exercise the real loader in subsequent fresh processes. */
void InstallProfileValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("ProfileSmoke"))) return;
    const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
    {
        auto* C=Weak.Get();if(!C) return false;
        auto* Profile=C->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return true;
        FString Case;FParse::Value(FCommandLine::Get(),TEXT("ProfileCase="),Case);
        bool Pass=true;FString Detail;
        if(Case==TEXT("PrepareV3"))
        {
            auto* Old=NewObject<UForbiddenProfileSave>();Old->Version=3;Old->Revision=42;Old->PlayerToken=FString::ChrN(64,TEXT('a'));Old->Username=TEXT("MigrationPilot");Old->CodeFragments=73;Old->Victories=2;
            Old->SelectedRider="Zero";Old->SelectedDeck=2;Old->UnlockedPrograms={"PulseDart","PrismBurst","HaloNova","KineticCards","PhotonLance","TalismanGuard","ArcRelay","NeonCataclysm","OrbitRazor"};
            FForbiddenDeckPreset Deck;Deck.DraftPool=Old->UnlockedPrograms;Deck.DraftPool.SetNum(8);Deck.Starters={"PhotonLance","OrbitRazor"};Old->Decks={Deck,Deck,Deck};
            Old->Settings.Add("Bloom",.37f);Old->Bindings.Add("Boost",EKeys::SpaceBar);FForbiddenRunRecord Record;Record.ID=TEXT("preserved-run");Record.Kills=321;Old->History.Add(Record);
            Pass=UGameplayStatics::SaveGameToSlot(Old,Profile->SlotName().Replace(TEXT("_v4"),TEXT("_v3")),0);
            Detail=TEXT("Created v3 fixture with purchased card, currency, rider/preset, settings, binding and history");
        }
        else if(Case==TEXT("Migration") || Case==TEXT("Reload"))
        {
            UForbiddenProfileSave* D=Profile->Data;
            Pass=D->Version==4 && D->CodeFragments==73 && D->Username==TEXT("MigrationPilot") && D->SelectedRider=="Zero" && D->SelectedDeck==2 && D->History.Num()==1 && D->History[0].Kills==321 && FMath::IsNearlyEqual(D->Settings.FindRef("Bloom"),.37f) && D->Bindings.FindRef("Boost")==EKeys::SpaceBar;
            Pass&=Profile->GetDeck().Starters==TArray<FName>({"PhotonLance","OrbitRazor"}) && Profile->GetDeck().DraftPool.Num()==16;
            for(const auto& R:ForbiddenRiders::All()) Pass&=D->RiderLoadouts.FindChecked(R.ID).UnlockedStarters.Contains("OrbitRazor");
            Pass&=Profile->Save();Detail=TEXT("One-way v3 import and fresh v4 reload preserved prior purchases, all progress, starters and preferences");
        }
        else if(Case==TEXT("Fresh"))
        {
            Pass=Profile->Data->Version==4 && Profile->Data->CodeFragments==0;
            for(const auto& R:ForbiddenRiders::All())
            {
                const auto& L=Profile->Data->RiderLoadouts.FindChecked(R.ID);
                Pass&=L.UnlockedStarters.Num()==4 && L.Decks.Num()==3;
                for(const auto& Deck:L.Decks) Pass&=Deck.Starters.Num()==2 && Deck.DraftPool.Num()==16;
            }
            auto Deck=Profile->GetDeck();Deck.Starters.Reset();Pass&=Profile->SaveDeck(1,Deck);
            Pass&=Profile->SelectRider("Mira");Pass&=Profile->Data->SelectedDeck==0;
            Pass&=Profile->SelectRider("NeonExorcist");Pass&=Profile->Data->SelectedDeck==1 && Profile->GetDeck().Starters.IsEmpty();
            Pass&=Profile->Save();Detail=TEXT("Five fresh rider collections each have four choices and three decks; incomplete autosaves and rider isolation verified");
        }
        else if(Case==TEXT("FreshReload"))
        {
            Pass=Profile->Data->SelectedRider=="NeonExorcist" && Profile->Data->SelectedDeck==1 && Profile->GetDeck().Starters.IsEmpty();
            Detail=TEXT("Intentionally empty preset survived process restart without automatic refill");
        }
        else {Pass=false;Detail=TEXT("Unknown profile test case");}
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/Profile");IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"result\":\"%s\",\"case\":\"%s\",\"detail\":\"%s\"}"),Pass ? TEXT("PROFILE_PASS") : TEXT("PROFILE_FAIL"),*Case,*Detail),*(Folder/(Case+TEXT(".json"))));
        FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
    }),2.f);
}
