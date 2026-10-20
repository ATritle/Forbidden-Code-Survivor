#include "MVPGame.h"
#include "ForbiddenCombatSubsystem.h"
#include "ForbiddenEnemyBrain.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenProfile.h"
#include "ForbiddenRiderAnimInstance.h"
#include "NeonArsenal.h"
#include "NeonSpellImpactDirector.h"
#include "NecromancyVisualComponent.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "MortisTerrain.h"
#include "NeonChallenge.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    struct FOverhaulQA
    {
        int32 Phase=0,Case=0,Route=0;
        double Since=FPlatformTime::Seconds();
        TWeakObjectPtr<AMVPEnemy> Probe;
        TArray<TSharedPtr<FJsonValue>> Cases;
        TArray<FName> CurrentIDs;
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
        FString Directory=FPaths::ProjectSavedDir()/TEXT("Review/OverhaulFunctional");
        void Advance(int32 Next) {Phase=Next;Since=FPlatformTime::Seconds();}
        void Capture(const TCHAR* Name) {FScreenshotRequest::RequestScreenshot(Directory/FString(Name),true,false);}
        bool End(bool Pass,const FString& Why)
        {
            IFileManager::Get().MakeDirectory(*Directory,true);Report->SetStringField(TEXT("result"),Pass ? TEXT("OVERHAUL_FUNCTIONAL_PASS") : TEXT("OVERHAUL_FUNCTIONAL_FAIL"));Report->SetStringField(TEXT("detail"),Why);Report->SetArrayField(TEXT("cards"),Cases);
            FString JSON;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&JSON));FFileHelper::SaveStringToFile(JSON,*(Directory/TEXT("report.json")));
            UE_LOG(LogTemp,Display,TEXT("%s phase=%d %s"),Pass ? TEXT("OVERHAUL_FUNCTIONAL_PASS") : TEXT("OVERHAUL_FUNCTIONAL_FAIL"),Phase,*Why);FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        }
        void ClearCombat(AMVPArenaGameMode* G)
        {
            auto* P=G->GetArenaPlayer();P->Deck->ResetActiveSpells();P->ClearTalismans();G->RunRecord.DamageByCard.Reset();
            const auto Active=G->Spawner->GetActiveEnemies().Array();for(APooledEnemy* E:Active) if(E) E->ReturnToPool();
            for(TActorIterator<AMortisProjectileBase> It(G->GetWorld());It;++It) It->Deactivate();
            if(auto* C=G->GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()) C->Clear();
            if(G->ArsenalDirector) G->ArsenalDirector->Destroy();G->ArsenalDirector=G->GetWorld()->SpawnActor<ANeonArsenalDirector>();
        }
        bool SetCase(AMVPArenaGameMode* G)
        {
            ClearCombat(G);auto* P=G->GetArenaPlayer();P->SetActorLocation(FVector(0,0,40));CurrentIDs.Reset();
            const int32 Group=Case%3,Variant=Case/3;
            const auto& IDs=UForbiddenProfile::ProgramIDs();
            for(int32 I=Group*6;I<FMath::Min(16,Group*6+6);++I)
            {
                UCardDataAsset* Card=nullptr;for(UCardDataAsset* C:P->Deck->UnlockedCards) if(C && C->CardID==IDs[I]) Card=C;if(!Card) return false;
                P->Deck->EquippedDeck={Card};P->Deck->DiscardHand();P->Deck->DrawLevelUpHand();ASpellBase* Spell=P->Deck->AddSelectedCardSpell(Card);if(!Spell) return false;
                if(Variant>0)
                {
                    const auto Rank=FGameplayTag::RequestGameplayTag(TEXT("Mutation.Program.Rank"));
                    if(!Spell->ApplyMutation(Rank,1) || !Spell->ApplyMutation(Rank,1) || !Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(Variant==1 ? TEXT("Mutation.Program.EvolutionA") : TEXT("Mutation.Program.EvolutionB")),1)) return false;
                }
                CurrentIDs.Add(Card->CardID);
            }
            P->Deck->EquippedDeck=P->Deck->UnlockedCards;
            for(int32 I=0;I<32;++I)
            {
                const float A=I*2*PI/8;const FVector Position=I<8 ? FVector(FMath::Cos(A)*110.f,FMath::Sin(A)*110.f,55) : FVector(350+((I-8)/6)*180,((I-8)%6-2.5f)*170,55);
                auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Position));if(!E) return false;
                E->ApplyEnemyArchetype(I<8 ? 0 : I%4==0 ? 4 : I%7==0 ? 6 : 0);E->MaxHealth=E->Health=1000000.f;E->bTrainingTarget=true;E->SetActorTickEnabled(false);E->SetActorLocation(Position);
            }
            Since=FPlatformTime::Seconds();return true;
        }
        bool Tick(AMVPController* PC)
        {
            auto* G=Cast<AMVPArenaGameMode>(PC->GetWorld()->GetAuthGameMode());auto* P=G ? G->GetArenaPlayer() : nullptr;if(!G || !P || !G->Spawner) return true;
            const double Age=FPlatformTime::Seconds()-Since;
            if(Age>35) return End(false,TEXT("Phase timeout"));
            if(Phase==0)
            {
                auto* Art=P->FindComponentByClass<UNecromancyVisualComponent>();if(!Art || !Art->IsArtReady() || P->Deck->UnlockedCards.Num()!=16) return true;
                if(!Art->UsesBlendedRider() || !Cast<UForbiddenRiderAnimInstance>(Art->GetAnimInstance())) return End(false,TEXT("Blended rider blueprint is not active"));
                auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile->Data->UnlockedPrograms=UForbiddenProfile::ProgramIDs();
                IFileManager::Get().MakeDirectory(*Directory,true);Advance(1);
            }
            else if(Phase==1 && Age>2)
            {Capture(TEXT("01-Menu.png"));Advance(2);}
            else if(Phase==2 && Age>0.5)
            {
                if(!G->StartSelectedRun()) return End(false,TEXT("Curated starting deck failed"));
                G->Spawner->StopWaves();G->bBroadcastBossSpawned=true;P->bBenchmarkInvulnerable=true;
                if(!SetCase(G)) return End(false,TEXT("Could not prepare base spell case"));Advance(3);
            }
            else if(Phase==3 && Age>5.2)
            {
                for(FName ID:CurrentIDs)
                {
                    const float Damage=G->RunRecord.DamageByCard.FindRef(ID);
                    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("card"),ID.ToString());Row->SetNumberField(TEXT("evolution"),Case/3);Row->SetNumberField(TEXT("damage"),Damage);Cases.Add(MakeShared<FJsonValueObject>(Row));
                    if(Damage<=0) return End(false,TEXT("No confirmed damage for ")+ID.ToString()+FString::Printf(TEXT(" evolution %d"),Case/3));
                }
                if(G->ArsenalDirector->DroppedGameplay>0) return End(false,TEXT("Gameplay slots exhausted during evolved casting"));
                if(Case==4) Capture(TEXT("02-Evolved-Combat.png"));
                ++Case;if(Case<9) {if(!SetCase(G)) return End(false,TEXT("Evolution setup failed"));}
                else {ClearCombat(G);Advance(4);}
            }
            else if(Phase==4)
            {
                static const FVector Starts[]={FVector(2400,-1800,40),FVector(-2300,3600,40),FVector(-4800,1500,40)};
                static const FVector Goals[]={FVector(2400,600,375),FVector(0,3600,755),FVector(-3200,1500,615)};
                auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(G->GetWorld());auto* Path=Nav ? Nav->FindPathToLocationSynchronously(G,Starts[Route],Goals[Route]) : nullptr;
                if(!Path || !Path->IsValid() || Path->IsPartial()) return End(false,FString::Printf(TEXT("Second elevation route %d is not navigable"),Route));
                P->SetActorLocation(Goals[Route]);Probe=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Starts[Route]));if(!Probe.IsValid()) return End(false,TEXT("No large navigation probe"));
                Probe->ApplyEnemyArchetype(9);Probe->BaseMovementSpeed=800.f;Probe->ContactDamage=0;Probe->FindComponentByClass<UForbiddenEnemyBrain>()->SetActive(false);Advance(5);
            }
            else if(Phase==5 && Age>9)
            {
                if(!Probe.IsValid() || FVector::Dist2D(Probe->GetActorLocation(),P->GetActorLocation())>260 || FMath::Abs(Probe->GetActorLocation().Z-P->GetActorLocation().Z)>150) return End(false,FString::Printf(TEXT("Largest enemy could not traverse route %d"),Route));
                Capture(*FString::Printf(TEXT("Route-%d.png"),Route+1));Probe->ReturnToPool();++Route;if(Route<3) Advance(4);else {Report->SetNumberField(TEXT("largeEnemyRoutes"),3);P->SetActorLocation(FVector(0,0,40));Advance(6);}
            }
            else if(Phase==6)
            {
                auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(FVector(450,0,55)));E->ApplyEnemyArchetype(2);E->Health=E->MaxHealth=1000;E->SetActorTickEnabled(false);
                E->TakeSpellDamage(100,FVector::ForwardVector*100,"Test");const float Front=1000-E->Health;
                E->Health=1000;E->TakeSpellDamage(100,-FVector::ForwardVector*100,"Test");const float Rear=1000-E->Health;
                if(!(Front<100 && Rear>100)) return End(false,TEXT("Directional armor/rear weakness failed"));E->ReturnToPool();
                G->ContractSucceeded("QA");Advance(7);
            }
            else if(Phase==7 && Age>0.5)
            {
                if(!G->DecisionWidget) return End(false,TEXT("Contract did not offer a corrupt perk"));G->DecisionWidget->Select(0);
                if(!G->CorruptPerks.Contains("Amplifier") || G->PerkDamageMultiplier()<=1 || G->PerkIncomingMultiplier()<=1) return End(false,TEXT("Corrupt perk has no paired downside"));Advance(8);
            }
            else if(Phase==8)
            {
                UCardDataAsset* Guard=nullptr;for(UCardDataAsset* C:P->Deck->UnlockedCards) if(C->CardID=="TalismanGuard") Guard=C;
                P->Deck->EquippedDeck={Guard};P->Deck->DiscardHand();P->Deck->DrawLevelUpHand();if(!P->Deck->AddSelectedCardSpell(Guard)) return End(false,TEXT("Guard setup failed"));P->AddTalismans(1);
                Probe=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetTalismanLocation(0)));Probe->ApplyEnemyArchetype(9);Probe->SetActorTickEnabled(false);Advance(9);
            }
            else if(Phase==9 && Age>0.4)
            {
                if(!Probe.IsValid() || !Probe->IsPoolActive() || Probe->Health<=0 || P->GetTalismanCount()!=0) return End(false,TEXT("Boss talisman contact executed the boss or failed to consume four charges"));
                const float Fraction=1-Probe->Health/Probe->MaxHealth;if(FMath::Abs(Fraction-0.08f)>0.001f) return End(false,TEXT("Boss talisman damage is not fixed at 8%"));Probe->ReturnToPool();
                P->Deck->ResetActiveSpells();P->ClearTalismans();P->Deck->EquippedDeck=P->Deck->UnlockedCards;P->Deck->RerollsRemaining=2;P->Deck->BanishesRemaining=1;G->AddXP(G->GetXPRequirement());Advance(10);
            }
            else if(Phase==10 && Age>0.7)
            {if(!G->SelectionWidget || P->Deck->CurrentHand.Num()!=3 || !PC->IsPaused()) return End(false,TEXT("Three-card paused drafting UI missing"));Capture(TEXT("03-Draft.png"));Advance(11);}
            else if(Phase==11 && Age>0.5)
            {
                if(!G->ChooseCard(0)) return End(false,TEXT("Draft choice failed"));
                if(PC->IsPaused() || G->bChoosingCard) return End(false,TEXT("Draft did not restore gameplay pause state"));
                auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();const int32 Currency=Profile->Data->CodeFragments;
                auto* Boss=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(FVector(800,0,111)));Boss->ApplyEnemyArchetype(9);Boss->TakeSpellDamage(1000000,FVector::ZeroVector,"QA",false);
                if(!G->bRunEnded || !G->bVictory || !PC->IsPaused() || Profile->Data->CodeFragments!=Currency) return End(false,TEXT("Boss victory or QA reward isolation failed"));Advance(12);
            }
            else if(Phase==12 && Age>0.7) {Capture(TEXT("04-Victory.png"));Advance(13);}
            else if(Phase==13 && Age>0.5) return End(true,TEXT("48 real timer-driven spell cases; blended rider; three large-enemy routes; armor; corrupt perk; boss-safe talismans; drafting pause; boss victory; isolated QA rewards"));
            return true;
        }
    };
}
void InstallForbiddenValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("OverhaulSmoke"))) return;
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))) {UE_LOG(LogTemp,Error,TEXT("OverhaulSmoke requires -ForbiddenQA to isolate player progress"));return;}
    const TWeakObjectPtr<AMVPController> Controller(PC);const auto State=MakeShared<FOverhaulQA>();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Controller,State](float){return Controller.IsValid() && State->Tick(Controller.Get());}));
}
