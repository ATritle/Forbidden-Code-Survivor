#include "MortisTestingDirector.h"
#include "NeonChallenge.h"
#include "MVPGame.h"
#include "MortisCharacter.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "CodexCardSelectionWidget.h"
#include "EnemySpawner.h"
#include "NeonArsenal.h"
#include "NecromancyVisualComponent.h"
#include "MortisProgressionManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "NiagaraSystem.h"

AMortisTestingDirector::AMortisTestingDirector()
{
    PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Floor=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeFloor"));Floor->SetupAttachment(RootComponent);
    Floor->SetCollisionObjectType(ECC_WorldStatic);Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Floor->SetCastShadow(false);
    Grid=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RangeGrid"));Grid->SetupAttachment(RootComponent);Grid->SetCollisionEnabled(ECollisionEnabled::NoCollision);Grid->SetCastShadow(false);
    EnemyCounts.Init(5,10);Numbers.SetNum(256);
}
UCardDataAsset* AMortisTestingDirector::FindCard(FName ID) const
{
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    const auto* Player=OriginalPawn ? OriginalPawn.Get() : Game ? Game->GetArenaPlayer() : nullptr;
    if(Player) for(UCardDataAsset* Card:Player->Deck->UnlockedCards) if(Card && Card->CardID==ID) return Card;
    return nullptr;
}
bool AMortisTestingDirector::Configure(FName ID,const TArray<int32>& Counts,bool bReset,FString& Error,float Spacing)
{
    return ConfigureCards({ID},Counts,bReset,Error,Spacing);
}
bool AMortisTestingDirector::ConfigureCards(const TArray<FName>& IDs,const TArray<int32>& Counts,bool bReset,FString& Error,float Spacing)
{
    if(IDs.IsEmpty() || IDs.Num()>16) {Error=TEXT("Select at least one unlocked attack (up to 16).");return false;}
    TSet<FName> Unique;
    for(FName ID:IDs) {if(!FindCard(ID) || Unique.Contains(ID)) {Error=TEXT("Select unique unlocked attacks.");return false;}Unique.Add(ID);}
    if(Counts.Num()!=10) {Error=TEXT("Invalid target roster.");return false;}
    int32 Total=0;for(int32 Count:Counts)
    {
        if(Count<0 || Count>50 || Count%5!=0) {Error=TEXT("Counts must be 0–50 in increments of five.");return false;}
        Total+=Count;
    }
    if(Total<5 || Total>200) {Error=TEXT("Select 5–200 targets in total.");return false;}
    if(!FMath::IsFinite(Spacing) || Spacing<200 || Spacing>500) {Error=TEXT("Spacing must be 200–500 cm.");return false;}
    SelectedCards=IDs;CardID=IDs[0];EnemyCounts=Counts;bAutoReset=bReset;GridSpacing=Spacing;return true;
}
bool AMortisTestingDirector::PrepareRange(FString& Error)
{
    if(PracticeSpawner) return true;
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    Floor->SetStaticMesh(Cube);Floor->SetRelativeLocation(FVector(0,0,19975));Floor->SetRelativeScale3D(FVector(90,90,0.5f));
    Floor->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cinematic/M_StoneTest.M_StoneTest")));
    Grid->SetStaticMesh(Cube);
    auto* GridMat=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Feedback/M_NeonBeam.M_NeonBeam")),this);
    if(GridMat) GridMat->SetVectorParameterValue(TEXT("Color"),FLinearColor(0.01f,0.16f,0.22f));Grid->SetMaterial(0,GridMat);
    for(int32 I=-12;I<=12;++I)
    {
        Grid->AddInstance(FTransform(FRotator::ZeroRotator,FVector(I*360,0,20000.5f),FVector(0.025f,86.4f,0.01f)));
        Grid->AddInstance(FTransform(FRotator::ZeroRotator,FVector(0,I*360,20000.5f),FVector(86.4f,0.025f,0.01f)));
    }
    PracticeSpawner=GetWorld()->SpawnActorDeferred<AEnemySpawner>(AEnemySpawner::StaticClass(),FTransform::Identity);
    if(!PracticeSpawner) {Error=TEXT("Could not create target pool.");return false;}
    PracticeSpawner->EnemyClass=AMVPEnemy::StaticClass();PracticeSpawner->PoolSize=200;PracticeSpawner->MaxActiveEnemies=200;PracticeSpawner->bAutoStart=false;
    PracticeSpawner->FinishSpawning(FTransform::Identity);
    if(!PracticeSpawner->InitializePool()) {Error=TEXT("Could not initialize target pool.");return false;}
    for(TActorIterator<AMVPEnemy> It(GetWorld());It;++It) if(It->GetOwner()==PracticeSpawner) It->bTrainingTarget=true;
    ProjectileMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    ProjectileFX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_Fireball.NS_Fireball"));BeamFX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_Lightning.NS_Lightning"));
    FActorSpawnParameters Params;Params.Owner=this;
    for(int32 I=0;I<256;++I) if(auto* Shot=GetWorld()->SpawnActor<AMVPProjectile>(FVector::ZeroVector,FRotator::ZeroRotator,Params)) PracticeProjectiles.Add(Shot);
    return true;
}
void AMortisTestingDirector::Freeze(AActor* Actor)
{
    if(!IsValid(Actor)) return;
    auto& State=Frozen.AddDefaulted_GetRef();State.Actor=Actor;State.bTick=Actor->IsActorTickEnabled();State.bHidden=Actor->IsHidden();State.bCollision=Actor->GetActorEnableCollision();
    TInlineComponentArray<UActorComponent*> Components(Actor);
    for(auto* Component:Components) if(Component->IsComponentTickEnabled()) {State.EnabledTicks.Add(Component);Component->SetComponentTickEnabled(false);}
    Actor->SetActorTickEnabled(false);Actor->SetActorHiddenInGame(true);Actor->SetActorEnableCollision(false);
}
bool AMortisTestingDirector::Enter(AMVPController* Controller,bool bPaused,FString& Error)
{
    if(bActive) return ApplySetup(Error);
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    if(!Game || !Controller || !Game->GetArenaPlayer() || !PrepareRange(Error)) return false;
    OriginalPawn=Game->GetArenaPlayer();OriginalSpawner=Game->Spawner;OriginalArsenal=Game->ArsenalDirector;
    PracticePawn=GetWorld()->SpawnActor<AMortisCharacter>(FVector(0,0,20055),FRotator::ZeroRotator);
    if(!PracticePawn) {Error=TEXT("Could not create practice player.");return false;}
    bReturnPaused=bPaused;bSavedMenu=Game->bInCodex;bSavedDraft=Game->bChoosingCard;bSavedEnded=Game->bRunEnded;
    if(OriginalPawn->Challenges) OriginalPawn->Challenges->SetTestingSuspended(true);
    OriginalSpawner->SetWavesPaused(true);
    for(ASpellBase* Spell:OriginalPawn->Deck->ActiveWeapons) if(IsValid(Spell)) Spell->SetCastingPaused(true);
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        auto* Actor=*It;if(Actor==PracticePawn) continue;
        if(Actor==OriginalPawn || Actor==OriginalArsenal || Cast<AMortisProgressionManager>(Actor)) Freeze(Actor);
        else if(auto* Enemy=Cast<AMVPEnemy>(Actor);Enemy && Enemy->IsPoolActive()) Freeze(Enemy);
        else if(auto* Gem=Cast<AMVPGem>(Actor);Gem && Gem->bActive) Freeze(Gem);
        else if(auto* Shot=Cast<AMortisProjectileBase>(Actor);Shot && Shot->bActive) {OriginalShots.Add(Actor);Freeze(Actor);}
    }
    if(Game->SelectionWidget) Game->SelectionWidget->SetVisibility(ESlateVisibility::Collapsed);
    PracticePawn->Deck->UnlockedCards=OriginalPawn->Deck->UnlockedCards;PracticePawn->Deck->EnemySpawner=PracticeSpawner;
    PracticePawn->bBenchmarkInvulnerable=true;PracticePawn->PreferredCameraDistance=OriginalPawn->PreferredCameraDistance;PracticePawn->CameraBoom->TargetArmLength=PracticePawn->PreferredCameraDistance;
    Game->Spawner=PracticeSpawner;Game->bInCodex=false;Game->bChoosingCard=false;Game->bRunEnded=false;
    bActive=true;SetActorHiddenInGame(false);Controller->Possess(PracticePawn);
    if(!ApplySetup(Error)) {Exit(Controller);return false;}return true;
}
void AMortisTestingDirector::ClearTestShots()
{
    for(AMVPProjectile* Shot:PracticeProjectiles) if(IsValid(Shot)) Shot->Deactivate();
    if(PracticeArsenal) {PracticeArsenal->Destroy();PracticeArsenal=nullptr;}
}
void AMortisTestingDirector::EmitProjectile(FVector Origin,FVector Direction,float Damage,const FSpellModifiers& Modifiers,FLinearColor Color,EProjectileType Type,ENeonSpellStyle Style)
{
    for(AMVPProjectile* Shot:PracticeProjectiles) if(IsValid(Shot) && !Shot->bActive)
    {
        Shot->bBloodSpell=false;Shot->SetSpellStyle(Type==EProjectileType::PhysicalMesh ? ENeonSpellStyle::Kinetic : Style);
        Shot->SetupProjectileVisuals(Type,ProjectileMesh,Type==EProjectileType::ContinuousBeam ? BeamFX.Get() : Type==EProjectileType::ConcentratedElemental ? ProjectileFX.Get() : nullptr);
        if(Type==EProjectileType::ContinuousBeam) Shot->FireContinuousBeam(Origin,Origin+Direction.GetSafeNormal()*1200.f,Damage,Modifiers);
        else Shot->Launch(Origin,Direction,Damage,Modifiers,Color);
        return;
    }
}
void AMortisTestingDirector::ActivateTarget(FTrainingTarget& Target)
{
    auto* Enemy=Cast<AMVPEnemy>(PracticeSpawner->SpawnEnemyFromPool(Target.Position));
    Target.Enemy=Enemy;Target.bAlive=Enemy!=nullptr;
    if(!Enemy) return;
    Enemy->bTrainingTarget=true;Enemy->ApplyEnemyArchetype(Target.Archetype);Enemy->SetActorLocation(Target.Position);Enemy->SetActorTickEnabled(false);
    if(auto* Art=Enemy->FindComponentByClass<UNecromancyVisualComponent>()) Art->UpdateLocomotion(FVector::ForwardVector,0);
}
bool AMortisTestingDirector::ApplySetup(FString& Error)
{
    if(!bActive || SelectedCards.IsEmpty() || !PracticePawn) {Error=TEXT("Practice session unavailable.");return false;}
    TArray<TObjectPtr<UCardDataAsset>> Cards;
    for(FName ID:SelectedCards) {auto* Card=FindCard(ID);if(!Card) {Error=TEXT("Selected attack unavailable.");return false;}Cards.Add(Card);}
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    PracticePawn->Deck->ResetActiveSpells();PracticePawn->ClearTalismans();ClearTestShots();
    for(auto& Target:Targets) if(Target.bAlive && Target.Enemy.IsValid()) Target.Enemy->ReturnToPool();
    Targets.Reset();for(auto& Number:Numbers) Number.Age=-1;
    HitCount=0;Defeats=0;TotalDamage=0;Elapsed=0;
    TArray<int32> Remaining=EnemyCounts;TArray<int32> Types;bool More=true;
    while(More) {More=false;for(int32 I=0;I<10;++I) if(Remaining[I]>0) {Types.Add(I);--Remaining[I];More=true;}}
    const int32 Columns=FMath::CeilToInt(FMath::Sqrt(float(Types.Num()))),Rows=FMath::DivideAndRoundUp(Types.Num(),Columns);
    for(int32 I=0;I<Types.Num();++I)
    {
        auto& Target=Targets.AddDefaulted_GetRef();Target.Archetype=Types[I];
        Target.Position=FVector((I/Columns-(Rows-1)*0.5f)*GridSpacing,(I%Columns-(Columns-1)*0.5f)*GridSpacing,20021+CyberEnemies::Roster()[Types[I]].CollisionRadius);
        ActivateTarget(Target);
    }
    PracticePawn->Movement->StopMovementImmediately();PracticePawn->SetActorLocation(FVector(-Rows*GridSpacing*0.5f-110.f,0,20055));PracticePawn->VerticalSpeed=0;
    PracticeArsenal=GetWorld()->SpawnActor<ANeonArsenalDirector>();Game->ArsenalDirector=PracticeArsenal;
    for(UCardDataAsset* Card:Cards)
    {
        PracticePawn->Deck->EquippedDeck={Card};PracticePawn->Deck->DiscardHand();PracticePawn->Deck->DrawLevelUpHand();
        if(!PracticePawn->Deck->AddSelectedCardSpell(Card)) {Error=TEXT("Could not equip a selected attack.");return false;}
    }
    PracticePawn->Deck->EquippedDeck=Cards;
    CardCaption=Cards.Num()==1 ? Cards[0]->Name.ToString() : FString::Printf(TEXT("%d ATTACKS ENABLED / HOVER A NAME FOR STATS"),Cards.Num());return true;
}
void AMortisTestingDirector::RecordDamage(AMVPEnemy* Enemy,float Amount,bool bDefeated)
{
    if(!bActive || !IsValid(Enemy) || Amount<=0) return;
    ++HitCount;TotalDamage+=Amount;
    auto* Number=Numbers.FindByPredicate([Enemy](const auto& N){return N.Age>=0 && N.Age<0.055f && N.Enemy==Enemy;});
    if(Number) Number->Amount+=Amount;
    else {auto& New=Numbers[NumberCursor++%Numbers.Num()];New.Enemy=Enemy;New.Position=Enemy->GetCombatVFXLocation()+FVector(0,0,40);New.Amount=Amount;New.Age=0;}
    if(bDefeated)
    {
        ++Defeats;for(auto& Target:Targets) if(Target.bAlive && Target.Enemy==Enemy) {Target.bAlive=false;Target.ResetAfter=Elapsed+1.5f;break;}
    }
}
void AMortisTestingDirector::Tick(float Dt)
{
    Super::Tick(Dt);if(!bActive) return;Elapsed+=Dt;
    for(auto& Number:Numbers) if(Number.Age>=0 && (Number.Age+=Dt)>0.85f) Number.Age=-1;
    if(bAutoReset) for(auto& Target:Targets) if(!Target.bAlive && Elapsed>=Target.ResetAfter) ActivateTarget(Target);
}
void AMortisTestingDirector::Exit(AMVPController* Controller)
{
    if(!bActive || !Controller) return;
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    PracticePawn->Deck->ResetActiveSpells();PracticePawn->ClearTalismans();ClearTestShots();
    for(auto& Target:Targets) if(Target.bAlive && Target.Enemy.IsValid()) Target.Enemy->ReturnToPool();Targets.Reset();
    Game->Spawner=OriginalSpawner;Game->ArsenalDirector=OriginalArsenal;
    Game->bInCodex=bSavedMenu;Game->bChoosingCard=bSavedDraft;Game->bRunEnded=bSavedEnded;bActive=false;
    Controller->Possess(OriginalPawn);
    for(auto& State:Frozen) if(auto* Actor=State.Actor.Get())
    {
        Actor->SetActorHiddenInGame(State.bHidden);Actor->SetActorEnableCollision(State.bCollision);Actor->SetActorTickEnabled(State.bTick);
        for(auto& Tick:State.EnabledTicks) if(Tick.IsValid()) Tick->SetComponentTickEnabled(true);
    }
    Frozen.Reset();OriginalShots.Reset();OriginalSpawner->SetWavesPaused(false);
    if(OriginalPawn->Challenges) OriginalPawn->Challenges->SetTestingSuspended(false);
    for(ASpellBase* Spell:OriginalPawn->Deck->ActiveWeapons) if(IsValid(Spell)) Spell->SetCastingPaused(false);
    PracticePawn->Destroy();PracticePawn=nullptr;OriginalPawn=nullptr;SetActorHiddenInGame(true);
    Controller->SetPause(bReturnPaused);Controller->SetMenuInput(bSavedMenu || bSavedDraft || bSavedEnded);
    if(bSavedDraft && Game->SelectionWidget)
    {
        Game->SelectionWidget->SetVisibility(ESlateVisibility::Visible);Game->SelectionWidget->SetIsEnabled(true);
        FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(Game->SelectionWidget->TakeWidget());Controller->SetInputMode(Input);
    }
}
