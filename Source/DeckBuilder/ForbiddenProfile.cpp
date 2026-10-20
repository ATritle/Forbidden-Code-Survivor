#include "ForbiddenProfile.h"
#include "ForbiddenRiderRoster.h"
#include "CodexLoadoutSave.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    USaveGame* LoadCheckedSave(const FString& Slot)
    {
        TArray<uint8> Bytes;
        if(!UGameplayStatics::LoadDataFromSlot(Bytes,Slot,0) || Bytes.Num()<16) return nullptr;
        // Unreal treats a non-GVAS header as a legacy archive; garbage can assert before fallback.
        if(Bytes[0]!='G' || Bytes[1]!='V' || Bytes[2]!='A' || Bytes[3]!='S') return nullptr;
        return UGameplayStatics::LoadGameFromMemory(Bytes);
    }
}

const TArray<FName>& UForbiddenProfile::ProgramIDs()
{
    static const TArray<FName> IDs={"PulseDart","PrismBurst","HaloNova","KineticCards","PhotonLance","TalismanGuard","ArcRelay","GhostLink","NeonCataclysm","OrbitRazor","RicochetSeal","StrobeRail","EchoPetals","EventHorizon","ShatterSigil","SatelliteChoir"};
    return IDs;
}

FString UForbiddenProfile::SlotName() const
{
    return FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))
        || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) ? TEXT("ForbiddenCode_Profile_v4_Automation") : TEXT("ForbiddenCode_Profile_v4");
}

namespace
{
    FForbiddenRiderLoadouts NewRiderLoadouts(FName Rider)
    {
        FForbiddenRiderLoadouts Result;
        if(Rider=="Vex") Result.UnlockedStarters={"ArcRelay","GhostLink","RicochetSeal","PulseDart"};
        else if(Rider=="Rook") Result.UnlockedStarters={"TalismanGuard","HaloNova","EventHorizon","PulseDart"};
        else if(Rider=="Zero") Result.UnlockedStarters={"PhotonLance","StrobeRail","KineticCards","PulseDart"};
        else if(Rider=="Mira") Result.UnlockedStarters={"SatelliteChoir","GhostLink","TalismanGuard","PrismBurst"};
        else Result.UnlockedStarters={"PulseDart","PrismBurst","KineticCards","HaloNova"};
        for(int32 I=0;I<3;++I)
        {
            FForbiddenDeckPreset Deck;Deck.Name=FString::Printf(TEXT("DECK %d"),I+1);
            Deck.DraftPool=UForbiddenProfile::ProgramIDs();
            Deck.Starters={Result.UnlockedStarters[0],Result.UnlockedStarters[1]};Result.Decks.Add(Deck);
        }
        return Result;
    }
}

void UForbiddenProfile::LoadSelectedRider()
{
    const auto& Rider=Data->RiderLoadouts.FindChecked(Data->SelectedRider);
    Data->UnlockedPrograms=Rider.UnlockedStarters;Data->Decks=Rider.Decks;Data->SelectedDeck=Rider.SelectedDeck;
}
void UForbiddenProfile::StoreSelectedRider()
{
    auto& Rider=Data->RiderLoadouts.FindOrAdd(Data->SelectedRider);
    Rider.UnlockedStarters=Data->UnlockedPrograms;Rider.Decks=Data->Decks;Rider.SelectedDeck=Data->SelectedDeck;
}
void UForbiddenProfile::SetDefaults()
{
    Data=NewObject<UForbiddenProfileSave>(this);
    Data->PlayerToken=FGuid::NewGuid().ToString(EGuidFormats::Digits)+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    for(const auto& Rider:ForbiddenRiders::All()) Data->RiderLoadouts.Add(Rider.ID,NewRiderLoadouts(Rider.ID));
    LoadSelectedRider();
    Data->UnlockedBoards={"Neon"};Data->UnlockedDistrictModifiers={"Standard"};
    Data->Settings={{"Flash",1.f},{"Bloom",0.7f},{"Shake",0.6f},{"DamageNumbers",1.f},{"ReducedFlashes",0.f},{"Graphics",2.f}};
    Data->Bindings={{"Boost",EKeys::LeftShift},{"Mark",EKeys::MiddleMouseButton},{"Interact",EKeys::E},{"Inspect",EKeys::Tab}};
}

void UForbiddenProfile::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); SetDefaults();
    auto* Loaded=Cast<UForbiddenProfileSave>(LoadCheckedSave(SlotName()));
    if(!Loaded || Loaded->Version!=4) Loaded=Cast<UForbiddenProfileSave>(LoadCheckedSave(SlotName()+TEXT("_Recovery")));
    if(Loaded && Loaded->Version==4) Data=Loaded;
    else
    {
        // Read the old slot once. Never write v3, so 0.4.4 remains independently revertible.
        const FString OldSlot=SlotName().Replace(TEXT("_v4"),TEXT("_v3"));
        auto* Old=Cast<UForbiddenProfileSave>(LoadCheckedSave(OldSlot));
        if(!Old || Old->Version!=3) Old=Cast<UForbiddenProfileSave>(LoadCheckedSave(OldSlot+TEXT("_Recovery")));
        if(Old && Old->Version==3)
        {
            Data=Old;Data->Version=4;Data->RiderLoadouts.Reset();
            for(const auto& Definition:ForbiddenRiders::All())
            {
                auto Rider=NewRiderLoadouts(Definition.ID);
                // Previously account-wide purchases remain available to every rider.
                for(FName ID:Data->UnlockedPrograms) if(ProgramIDs().Contains(ID)) Rider.UnlockedStarters.AddUnique(ID);
                if(Definition.ID==Data->SelectedRider) {Rider.Decks=Data->Decks;Rider.SelectedDeck=Data->SelectedDeck;}
                Data->RiderLoadouts.Add(Definition.ID,Rider);
            }
        }
        else
        {
            const FString LegacySlot=SlotName().EndsWith(TEXT("Automation")) ? TEXT("CodexDeck_Automation") : TEXT("CodexDeck");
            if(auto* Legacy=Cast<UCodexLoadoutSave>(LoadCheckedSave(LegacySlot)))
            {
                auto& Rider=Data->RiderLoadouts.FindChecked(Data->SelectedRider);TArray<FName> Starters;
                const TMap<FName,FName> Migration={{"BoneLance","PulseDart"},{"EmberFan","PrismBurst"},{"FrostNova","HaloNova"},{"StormLance","KineticCards"},{"BloodRite","PhotonLance"},{"RaiseSkeletons","TalismanGuard"}};
                for(FName ID:Legacy->StartingCards)
                {
                    if(const auto* New=Migration.Find(ID)) ID=*New;
                    if(ProgramIDs().Contains(ID) && Starters.Num()<2) {Starters.AddUnique(ID);Rider.UnlockedStarters.AddUnique(ID);}
                }
                if(!Starters.IsEmpty()) Rider.Decks[0].Starters=Starters;
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
    for(const auto& Definition:ForbiddenRiders::All())
    {
        if(!Data->RiderLoadouts.Contains(Definition.ID)) Data->RiderLoadouts.Add(Definition.ID,NewRiderLoadouts(Definition.ID));
        auto& Rider=Data->RiderLoadouts.FindChecked(Definition.ID);
        TArray<FName> Owned;
        for(FName ID:Rider.UnlockedStarters) if(ProgramIDs().Contains(ID)) Owned.AddUnique(ID);
        for(FName ID:NewRiderLoadouts(Definition.ID).UnlockedStarters) Owned.AddUnique(ID);
        Rider.UnlockedStarters=MoveTemp(Owned);
        while(Rider.Decks.Num()<3) Rider.Decks.Add(NewRiderLoadouts(Definition.ID).Decks[Rider.Decks.Num()]);
        Rider.Decks.SetNum(3);Rider.SelectedDeck=FMath::Clamp(Rider.SelectedDeck,0,2);
        for(auto& Deck:Rider.Decks)
        {
            Deck.DraftPool=ProgramIDs();TArray<FName> Starters;
            for(FName ID:Deck.Starters) if(Starters.Num()<2 && Rider.UnlockedStarters.Contains(ID)) Starters.AddUnique(ID);
            Deck.Starters=MoveTemp(Starters); // Incomplete presets are valid edits, not lost saves.
        }
    }
    LoadSelectedRider();
    if(Data->History.Num()>32) Data->History.SetNum(32);
}

bool UForbiddenProfile::Save()
{
    if(!Data) return false;
    StoreSelectedRider();
    ++Data->Revision;
    // Keep the previous complete revision. A failed write never erases the recovery copy.
    if(auto* Previous=LoadCheckedSave(SlotName())) UGameplayStatics::SaveGameToSlot(Previous,SlotName()+TEXT("_Recovery"),0);
    const bool OK=UGameplayStatics::SaveGameToSlot(Data,SlotName(),0);
    LastError=OK ? TEXT("") : TEXT("Profile could not be saved. Your old build and profile are unchanged."); return OK;
}

bool UForbiddenProfile::IsUnlocked(FName ID) const {return Data && Data->UnlockedPrograms.Contains(ID);}
FForbiddenDeckPreset UForbiddenProfile::GetDeck() const {return Data && Data->Decks.IsValidIndex(Data->SelectedDeck) ? Data->Decks[Data->SelectedDeck] : FForbiddenDeckPreset();}

bool UForbiddenProfile::ValidateDeck(const FForbiddenDeckPreset& Deck,const TArray<FName>& Unlocked,FString& Error)
{
    if(Deck.Starters.Num()>2 || TSet<FName>(Deck.Starters).Num()!=Deck.Starters.Num()) {Error=TEXT("Choose at most two different starting cards.");return false;}
    for(FName ID:Deck.Starters) if(!ProgramIDs().Contains(ID) || !Unlocked.Contains(ID)) {Error=TEXT("Unlock this starting card for the selected rider first.");return false;}
    Error.Reset();return true;
}

bool UForbiddenProfile::SaveDeck(int32 Slot,const FForbiddenDeckPreset& Deck)
{
    if(!Data || !Data->Decks.IsValidIndex(Slot) || !ValidateDeck(Deck,Data->UnlockedPrograms,LastError)) return false;
    const auto Previous=Data->Decks[Slot];const int32 PreviousSlot=Data->SelectedDeck;
    Data->Decks[Slot]=Deck;Data->Decks[Slot].DraftPool=ProgramIDs();Data->SelectedDeck=Slot;
    if(Save()) return true;
    Data->Decks[Slot]=Previous;Data->SelectedDeck=PreviousSlot;StoreSelectedRider();return false;
}

bool UForbiddenProfile::SelectRider(FName ID)
{
    if(!Data || !ForbiddenRiders::Find(ID) || !Data->RiderLoadouts.Contains(ID)) return false;
    StoreSelectedRider();const FName Previous=Data->SelectedRider;
    Data->SelectedRider=ID;LoadSelectedRider();
    if(Save()) return true;
    Data->SelectedRider=Previous;LoadSelectedRider();return false;
}

bool UForbiddenProfile::UnlockProgram(FName ID)
{
    if(!Data || !ProgramIDs().Contains(ID) || IsUnlocked(ID) || Data->CodeFragments<30) return false;
    const int32 PreviousFragments=Data->CodeFragments;
    Data->CodeFragments-=30;Data->UnlockedPrograms.Add(ID);
    if(Save()) return true;
    Data->CodeFragments=PreviousFragments;Data->UnlockedPrograms.Remove(ID);StoreSelectedRider();return false;
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
