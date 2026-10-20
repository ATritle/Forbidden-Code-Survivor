#include <limits>
#include "MVPGame.h"
#include "ForbiddenProfile.h"
#include "ForbiddenEnemyBrain.h"
#include "ForbiddenRiderSubsystem.h"
#include "ForbiddenCombatSubsystem.h"
#include "NeonArsenal.h"
#include "NeonChallenge.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "EnemySpawner.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

/** Numeric integration checks against health removed and actual run attribution, not visual size. */
void InstallDamageAudit(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("DamageAudit"))) return;
    const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
    {
        auto* C=Weak.Get();if(!C) return false;auto* G=C->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();
        auto* P=G ? G->GetArenaPlayer() : nullptr;if(!P || !G->Spawner || P->Deck->UnlockedCards.Num()!=16) return true;
        auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile->SelectRider("NeonExorcist");
        auto Deck=Profile->GetDeck();Deck.Starters={"PulseDart","PrismBurst"};Profile->SaveDeck(0,Deck);G->LoadStartingDeck();
        bool Pass=G->StartSelectedRun();G->Spawner->StopWaves();G->bBroadcastBossSpawned=true;P->bBenchmarkInvulnerable=true;
        P->Deck->ResetActiveSpells();P->ClearTalismans();
        const auto Active=G->Spawner->GetActiveEnemies().Array();for(APooledEnemy* E:Active) E->ReturnToPool();
        P->SetActorLocation(FVector(0,0,1000));
        auto* Rider=C->GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>();
        auto* Fields=C->GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>();
        TArray<TSharedPtr<FJsonValue>> Rows;
        auto Check=[&](const FString& Name,float Expected,float Actual)
        {
            const bool OK=FMath::IsNearlyEqual(Expected,Actual,.02f);Pass&=OK;
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("case"),Name);Row->SetNumberField(TEXT("expected"),Expected);Row->SetNumberField(TEXT("actual"),Actual);Row->SetBoolField(TEXT("pass"),OK);Rows.Add(MakeShared<FJsonValueObject>(Row));
        };
        auto Spawn=[&](FVector Location,int32 Archetype=0)
        {
            auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Location));
            if(E) {E->ApplyEnemyArchetype(Archetype);E->MaxHealth=E->Health=100000;E->SetActorLocation(Location);E->SetActorTickEnabled(false);}
            return E;
        };
        auto* Target=Spawn(FVector(350,0,1000));if(!Target) {Pass=false;}
        if(Target)
        {
            // Every authored card's base value reaches the common damage path and attribution once.
            for(UCardDataAsset* Card:P->Deck->UnlockedCards)
            {
                Target->Health=100000;const float Before=G->RunRecord.DamageByCard.FindRef(Card->CardID);
                Target->TakeSpellDamage(Card->BaseDamage,FVector::ZeroVector,Card->CardID,false);
                Check(Card->CardID.ToString()+TEXT(" base health"),Card->BaseDamage,100000-Target->Health);
                Check(Card->CardID.ToString()+TEXT(" actual attribution"),Card->BaseDamage,G->RunRecord.DamageByCard.FindRef(Card->CardID)-Before);
            }
            // Invalid inputs must leave health untouched.
            const float Before=Target->Health;Target->TakeSpellDamage(-10,FVector::ZeroVector,"QA",false);Target->TakeSpellDamage(0,FVector::ZeroVector,"QA",false);
            Target->TakeSpellDamage(std::numeric_limits<float>::quiet_NaN(),FVector::ZeroVector,"QA",false);Check(TEXT("Invalid damage rejected"),Before,Target->Health);
            auto Hit=[&](const FString& Name,float Expected,FVector Velocity=FVector::ZeroVector,FName Program="QA")
            {Target->Health=100000;Target->TakeSpellDamage(100,Velocity,Program,false);Check(Name,Expected,100000-Target->Health);};
            G->CorruptPerks={"Amplifier"};Hit(TEXT("Amplifier outgoing"),115);G->CorruptPerks.Reset();
            for(const TPair<ENeonChallenge,float>& Contract:TArray<TPair<ENeonChallenge,float>>{{ENeonChallenge::OverloadOrb,300},{ENeonChallenge::DeadmanBattery,200},{ENeonChallenge::GlassProtocol,175}})
            {
                auto* D=P->Challenges->Definitions.FindByPredicate([&](const UNeonChallengeDefinition* Item){return Item && Item->Contract==Contract.Key;});
                auto* Station=C->GetWorld()->SpawnActor<ANeonChallengeStation>();if(!D) {Pass=false;continue;}Station->Configure(*D);Station->SetContractState(ENeonContractState::Active);P->Challenges->ActiveStation=Station;P->Challenges->RemainingTime=100;
                Hit((*D)->Title.ToString(),Contract.Value);G->CorruptPerks={"Amplifier"};Hit((*D)->Title.ToString()+TEXT(" + Amplifier"),Contract.Value*1.15f);
                G->CorruptPerks.Reset();P->Challenges->ActiveStation=nullptr;Station->Destroy();
            }
            Target->ApplyEnemyArchetype(2);Hit(TEXT("Directional front armor"),55,FVector::ForwardVector);Hit(TEXT("Directional rear weakness"),130,-FVector::ForwardVector);
            Target->ApplyEnemyArchetype(4);Hit(TEXT("Shield carrier front armor"),40,FVector::ForwardVector);Target->ApplyEnemyArchetype(0);
            Rider->EndRun();Profile->SelectRider("Mira");Rider->BeginRun(P);Hit(TEXT("Mira own weapon"),85);Hit(TEXT("Mira summon remains full power"),100,FVector::ZeroVector,"BoundDaemon");
            Rider->EndRun();Profile->SelectRider("Zero");Rider->BeginRun(P);
            for(int32 I=0;I<3;++I) Target->TakeSpellDamage(1,FVector::ZeroVector,"PulseDart",true);
            Hit(TEXT("Zero marked target"),125);Hit(TEXT("Zero summon bypasses own weapon perk"),100,FVector::ZeroVector,"BoundDaemon");
            Rider->EndRun();Profile->SelectRider("NeonExorcist");Rider->BeginRun(P);
            // Chain health deltas prove count, unique hits and per-hop attenuation.
            Target->SetActorLocation(FVector(350,0,1000));TArray<AMVPEnemy*> Chain={Target};
            for(int32 I=1;I<6;++I) Chain.Add(Spawn(FVector(350+I*220,0,1000)));
            for(int32 Pattern=0;Pattern<2;++Pattern)
            {
                for(AMVPEnemy* E:Chain) if(E) E->Health=100000;
                FSpellModifiers Snapshot;Snapshot.ProgramID=Pattern==0 ? FName("ArcRelay") : FName("GhostLink");
                G->ArsenalDirector->FirePattern(Pattern,P,Target,Pattern==0 ? 22.f : 14.f,Snapshot);
                for(int32 I=0;I<6;++I) if(Chain[I]) Check(FString::Printf(TEXT("%s hop %d"),*Snapshot.ProgramID.ToString(),I+1),I<(Pattern==0 ? 4 : 6) ? (Pattern==0 ? 22.f : 14.f)*FMath::Pow(Pattern==0 ? .82f : .9f,I) : 0.f,100000-Chain[I]->Health);
            }
            for(int32 I=1;I<6;++I) if(Chain[I]) Chain[I]->ReturnToPool();
            Fields->Clear();Target->Health=100000;
            Fields->QueueField("HaloNova",ENeonSpellStyle::Halo,Target->GetActorLocation(),30,200,0,1,.1f,true);
            for(int32 I=0;I<10;++I) Fields->Tick(.1f);Check(TEXT("Expanding field hits once across frames"),30,100000-Target->Health);
            Fields->Clear();Target->Health=100000;
            Fields->QueueField("EventHorizon",ENeonSpellStyle::EventHorizon,Target->GetActorLocation(),16,230,0,4,.35f);
            for(int32 I=0;I<20;++I) Fields->Tick(.1f);Check(TEXT("Scheduled field four pulses"),64,100000-Target->Health);
            Fields->Clear();
            // Echo damage is based on a confirmed hit, so rider scaling must not run twice.
            Rider->EndRun();Profile->SelectRider("Mira");Rider->BeginRun(P);
            auto* EchoDefinition=P->Challenges->Definitions.FindByPredicate([](const UNeonChallengeDefinition* Item){return Item && Item->Contract==ENeonChallenge::UnstableEcho;});
            auto* EchoStation=C->GetWorld()->SpawnActor<ANeonChallengeStation>();
            if(EchoDefinition)
            {
                EchoStation->Configure(*EchoDefinition);EchoStation->SetContractState(ENeonContractState::Active);P->Challenges->ActiveStation=EchoStation;P->Challenges->RemainingTime=40;
                auto* EchoVictim=Spawn(Target->GetActorLocation()+FVector(0,100,0));Target->Health=100000;
                for(int32 I=0;I<3;++I) Target->TakeSpellDamage(100,FVector::ZeroVector,"PulseDart",true);
                Check(TEXT("Mira direct hit before echo"),255,100000-Target->Health);
                P->Challenges->TickComponent(1.1f,LEVELTICK_All,nullptr);
                if(EchoVictim) {Check(TEXT("Echo is twice the confirmed hit without double rider scaling"),170,100000-EchoVictim->Health);EchoVictim->ReturnToPool();}
                else Pass=false;
                P->Challenges->ActiveStation=nullptr;EchoStation->Destroy();
            }
            else Pass=false;
            Rider->EndRun();Profile->SelectRider("NeonExorcist");Rider->BeginRun(P);
            // Armor Decoder and finite ranks use actual owned spell state.
            UCardDataAsset* Photon=nullptr;
            for(UCardDataAsset* Card:P->Deck->UnlockedCards) if(Card->CardID=="PhotonLance") Photon=Card;
            if(Photon)
            {
                P->Deck->DiscardHand();P->Deck->EquippedDeck={Photon};P->Deck->DrawLevelUpHand();auto* Spell=P->Deck->AddSelectedCardSpell(Photon);
                if(Spell)
                {
                    const auto Rank=FGameplayTag::RequestGameplayTag(TEXT("Mutation.Program.Rank"));Spell->ApplyMutation(Rank,1);Check(TEXT("Rank 2 damage"),Photon->BaseDamage*1.12f,Spell->Damage);
                    Spell->ApplyMutation(Rank,1);Check(TEXT("Rank 3 damage"),Photon->BaseDamage*1.24f,Spell->Damage);Check(TEXT("Rank 3 cooldown"),Photon->Cooldown*.92f,Spell->Cooldown);
                    Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.Program.EvolutionA")),1);Spell->StopCasting();
                    Target->ApplyEnemyArchetype(4);Hit(TEXT("Armor Decoder bypasses front armor"),100,FVector::ForwardVector,"PhotonLance");
                    Target->ApplyEnemyArchetype(6);Hit(TEXT("Armor Decoder elite bonus"),150,FVector::ForwardVector,"PhotonLance");Target->ApplyEnemyArchetype(0);
                }
                else Pass=false;
            }
            P->Deck->ResetActiveSpells();Target->Health=7;const float Recorded=G->RunRecord.DamageByCard.FindRef("AuditOverkill");const int32 Kills=G->Kills;
            Target->TakeSpellDamage(1000,FVector::ZeroVector,"AuditOverkill",false);Target->TakeSpellDamage(1000,FVector::ZeroVector,"AuditOverkill",false);
            Check(TEXT("Overkill credits only remaining HP once"),7,G->RunRecord.DamageByCard.FindRef("AuditOverkill")-Recorded);Check(TEXT("Pooled corpse cannot die twice"),1,G->Kills-Kills);
        }
        auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("result"),Pass ? TEXT("DAMAGE_AUDIT_PASS") : TEXT("DAMAGE_AUDIT_FAIL"));Report->SetArrayField(TEXT("checks"),Rows);
        Report->SetStringField(TEXT("scope"),TEXT("Numeric health and attribution integration; separate overhaul playback validates 16 timer-driven spells at base and both evolutions"));
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/DamageAudit");IFileManager::Get().MakeDirectory(*Folder,true);FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Folder/TEXT("report.json")));
        FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
    }),5.f);
}
