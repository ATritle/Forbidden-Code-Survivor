#include "MVPGame.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenProfile.h"
#include "ForbiddenSpirit.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "CodexCardSelectionWidget.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"

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
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!bInCodex || !Profile || !Profile->Data) return;
    TArray<FString> Options;
    for(int32 I=0;I<3;++I) Options.Add(FString::Printf(TEXT("PRESET %d / %s%s"),I+1,*Profile->Data->Decks[I].Name,I==Profile->Data->SelectedDeck ? TEXT(" / ACTIVE") : TEXT("")));
    Options.Add(TEXT("PROGRAMS / UNLOCKS / EIGHT-CARD DRAFT POOL"));Options.Add(TEXT("RUN HISTORY / DAMAGE / CONTRACTS / RECIPES"));
    Options.Add(TEXT("BOARD & DISTRICT MODIFIER"));Options.Add(TEXT("RETURN TO CODEX"));
    ShowDecision(FString::Printf(TEXT("HIDEOUT / %d CODE FRAGMENTS"),Profile->Data->CodeFragments),Options,[this,Profile](int32 I)
    {
        if(I<3) {Profile->Data->SelectedDeck=I;Profile->Save();LoadStartingDeck();OpenHideout();}
        if(I==3)
        {
            TArray<FString> Names;for(FName ID:UForbiddenProfile::ProgramIDs()) Names.Add(ID.ToString()+(Profile->IsUnlocked(ID) ? DraftPool.Contains(ID) ? TEXT(" / IN DRAFT POOL") : TEXT(" / UNLOCKED") : TEXT(" / LOCKED / 30 FRAGMENTS")));
            Names.Add(TEXT("BACK"));ShowDecision(TEXT("PROGRAM LIBRARY"),Names,[this](int32 Pick){if(UForbiddenProfile::ProgramIDs().IsValidIndex(Pick)) OpenProgramEditor(UForbiddenProfile::ProgramIDs()[Pick]);else OpenHideout();});
        }
        if(I==4)
        {
            TArray<FString> History;
            for(const auto& R:Profile->Data->History)
            {
                FString Detail=FString::Printf(TEXT("%s / %s / %d SCORE / +%d FRAGMENTS\n%.0fs / %d KILLS / %d CONTRACTS\n%s"),*R.DateUTC,R.Victory ? TEXT("VICTORY") : TEXT("DEFEAT"),R.Score,R.Currency,R.Seconds,R.Kills,R.Contracts,*R.DeathCause);
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
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile) return;
    UCardDataAsset* Card=nullptr;for(UCardDataAsset* C:GetArenaPlayer()->Deck->UnlockedCards) if(C && C->CardID==ID) Card=C;if(!Card) return;
    const FString Detail=Card->Name.ToString()+FString::Printf(TEXT(" / %.1f DMG / %.2fs\n"),Card->BaseDamage,Card->Cooldown)+Card->Description.ToString();
    if(!Profile->IsUnlocked(ID))
    {
        ShowDecision(Detail,{TEXT("UNLOCK / 30 CODE FRAGMENTS"),TEXT("BACK")},[this,Profile,ID](int32 I){if(I==0) {Profile->UnlockProgram(ID);OpenProgramEditor(ID);}else OpenHideout();});return;
    }
    ShowDecision(Detail,{DraftPool.Contains(ID) ? TEXT("IN DRAFT POOL / SWAP ANOTHER PROGRAM TO CHANGE IT") : TEXT("ADD TO DRAFT POOL / REPLACE ONE OF EIGHT"),TEXT("SET AS STARTER / TWO SLOTS"),TEXT("EVOLUTIONS / DISCOVER STRUCTURAL BRANCHES"),TEXT("BACK")},[this,ID,Card](int32 I)
    {
        if(I==0 && !DraftPool.Contains(ID))
        {
            TArray<FString> Names;for(FName Old:DraftPool) Names.Add(TEXT("REPLACE ")+Old.ToString());Names.Add(TEXT("CANCEL"));
            ShowDecision(TEXT("EIGHT-CARD DRAFT POOL"),Names,[this,ID](int32 Pick)
            {
                if(DraftPool.IsValidIndex(Pick))
                {
                    const FName Old=DraftPool[Pick];DraftPool[Pick]=ID;
                    for(FName& Starter:StartingCards) if(Starter==Old) Starter=ID;
                    SaveStartingDeck();
                }OpenProgramEditor(ID);
            });
        }
        else if(I==1)
        {
            if(!DraftPool.Contains(ID)) {DeckMessage=TEXT("Add this program to the draft pool first.");OpenProgramEditor(ID);return;}
            ShowDecision(TEXT("REPLACE A STARTER"),{StartingCards[0].ToString(),StartingCards[1].ToString(),TEXT("CANCEL")},[this,ID](int32 Pick)
            {if(Pick<2 && !StartingCards.Contains(ID)) {StartingCards[Pick]=ID;SaveStartingDeck();}OpenProgramEditor(ID);});
        }
        else if(I==2) ShowDecision(Card->Name.ToString(),{Card->EvolutionA.ToString()+TEXT("\n")+Card->EvolutionDescriptionA.ToString(),Card->EvolutionB.ToString()+TEXT("\n")+Card->EvolutionDescriptionB.ToString(),TEXT("BACK")},[this,ID](int32){OpenProgramEditor(ID);});
        else OpenHideout();
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
        if(I==1) if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) {Profile->Data->CodeFragments+=4;Profile->Data->Research.FindOrAdd("Captures")+=1;if(Profile->Data->Research["Captures"]>=3) Profile->Data->UnlockedBoards.AddUnique("Ghost");Profile->Save();}
    });
}
