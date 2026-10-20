#include "ForbiddenEnemyBrain.h"
#include "MVPGame.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "MortisTerrain.h"
#include "NecromancyVisualComponent.h"
#include "NeonSpellImpactDirector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Animation/AnimSequence.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "ForbiddenSpirit.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NeonAudioDirector.h"
#include "ForbiddenDistrictDirector.h"

UForbiddenEnemyBrain::UForbiddenEnemyBrain() {PrimaryComponentTick.bCanEverTick=false;}
void UForbiddenEnemyBrain::BeginPlay()
{
    Super::BeginPlay();Enemy=Cast<AMVPEnemy>(GetOwner());if(!Enemy.IsValid()) return;
    Telegraph=NewObject<UStaticMeshComponent>(GetOwner());GetOwner()->AddInstanceComponent(Telegraph);Telegraph->SetupAttachment(GetOwner()->GetRootComponent());
    Telegraph->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Telegraph->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Challenges/M_ChallengeRing.M_ChallengeRing")));
    if(auto* Ring=Telegraph->CreateDynamicMaterialInstance(0)) {Ring->SetVectorParameterValue("Tint",FLinearColor(1,0.02f,0.3f));Ring->SetScalarParameterValue("Charge",0.15f);}
    Telegraph->SetCollisionEnabled(ECollisionEnabled::NoCollision);Telegraph->SetCanEverAffectNavigation(false);Telegraph->SetCastShadow(false);Telegraph->SetVisibility(false);Telegraph->RegisterComponent();
}
void UForbiddenEnemyBrain::ResetLife()
{
    if(!Enemy.IsValid()) return;
    SetActive(true);
    Life=Enemy->Generation;Archetype=Enemy->EnemyArchetype;State=0;StateTime=0;LastHit=-10.f;ZoneTick=0;Cooldown=1.f+FMath::FRandRange(0.f,2.f);
    SlowRemaining=ShieldRemaining=ExposedRemaining=0;SlowMultiplier=1;BossPhase=0;Nodes.Reset();Marks=0;MarkPower=0;
    PulseStacks=0;LastPulseTime=-1000;
    ShieldSource.Reset();ShieldLife=0;
    if(Telegraph) Telegraph->SetVisibility(false);
    const FString ID=CyberEnemies::Roster()[FMath::Clamp(Archetype,0,9)].ID;
    AttackClip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Art/Characters/%s/A_Attack.A_Attack"),*ID));
}
void UForbiddenEnemyBrain::ApplySlow(float Amount,float Seconds)
{
    if(!Enemy.IsValid()) return;
    SlowMultiplier=FMath::Min(SlowRemaining>0 ? SlowMultiplier : 1.f,1.f-FMath::Clamp(Amount,0.f,Enemy->EnemyTier>=3 ? 0.08f : 0.5f));SlowRemaining=FMath::Max(SlowRemaining,Seconds);
}
float UForbiddenEnemyBrain::ModifyIncoming(float Damage,FVector Velocity,FName ID)
{
    if(!Enemy.IsValid() || Enemy->bTrainingTarget) return Damage;
    if(ID=="TalismanGuard") return Damage;
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());const auto* Player=G ? G->GetArenaPlayer() : nullptr;
    const auto* Spell=Player && Player->Deck ? Player->Deck->FindOwnedSpell(ID) : nullptr;
    const bool Decoder=ID=="PhotonLance" && Spell && Spell->Modifiers.Evolution==1;
    if(ID=="PulseDart" && Spell && Spell->Modifiers.Evolution==1)
    {
        const float Now=GetWorld()->GetTimeSeconds();if(Now-LastPulseTime>2.f) PulseStacks=0;
        Damage*=1.f+0.15f*PulseStacks;PulseStacks=FMath::Min(3,PulseStacks+1);LastPulseTime=Now;
    }
    if(Decoder && Enemy->EnemyTier>=2) Damage*=1.5f;
    if(!Decoder)
    {
        if(ShieldRemaining>0)
        {
            const bool Directional=ShieldSource.IsValid() && ShieldSource->IsPoolActive() && ShieldSource->Generation==ShieldLife;
            if((ShieldLife==0 || Directional) && (!Directional || !Player || FVector::DotProduct((Player->GetActorLocation()-ShieldSource->GetActorLocation()).GetSafeNormal2D(),Velocity.GetSafeNormal2D())<-0.25f)) Damage*=0.75f;
        }
        if((Archetype==2 || Archetype==4) && Player && !Velocity.IsNearlyZero())
        {
            const float Facing=FVector::DotProduct((Player->GetActorLocation()-Enemy->GetActorLocation()).GetSafeNormal2D(),Velocity.GetSafeNormal2D());
            if(Facing<-0.35f) Damage*=Archetype==2 ? 0.55f : 0.4f;
            else if(Facing>0.25f) Damage*=1.3f;
        }
        if(Archetype==9)
        {
            int32 Live=0;for(const auto& Node:Nodes) if(Node.Key.IsValid() && Node.Key->IsPoolActive() && Node.Key->Generation==Node.Value) ++Live;
            if(Live>0) Damage*=0.55f;
        }
    }
    if(ExposedRemaining>0) Damage*=1.2f;
    return Damage;
}
void UForbiddenEnemyBrain::AddMark(float Damage)
{
    if(!Enemy.IsValid() || !Enemy->IsPoolActive()) return;
    ++Marks;MarkPower=FMath::Max(MarkPower,Damage);
    if(Marks<3) return;Marks=0;
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!G || !G->Spawner) return;
    TArray<TObjectPtr<APooledEnemy>> Victims=G->Spawner->GetActiveEnemies().Array();
    for(APooledEnemy* A:Victims) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive() && FVector::DistSquared(E->GetActorLocation(),Enemy->GetActorLocation())<FMath::Square(150.f) && UMortisTerrain::ClearSight(Enemy.Get(),E->GetActorLocation())) E->TakeSpellDamage(MarkPower*0.9f,FVector::ZeroVector,"KineticCards",false);
    if(G->SpellImpactDirector) G->SpellImpactDirector->PlayImpact(ENeonSpellStyle::Kinetic,Enemy->GetCombatVFXLocation());MarkPower=0;
}
void UForbiddenEnemyBrain::BeginAttack(AMVPPlayer* Player,float Delay,float Radius)
{
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->AudioDirector) G->AudioDirector->PlayWarning(Archetype==9 ? FName("Boss") : FName("Charge"),Enemy->GetActorLocation());
    State=1;Windup=Delay;StateTime=Delay;AttackPoint=Player->GetActorLocation();
    if(Archetype==6) {AttackPoint=Enemy->GetActorLocation();Radius=120.f;}
    if(Archetype==9 && BossPhase==2) {AttackPoint=Enemy->GetActorLocation();DashDirection=(Player->GetActorLocation()-AttackPoint).GetSafeNormal2D();}
    if(Archetype==5 || Archetype==7) DashDirection=(AttackPoint-Enemy->GetActorLocation()).GetSafeNormal2D();
    if(Telegraph) {Telegraph->SetWorldLocation(AttackPoint+FVector(0,0,-48));Telegraph->SetWorldScale3D(FVector(Radius/50.f,Radius/50.f,0.015f));Telegraph->SetVisibility(true);}
    if(Telegraph) if(auto* Ring=Cast<UMaterialInstanceDynamic>(Telegraph->GetMaterial(0))) Ring->SetVectorParameterValue("Tint",Archetype==6 ? FLinearColor(0.05f,0.55f,1.f) : FLinearColor(1.f,0.02f,0.3f));
    if(Archetype==9 && BossPhase==2) UpdateBroadcastLane(0.f);
    if(AttackClip) if(auto* Art=Enemy->FindComponentByClass<UNecromancyVisualComponent>()) {Art->PlayAnimation(AttackClip,false);Art->SetPlayRate(AttackClip->GetPlayLength()/FMath::Max(0.6f,Delay+0.3f));}
}
void UForbiddenEnemyBrain::GrantShield()
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!G || !G->Spawner) return;
    for(APooledEnemy* A:G->Spawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E!=Enemy.Get() && FVector::DistSquared(E->GetActorLocation(),Enemy->GetActorLocation())<FMath::Square(Archetype==6 ? 500.f : 260.f))
        if(auto* Brain=E->FindComponentByClass<UForbiddenEnemyBrain>()) {Brain->ShieldRemaining=2.5f;Brain->ShieldSource=Archetype==4 ? Enemy : TWeakObjectPtr<AMVPEnemy>();Brain->ShieldLife=Archetype==4 ? Enemy->Generation : 0;}
}
void UForbiddenEnemyBrain::UpdateBroadcastLane(float Progress)
{
    if(!Telegraph || !Enemy.IsValid()) return;
    const FVector Direction=DashDirection.RotateAngleAxis(-45.f+90.f*Progress,FVector::UpVector);
    Telegraph->SetWorldLocation(AttackPoint+Direction*600.f+FVector(0,0,-48));
    Telegraph->SetWorldRotation(Direction.Rotation());Telegraph->SetWorldScale3D(FVector(12.f,2.4f,0.015f));
}
void UForbiddenEnemyBrain::CancelChannel()
{
    if(Archetype==6 && State==1) {State=3;StateTime=0.8f;ExposedRemaining=0.8f;if(Telegraph) Telegraph->SetVisibility(false);}
}
void UForbiddenEnemyBrain::Strike(AMVPPlayer* Player,float Damage,float Radius)
{
    if(FVector::DistSquared2D(Player->GetActorLocation(),AttackPoint)<=FMath::Square(Radius) && FMath::Abs(Player->GetActorLocation().Z-AttackPoint.Z)<150.f && UMortisTerrain::ClearSight(Enemy.Get(),Player->GetActorLocation()))
    {
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode())) G->LastDamageCause=FString(CyberEnemies::Roster()[Archetype].Name)+TEXT(" / telegraphed attack");
        Player->ReceiveContactDamage(Damage);
    }
}
void UForbiddenEnemyBrain::SpawnBossNodes()
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!G || !G->Spawner || !Nav) return;
    for(int32 I=0;I<3;++I)
    {
        const float Angle=I*2*PI/3;FNavLocation Ground;
        const FVector P=Enemy->GetActorLocation()+FVector(FMath::Cos(Angle)*350,FMath::Sin(Angle)*350,0);
        if(!Nav->ProjectPointToNavigation(P,Ground,FVector(150,150,350))) continue;
        if(G->Spawner->GetActiveEnemyCount()>=G->Spawner->MaxActiveEnemies)
        {
            AMVPEnemy* Far=nullptr;float Best=0;
            for(APooledEnemy* A:G->Spawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->EnemyTier<2 && !E->Tags.Contains("BroadcastNode"))
            {const float D=FVector::DistSquared(E->GetActorLocation(),G->GetArenaPlayer()->GetActorLocation());if(D>Best){Best=D;Far=E;}}
            if(Far) Far->ReturnToPool();
        }
        TGuardValue<bool> Guard(G->bSuppressBossSelection,true);
        if(auto* Node=Cast<AMVPEnemy>(G->Spawner->SpawnEnemyFromPool(Ground.Location+FVector(0,0,55))))
        {
            Node->ApplyEnemyArchetype(BossPhase==3 ? 1 : 3);Node->MaxHealth=Node->Health=BossPhase==3 ? 80.f : 180.f;Node->ContactDamage=BossPhase==3 ? 12.f : 0.f;Node->EssenceReward=2;
            if(BossPhase==3) {if(auto* Art=Node->FindComponentByClass<UNecromancyVisualComponent>()) {Art->LoadCharacter(TEXT("DigitalSpirit"),170.f);Art->SetCastShadow(false);}}
            else {Node->Tags.AddUnique("BroadcastNode");Nodes.Add({Node,Node->Generation});}
        }
    }
}
bool UForbiddenEnemyBrain::Advance(float Dt,AMVPPlayer* Player,FVector& Goal,float& Speed)
{
    if(!IsActive()) return false;
    if(!Enemy.IsValid() || !Player) return false;
    if(Life!=Enemy->Generation || Archetype!=Enemy->EnemyArchetype) ResetLife();
    if(!Enemy->IsPoolActive() || Enemy->bTrainingTarget) {if(Telegraph) Telegraph->SetVisibility(false);return false;}
    SlowRemaining=FMath::Max(0.f,SlowRemaining-Dt);ShieldRemaining=FMath::Max(0.f,ShieldRemaining-Dt);ExposedRemaining=FMath::Max(0.f,ExposedRemaining-Dt);
    if(SlowRemaining>0) Speed*=SlowMultiplier;
    if(Enemy->Tags.Contains("BroadcastNode")) return true;
    Cooldown-=Dt;LastHit-=Dt;
    const float Distance=FVector::Dist2D(Enemy->GetActorLocation(),Player->GetActorLocation());
    if(Archetype==9)
    {
        const int32 Phase=Enemy->Health<Enemy->MaxHealth*0.33f ? 3 : Enemy->Health<Enemy->MaxHealth*0.66f ? 2 : 1;
        if(BossPhase==0) SpawnBossNodes();
        if(BossPhase!=Phase)
        {
            // A health threshold can be crossed during a wind-up. Never reinterpret the
            // previous phase's target/shape as a lane with an uninitialized direction.
            State=0;StateTime=0;Cooldown=0.8f;if(Telegraph) Telegraph->SetVisibility(false);
            if(auto* Art=Enemy->FindComponentByClass<UNecromancyVisualComponent>()) Art->SetDormant(false);
            BossPhase=Phase;if(Phase==3) SpawnBossNodes();
        }
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->DistrictDirector) G->DistrictDirector->BroadcastPhase=Phase;
    }
    if(State==1)
    {
        StateTime-=Dt;
        if(auto* Ring=Cast<UMaterialInstanceDynamic>(Telegraph->GetMaterial(0))) Ring->SetScalarParameterValue("Charge",1.f-FMath::Clamp(StateTime/FMath::Max(0.1f,Windup),0.f,1.f));
        if(StateTime<=0)
        {
            if(Archetype==9 && BossPhase==2) {State=5;StateTime=2.4f;ZoneTick=0;}
            else if(Archetype==6) {GrantShield();State=3;StateTime=0.7f;if(Telegraph) Telegraph->SetVisibility(false);}
            else if(Archetype==5 || Archetype==7) {State=2;StateTime=Archetype==7 ? 0.55f : 0.4f;}
            else if(Archetype==3 || Archetype==8 || (Archetype==9 && BossPhase==3)) {State=4;StateTime=Archetype==8 ? 3.f : 2.f;ZoneTick=0;}
            else {Strike(Player,Archetype==9 ? 30.f : Enemy->ContactDamage,Archetype==9 ? 310.f : 180.f);State=3;StateTime=Archetype<=2 ? 0.4f : 0.8f;ExposedRemaining=Archetype<=2 ? 0.f : 1.f;if(Telegraph) Telegraph->SetVisibility(false);}
        }
        return true;
    }
    if(State==5)
    {
        StateTime-=Dt;ZoneTick-=Dt;const float Progress=1.f-FMath::Clamp(StateTime/2.4f,0.f,1.f);UpdateBroadcastLane(Progress);
        if(ZoneTick<=0)
        {
            ZoneTick=0.6f;const FVector Direction=DashDirection.RotateAngleAxis(-45.f+90.f*Progress,FVector::UpVector);
            const FVector Relative=Player->GetActorLocation()-AttackPoint;const float Along=FVector::DotProduct(Relative,Direction);
            const float Across=FMath::Abs(FVector::DotProduct(Relative,FVector::CrossProduct(Direction,FVector::UpVector)));
            if(Along>=0 && Along<=1200.f && Across<120.f && FMath::Abs(Relative.Z)<150.f && UMortisTerrain::ClearSight(Enemy.Get(),Player->GetActorLocation()))
            {if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode())) G->LastDamageCause=TEXT("Sovereign sweeping broadcast lane");Player->ReceiveContactDamage(24.f);}
        }
        if(StateTime<=0) {State=3;StateTime=1.f;ExposedRemaining=1.f;if(Telegraph) Telegraph->SetVisibility(false);}return true;
    }
    if(State==2)
    {
        FHitResult Hit;Enemy->AddActorWorldOffset(DashDirection*(Archetype==7 ? 1100.f : 950.f)*Dt,true,&Hit);
        if(FVector::DistSquared2D(Enemy->GetActorLocation(),Player->GetActorLocation())<FMath::Square(100.f) && LastHit<=0) {AttackPoint=Enemy->GetActorLocation();Strike(Player,Archetype==7 ? 24.f : 18.f,100);LastHit=1;}
        StateTime-=Dt;if(StateTime<=0 || Hit.bBlockingHit) {State=3;StateTime=1.1f;ExposedRemaining=1.1f;if(Telegraph) Telegraph->SetVisibility(false);}return true;
    }
    if(State==4)
    {
        StateTime-=Dt;ZoneTick-=Dt;if(ZoneTick<=0) {Strike(Player,Archetype==9 ? 22.f : Archetype==8 ? 18.f : 8.f,Archetype==9 ? 310.f : Archetype==8 ? 260.f : 180.f);ZoneTick=0.9f;}
        if(StateTime<=0) {State=3;StateTime=0.7f;if(Telegraph) Telegraph->SetVisibility(false);}return true;
    }
    if(State==3) {StateTime-=Dt;if(StateTime<=0) {State=0;if(auto* Art=Enemy->FindComponentByClass<UNecromancyVisualComponent>()) Art->SetDormant(false);}return true;}
    if(Cooldown<=0)
    {
        Cooldown=Archetype==9 ? 3.8f : Archetype<=2 ? 1.2f : 5.5f+FMath::FRandRange(0,1.5f);
        if(Archetype<=2 && Distance<Enemy->GetSimpleCollisionRadius()+110.f) BeginAttack(Player,0.35f,180.f);
        if(Archetype==4 || Archetype==6)
        {
            if(Archetype==4) GrantShield();
            else BeginAttack(Player,1.3f,180);
        }
        else if(Archetype>=3 && Distance<900.f) BeginAttack(Player,Archetype==9 ? BossPhase==2 ? 2.f : 1.4f : Archetype==7 ? 0.9f : 1.1f,Archetype==8 ? 260.f : Archetype==9 ? 310.f : 180.f);
    }
    if(Archetype==1 && Distance>160.f)
    {
        const FVector Side=FVector::CrossProduct((Player->GetActorLocation()-Enemy->GetActorLocation()).GetSafeNormal2D(),FVector::UpVector);
        Goal+=Player->GetVelocity().GetClampedToMaxSize(470)*0.5f+Side*(Enemy->Generation%2 ? 180.f : -180.f);
    }
    if(Archetype<=1) if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->DecoySpirit && G->DecoySpirit->IsDecoyActive() && FVector::DistSquared(Enemy->GetActorLocation(),G->DecoySpirit->GetActorLocation())<FMath::Square(800.f)) Goal=G->DecoySpirit->GetActorLocation();
    return false;
}
