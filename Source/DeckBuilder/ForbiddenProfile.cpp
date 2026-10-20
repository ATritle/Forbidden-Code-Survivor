#include "ForbiddenProfile.h"
#include "ForbiddenRiderRoster.h"
#include "CodexLoadoutSave.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const TArray<FName>& UForbiddenProfile::ProgramIDs()
{
    static const TArray<FName> IDs={"PulseDart","PrismBurst","HaloNova","KineticCards","PhotonLance","TalismanGuard","ArcRelay","GhostLink","NeonCataclysm","OrbitRazor","RicochetSeal","StrobeRail","EchoPetals","EventHorizon","ShatterSigil","SatelliteChoir"};
    return IDs;
}

FString UForbiddenProfile::SlotName() const
{
    return FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))
        || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) ? TEXT("ForbiddenCode_Profile_v3_Automation") : TEXT("ForbiddenCode_Profile_v3");
}

void UForbiddenProfile::SetDefaults()
{
    Data=NewObject<UForbiddenProfileSave>(this);
    Data->PlayerToken=FGuid::NewGuid().ToString(EGuidFormats::Digits)+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Data->UnlockedPrograms={"PulseDart","PrismBurst","HaloNova","KineticCards","PhotonLance","TalismanGuard","ArcRelay","NeonCataclysm"};
    FForbiddenDeckPreset Deck; Deck.DraftPool=Data->UnlockedPrograms; Deck.Starters={"PulseDart","PrismBurst"};
    Data->Decks={Deck,Deck,Deck}; Data->Decks[1].Name=TEXT("PRESET 2"); Data->Decks[2].Name=TEXT("PRESET 3");
    Data->UnlockedBoards={"Neon"}; Data->UnlockedDistrictModifiers={"Standard"};
    Data->Settings={{"Flash",1.f},{"Bloom",0.7f},{"Shake",0.6f},{"DamageNumbers",1.f},{"ReducedFlashes",0.f},{"Graphics",2.f}};
    Data->Bindings={{"Boost",EKeys::LeftShift},{"Mark",EKeys::MiddleMouseButton},{"Interact",EKeys::E},{"Inspect",EKeys::Tab}};
}

void UForbiddenProfile::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); SetDefaults();
    auto* Loaded=Cast<UForbiddenProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotName(),0));
    if(!Loaded || Loaded->Version!=3) Loaded=Cast<UForbiddenProfileSave>(UGameplayStatics::LoadGameFromSlot(SlotName()+TEXT("_Recovery"),0));
    if(Loaded && Loaded->Version==3) Data=Loaded;
    else
    {
        // One-way import. The original slot remains unchanged and usable by the baseline build.
        const FString Old=SlotName().EndsWith(TEXT("Automation")) ? TEXT("CodexDeck_Automation") : TEXT("CodexDeck");
        if(auto* Legacy=Cast<UCodexLoadoutSave>(UGameplayStatics::LoadGameFromSlot(Old,0)))
        {
            TArray<FName> Starters;
            const TMap<FName,FName> Migration={{"BoneLance","PulseDart"},{"EmberFan","PrismBurst"},{"FrostNova","HaloNova"},{"StormLance","KineticCards"},{"BloodRite","PhotonLance"},{"RaiseSkeletons","TalismanGuard"}};
            for(FName ID:Legacy->StartingCards)
            {
                if(const auto* New=Migration.Find(ID)) ID=*New;
                if(ProgramIDs().Contains(ID) && Starters.Num()<2) {Starters.AddUnique(ID);Data->UnlockedPrograms.AddUnique(ID);}
            }
            if(!Starters.IsEmpty())
            {
                if(Starters.Num()==1) Starters.AddUnique(Starters[0]=="PulseDart" ? FName("PrismBurst") : FName("PulseDart"));
                Data->Decks[0].Starters=Starters;
                for(FName ID:Starters) if(!Data->Decks[0].DraftPool.Contains(ID))
                    for(int32 I=Data->Decks[0].DraftPool.Num()-1;I>=0;--I) if(!Starters.Contains(Data->Decks[0].DraftPool[I])) {Data->Decks[0].DraftPool[I]=ID;break;}
            }
        }
    }
    Sanitize();
}

void UForbiddenProfile::Sanitize()
{
    if(!ForbiddenRiders::Find(Data->SelectedRider)) Data->SelectedRider=TEXT("NeonExorcist");
    Data->PlayerToken.ToLowerInline();
    Data->CodeFragments=FMath::Clamp(Data->CodeFragments,0,1000000);
    if(Data->PlayerToken.Len()!=64) Data->PlayerToken=FGuid::NewGuid().ToString(EGuidFormats::Digits)+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Data->UnlockedPrograms.RemoveAll([](FName ID){return !ProgramIDs().Contains(ID);});
    for(FName ID:{FName("PulseDart"),FName("PrismBurst"),FName("HaloNova"),FName("KineticCards"),FName("PhotonLance"),FName("TalismanGuard"),FName("ArcRelay"),FName("NeonCataclysm")}) Data->UnlockedPrograms.AddUnique(ID);
    Data->Decks.SetNum(3); Data->SelectedDeck=FMath::Clamp(Data->SelectedDeck,0,2);
    for(auto& Deck:Data->Decks)
    {
        FString Error;
        if(!ValidateDeck(Deck,Data->UnlockedPrograms,Error))
        {
            Deck.DraftPool.Reset(); for(int32 I=0;I<8;++I) Deck.DraftPool.Add(Data->UnlockedPrograms[I]);
            Deck.Starters={"PulseDart","PrismBurst"};
        }
    }
    if(Data->History.Num()>32) Data->History.SetNum(32);
}

bool UForbiddenProfile::Save()
{
    if(!Data) return false;
    ++Data->Revision;
    // Keep the previous complete revision. A failed write never erases the recovery copy.
    if(auto* Previous=UGameplayStatics::LoadGameFromSlot(SlotName(),0)) UGameplayStatics::SaveGameToSlot(Previous,SlotName()+TEXT("_Recovery"),0);
    const bool OK=UGameplayStatics::SaveGameToSlot(Data,SlotName(),0);
    LastError=OK ? TEXT("") : TEXT("Profile could not be saved. Your old build and profile are unchanged."); return OK;
}

bool UForbiddenProfile::IsUnlocked(FName ID) const {return Data && Data->UnlockedPrograms.Contains(ID);}
FForbiddenDeckPreset UForbiddenProfile::GetDeck() const {return Data && Data->Decks.IsValidIndex(Data->SelectedDeck) ? Data->Decks[Data->SelectedDeck] : FForbiddenDeckPreset();}

bool UForbiddenProfile::ValidateDeck(const FForbiddenDeckPreset& Deck,const TArray<FName>& Unlocked,FString& Error)
{
    if(Deck.DraftPool.Num()!=8 || TSet<FName>(Deck.DraftPool).Num()!=8) {Error=TEXT("Choose eight different unlocked programs for the draft pool.");return false;}
    for(FName ID:Deck.DraftPool) if(!ProgramIDs().Contains(ID) || !Unlocked.Contains(ID)) {Error=TEXT("Draft pool contains a locked program.");return false;}
    if(Deck.Starters.Num()!=2 || Deck.Starters[0]==Deck.Starters[1]) {Error=TEXT("Choose two different starting programs.");return false;}
    for(FName ID:Deck.Starters) if(!Deck.DraftPool.Contains(ID)) {Error=TEXT("Starting programs must be in the draft pool.");return false;}
    Error.Reset();return true;
}

bool UForbiddenProfile::SaveDeck(int32 Slot,const FForbiddenDeckPreset& Deck)
{
    if(!Data || !Data->Decks.IsValidIndex(Slot) || !ValidateDeck(Deck,Data->UnlockedPrograms,LastError)) return false;
    const auto Previous=Data->Decks[Slot];const int32 PreviousSlot=Data->SelectedDeck;
    Data->Decks[Slot]=Deck;Data->SelectedDeck=Slot;
    if(Save()) return true;
    Data->Decks[Slot]=Previous;Data->SelectedDeck=PreviousSlot;return false;
}

bool UForbiddenProfile::UnlockProgram(FName ID)
{
    if(!Data || !ProgramIDs().Contains(ID) || IsUnlocked(ID) || Data->CodeFragments<30) return false;
    const int32 PreviousFragments=Data->CodeFragments;
    Data->CodeFragments-=30;Data->UnlockedPrograms.Add(ID);
    if(Save()) return true;
    Data->CodeFragments=PreviousFragments;Data->UnlockedPrograms.Remove(ID);return false;
}

bool UForbiddenProfile::RecordRun(const FForbiddenRunRecord& Record,bool Practice)
{
    if(!Data || Practice || Record.ID.IsEmpty() || Data->History.ContainsByPredicate([&](const auto& R){return R.ID==Record.ID;})) return false;
    FForbiddenRunRecord R=Record;
    R.Currency=FMath::Clamp(R.Kills/20+R.Contracts*4+(R.Victory ? 25 : 0),0,200);
    R.Score=FMath::Max(0,R.Kills*100+FMath::FloorToInt(R.Seconds)*10+FMath::Max(0,R.Level-1)*500+R.Contracts*1500+(R.Victory ? 10000 : 0));
    Data->CodeFragments=FMath::Min(1000000,Data->CodeFragments+R.Currency);
    if(R.Victory) {++Data->Victories;Data->UnlockedBoards.AddUnique("Spectral");Data->UnlockedDistrictModifiers.AddUnique("Blackout");}
    for(FName ID:R.Evolutions) Data->UnlockedRecipes.AddUnique(ID);
    Data->History.Insert(R,0);if(Data->History.Num()>32) Data->History.SetNum(32);return Save();
}
