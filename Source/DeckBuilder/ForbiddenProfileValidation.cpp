#include "MVPGame.h"
#include "ForbiddenProfile.h"
#include "ForbiddenPrograms.h"
#include "CardDataAsset.h"
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
        if(Case==TEXT("PrepareV3") || Case==TEXT("PrepareV4"))
        {
            auto* Old=NewObject<UForbiddenProfileSave>();Old->Version=Case==TEXT("PrepareV4") ? 4 : 3;Old->Revision=42;Old->PlayerToken=FString::ChrN(64,TEXT('a'));Old->Username=TEXT("MigrationPilot");Old->CodeFragments=73;Old->Victories=2;
            Old->SelectedRider="Zero";Old->SelectedDeck=2;Old->UnlockedPrograms={"PulseDart","PrismBurst","HaloNova","KineticCards","PhotonLance","TalismanGuard","ArcRelay","NeonCataclysm","OrbitRazor"};
            FForbiddenDeckPreset Deck;Deck.DraftPool=Old->UnlockedPrograms;Deck.DraftPool.SetNum(8);Deck.Starters={"PhotonLance","OrbitRazor"};Old->Decks={Deck,Deck,Deck};
            Old->Settings.Add("Bloom",.37f);Old->Bindings.Add("Boost",EKeys::SpaceBar);FForbiddenRunRecord Record;Record.ID=TEXT("preserved-run");Record.Kills=321;Old->History.Add(Record);
                        if(Old->Version==4)
            {
                Old->RiderLoadouts=Profile->Data->RiderLoadouts;
                auto& Rider=Old->RiderLoadouts.FindChecked("Zero");Rider.UnlockedStarters=Old->UnlockedPrograms;Rider.Decks=Old->Decks;Rider.SelectedDeck=2;
            }
            Pass=UGameplayStatics::SaveGameToSlot(Old,Profile->SlotName().Replace(TEXT("_v5"),Old->Version==4 ? TEXT("_v4") : TEXT("_v3")),0);
            Detail=TEXT("Created v3 fixture with purchased card, currency, rider/preset, settings, binding and history");
        }
        else if(Case.StartsWith(TEXT("MigrationV")) || Case.StartsWith(TEXT("ReloadV")))
        {
            UForbiddenProfileSave* D=Profile->Data;
            Pass=D->Version==5 && D->CodeFragments==73 && D->Username==TEXT("MigrationPilot") && D->SelectedRider=="Zero" && D->SelectedDeck==2 && D->History.Num()==1 && D->History[0].Kills==321 && FMath::IsNearlyEqual(D->Settings.FindRef("Bloom"),.37f) && D->Bindings.FindRef("Boost")==EKeys::SpaceBar;
            Pass&=Profile->GetDeck().Starters==TArray<FName>({"PhotonLance","OrbitRazor"}) && Profile->GetDeck().DraftPool.Num()==16;
            for(const auto& R:ForbiddenRiders::All())
            {
                const auto& Wallet=D->RiderLoadouts.FindChecked(R.ID);
                Pass&=Wallet.UnlockedStarters.Contains("OrbitRazor")==(!Case.EndsWith(TEXT("4")) || R.ID=="Zero");
                Pass&=Wallet.CodeFragments==(R.ID=="Zero" ? 73 : 0);
            }
            Pass&=D->bRiderWalletsInitialized;
            Pass&=Profile->SelectRider("Mira") && D->CodeFragments==0;
            Pass&=Profile->SelectRider("Zero") && D->CodeFragments==73;
            Pass&=Profile->Save();Detail=TEXT("One-way older-profile import and v5 reload preserve purchases, currency, rider scope, presets, settings and history");
        }
        else if(Case==TEXT("Fresh"))
        {
            Pass=Profile->Data->Version==5 && Profile->Data->CodeFragments==0;
            for(const auto& R:ForbiddenRiders::All())
            {
                const auto& L=Profile->Data->RiderLoadouts.FindChecked(R.ID);
                Pass&=L.UnlockedStarters.Num()==4 && L.Decks.Num()==3 && L.CodeFragments==0;
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
        else if(Case==TEXT("Economy"))
        {
            Pass=Profile->SelectRider("NeonExorcist");
            for(const TPair<FName,int32>& Entry:TArray<TPair<FName,int32>>{{"ArcRelay",75},{"GhostLink",150},{"NeonCataclysm",300}})
            {
                Pass&=ForbiddenPrograms::UnlockCost(Entry.Key)==Entry.Value;
                Profile->Data->UnlockedPrograms.Remove(Entry.Key);Profile->Data->CodeFragments=Entry.Value-1;
                Pass&=!Profile->UnlockProgram(Entry.Key) && Profile->Data->CodeFragments==Entry.Value-1 && !Profile->IsUnlocked(Entry.Key);
                Profile->Data->CodeFragments=Entry.Value;Pass&=Profile->UnlockProgram(Entry.Key) && Profile->Data->CodeFragments==0 && Profile->IsUnlocked(Entry.Key);
                Pass&=!Profile->UnlockProgram(Entry.Key) && Profile->Data->CodeFragments==0;
            }
            Pass&=Profile->SelectRider("Mira") && !Profile->IsUnlocked("ArcRelay") && Profile->Data->CodeFragments==0;
            Pass&=Profile->SelectRider("NeonExorcist") && Profile->IsUnlocked("ArcRelay") && Profile->IsUnlocked("NeonCataclysm");
            Pass&=!Profile->UnlockProgram("MissingCard") && Profile->Data->CodeFragments==0;
            for(FName ID:UForbiddenProfile::ProgramIDs())
            {
                auto* Card=NewObject<UCardDataAsset>();Card->CardID=ID;ForbiddenPrograms::Configure(Card);
                Pass&=ForbiddenPrograms::UnlockCost(ID)==ForbiddenPrograms::CostForRarity(Card->Rarity) && ForbiddenPrograms::UnlockCost(ID)>30;
            }
            FForbiddenRunRecord Reward;Reward.RiderID="NeonExorcist";Reward.ID=TEXT("economy-idempotence");Reward.Kills=100;Reward.Contracts=2;Reward.Victory=true;
            Pass&=Profile->RecordRun(Reward,false) && Profile->Data->CodeFragments==38;
            Pass&=!Profile->RecordRun(Reward,false) && Profile->Data->CodeFragments==38;
            Reward.ID=TEXT("practice-excluded");Pass&=!Profile->RecordRun(Reward,true) && Profile->Data->CodeFragments==38;
            Reward.ID=TEXT("neon-large-reward");Reward.Kills=20000;Reward.Contracts=0;Reward.Victory=false;
            Pass&=Profile->RecordRun(Reward,false) && Profile->Data->CodeFragments==238;
            // Credit the run rider, not whichever rider the menu happens to show.
            Reward.ID=TEXT("mira-off-selection");Reward.RiderID="Mira";Reward.Kills=760;
            Pass&=Profile->RecordRun(Reward,false) && Profile->Data->CodeFragments==238;
            Pass&=Profile->RewardCapturedCode("Mira",false) && Profile->Data->CodeFragments==238;
            Pass&=Profile->SelectRider("Mira") && Profile->Data->CodeFragments==42;
            Pass&=!Profile->UnlockProgram("ArcRelay") && Profile->Data->CodeFragments==42; // Neon can afford it; Mira cannot.
            Reward.ID=TEXT("mira-own-run");Reward.Kills=660;
            Pass&=Profile->RecordRun(Reward,false) && Profile->Data->CodeFragments==75;
            Pass&=Profile->UnlockProgram("ArcRelay") && Profile->Data->CodeFragments==0;
            Pass&=Profile->RewardCapturedCode("NeonExorcist",false) && Profile->Data->CodeFragments==0;
            Pass&=!Profile->RewardCapturedCode("Mira",true) && !Profile->RewardCapturedCode("UnknownRider",false);
            Reward.ID=TEXT("unattributed-rejected");Reward.RiderID=NAME_None;Pass&=!Profile->RecordRun(Reward,false);
            Pass&=Profile->SelectRider("NeonExorcist") && Profile->Data->CodeFragments==242;
            Pass&=Profile->Data->RiderLoadouts.FindChecked("Mira").CodeFragments==0;
            Pass&=Profile->Data->History.ContainsByPredicate([](const auto& R){return R.ID==TEXT("mira-off-selection") && R.RiderID=="Mira" && R.Currency==38;});
            Pass&=Profile->Save();
            auto* Reload=Cast<UForbiddenProfileSave>(UGameplayStatics::LoadGameFromSlot(Profile->SlotName(),0));
            Pass&=Reload && Reload->Version==5 && Reload->CodeFragments==242 && Reload->RiderLoadouts.FindChecked("NeonExorcist").UnlockedStarters.Contains("NeonCataclysm") && Reload->RiderLoadouts.FindChecked("Mira").UnlockedStarters.Contains("ArcRelay") && Reload->RiderLoadouts.FindChecked("Mira").CodeFragments==0 && Reload->RiderLoadouts.FindChecked("NeonExorcist").CodeFragments==242;
            Detail=TEXT("All 16 rarities map to central costs; purchases charge only rider wallets; off-selection run/capture attribution, independent balances, cap, duplicate/practice exclusion and save reload verified");
        }
        else if(Case==TEXT("EconomyReload"))
        {
            Pass=Profile->Data->SelectedRider=="NeonExorcist" && Profile->Data->CodeFragments==242;
            Pass&=Profile->SelectRider("Mira") && Profile->Data->CodeFragments==0 && Profile->IsUnlocked("ArcRelay");
            Pass&=Profile->SelectRider("NeonExorcist") && Profile->Data->CodeFragments==242 && Profile->IsUnlocked("NeonCataclysm");
            for(FName ID:TArray<FName>{"Vex","Rook","Zero"}) Pass&=Profile->Data->RiderLoadouts.FindChecked(ID).CodeFragments==0;
            Detail=TEXT("Rider wallets, separate purchases and off-selection rewards survived a fresh process; other three wallets remain zero");
        }
        else {Pass=false;Detail=TEXT("Unknown profile test case");}
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/Profile");IFileManager::Get().MakeDirectory(*Folder,true);
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"result\":\"%s\",\"case\":\"%s\",\"detail\":\"%s\"}"),Pass ? TEXT("PROFILE_PASS") : TEXT("PROFILE_FAIL"),*Case,*Detail),*(Folder/(Case+TEXT(".json"))));
        FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
    }),2.f);
}
