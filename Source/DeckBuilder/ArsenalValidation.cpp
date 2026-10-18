#include "MVPGame.h"
#include "NeonArsenal.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "NecromancyVisualComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimSequence.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

/** Real standalone world: actual deck validation, pooled collision, damage and pause ownership. */
void RunArsenalSmoke(AMVPController* C)
{
#if !UE_BUILD_SHIPPING
    static int32 Phase=0;static double Since=FPlatformTime::Seconds();static TArray<TWeakObjectPtr<AMVPEnemy>> Targets;
    auto* G=Cast<AMVPArenaGameMode>(C->GetWorld()->GetAuthGameMode());auto* P=Cast<AMVPPlayer>(C->GetPawn());
    if(!G || !P || !G->ArsenalDirector || !G->Spawner) return;
    ANeonArsenalDirector* A=G->ArsenalDirector;
    auto Fail=[&](const FString& Why){UE_LOG(LogTemp,Error,TEXT("ARSENAL_FAIL phase=%d %s"),Phase,*Why);FPlatformMisc::RequestExitWithStatus(false,1);};
    auto Advance=[&](int32 Next){Phase=Next;Since=FPlatformTime::Seconds();};
    auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,true,false);};
    const double Elapsed=FPlatformTime::Seconds()-Since;
    if(Elapsed>35) {Fail(TEXT("Timeout"));return;}
    if(Phase==0)
    {
        if(!A->IsReady() || P->Deck->UnlockedCards.Num()!=16 || G->GetXPRequirement()!=18) {Fail(TEXT("Missing authored arsenal or balance data"));return;}
        TSet<FName> IDs;
        for(UCardDataAsset* Card:P->Deck->UnlockedCards) {if(!Card->Icon || IDs.Contains(Card->CardID)) {Fail(TEXT("Missing icon or duplicate ID"));return;}IDs.Add(Card->CardID);}
        C->SetMouseLocation(800,390);Advance(1);
    }
    else if(Phase==1 && Elapsed>2)
    {
        Capture(TEXT("NeonCodex-SquareArt.png"));Advance(11);
    }
    else if(Phase==11 && Elapsed>0.3) {G->LibraryPage=2;Advance(2);}
    else if(Phase==2 && Elapsed>0.8)
    {
        Capture(TEXT("NeonCodex-NewCards.png"));Advance(12);
    }
    else if(Phase==12 && Elapsed>0.3)
    {
        G->StartingCards={TEXT("ArcRelay"),TEXT("NeonCataclysm")};
        if(!G->StartSelectedRun()) {Fail(TEXT("New starting cards were rejected"));return;}
        G->Spawner->StopWaves();P->bBenchmarkInvulnerable=true;
        for(UCardDataAsset* Card:P->Deck->UnlockedCards)
        {
            if(!P->Deck->FindOwnedSpell(Card->CardID)) {P->Deck->EquippedDeck={Card};P->Deck->DiscardHand();P->Deck->DrawLevelUpHand();if(!P->Deck->AddSelectedCardSpell(Card)) {Fail(TEXT("Spell equip failed"));return;}}
        }
        P->Deck->EquippedDeck=P->Deck->UnlockedCards;for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();P->ClearTalismans();
        P->SetActorLocation(FVector(0,0,55));
        for(int32 I=0;I<12;++I)
        {
            const FVector Location=I<6 ? FVector(230+I*140,0,55) : FVector(FMath::Cos((I-6)*PI/3)*230,FMath::Sin((I-6)*PI/3)*230,55);
            auto* E=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Location));if(!E) {Fail(TEXT("Pool exhausted"));return;}
            E->ApplyEnemyArchetype(0);E->Health=1.e7f;E->SetActorTickEnabled(false);Targets.Add(E);
        }
        // Every imported tier must actually bind matching walk/run clips and retain its health tuning.
        auto* E=Targets[0].Get();
        for(int32 I=0;I<10;++I)
        {
            E->ApplyEnemyArchetype(I);auto* Art=E->FindComponentByClass<UNecromancyVisualComponent>();
            if(!Art->IsArtReady() || E->MaxHealth!=CyberEnemies::Roster()[I].Health) {Fail(FString::Printf(TEXT("Enemy rig/stat failure %d"),I));return;}
            auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Art/Characters/%s/A_Walk.A_Walk"),CyberEnemies::Roster()[I].ID));
            Art->PlayAnimation(Clip,true);Art->SetPosition(0.1f,false);Art->TickAnimation(0,false);Art->RefreshBoneTransforms();
            TArray<FVector> Pose;for(int32 B=0;B<Art->GetNumBones();++B) Pose.Add(Art->GetBoneLocation(Art->GetBoneName(B)));
            Art->SetPosition(Clip->GetPlayLength()*0.6f,false);Art->TickAnimation(0,false);Art->RefreshBoneTransforms();
            float Motion=0;for(int32 B=0;B<Pose.Num();++B) Motion+=FVector::Dist(Pose[B],Art->GetBoneLocation(Art->GetBoneName(B)));
            if(Motion<1) {Fail(FString::Printf(TEXT("Enemy walk is static %d"),I));return;}
        }
        E->ApplyEnemyArchetype(0);E->Health=1.e7f;
        for(float T:{0.f,20.f,35.f,74.f,75.f,105.f,180.f,210.f,240.f}) for(int32 I=0;I<100;++I) {const int32 Index=CyberEnemies::Select(T,I/100.f);if(Index>=9 || CyberEnemies::Roster()[Index].UnlockSeconds>T) {Fail(TEXT("Enemy unlocked before its time"));return;}}
        const FSpellModifiers Base;
        const int32 Before=A->Hits[0];A->FirePattern(0,P,Targets[0].Get(),22,Base);
        if(A->Hits[0]-Before!=4) {Fail(TEXT("Arc Relay did not strike four unique enemies"));return;}
        const int32 BeforeGhost=A->Hits[1];A->FirePattern(1,P,Targets[0].Get(),14,Base);
        if(A->Hits[1]-BeforeGhost!=6) {Fail(TEXT("Ghost Link did not strike six unique enemies"));return;}
        for(int32 I=2;I<10;++I)
        {
            const int32 Prior=A->Hits[I];A->FirePattern(I,P,Targets[0].Get(),NeonArsenal::Profiles()[I].Damage,Base);
            for(int32 Step=0;Step<130;++Step) A->Tick(1.f/60.f);
            if(A->Hits[I]<=Prior) {Fail(FString::Printf(TEXT("Pattern %d dealt no damage"),I));return;}
        }
        FSpellModifiers Expanded;Expanded.ProjectileCount=3;
        const int32 BeforeMore=A->Hits[0];A->FirePattern(0,P,Targets[0].Get(),22,Expanded);
        if(A->Hits[0]-BeforeMore!=6) {Fail(TEXT("Chain count mutation ignored"));return;}
        // A solid, query-only wall must interrupt target acquisition and all chained damage.
        auto* Blocker=C->GetWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Blocker);Blocker->SetRootComponent(Box);Box->SetBoxExtent(FVector(20,600,180));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();Blocker->SetActorLocation(FVector(110,0,80));
        const int32 BeforeWall=A->Hits[0];A->FirePattern(0,P,Targets[0].Get(),22,Base);Blocker->Destroy();
        if(A->Hits[0]!=BeforeWall) {Fail(TEXT("Energy chain passed through a wall"));return;}
        // Exhaust only the effect pool: damage must still resolve on every chain cast.
        const int32 BeforeOverflow=A->Hits[0];for(int32 I=0;I<80;++I) A->FirePattern(0,P,Targets[0].Get(),1,Base);
        if(A->Hits[0]-BeforeOverflow!=320 || A->DroppedVisuals==0 || A->ActiveCount()>640) {Fail(TEXT("Pool budget affected chain damage"));return;}
        for(int32 Step=0;Step<180;++Step) A->Tick(1.f/60.f);
        G->AddXP(17);if(G->bChoosingCard || G->Level!=1) {Fail(TEXT("Draft opened below 18 essence"));return;}
        G->AddXP(1);if(!G->bChoosingCard || !C->IsPaused() || G->Level!=2 || G->GetXPRequirement()!=28) {Fail(TEXT("18 essence draft threshold failed"));return;}
        TSet<FName> Choices;for(UCardDataAsset* Card:P->Deck->CurrentHand) Choices.Add(Card->CardID);
        if(Choices.Num()!=3 || !G->ChooseCard(0) || C->IsPaused()) {Fail(TEXT("Unique hand/resume failed"));return;}
        for(ASpellBase* Weapon:P->Deck->ActiveWeapons) Weapon->StopCasting();P->ClearTalismans();
        G->RunSeconds=360;auto* Boss=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(FVector(-700,-300,55)));auto* Other=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(FVector(-600,-300,55)));
        if(!Boss || !Other || Boss->EnemyArchetype!=9 || Other->EnemyArchetype==9) {Fail(TEXT("Single boss admission failed"));return;}
        Boss->ReturnToPool();Other->ReturnToPool();G->RunSeconds=0;
        for(int32 I=0;I<10;++I) {auto* Target=Targets[I].Get();Target->ApplyEnemyArchetype(I);Target->SetActorLocation(FVector(-600+(I%5)*280,-450+(I/5)*400,CyberEnemies::Roster()[I].CollisionRadius+21));Target->Health=1.e7f;Target->FindComponentByClass<UNecromancyVisualComponent>()->UpdateLocomotion(FVector(-1,0,0),Target->BaseMovementSpeed);}
        P->PreferredCameraDistance=2400;P->CameraBoom->TargetArmLength=2400;Advance(3);
    }
    else if(Phase==3 && Elapsed>1.5)
    {
        Capture(TEXT("CyberRoster-InGame.png"));
        for(int32 I=0;I<10;++I) A->FirePattern(I,P,Targets[0].Get(),NeonArsenal::Profiles()[I].Damage,FSpellModifiers());
        Advance(4);
    }
    else if(Phase==4 && Elapsed>0.12)
    {
        Capture(TEXT("NeonArsenal-Combat.png"));Advance(5);
    }
    else if(Phase==5 && Elapsed>0.6)
    {
        UE_LOG(LogTemp,Display,TEXT("ARSENAL_PASS: 16 card icons; 10 damaging patterns; 4/6 unique chains; chain mutation; delayed AOE; wall occlusion; fixed pools; XP 18->28; unique draft/resume; 10 animated cyborgs; timed unlocks; single boss"));
        UGameplayStatics::DeleteGameInSlot(TEXT("CodexDeck_Automation"),0);UGameplayStatics::DeleteGameInSlot(TEXT("CodexAudio_Automation"),0);FPlatformMisc::RequestExit(false);Advance(6);
    }
#endif
}
