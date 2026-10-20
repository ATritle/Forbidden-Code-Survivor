#include "MVPGame.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "EnemySpawner.h"
#include "NeonArsenal.h"
#include "ForbiddenProfile.h"
#include "ForbiddenScoreService.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

/** Draft through the actual essence/choice pipeline, never assign ActiveWeapons directly. */
void InstallAllCardsValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("AllCardsSmoke"))) return;
    struct FState {int32 Phase=0,Hands=0;double Since=FPlatformTime::Seconds();};auto S=MakeShared<FState>();
    const bool Expanded=FParse::Param(FCommandLine::Get(),TEXT("AllCardsPlaytest"));const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak,Expanded](float)
    {
        auto* C=Weak.Get();if(!C) return false;auto* G=C->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();
        auto* P=G ? G->GetArenaPlayer() : nullptr;if(!P || !G->Spawner || P->Deck->UnlockedCards.Num()!=16) return true;
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/All16");IFileManager::Get().MakeDirectory(*Folder,true);
        auto End=[&](bool Pass,const FString& Detail)
        {
            auto Json=MakeShared<FJsonObject>();Json->SetStringField(TEXT("result"),Pass ? TEXT("ALL16_PASS") : TEXT("ALL16_FAIL"));Json->SetStringField(TEXT("detail"),Detail);
            Json->SetBoolField(TEXT("expanded"),Expanded);Json->SetNumberField(TEXT("capacity"),P->Deck->MaximumActiveWeapons);Json->SetNumberField(TEXT("weapons"),P->Deck->ActiveWeapons.Num());Json->SetNumberField(TEXT("hands"),S->Hands);
            auto Damage=MakeShared<FJsonObject>();for(const auto& Pair:G->RunRecord.DamageByCard) Damage->SetNumberField(Pair.Key.ToString(),Pair.Value);Json->SetObjectField(TEXT("damageByCard"),Damage);
            FString Output;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Output));FFileHelper::SaveStringToFile(Output,*(Folder/TEXT("report.json")));
            FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        const double Age=FPlatformTime::Seconds()-S->Since;
        auto Next=[&](int32 Phase){S->Phase=Phase;S->Since=FPlatformTime::Seconds();};
        if(Age>45) return End(false,TEXT("Validation phase timed out"));
        if(S->Phase==0 && Age>3)
        {
            if(P->Deck->MaximumActiveWeapons!=16) return End(false,TEXT("Flag did not configure the expected attack cap"));
            auto* Profile=G->GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile->SelectRider("NeonExorcist");
            auto Deck=Profile->GetDeck();Deck.Starters={"PulseDart","PrismBurst"};if(!Profile->SaveDeck(0,Deck)) return End(false,TEXT("Starter setup failed"));G->LoadStartingDeck();
            if(!G->StartSelectedRun()) return End(false,TEXT("Could not start the ordinary two-starter run"));
            P->bBenchmarkInvulnerable=true;G->Spawner->StopWaves();G->bBroadcastBossSpawned=true;
            const auto Active=G->Spawner->GetActiveEnemies().Array();for(APooledEnemy* Enemy:Active) Enemy->ReturnToPool();
            if(P->Deck->EquippedDeck.Num()!=16) return End(false,TEXT("Full catalog absent from draft"));
            TMap<FName,ASpellBase*> Prior;
            for(ASpellBase* Spell:P->Deck->ActiveWeapons) Prior.Add(Spell->CardDefinition->CardID,Spell);
            while(P->Deck->ActiveWeapons.Num()<16 && S->Hands<80)
            {
                const int32 Before=P->Deck->ActiveWeapons.Num();G->AddXP(G->GetXPRequirement());++S->Hands;
                const auto Hand=P->Deck->CurrentHand;TSet<FName> IDs;for(UCardDataAsset* Card:Hand) IDs.Add(Card->CardID);
                if(!G->bChoosingCard || Hand.Num()!=3 || IDs.Num()!=3) return End(false,TEXT("Essence draft did not offer three unique cards"));
                int32 Pick=0;for(int32 I=0;I<Hand.Num();++I) if(!P->Deck->FindOwnedSpell(Hand[I]->CardID)) {Pick=I;break;}
                if(!G->ChooseCard(Pick) || G->DecisionWidget || P->Deck->ActiveWeapons.Num()<Before) return End(false,TEXT("A draft opened replacement or removed an attack"));
                for(const auto& Pair:Prior) if(P->Deck->FindOwnedSpell(Pair.Key)!=Pair.Value) return End(false,TEXT("An earlier weapon was replaced"));
                for(ASpellBase* Spell:P->Deck->ActiveWeapons) Prior.Add(Spell->CardDefinition->CardID,Spell);
            }
            if(P->Deck->ActiveWeapons.Num()!=16 || C->IsPaused()) return End(false,TEXT("Could not retain all 16 through actual RNG drafts"));
            P->SetActorLocation(FVector(0,0,40));
            for(int32 I=0;I<32;++I)
            {
                const float A=I*2*PI/8;
                const FVector Position=I<8 ? FVector(FMath::Cos(A)*110.f,FMath::Sin(A)*110.f,55) : FVector(350+((I-8)/6)*180,((I-8)%6-2.5f)*170,55);
                auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Position));if(!E) return End(false,TEXT("Could not prepare combat probes"));
                E->ApplyEnemyArchetype(0);E->Health=E->MaxHealth=1000000;E->bTrainingTarget=true;E->SetActorLocation(Position);E->SetActorTickEnabled(false);
            }
            Next(1);
        }
        else if(S->Phase==1 && Age>8)
        {
            for(FName ID:UForbiddenProfile::ProgramIDs()) if(G->RunRecord.DamageByCard.FindRef(ID)<=0) return End(false,TEXT("No confirmed damage for ")+ID.ToString());
            if(P->Deck->ActiveWeapons.Num()!=16 || G->ArsenalDirector->DroppedGameplay>0) return End(false,TEXT("All-card combat lost a weapon or gameplay admission"));
            FScreenshotRequest::RequestScreenshot(Folder/TEXT("All16-Combat.png"),true,false);Next(2);
        }
        else if(S->Phase==2 && Age>.4)
        {C->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Tab,IE_Pressed,1.f));Next(3);}
        else if(S->Phase==3 && Age>.4)
        {FScreenshotRequest::RequestScreenshot(Folder/TEXT("All16-Inspect.png"),true,false);Next(4);}
        else if(S->Phase==4 && Age>.4)
        {
            C->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Tab,IE_Released,0.f));
            return End(true,TEXT("Sixteen distinct weapons acquired through real three-card essence drafts; no replacements; all 16 dealt damage together; full HUD and inspection captured"));
        }
        return true;
    }));
}
