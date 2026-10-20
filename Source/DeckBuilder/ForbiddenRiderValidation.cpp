#include "MVPGame.h"
#include "Engine/World.h"
#include "ForbiddenRiderRoster.h"
#include "ForbiddenRiderPreview.h"
#include "ForbiddenRiderSubsystem.h"
#include "NecromancyVisualComponent.h"
#include "EnemySpawner.h"
#include "MortisTestingDirector.h"
#include "CodexDeckComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
    FString CheckFootContacts(AMVPController* PC,FName ID,float& Worst)
    {
        auto* Probe=NewObject<USkeletalMeshComponent>(PC);Probe->RegisterComponent();Probe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const FString Folder=TEXT("/Game/Art/Characters/")+ID.ToString()+TEXT("/");
        auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("SK_")+ID.ToString()+TEXT(".SK_")+ID.ToString()));
        if(!Mesh) {Probe->DestroyComponent();return TEXT("Missing rider mesh");}Probe->SetSkeletalMesh(Mesh);
        const FName Board=Probe->GetBoneIndex("Hoverboard")!=INDEX_NONE ? FName("Hoverboard") : FName("Hoverboard1");
        if(Probe->GetBoneIndex(Board)==INDEX_NONE || Probe->GetBoneIndex("LeftFoot")==INDEX_NONE || Probe->GetBoneIndex("RightFoot")==INDEX_NONE) {Probe->DestroyComponent();return TEXT("Rider rig is missing board or foot bones");}
        FVector Feet[2];bool First=true;
        for(const TCHAR* State:{TEXT("HoverIdle"),TEXT("Cruise"),TEXT("Accelerate"),TEXT("Brake"),TEXT("TurnLeft"),TEXT("TurnRight"),TEXT("Cast"),TEXT("Hit"),TEXT("Fall"),TEXT("Land"),TEXT("Defeat"),TEXT("CastBlood"),TEXT("CastStorm"),TEXT("HoverIdleAlt")})
        {
            auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("A_")+State+TEXT(".A_")+State));
            if(!Clip || Clip->GetSkeleton()!=Mesh->GetSkeleton()) {Probe->DestroyComponent();return TEXT("Missing or incompatible rider animation");}
            Probe->PlayAnimation(Clip,false);FVector Hand;float Travel=0;
            for(int32 S=0;S<=4;++S)
            {
                Probe->SetPosition(Clip->GetPlayLength()*S/4.f,false);Probe->TickAnimation(0,false);Probe->RefreshBoneTransforms();
                const auto T=Probe->GetSocketTransform(Board);
                const FVector H=T.InverseTransformPosition(Probe->GetSocketLocation("RightHand"))*T.GetScale3D().GetAbs();
                if(S==0) Hand=H;Travel=FMath::Max(Travel,float(FVector::Dist(H,Hand)));
                for(int32 I=0;I<2;++I) {const FVector F=T.InverseTransformPosition(Probe->GetSocketLocation(I==0 ? "LeftFoot" : "RightFoot"))*T.GetScale3D().GetAbs();if(First) Feet[I]=F;Worst=FMath::Max(Worst,float(FVector::Dist(F,Feet[I])));}
                First=false;
            }
            if((FName(State)=="Cast" || FName(State)=="CastBlood") && Travel<1) {Probe->DestroyComponent();return TEXT("Attack clip has no visible hand motion");}
        }
        Probe->DestroyComponent();return Worst>1.5f ? TEXT("Imported feet slide more than 1.5cm relative to board") : TEXT("");
    }
    bool CheckImage(AForbiddenRiderPreview* Preview,FString& Detail)
    {
        if(!Preview || !Preview->GetImage()) {Detail=TEXT("No 3D render target");return false;}
        TArray<FColor> Pixels;auto* Image=Preview->GetImage();
        if(!Image->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)) {Detail=TEXT("Render target readback failed");return false;}
        int32 Count=0,MinX=Image->SizeX,MinY=Image->SizeY,MaxX=0,MaxY=0;
        for(int32 Y=0;Y<Image->SizeY;++Y) for(int32 X=0;X<Image->SizeX;++X)
        {const auto& P=Pixels[Y*Image->SizeX+X];if(FMath::Max3(P.R,P.G,P.B)>45) {++Count;MinX=FMath::Min(MinX,X);MaxX=FMath::Max(MaxX,X);MinY=FMath::Min(MinY,Y);MaxY=FMath::Max(MaxY,Y);}}
        if(Count<Pixels.Num()/100 || MinX<2 || MinY<2 || MaxX>Image->SizeX-3 || MaxY>Image->SizeY-3)
        {Detail=FString::Printf(TEXT("3D image blank or cropped: pixels=%d bounds=%d,%d to %d,%d"),Count,MinX,MinY,MaxX,MaxY);return false;}
        return true;
    }
}
void InstallRiderSelectionValidation(AMVPController* PC)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || !FParse::Param(FCommandLine::Get(),TEXT("RiderSelectionSmoke"))) return;
    struct FState {int32 Phase=0,Angle=0;double Since=FPlatformTime::Seconds();float Yaw=0,Worst=0;FName ID;TWeakObjectPtr<AMVPEnemy> Enemy;TWeakObjectPtr<AMVPPlayer> OriginalPawn;int32 Kills=0,XP=0,Actions=0,Hits=0;};
    auto S=MakeShared<FState>();FString ID=TEXT("NeonExorcist");FParse::Value(FCommandLine::Get(),TEXT("Rider="),ID);S->ID=*ID;
    const bool Reload=FParse::Param(FCommandLine::Get(),TEXT("RiderSelectionReload"));const TWeakObjectPtr<AMVPController> Weak(PC);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([S,Weak,Reload](float)
    {
        auto* C=Weak.Get();if(!C) return false;auto* G=C->GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();auto* H=Cast<AMVPHUD>(C->GetHUD());
        if(!G || !H || !G->GetArenaPlayer() || !G->GetArenaPlayer()->Deck) return true;
        auto* P=G->GetArenaPlayer();auto* R=C->GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>();
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("Review/RiderSelection");IFileManager::Get().MakeDirectory(*Folder,true);
        auto End=[&](bool Pass,FString Detail)
        {
            Detail.ReplaceInline(TEXT("\""),TEXT("'"));
            const FString Json=FString::Printf(TEXT("{\"result\":\"%s\",\"rider\":\"%s\",\"detail\":\"%s\",\"maxFootSlipCm\":%.5f,\"rotationViews\":%d}"),Pass ? TEXT("RIDER_SELECTION_PASS") : TEXT("RIDER_SELECTION_FAIL"),*S->ID.ToString(),*Detail,S->Worst,S->Angle);
            FFileHelper::SaveStringToFile(Json,*(Folder/(Reload ? TEXT("reload.json") : TEXT("report.json"))));FPlatformMisc::RequestExitWithStatus(false,Pass ? 0 : 1);return false;
        };
        auto Next=[&](int32 Phase){S->Phase=Phase;S->Since=FPlatformTime::Seconds();};const double Age=FPlatformTime::Seconds()-S->Since;
        if(Age>60) return End(false,FString::Printf(TEXT("Phase %d timed out"),S->Phase));
        if(S->Phase==0 && Age>3)
        {
            if(P->Deck->UnlockedCards.Num()<16) return true;
            if(Reload) return End(ForbiddenRiders::Selected(C)==S->ID,TEXT("Selected rider restored in a fresh process"));
            G->OpenRiderSelection();if(!G->SelectRider(S->ID)) return End(false,TEXT("Could not select the requested rider"));
            const FString Error=CheckFootContacts(C,S->ID,S->Worst);if(!Error.IsEmpty()) return End(false,Error);
            if(!C->IsPaused()) return End(false,TEXT("Selection menu did not keep gameplay paused"));
            S->Yaw=G->RiderPreview->GetYaw();Next(1);
        }
        else if(S->Phase==1 && Age>1.6)
        {
            if(G->RiderPreview->GetYaw()==S->Yaw) return End(false,TEXT("Turntable did not rotate while the world was paused"));
            FString Error;if(!CheckImage(G->RiderPreview,Error)) return End(false,Error);
            G->RiderPreview->AutoRotate=false;G->RiderPreview->ResetView();Next(2);
        }
        else if(S->Phase==2 && Age>.35)
        {
            FString Error;if(!CheckImage(G->RiderPreview,Error)) return End(false,Error);
            FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("%s-View-%d.png"),*S->ID.ToString(),S->Angle),true,false);
            Next(20);
        }
        else if(S->Phase==20 && Age>.25) {++S->Angle;if(S->Angle<4) {G->RiderPreview->RotateBy(90);Next(2);}else Next(3);}
        else if(S->Phase==3 && Age>.35) {G->RiderPreview->ResetView();G->RiderPreview->SetClip(2);Next(4);}
        else if(S->Phase==4 && Age>.35)
        {
            if(G->RiderPreview->GetClip()!=2) return End(false,TEXT("Attack preview selection failed"));
            FScreenshotRequest::RequestScreenshot(Folder/(S->ID.ToString()+TEXT("-Attack.png")),true,false);
            Next(21);
        }
        else if(S->Phase==21 && Age>.25) {G->ContinueToDeck();if(G->bCharacterSelectionOpen) return End(false,TEXT("Continue did not open deckbuilding"));Next(5);}
        else if(S->Phase==5 && Age>.35)
        {
            G->OpenRiderSelection();if(G->RiderPreview->GetRider()!=S->ID) return End(false,TEXT("Returning from deck changed the selected rider"));
            if(!G->StartSelectedRun()) return End(false,TEXT("Selected rider could not enter a run"));G->Spawner->StopWaves();
            auto* Art=P->FindComponentByClass<UNecromancyVisualComponent>();
            if(!Art || !Art->GetSkeletalMeshAsset() || Art->GetSkeletalMeshAsset()->GetName()!=TEXT("SK_")+S->ID.ToString() || !Art->UsesBlendedRider()) return End(false,TEXT("Gameplay did not use the selected rider and animation Blueprint"));
            S->Enemy=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(220,0,0)));if(!S->Enemy.IsValid()) return End(false,TEXT("No pooled target available"));
            auto* E=S->Enemy.Get();E->bTrainingTarget=true;E->Health=10000;E->MaxHealth=10000;
            FSpellModifiers Snapshot;
            if(S->ID=="Vex")
            {
                const auto Chain=R->ModifySnapshot(Snapshot,true);
                if(!FMath::IsNearlyEqual(P->MaxHealth,80.f) || Chain.ProjectileCount!=Snapshot.ProjectileCount+1 || !FMath::IsNearlyEqual(Chain.RadiusMultiplier,1.15f)) return End(false,TEXT("VEX health or chain perk failed"));
                R->OnBoost();R->OnBoost();FVector Goal;E->EnemyTier=0;if(R->Decoys!=1 || !R->DecoyGoal(E,Goal)) return End(false,TEXT("VEX decoy or cooldown failed"));
                E->EnemyTier=3;if(R->DecoyGoal(E,Goal)) return End(false,TEXT("VEX decoy affected a boss"));E->EnemyTier=0;
            }
            if(S->ID=="Rook")
            {
                if(!FMath::IsNearlyEqual(P->Movement->MaxSpeed,423.f)) return End(false,TEXT("ROOK speed tradeoff failed"));
                R->OnEssence(24);if(R->GetShield()!=0) return End(false,TEXT("Shield charged before 25 essence"));R->OnEssence(1);
                if(!FMath::IsNearlyEqual(R->GetShield(),15.f,.001f)) return End(false,TEXT("Shield recharge failed"));R->OnEssence(100);
                if(!FMath::IsNearlyEqual(R->GetShield(),30.f,.001f)) return End(false,TEXT("Shield cap failed"));
                const float HP=P->Health;P->ReceiveContactDamage(10);if(P->Health!=HP || !FMath::IsNearlyEqual(R->GetShield(),20.f,.001f)) return End(false,TEXT("Contact damage did not use ROOK shield"));
                P->ReceiveChallengeDamage(10,false);if(P->Health!=HP-10 || !FMath::IsNearlyEqual(R->GetShield(),20.f,.001f)) return End(false,TEXT("Shield incorrectly absorbed a challenge health cost"));
                R->OnBoost();R->OnBoost();if(R->Repulsors!=1) return End(false,TEXT("ROOK repulsor cooldown failed"));
            }
            if(S->ID=="Zero")
            {
                for(int32 I=0;I<3;++I) E->TakeSpellDamage(1,FVector::ZeroVector,"PulseDart",false);
                if(R->IsLocked(E)) return End(false,TEXT("Secondary hits built Target Lock"));
                for(int32 I=0;I<3;++I) E->TakeSpellDamage(10,FVector::ZeroVector,"PulseDart");
                const float Before=E->Health;E->TakeSpellDamage(10,FVector::ZeroVector,"PulseDart");
                if(!R->IsLocked(E) || !FMath::IsNearlyEqual(Before-E->Health,12.5f)) return End(false,TEXT("ZERO lock or 25-percent damage bonus failed"));
                if(R->ProjectileCast()!=0 || R->ProjectileCast()!=0 || R->ProjectileCast()!=2 || !FMath::IsNearlyEqual(R->ModifySnapshot(Snapshot).RadiusMultiplier,.8f)) return End(false,TEXT("ZERO pierce cadence or radius tradeoff failed"));
                E->ReturnToPool();G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(220,0,0));if(R->IsLocked(E)) return End(false,TEXT("Target Lock followed a recycled enemy life"));
            }
            if(S->ID=="Mira")
            {
                const float HP=E->Health;E->TakeSpellDamage(10,FVector::ZeroVector,"PulseDart");
                if(!FMath::IsNearlyEqual(HP-E->Health,8.5f)) return End(false,TEXT("MIRA own-weapon tradeoff failed"));
                for(int32 I=0;I<20;++I) E->TakeSpellDamage(1,FVector::ZeroVector,"BoundDaemon");
                auto* Other=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(P->GetActorLocation()+FVector(300,0,0)));if(!Other) return End(false,TEXT("No possession target"));
                Other->ApplyEnemyArchetype(0);Other->Health=10000;Other->MaxHealth=10000;Other->bTrainingTarget=false;
                S->Kills=G->Kills;S->XP=G->XP;
            }
            Next(6);
        }
        else if(S->Phase==6 && Age>2)
        {
            if(S->ID=="Mira" && (!R->HasPossessedAlly() || R->Possessions!=1 || G->Kills!=S->Kills || G->XP!=S->XP)) return End(false,TEXT("MIRA possession failed or awarded free kill/XP credit"));
            if(S->ID=="Mira" && R->ConfirmedSummonHits<=20) return End(false,TEXT("MIRA familiar did not land a real pooled projectile hit"));
            FScreenshotRequest::RequestScreenshot(Folder/(S->ID.ToString()+TEXT("-Gameplay.png")),true,false);Next(7);
        }
        else if(S->Phase==7 && Age>.5)
        {
            if(!G->TestingDirector) G->TestingDirector=C->GetWorld()->SpawnActor<AMortisTestingDirector>();
            S->OriginalPawn=P;TArray<int32> Counts;Counts.Init(0,10);Counts[0]=5;FString Error;
            if(!G->TestingDirector || !G->TestingDirector->Configure("PulseDart",Counts,true,Error) || !G->TestingDirector->Enter(C,false,Error)) return End(false,TEXT("Could not verify practice isolation: ")+Error);
            S->Actions=R->Decoys+R->Repulsors;S->Hits=R->ConfirmedSummonHits;R->OnBoost();
            FSpellModifiers Original;const auto Snapshot=R->ModifySnapshot(Original,true);
            if(R->Decoys+R->Repulsors!=S->Actions || R->ProjectileCast()!=0 || Snapshot.ProjectileCount!=Original.ProjectileCount || Snapshot.RadiusMultiplier!=Original.RadiusMultiplier || R->ModifyDamage(nullptr,10,"PulseDart",true)!=10) return End(false,TEXT("Rider perks leaked into isolated card testing"));
            Next(8);
        }
        else if(S->Phase==8 && Age>.5)
        {
            if(R->ConfirmedSummonHits!=S->Hits) return End(false,TEXT("Practice altered saved summon-hit progress"));
            G->TestingDirector->Exit(C);if(C->GetPawn()!=S->OriginalPawn.Get() || ForbiddenRiders::Selected(C)!=S->ID) return End(false,TEXT("Practice failed to restore selected rider"));
            return End(true,TEXT("Saved choice/restart, four lit 3D angles, paused-world rotation, 14 board-safe clips, attack preview, deck navigation, gameplay perks and isolated card-testing restore verified"));
        }
        return true;
    }));
}
