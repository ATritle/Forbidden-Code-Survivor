#include "MVPGame.h"
#include "ForbiddenRiderSubsystem.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenProfile.h"
#include "ForbiddenPrograms.h"
#include "ForbiddenSpirit.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "CodexCardSelectionWidget.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "NeonMenuAudio.h"
#include "MortisTestingDirector.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AMVPArenaGameMode::ShowDecision(const FString& Title,const TArray<FString>& Choices,TFunction<void(int32)> Callback)
{
    auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0));if(!PC || Choices.IsEmpty()) return;
    if(DecisionWidget) DecisionWidget->RemoveFromParent();
    DecisionWidget=CreateWidget<UForbiddenChoiceWidget>(PC,UForbiddenChoiceWidget::StaticClass());if(!DecisionWidget) return;
    if(SelectionWidget) SelectionWidget->SetIsEnabled(false);
    PC->SetPause(true);PC->SetMenuInput(true);
    const TWeakObjectPtr<AMVPArenaGameMode> WeakThis(this);
    DecisionWidget->Configure(Title,Choices,[WeakThis,Callback=MoveTemp(Callback)](int32 Index)
    {
        auto* G=WeakThis.Get();if(!G) return;G->DecisionWidget=nullptr;
        if(Callback) Callback(Index);
        if(G->DecisionWidget) return;
        auto* Controller=Cast<AMVPController>(UGameplayStatics::GetPlayerController(G,0));if(!Controller) return;
        if(G->SelectionWidget && G->bChoosingCard)
        {
            G->SelectionWidget->SetIsEnabled(true);FInputModeGameAndUI Input;Input.SetWidgetToFocus(G->SelectionWidget->TakeWidget());Controller->SetInputMode(Input);
        }
        else {Controller->SetMenuInput(G->bInCodex || G->bRunEnded);Controller->SetPause(G->bInCodex || G->bRunEnded);}
    });
    DecisionWidget->AddToViewport(80);FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(DecisionWidget->TakeWidget());PC->SetInputMode(Input);
}
void AMVPArenaGameMode::OpenHideout()
{
    bProgramEditorFromHideout=true;
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!bInCodex || !Profile || !Profile->Data) return;
    TArray<FString> Options;
    for(int32 I=0;I<3;++I) Options.Add(FString::Printf(TEXT("DECK %d / SAVED LOADOUT%s"),I+1,I==Profile->Data->SelectedDeck ? TEXT(" / ACTIVE") : TEXT("")));
    Options.Add(TEXT("STARTING CARD UNLOCKS"));Options.Add(TEXT("RUN HISTORY / DAMAGE / CONTRACTS / RECIPES"));
    Options.Add(TEXT("BOARD & DISTRICT MODIFIER"));Options.Add(TEXT("RETURN TO CODEX"));
    ShowDecision(FString::Printf(TEXT("%s HIDEOUT / %d RIDER FRAGMENTS"),*Profile->Data->SelectedRider.ToString(),Profile->Data->CodeFragments),Options,[this,Profile](int32 I)
    {
        if(I>=0 && I<3) {SelectDeckPreset(I);OpenHideout();}
        if(I==3)
        {
            TArray<FString> Names;for(FName ID:UForbiddenProfile::ProgramIDs()) Names.Add([&](){for(UCardDataAsset* Card:GetArenaPlayer()->Deck->UnlockedCards) if(Card && Card->CardID==ID) return Card->Name.ToString();return ID.ToString();}()+(Profile->IsUnlocked(ID) ? StartingCards.Contains(ID) ? TEXT(" / STARTING") : TEXT(" / AVAILABLE STARTER") : FString::Printf(TEXT(" / %s / LOCKED / %d FRAGMENTS"),*ForbiddenPrograms::RarityName(ID),ForbiddenPrograms::UnlockCost(ID))));
            Names.Add(TEXT("BACK"));ShowDecision(TEXT("PROGRAM LIBRARY"),Names,[this](int32 Pick){if(UForbiddenProfile::ProgramIDs().IsValidIndex(Pick)) OpenProgramEditor(UForbiddenProfile::ProgramIDs()[Pick]);else OpenHideout();});
        }
        if(I==4)
        {
            TArray<FString> History;
            for(const auto& R:Profile->Data->History)
            {
                FString Detail=(R.RiderID.IsNone() ? TEXT("LEGACY RUN / RIDER UNRECORDED\n") : R.RiderID.ToString()+TEXT("\n"))+FString::Printf(TEXT("%s / %s / %d SCORE / +%d FRAGMENTS\n%.0fs / %d KILLS / %d CONTRACTS\n%s"),*R.DateUTC,R.Victory ? TEXT("VICTORY") : TEXT("DEFEAT"),R.Score,R.Currency,R.Seconds,R.Kills,R.Contracts,*R.DeathCause);
                TArray<FName> IDs;R.DamageByCard.GetKeys(IDs);IDs.Sort([&](FName A,FName B){return R.DamageByCard[A]>R.DamageByCard[B];});
                for(FName ID:IDs) Detail+=FString::Printf(TEXT("\n%s: %.0f damage"),*ID.ToString(),R.DamageByCard[ID]);
                History.Add(Detail);if(History.Num()>=8) break;
            }
            History.Add(FString::Printf(TEXT("BACK / %d EVOLUTION RECIPES DISCOVERED"),Profile->Data->UnlockedRecipes.Num()));
            ShowDecision(TEXT("RECENT RUNS / PRACTICE EXCLUDED"),History,[this](int32){OpenHideout();});
        }
        if(I==5)
        {
            ShowDecision(TEXT("HORIZONTAL UNLOCKS"),{TEXT("NEON BOARD / STANDARD DISTRICT"),Profile->Data->Victories>0 ? TEXT("SPECTRAL BOARD / BLACKOUT DISTRICT\nMint trails. More elite patrols after 03:00, +20% essence, street lights -25%.") : TEXT("SPECTRAL BOARD / BLACKOUT / WIN A RUN TO UNLOCK"),Profile->Data->UnlockedBoards.Contains("Ghost") ? TEXT("ANCESTOR BOARD / STANDARD DISTRICT\nResearch unlock: spectral wake without the district penalty.") : TEXT("ANCESTOR BOARD / RESEARCH THREE CAPTURES TO UNLOCK"),TEXT("BACK")},[this,Profile](int32 Pick)
            {
                if(Pick==0 || (Pick==1 && Profile->Data->Victories>0) || (Pick==2 && Profile->Data->UnlockedBoards.Contains("Ghost"))) {Profile->Data->Settings.Add("District",Pick==1 ? 1.f : 0.f);Profile->Data->Settings.Add("Board",float(Pick));Profile->Save();}
                OpenHideout();
            });
        }
    });
}
void AMVPArenaGameMode::OpenProgramEditor(FName ID)
{
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();auto* Player=GetArenaPlayer();
    if(!Profile || !Profile->Data || !Player || !Player->Deck) return;
    UCardDataAsset* Card=nullptr;for(UCardDataAsset* Candidate:Player->Deck->UnlockedCards) if(Candidate && Candidate->CardID==ID) Card=Candidate;
    if(!Card) return;
    const FString Back=bProgramEditorFromHideout ? TEXT("BACK TO HIDEOUT") : TEXT("BACK TO MY DECK");
    const FString Detail=FString::Printf(TEXT("%s / STARTING CARD\n%.1f DAMAGE / %.2fs COOLDOWN\n"),*Card->Name.ToString(),Card->BaseDamage,Card->Cooldown)+Card->Description.ToString();
    const int32 Cost=ForbiddenPrograms::UnlockCost(ID);
    if(!Profile->IsUnlocked(ID))
    {
        ShowDecision(Detail+FString::Printf(TEXT("\nUNLOCK FOR THIS RIDER / %s / COST %d CODE FRAGMENTS / YOU HAVE %d\nFragments belong only to the selected rider and are earned on that rider\'s runs. This card is already available in the random in-run draft. Unlocking lets this rider start with it in any of their 3 decks. It does not equip the card."),*ForbiddenPrograms::RarityName(ID),Cost,Profile->Data->CodeFragments),{FString::Printf(TEXT("UNLOCK STARTING CARD / %d FRAGMENTS"),Cost),Back},[this,Profile,ID](int32 Pick)
        {
            if(Pick==0)
            {
                if(!Profile->UnlockProgram(ID))
                {
                    ShowDecision(TEXT("STARTER NOT UNLOCKED\n")+Profile->LastError,{TEXT("BACK")},[this,ID](int32){OpenProgramEditor(ID);});return;
                }
                DeckMessage=TEXT("Starting card unlocked for this rider. Click it to add it to the selected deck.");
            }
            if(bProgramEditorFromHideout) OpenHideout();
        });return;
    }
    ShowDecision(Detail+TEXT("\nAll 16 programs are eligible for random level-up choices. Starting cards save automatically to this rider's selected deck."),{StartingCards.Contains(ID) ? TEXT("REMOVE STARTING CARD") : TEXT("ADD STARTING CARD / MAX 2"),TEXT("VIEW UPGRADE PATHS"),Back},[this,ID,Card](int32 Pick)
    {
        if(Pick==0) {ToggleStartingCard(ID);if(bProgramEditorFromHideout) OpenHideout();}
        else if(Pick==1) ShowDecision(Card->Name.ToString()+TEXT(" / IN-RUN EVOLUTIONS"),{Card->EvolutionA.ToString()+TEXT("\n")+Card->EvolutionDescriptionA.ToString(),Card->EvolutionB.ToString()+TEXT("\n")+Card->EvolutionDescriptionB.ToString(),TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});
        else if(bProgramEditorFromHideout) OpenHideout();
    });
}

float AMVPArenaGameMode::PerkDamageMultiplier() const {return CorruptPerks.Contains("Amplifier") ? 1.15f : 1.f;}
float AMVPArenaGameMode::PerkIncomingMultiplier() const {return (CorruptPerks.Contains("Amplifier") ? 1.15f : 1.f)*(CorruptPerks.Contains("Overclock") ? 1.1f : 1.f);}
void AMVPArenaGameMode::ChooseCorruptPerk()
{
    if(PendingCorruptRewards<=0 || DecisionWidget || bChoosingCard || bRunEnded) return;
    --PendingCorruptRewards;TArray<FName> IDs;TArray<FString> Names;
    if(!CorruptPerks.Contains("Amplifier")) {IDs.Add("Amplifier");Names.Add(TEXT("CORRUPT AMPLIFIER\n+15% damage / +15% incoming damage for the rest of this run."));}
    if(!CorruptPerks.Contains("Collector")) {IDs.Add("Collector");Names.Add(TEXT("GREED ROUTINE\n+25% collected essence / -10% movement speed for this run."));}
    if(!CorruptPerks.Contains("Overclock")) {IDs.Add("Overclock");Names.Add(TEXT("ILLEGAL OVERCLOCK\n15% shorter spell cooldowns / +10% incoming damage for this run."));}
    IDs.Add(NAME_None);Names.Add(TEXT("BANK THE CODE\nKeep the contract salvage and decline further corruption."));
    ShowDecision(TEXT("CONTRACT CLEARED / CHOOSE A CORRUPT PERK"),Names,[this,IDs](int32 I)
    {
        if(!IDs.IsValidIndex(I) || IDs[I].IsNone()) return;CorruptPerks.AddUnique(IDs[I]);
        if(auto* P=GetArenaPlayer())
        {
            P->Movement->MaxSpeed=CorruptPerks.Contains("Collector") ? 423.f : 470.f;
            if(const auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>();Rider && Rider->RiderID()=="Rook") P->Movement->MaxSpeed*=.9f;
            P->Deck->GlobalCooldownScale=CorruptPerks.Contains("Overclock") ? 0.85f : 1.f;
            for(ASpellBase* Spell:P->Deck->ActiveWeapons) if(Spell) Spell->SetGlobalCooldownScale(P->Deck->GlobalCooldownScale);
        }
    });
}
void AMVPArenaGameMode::OfferCapturedCode(int32 Archetype)
{
    if(bRunEnded || bInCodex || DecisionWidget) return;
    ShowDecision(TEXT("ENEMY CODE CAPTURED"),{TEXT("STEAL ROUTINE / 20 SECONDS\nA digital spirit fires assault chains or slowing disruption links based on the captured enemy. Replaces the current routine."),TEXT("RESEARCH THE CODE\nBank 4 code fragments and unlock progress for horizontal upgrades."),TEXT("LEAVE THE CODE")},[this,Archetype](int32 I)
    {
        if(I==0) {CapturedRoutine=Archetype;CapturedRoutineRemaining=20.f;if(SupportSpirit) SupportSpirit->ActivateSupport(GetArenaPlayer(),20.f);}
        if(I==1) if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>())
            Profile->RewardCapturedCode(RunRecord.RiderID,(TestingDirector && TestingDirector->IsActive()) || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")));
    });
}
