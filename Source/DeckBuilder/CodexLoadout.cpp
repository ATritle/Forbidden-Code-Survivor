#include "MVPGame.h"
#include "NeonChallenge.h"
#include "NeonAudioDirector.h"
#include "MortisProgressionManager.h"
#include "EngineUtils.h"
#include "CodexLoadoutSave.h"
#include "ForbiddenProfile.h"
#include "ForbiddenScoreService.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

FString AMVPArenaGameMode::DeckSaveSlot() const
{
    // Automated runs must never overwrite a player's deck.
    return FParse::Param(FCommandLine::Get(), TEXT("MVPSmoke")) ? TEXT("ForbiddenDeck_v3_Automation") : TEXT("ForbiddenDeck_v3");
}

TArray<UCardDataAsset*> AMVPArenaGameMode::GetLibraryCards() const
{
    TArray<UCardDataAsset*> Result;
    const AMVPPlayer* Player = GetArenaPlayer();
    if (!Player) return Result;
    for (UCardDataAsset* Card : Player->Deck->UnlockedCards)
        if (Card && (SchoolFilter < 0 || static_cast<int32>(Card->MagicSchool) == SchoolFilter)) Result.Add(Card);
    return Result;
}

void AMVPArenaGameMode::LoadStartingDeck()
{
    if (!bInCodex) return;
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>())
    {
        const auto Preset=Profile->GetDeck();StartingCards=Preset.Starters;DraftPool=Preset.DraftPool;
        DeckMessage=TEXT("Eight programs in the draft pool. Choose two starters; six active slots per run.");
        return;
    }
    StartingCards.Reset();
    const bool Exists = UGameplayStatics::DoesSaveGameExist(DeckSaveSlot(), 0);
    UCodexLoadoutSave* Save = Exists ? Cast<UCodexLoadoutSave>(UGameplayStatics::LoadGameFromSlot(DeckSaveSlot(), 0)) : nullptr;
    const AMVPPlayer* Player = GetArenaPlayer();
    if (Save && Save->Version == 1 && Player)
        for (FName ID : Save->StartingCards)
        {
            const TMap<FName,FName> Migration={{"BoneLance","PulseDart"},{"EmberFan","PrismBurst"},{"FrostNova","HaloNova"},{"StormLance","KineticCards"},{"BloodRite","PhotonLance"},{"RaiseSkeletons","TalismanGuard"}};
            if(const FName* Updated=Migration.Find(ID)) ID=*Updated;
            const bool Valid = Player->Deck->UnlockedCards.ContainsByPredicate([ID](const UCardDataAsset* C) { return C && C->CardID == ID && C->SpellClass; });
            if (Valid && StartingCards.Num() < 2) StartingCards.AddUnique(ID);
        }
    if (StartingCards.IsEmpty() && Cards.Num()) StartingCards.Add(Cards[0]->CardID);
    // A finite barrier alone cannot earn the first essence draft. Preserve a
    // migrated summon-only save by supplying one offensive starter alongside it.
    if(StartingCards.Num()==1 && StartingCards[0]==TEXT("TalismanGuard") && Cards.Num()) StartingCards.AddUnique(Cards[0]->CardID);
    DeckMessage = !Exists ? TEXT("Choose your starting spells. Save your deck or enter the arena.")
        : Save && Save->Version == 1 ? TEXT("Saved deck loaded. Unavailable cards are omitted.")
        : TEXT("Could not read the saved deck. A default loadout is ready.");
}

bool AMVPArenaGameMode::ToggleStartingCard(FName ID)
{
    AMVPPlayer* Player = GetArenaPlayer();
    if (!bInCodex || !Player || !Player->Deck->UnlockedCards.ContainsByPredicate([ID](const UCardDataAsset* C) { return C && C->CardID == ID && C->SpellClass; })) return false;
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && !Profile->IsUnlocked(ID)) {DeckMessage=TEXT("Unlock this program in the hideout for 30 code fragments.");return false;}
    if(!DraftPool.Contains(ID)) {DeckMessage=TEXT("Add this program to the eight-card draft pool first.");return false;}
    if (StartingCards.Contains(ID)) StartingCards.Remove(ID);
    else if (StartingCards.Num() < 2) StartingCards.Add(ID);
    else { DeckMessage = TEXT("Two starting slots are full. Remove a spell first."); if(AudioDirector) AudioDirector->PlayUI(TEXT("UI_Deny")); return false; }
    if(AudioDirector) AudioDirector->PlayUI(TEXT("UI_Click"));
    DeckMessage = TEXT("Unsaved changes. Entering the arena also saves this deck.");
    return true;
}

bool AMVPArenaGameMode::SaveStartingDeck()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark"))) return true;
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>())
    {
        if(!bInCodex) return false;
        FForbiddenDeckPreset Preset=Profile->GetDeck();Preset.Starters=StartingCards;Preset.DraftPool=DraftPool;
        const bool OK=Profile->SaveDeck(Profile->Data->SelectedDeck,Preset);
        DeckMessage=OK ? TEXT("Deck saved. Eight draft programs / two starting programs / six active slots.") : Profile->LastError;
        return OK;
    }
    if (!bInCodex || StartingCards.IsEmpty() || StartingCards.Num() > 2)
    { DeckMessage = TEXT("Choose one or two starting spells before saving."); return false; }
    if(!StartingCards.ContainsByPredicate([](FName ID){return ID!=TEXT("TalismanGuard");}))
    { DeckMessage=TEXT("Pair Talisman Guard with an energy attack to keep earning XP after its charges are spent.");return false; }
    const AMVPPlayer* Player = GetArenaPlayer();
    TSet<FName> Unique;
    for (const FName ID : StartingCards)
    {
        if (!Player || Unique.Contains(ID) || !Player->Deck->UnlockedCards.ContainsByPredicate([ID](const UCardDataAsset* C) { return C && C->CardID == ID && C->SpellClass; })) return false;
        Unique.Add(ID);
    }
    auto* Save = Cast<UCodexLoadoutSave>(UGameplayStatics::CreateSaveGameObject(UCodexLoadoutSave::StaticClass()));
    if (!Save) return false;
    Save->StartingCards = StartingCards;
    const bool Saved = UGameplayStatics::SaveGameToSlot(Save, DeckSaveSlot(), 0);
    DeckMessage = Saved ? TEXT("Deck saved. Your starting spells will be remembered between runs.") : TEXT("Could not save the deck. Check disk space and folder permissions, then retry.");
    if(AudioDirector) {if(Saved) AudioDirector->PlayMenuFeedback();else AudioDirector->PlayUI(TEXT("UI_Deny"));}
    return Saved;
}

bool AMVPArenaGameMode::StartSelectedRun()
{
    if(const auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0)); PC && (PC->HelpWidget || PC->IsSettingsOpen() || PC->ScoreWidget || DecisionWidget)) return false;
    AMVPPlayer* Player = GetArenaPlayer();
    if (!bInCodex || !bRunInitialized || !Player || !SaveStartingDeck()) return false;
    // Reuse validated offer/equip logic one card at a time; never mutate authored data.
    for (const FName ID : StartingCards)
    {
        UCardDataAsset* Card = nullptr;
        for (UCardDataAsset* C : Cards) if (C->CardID == ID) { Card = C; break; }
        if (Player->Deck->FindOwnedSpell(ID)) continue;
        Player->Deck->EquippedDeck = {Card};
        Player->Deck->DiscardHand();
        Player->Deck->DrawLevelUpHand();
        if (!Player->Deck->AddSelectedCardSpell(Card))
        { DeckMessage = TEXT("Spell could not be equipped. Retry entering the arena."); return false; }
    }
    Player->Deck->EquippedDeck.Reset();
    for(UCardDataAsset* Card:Player->Deck->UnlockedCards) if(Card && DraftPool.Contains(Card->CardID)) Player->Deck->EquippedDeck.Add(Card);
    bInCodex = false;
    RunRecord.ID=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    if(auto* Scores=GetGameInstance()->GetSubsystem<UForbiddenScoreService>()) Scores->BeginRun();
    if(Player->Challenges) Player->Challenges->BeginRun();
    if (auto* PC = Cast<AMVPController>(Player->GetController()))
    {
        PC->SetPause(false);
        PC->SetMenuInput(false);
    }
    Spawner->StartWaves();
    if(AudioDirector) AudioDirector->UpdateMusicState();
    for (TActorIterator<AMortisProgressionManager> It(GetWorld()); It; ++It)
    {
        It->CurrentWaveIndex = FMath::Max(1, UGameplayStatics::GetIntOption(OptionsString, TEXT("Wave"), 1));
        It->BeginSession(Spawner);
    }
    int32 Stress=0;
    if (FParse::Value(FCommandLine::Get(),TEXT("StressEnemies="),Stress))
    {
        const int32 Original=Spawner->EnemiesPerWave;
        Spawner->EnemiesPerWave=FMath::Clamp(Stress,1,1000);
        Spawner->SpawnWave();
        Spawner->EnemiesPerWave=Original;
    }
    return true;
}
