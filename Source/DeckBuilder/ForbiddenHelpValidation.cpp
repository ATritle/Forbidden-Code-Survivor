#include "MVPGame.h"
#include "ForbiddenHelpWidget.h"
#include "NeonSettingsWidget.h"
#include "CodexCardSelectionWidget.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "NecromancyVisualComponent.h"
#include "ForbiddenDistrictDirector.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FGuideQA
{
    int32 Phase=0,Page=0,Ramp=0,Still=0,Images=0;
    double Since=FPlatformTime::Seconds();float FrozenTime=0;
    bool Quit=false,QuitSettings=false;
    TArray<FTransform> Ramps;
    TArray<TSharedPtr<FJsonValue>> Checks;
    FString Directory=FPaths::ProjectSavedDir()/TEXT("Review/RampGuide");
    void Next(int32 To) {Phase=To;Since=FPlatformTime::Seconds();}
    void Check(const TCHAR* Copy) {Checks.Add(MakeShared<FJsonValueString>(Copy));}
    void Capture(const FString& Name) {FScreenshotRequest::RequestScreenshot(Directory/(Name+TEXT(".png")),true,false);}
    bool End(bool Success,const FString& Detail)
    {
        auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("result"),Success ? TEXT("RAMP_GUIDE_PASS") : TEXT("RAMP_GUIDE_FAIL"));Report->SetStringField(TEXT("detail"),Detail);
        Report->SetNumberField(TEXT("illustratedEntries"),Images);Report->SetNumberField(TEXT("ramps"),Ramps.Num());Report->SetArrayField(TEXT("checks"),Checks);
        FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));FFileHelper::SaveStringToFile(JSON,*(Directory/TEXT("report.json")));
        FPlatformMisc::RequestExitWithStatus(false,Success ? 0 : 1);return false;
    }
    bool Tick(AMVPController* PC)
    {
        auto* G=Cast<AMVPArenaGameMode>(PC->GetWorld()->GetAuthGameMode());auto* P=G ? G->GetArenaPlayer() : nullptr;
        if(!P || !G->Spawner) return true;const double Age=FPlatformTime::Seconds()-Since;
        if(Age>60) return End(false,FString::Printf(TEXT("Timeout in phase %d"),Phase));
        if(Phase==0)
        {
            auto* Art=P->FindComponentByClass<UNecromancyVisualComponent>();if(!Art || !Art->IsArtReady() || P->Deck->UnlockedCards.Num()!=16) return true;
            IFileManager::Get().MakeDirectory(*Directory,true);Next(1);
        }
        else if(Phase==1 && Age>3)
        {
            if(Quit)
            {
                FFileHelper::SaveStringToFile(TEXT("CLOSE_GAME_BUTTON_REQUESTED"),*(Directory/TEXT("quit-requested.txt")));
                if(QuitSettings)
                {
                    PC->ToggleSettings();
                    auto* Button=PC->SettingsWidget ? Cast<UButton>(PC->SettingsWidget->WidgetTree->FindWidget(TEXT("QuitGame"))) : nullptr;
                    if(!Button) return End(false,TEXT("Settings Close Game button missing"));Button->OnClicked.Broadcast();
                }
                else if(auto* HUD=Cast<AMVPHUD>(PC->GetHUD())) HUD->ClickAt(720,160);
                Next(99);return true;
            }
            Capture(TEXT("Menu"));Next(12);
        }
        else if(Phase==12 && Age>.4)
        {
            PC->OpenHelp();if(!PC->HelpWidget || !PC->IsPaused()) return End(false,TEXT("Main-menu guide did not pause/open"));
            if(G->StartSelectedRun()) return End(false,TEXT("Guide allowed the underlying Enter Arena action"));
            Check(TEXT("Main-menu guide blocks underlying start and pauses safely"));Next(2);
        }
        else if(Phase==2 && Age>.7)
        {
            auto* Body=Cast<UTextBlock>(PC->HelpWidget->WidgetTree->FindWidget(TEXT("HelpBody")));auto* Title=Cast<UTextBlock>(PC->HelpWidget->WidgetTree->FindWidget(TEXT("HelpTitle")));
            if(!Body || Body->GetFont().Size!=13 || !Title || Title->GetFont().Size!=28) return End(false,TEXT("Help typography does not match Settings"));
            Check(TEXT("Guide uses Settings typography: 28-point heading and 13-point explanatory text"));
            Capture(TEXT("Guide-Overview"));TArray<int32> Counts;Counts.Init(0,7);
            for(const auto& E:PC->HelpWidget->GetEntries())
            {
                if(!LoadObject<UTexture>(nullptr,*E.ImagePath) || E.Copy.IsEmpty()) return End(false,TEXT("Missing illustration/text: ")+E.Title);
                ++Images;++Counts[E.Topic];
            }
            if(Counts[1]!=16 || Counts[2]!=10 || Counts[4]!=8) return End(false,TEXT("Program/enemy/contract coverage incomplete"));
            Check(TEXT("Every guide entry has a cooked illustration and text; all 16 programs, 10 enemies and seven contracts plus perks covered"));
            Next(20);
        }
        else if(Phase==20 && Age>.4) {PC->HelpWidget->SelectTopic(1);Next(3);}
        else if(Phase==3 && Age>.7)
        {Capture(TEXT("Guide-Program"));Next(21);}
        else if(Phase==21 && Age>.4) {PC->HelpWidget->SelectTopic(2);Next(4);}
        else if(Phase==4 && Age>.7)
        {Capture(TEXT("Guide-Enemy"));Next(22);}
        else if(Phase==22 && Age>.4) {PC->HelpWidget->SelectTopic(3);Next(5);}
        else if(Phase==5 && Age>.7)
        {Capture(TEXT("Guide-District"));Next(23);}
        else if(Phase==23 && Age>.4)
        {
            auto* ReturnButton=Cast<UButton>(PC->HelpWidget->WidgetTree->FindWidget(TEXT("HelpReturn")));if(!ReturnButton) return End(false,TEXT("Guide Return button missing"));ReturnButton->OnClicked.Broadcast();
            if(PC->HelpWidget || !PC->IsPaused()) return End(false,TEXT("Guide Return button did not close safely"));
            PC->ToggleSettings();PC->OpenHelp();if(!PC->HelpWidget || !PC->SettingsWidget || PC->SettingsWidget->GetVisibility()!=ESlateVisibility::Collapsed) return End(false,TEXT("Settings guide did not suspend parent"));
            PC->CloseHelp();if(!PC->SettingsWidget || !PC->IsPaused() || PC->SettingsWidget->GetVisibility()!=ESlateVisibility::Visible) return End(false,TEXT("Guide did not restore settings"));
            Check(TEXT("Guide returns to the same settings widget, focus and pause owner"));PC->CloseSettings();
            if(!G->StartSelectedRun()) return End(false,TEXT("Could not start run after closing guide"));
            G->Spawner->StopWaves();G->bBroadcastBossSpawned=true;P->bBenchmarkInvulnerable=true;
            for(APooledEnemy* E:G->Spawner->GetActiveEnemies().Array()) E->ReturnToPool();
            P->Deck->ResetActiveSpells();FrozenTime=G->RunSeconds;PC->OpenHelp();Next(6);
        }
        else if(Phase==6 && Age>1)
        {
            if(!PC->IsPaused() || G->RunSeconds!=FrozenTime) return End(false,TEXT("Live run advanced behind guide"));
            PC->CloseHelp();if(PC->IsPaused()) return End(false,TEXT("Live run did not resume"));
            G->AddXP(100);if(!G->bChoosingCard || !G->SelectionWidget) return End(false,TEXT("No draft for modal preservation check"));
            PC->OpenHelp();if(G->ChooseCard(0) || G->SelectionWidget->GetIsEnabled()) return End(false,TEXT("Guide allowed underlying draft choice"));
            PC->CloseHelp();if(!PC->IsPaused() || !G->SelectionWidget->GetIsEnabled()) return End(false,TEXT("Guide lost paused draft"));
            while(G->bChoosingCard) if(!G->ChooseCard(0)) return End(false,TEXT("Could not resume draft"));
            Check(TEXT("Live help freezes run time; closing resumes play; card drafts stay paused, disabled while covered and intact afterward"));
            for(TActorIterator<AActor> It(PC->GetWorld());It;++It)
            {
                TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
                for(auto* C:Components) if(C->GetStaticMesh() && C->GetStaticMesh()->GetName()==TEXT("SM_CityRamp"))
                {
                    if(!C->GetMaterial(0) || C->GetMaterial(0)->GetBlendMode()!=BLEND_Opaque) return End(false,TEXT("Ramp material is not opaque"));
                    for(int32 I=0;I<C->GetInstanceCount();++I)
                    {
                        FTransform T;C->GetInstanceTransform(I,T,true);Ramps.Add(T);
                        for(float X:{-35.f,0.f,35.f})
                        {
                            const FVector On=T.TransformPosition(FVector(X,0,X+50));FHitResult Hit;
                            if(!PC->GetWorld()->LineTraceSingleByObjectType(Hit,On+FVector(0,0,25),On-FVector(0,0,25),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(GuideRamp),true,P)) || FMath::Abs(Hit.ImpactPoint.Z-On.Z)>3 || Hit.ImpactNormal.Z<.65f) return End(false,FString::Printf(TEXT("Missing ramp top or bad normal at ramp %d sample %.0f"),Ramps.Num(),X));
                        }
                    }
                }
            }
            if(Ramps.Num()!=8) return End(false,TEXT("Expected eight walkable ramps"));
            Check(TEXT("All eight ramp instances have opaque material and three upward-facing, height-correct complex-collision samples"));
            P->PreferredCameraDistance=1500;P->CameraBoom->TargetArmLength=1500;PC->GetHUD()->bShowHUD=false;Next(7);
        }
        else if(Phase==7)
        {
            P->SetActorLocation(Ramps[Ramp].TransformPosition(FVector(0,0,50))+FVector(0,0,55));Next(8);
        }
        else if(Phase==8 && Age>1.5)
        {
            Capture(FString::Printf(TEXT("Ramp-%d"),Ramp+1));if(Ramp==0) Capture(TEXT("Ramp"));++Ramp;if(Ramp<Ramps.Num()) Next(7);else Next(9);
        }
        else if(Phase==9)
        {
            static const FVector Views[]={FVector(2410,650,375),FVector(-1050,500,55),FVector(-2150,-1000,55),FVector(0,3250,755),FVector(-1200,-3950,55),FVector(200,150,55)};
            P->SetActorLocation(Views[Still]);if(Still==5) G->DistrictDirector->OfferCapture(6,FVector(400,200,0));Next(10);
        }
        else if(Phase==10 && Age>1)
        {
            static const TCHAR* Names[]={TEXT("Relay"),TEXT("Cache"),TEXT("Clinic"),TEXT("Ward"),TEXT("Subway"),TEXT("Spirit")};Capture(Names[Still]);++Still;
            if(Still<6) Next(9);else Next(11);
        }
        else if(Phase==11 && Age>.5) return End(true,TEXT("Native guide navigation/modal regression and eight ramp renders; inspect screenshots for visual acceptance"));
        else if(Phase==99 && Age>2) return End(false,TEXT("Close Game did not exit the application"));
        return true;
    }
};
}
void InstallForbiddenHelpValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("GuideSmoke")) && !FParse::Param(FCommandLine::Get(),TEXT("QuitMenuSmoke")) && !FParse::Param(FCommandLine::Get(),TEXT("QuitSettingsSmoke"))) return;
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA"))) return;
    const auto State=MakeShared<FGuideQA>();State->QuitSettings=FParse::Param(FCommandLine::Get(),TEXT("QuitSettingsSmoke"));State->Quit=State->QuitSettings || FParse::Param(FCommandLine::Get(),TEXT("QuitMenuSmoke"));
    const TWeakObjectPtr<AMVPController> Controller(PC);FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Controller,State](float){return Controller.IsValid() && State->Tick(Controller.Get());}));
}
