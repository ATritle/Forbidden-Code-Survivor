#include "MVPGame.h"
#include "ForbiddenRiderSubsystem.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenProfile.h"
#include "ForbiddenSpirit.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "CodexCardSelectionWidget.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "NeonMenuAudio.h"

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
    Options.Add(TEXT("ALL PROGRAMS / UNLOCK CARDS / EDIT DECK"));Options.Add(TEXT("RUN HISTORY / DAMAGE / CONTRACTS / RECIPES"));
    Options.Add(TEXT("BOARD & DISTRICT MODIFIER"));Options.Add(TEXT("RETURN TO CODEX"));
    ShowDecision(FString::Printf(TEXT("HIDEOUT / %d CODE FRAGMENTS"),Profile->Data->CodeFragments),Options,[this,Profile](int32 I)
    {
        if(I>=0 && I<3) {SelectDeckPreset(I);OpenHideout();}
        if(I==3)
        {
            TArray<FString> Names;for(FName ID:UForbiddenProfile::ProgramIDs()) Names.Add([&](){for(UCardDataAsset* Card:GetArenaPlayer()->Deck->UnlockedCards) if(Card && Card->CardID==ID) return Card->Name.ToString();return ID.ToString();}()+(Profile->IsUnlocked(ID) ? DraftPool.Contains(ID) ? TEXT(" / IN THIS DECK") : TEXT(" / UNLOCKED") : TEXT(" / LOCKED / 30 FRAGMENTS")));
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
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();auto* Player=GetArenaPlayer();
    if(!Profile || !Profile->Data || !Player || !Player->Deck) return;
    UCardDataAsset* Card=nullptr;for(UCardDataAsset* Candidate:Player->Deck->UnlockedCards) if(Candidate && Candidate->CardID==ID) Card=Candidate;
    if(!Card) return;
    const int32 DeckNumber=Profile->Data->SelectedDeck+1;
    auto Name=[Player](FName Key){for(UCardDataAsset* Definition:Player->Deck->UnlockedCards) if(Definition && Definition->CardID==Key) return Definition->Name.ToString();return Key.ToString();};
    FString Detail=FString::Printf(TEXT("DECK %d / %s\n%.1f DAMAGE / %.2fs COOLDOWN\n"),DeckNumber,*Card->Name.ToString(),Card->BaseDamage,Card->Cooldown)+Card->Description.ToString();
    const FString Back=bProgramEditorFromHideout ? TEXT("BACK TO HIDEOUT") : TEXT("BACK TO MY DECK");
    if(!Profile->IsUnlocked(ID))
    {
        Detail+=FString::Printf(TEXT("\nLOCKED / COST 30 FRAGMENTS / YOU HAVE %d\nUnlocking adds this card to your collection. Add it to a deck separately."),Profile->Data->CodeFragments);
        ShowDecision(Detail,{TEXT("UNLOCK CARD / 30 FRAGMENTS"),Back},[this,Profile,ID](int32 Pick)
        {
            if(Pick==0)
            {
                if(!Profile->UnlockProgram(ID)) {ShowDecision(Profile->Data->CodeFragments<30 ? TEXT("CARD NOT UNLOCKED\nYou need 30 code fragments earned from normal runs. Testing does not earn fragments.") : TEXT("CARD NOT UNLOCKED\n")+Profile->LastError,{TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});return;}
                OpenProgramEditor(ID);
            }
            else if(bProgramEditorFromHideout) OpenHideout();
        });return;
    }
    const bool InDeck=DraftPool.Contains(ID);const int32 ExistingStarter=StartingCards.IndexOfByKey(ID);
    Detail+=InDeck ? FString::Printf(TEXT("\nIN DECK %d / ELIGIBLE FOR LEVEL-UP CHOICES"),DeckNumber) : FString::Printf(TEXT("\nNOT IN DECK %d / ADD IT BEFORE USING IT THIS RUN"),DeckNumber);
    if(ExistingStarter!=INDEX_NONE) Detail+=FString::Printf(TEXT("\nSTARTING ATTACK / SLOT %d"),ExistingStarter+1);
    ShowDecision(Detail,{
        InDeck ? TEXT("HOW TO CHANGE THIS 8-CARD DECK") : FString::Printf(TEXT("ADD TO DECK %d / REPLACE ONE CARD"),DeckNumber),
        InDeck ? TEXT("CHOOSE AS STARTING ATTACK / SLOT 1 OR 2") : TEXT("STARTING ATTACK / ADD TO DECK FIRST"),
        TEXT("VIEW UPGRADE PATHS / EARNED DURING A RUN"),Back},[this,ID,Card,Name,InDeck,DeckNumber](int32 Pick)
    {
        if(Pick==0)
        {
            if(InDeck)
            {
                ShowDecision(FString::Printf(TEXT("DECK %d / 8 CARDS + 2 STARTING ATTACKS\nThis card is already in the deck. To replace it, return to the menu, click BROWSE ALL PROGRAMS, choose another unlocked card, then ADD TO DECK.\nEach deck saves its own card pool and two starting attacks. All 3 decks initially contain the same cards until you customize them. Changing one deck does not change the others."),DeckNumber),{TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});return;
            }
            TArray<FString> Choices;
            for(FName Old:DraftPool)
            {
                FString Copy=TEXT("REPLACE ")+Name(Old)+TEXT(" WITH ")+Name(ID);
                if(StartingCards.Contains(Old)) Copy+=TEXT("\nThis is a starting attack. The new card will also start equipped.");
                Choices.Add(Copy);
            }
            Choices.Add(TEXT("CANCEL / KEEP MY DECK"));
            ShowDecision(FString::Printf(TEXT("ADD %s TO DECK %d\nChoose the card that leaves this deck. The total stays at 8. Your other saved decks are unchanged."),*Name(ID),DeckNumber),Choices,[this,ID](int32 Index)
            {
                if(DraftPool.IsValidIndex(Index))
                {
                    const auto PreviousPool=DraftPool,PreviousStarters=StartingCards;
                    const FName Old=DraftPool[Index];DraftPool[Index]=ID;
                    for(FName& Starter:StartingCards) if(Starter==Old) Starter=ID;
                    if(!SaveStartingDeck())
                    {
                        DraftPool=PreviousPool;StartingCards=PreviousStarters;
                        ShowDecision(TEXT("COULD NOT SAVE DECK\n")+DeckMessage,{TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});return;
                    }
                }
                OpenProgramEditor(ID);
            });
        }
        else if(Pick==1)
        {
            if(!DraftPool.Contains(ID))
            {
                ShowDecision(TEXT("ADD THIS CARD TO THE DECK FIRST\nChoose ADD TO DECK and replace one of the 8 cards. You can then assign it to starting slot 1 or 2."),{TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});return;
            }
            if(StartingCards.Num()!=2) return;
            TArray<FString> Choices;
            const int32 Current=StartingCards.IndexOfByKey(ID);
            for(int32 Index=0;Index<2;++Index)
                Choices.Add(FString::Printf(TEXT("STARTING SLOT %d / %s\n%s"),Index+1,*Name(StartingCards[Index]),Index==Current ? TEXT("Already equipped in this slot.") : Current!=INDEX_NONE ? TEXT("Swap the order of your two starting attacks.") : *(TEXT("Replace with ")+Name(ID))));
            Choices.Add(TEXT("CANCEL / KEEP MY STARTING ATTACKS"));
            ShowDecision(FString::Printf(TEXT("START WITH %s\nPick one of the two attacks equipped immediately when the run begins. The other 6 deck cards become available through level-up choices."),*Name(ID)),Choices,[this,ID](int32 Index)
            {
                if(Index>=0 && Index<2)
                {
                    const auto Previous=StartingCards;const int32 Current=StartingCards.IndexOfByKey(ID);
                    if(Current!=INDEX_NONE) StartingCards.Swap(Current,Index);else StartingCards[Index]=ID;
                    if(!SaveStartingDeck())
                    {
                        StartingCards=Previous;
                        ShowDecision(TEXT("COULD NOT SAVE STARTING ATTACKS\n")+DeckMessage,{TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});return;
                    }
                }
                OpenProgramEditor(ID);
            });
        }
        else if(Pick==2) ShowDecision(Card->Name.ToString()+TEXT(" / UPGRADE PATHS\nThese are chosen through level-ups during a run, not equipped here."),{Card->EvolutionA.ToString()+TEXT("\n")+Card->EvolutionDescriptionA.ToString(),Card->EvolutionB.ToString()+TEXT("\n")+Card->EvolutionDescriptionB.ToString(),TEXT("BACK TO CARD")},[this,ID](int32){OpenProgramEditor(ID);});
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
        if(I==1) if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) {Profile->Data->CodeFragments+=4;Profile->Data->Research.FindOrAdd("Captures")+=1;if(Profile->Data->Research["Captures"]>=3) Profile->Data->UnlockedBoards.AddUnique("Ghost");Profile->Save();}
    });
}
