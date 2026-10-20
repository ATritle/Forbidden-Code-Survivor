#include "MVPGame.h"
#include "ForbiddenRiderSubsystem.h"
#include "ForbiddenPrograms.h"
#include "ForbiddenDistrictDirector.h"
#include "ForbiddenEnemyBrain.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenSpirit.h"
#include "ForbiddenCombatSubsystem.h"
#include "ForbiddenSettingsControls.h"
#include "ForbiddenScoreService.h"
#include "ForbiddenScoreWidget.h"
#include "ForbiddenCodeBranding.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "NeonArsenal.h"
#include "CyberEnemyRoster.h"
#include "MortisTerrain.h"
#include "BloodSurfaceDirector.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "MortisProgressionManager.h"
#include "EngineUtils.h"
#include "MortisCharacter.h"
#include "CodexCardSelectionWidget.h"
#include "GameFramework/SpringArmComponent.h"
#include "ExplosionBenchmark.h"
#include "NecromancyVisualComponent.h"
#include "SkeletonMinion.h"
#include "CinematicExplosionDirector.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NeonCityDirector.h"
#include "NeonAudioDirector.h"
#include "KineticCardVFXDirector.h"
#include "NeonSpellImpactDirector.h"
#include "MortisTestingDirector.h"
#include "NeonChallenge.h"

void InstallReleaseValidation(AMVPController* Controller);
void InstallForbiddenValidation(AMVPController* Controller);
void InstallForbiddenSystemsValidation(AMVPController* Controller);
void InstallForbiddenHelpValidation(AMVPController* Controller);
void InstallNeonMenuAudioValidation(AMVPController* Controller);

namespace MVP
{
    constexpr ECollisionChannel EnemyChannel = ECC_GameTraceChannel1;
    constexpr float ArenaLimit = 7000.f;

    void SetupVisual(UStaticMeshComponent* Visual, UStaticMesh* Mesh, FVector Scale)
    {
        Visual->SetStaticMesh(Mesh);
        Visual->SetRelativeScale3D(Scale);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetGenerateOverlapEvents(false);
        Visual->SetCastShadow(false);
    }

    void Tint(UStaticMeshComponent* Visual, FLinearColor Color)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MVP/M_Prototype.M_Prototype"));
        if (!Material) Material = UMaterial::GetDefaultMaterial(MD_Surface);
        UMaterialInstanceDynamic* Instance = Cast<UMaterialInstanceDynamic>(Visual->GetMaterial(0));
        if (!Instance) Instance = UMaterialInstanceDynamic::Create(Material, Visual);
        Instance->SetVectorParameterValue(TEXT("Color"), Color);
        Visual->SetMaterial(0, Instance);
    }

    AMVPArenaGameMode* Mode(const UObject* Context)
    {
        return Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(Context));
    }
}

AMVPPlayer::AMVPPlayer()
{
    PrimaryActorTick.bCanEverTick = true;
    Challenges=CreateDefaultSubobject<UNeonChallengeComponent>(TEXT("DangerContracts"));
    USphereComponent* Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
    Body->InitSphereRadius(30.f);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionObjectType(ECC_Pawn);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    SetRootComponent(Body);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerVisual"));
    Visual->SetupAttachment(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cone.Cone"));
    MVP::SetupVisual(Visual, Shape.Object, FVector(0.65f, 0.65f, 1.1f));
    CharacterArt = CreateDefaultSubobject<UNecromancyVisualComponent>(TEXT("NecromancerArt"));
    CharacterArt->SetupAttachment(Body);
    CharacterArt->SetLightingChannels(true,true,false);
    Visual->SetLightingChannels(true,true,false);
    LandingDust=CreateDefaultSubobject<UNiagaraComponent>(TEXT("LandingDust"));
    LandingDust->SetupAttachment(Body); LandingDust->SetAutoActivate(false);
    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->SetUpdatedComponent(Body);
    Movement->MaxSpeed = 470.f;
    Movement->Acceleration = 4200.f;
    Movement->Deceleration = 6000.f;
    Movement->SetPlaneConstraintEnabled(false);
    Movement->SetPlaneConstraintNormal(FVector::UpVector);
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Body);
    CameraBoom->TargetArmLength = 2100.f;
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bUsePawnControlRotation = false;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    UCameraComponent* Camera = FollowCamera;
    Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->FieldOfView = 65.f;
    Camera->bConstrainAspectRatio = false;
    Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Camera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
    Camera->PostProcessSettings.AutoExposureBias = 0.f;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Deck = CreateDefaultSubobject<UCodexDeckComponent>(TEXT("CodexDeck"));
    WeaponMuzzle = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponMuzzle"));
    WeaponMuzzle->SetupAttachment(Body);
    WeaponMuzzle->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
}

void AMVPPlayer::BeginPlay()
{
    Super::BeginPlay();
    MVP::Tint(Visual, FLinearColor(0.55f, 1.f, 0.82f));
    MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.f, MaxHealth) : 100.f;
    Health = MaxHealth;
    ForbiddenSettings::Apply(this);
    ForbiddenSettings::ApplyGraphics(this);
    LoadCameraPreference();
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data)
        TargetPriority=FMath::Clamp(FMath::RoundToInt(Profile->Data->Settings.FindRef("TargetPriority")),0,2);
    for(TActorIterator<ANeonCityDirector> City(GetWorld());City;++City)
    {CameraBoom->SetRelativeRotation(FRotator(-55.f,-35.f,0));break;}
    LandingDust->SetAsset(LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_LandingDust.NS_LandingDust")));
    if (ApplySelectedRider()) {Visual->SetVisibility(false);}
    else if (CharacterArt->LoadCharacter(TEXT("Necromancer"),145.f)) Visual->SetVisibility(false);
    CharacterArt->SetCastShadow(GetWorld()->GetMapName().Contains(TEXT("NeonDistrict")) || FParse::Param(FCommandLine::Get(), TEXT("CinematicFX")) || FParse::Param(FCommandLine::Get(), TEXT("CinematicTest")));
}

void AMVPPlayer::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"), this, &AMVPPlayer::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &AMVPPlayer::MoveRight);
    Input->BindAxis(TEXT("CameraZoom"), this, &AMVPPlayer::ZoomCamera);
    auto Bindings=ForbiddenSettings::DefaultBindings();
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) for(const auto& Pair:Profile->Data->Bindings) Bindings.Add(Pair.Key,Pair.Value);
    Input->BindKey(Bindings["Boost"],IE_Pressed,this,&AMVPPlayer::Boost);
    Input->BindKey(EKeys::Gamepad_FaceButton_Bottom,IE_Pressed,this,&AMVPPlayer::Boost);
    Input->BindKey(Bindings["Mark"],IE_Pressed,this,&AMVPPlayer::MarkTarget);
    if(auto* PC=Cast<AMVPController>(Controller)) Input->BindKey(Bindings["Interact"],IE_Pressed,PC,&AMVPController::InteractChallenge);
    Input->BindKey(EKeys::Gamepad_RightThumbstick,IE_Pressed,this,&AMVPPlayer::MarkTarget);
    Input->BindKey(EKeys::T,IE_Pressed,this,&AMVPPlayer::CycleTargetPriority);
}
void AMVPPlayer::MoveForward(float Value) { AddMovementInput(FRotator(0,CameraBoom->GetComponentRotation().Yaw,0).Vector(),Value); }
void AMVPPlayer::MoveRight(float Value) { AddMovementInput(FRotationMatrix(FRotator(0,CameraBoom->GetComponentRotation().Yaw,0)).GetUnitAxis(EAxis::Y),Value); }

void AMVPPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, PreferredCameraDistance, DeltaSeconds, 10.f);
    if (bZoomDirty && (ZoomSaveDelay -= DeltaSeconds) <= 0.f) { SaveCameraPreference(); ZoomSaveDelay = 5.f; }
    InvulnerabilityRemaining = FMath::Max(0.f, InvulnerabilityRemaining - DeltaSeconds);
    BoostCooldown=FMath::Max(0.f,BoostCooldown-DeltaSeconds);
    LandingRewardCooldown=FMath::Max(0.f,LandingRewardCooldown-DeltaSeconds);
    LandingCharge=FMath::Max(0.f,LandingCharge-DeltaSeconds);
    if(const auto* Guard=Deck->FindOwnedSpell("TalismanGuard");Guard && Guard->Modifiers.Evolution==1 && GetTalismanCount()>0 && (TalismanRecoveryRemaining-=DeltaSeconds)<=0)
    {AddTalismans(1);TalismanRecoveryRemaining=18.f;}
    if(BoostRemaining>0)
    {
        const float Step=FMath::Min(BoostRemaining,DeltaSeconds);BoostRemaining-=Step;
        FHitResult Hit;AddActorWorldOffset(BoostDirection*1050.f*Step,true,&Hit);
        if(Hit.bBlockingHit) BoostRemaining=0;
    }
    // A finite arena keeps both the player and follow camera on the floor.
    FVector Position = GetActorLocation();
    const auto* ArenaGame=MVP::Mode(this);
    const float Limit=ArenaGame && ArenaGame->TestingDirector && ArenaGame->TestingDirector->IsActive() ? 4300.f : MVP::ArenaLimit;
    Position.X = FMath::Clamp(Position.X, -Limit, Limit);
    Position.Y = FMath::Clamp(Position.Y, -Limit, Limit);

    SetActorLocation(Position,true);
    UpdateGroundPhysics(DeltaSeconds);
    const bool bVisible = InvulnerabilityRemaining <= 0.f || ForbiddenSettings::Get(this,"ReducedFlashes",0)>0.5f || FMath::Fmod(InvulnerabilityRemaining, 0.15f) < 0.09f;
    Visual->SetVisibility(!CharacterArt->IsArtReady() && bVisible);
    if (CharacterArt->IsArtReady())
    {
        CharacterArt->SetVisibility(bVisible);
        if(CharacterArt->IsHoverRider()) CharacterArt->UpdateRider(DeltaSeconds,Movement->Velocity,bFalling,LandingCount,Health<=0.f);
        else CharacterArt->UpdateLocomotion(Movement->Velocity, Movement->Velocity.Size2D());
        UpdateNeonVFX(DeltaSeconds,bVisible);
    }
}

void AMVPPlayer::ReceiveContactDamage(float Amount)
{
    if(bBenchmarkInvulnerable) return;
    AMVPArenaGameMode* Game = MVP::Mode(this);
    if (!Game || Game->bInCodex || Game->bRunEnded || Game->bChoosingCard || UGameplayStatics::IsGamePaused(this) || InvulnerabilityRemaining > 0.f || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    float EffectiveDamage=Amount*(Challenges ? Challenges->GetIncomingDamageMultiplier() : 1.f);
    if(auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>()) EffectiveDamage=Rider->AbsorbContact(EffectiveDamage);
    Health = FMath::Max(0.f, Health - EffectiveDamage);
    InvulnerabilityRemaining = 0.8f;
    CharacterArt->TriggerRiderHit();
    OnPlayerDamagedVFX(Health / MaxHealth);
    if (Health <= 0.f) Game->FinishRun();
}

void AMVPPlayer::ReceiveChallengeDamage(float Amount,bool bPlayHitFeedback)
{
    auto* Game=MVP::Mode(this);
    if(bBenchmarkInvulnerable || !Game || Game->bInCodex || Game->bRunEnded || Game->bChoosingCard || UGameplayStatics::IsGamePaused(this) || !FMath::IsFinite(Amount) || Amount<=0) return;
    Health=FMath::Max(0.f,Health-Amount*(Challenges ? Challenges->GetIncomingDamageMultiplier() : 1.f));
    if(bPlayHitFeedback) CharacterArt->TriggerRiderHit();
    OnPlayerDamagedVFX(Health/FMath::Max(1.f,MaxHealth));
    if(Health<=0) Game->FinishRun();
}

AActor* AMVPPlayer::GetClosestEnemy(float ScanRadius) const
{
    if(!FMath::IsFinite(ScanRadius) || ScanRadius<=0 || !Deck || !IsValid(Deck->EnemySpawner)) return nullptr;
    auto Visible=[&](AMVPEnemy* E)
    {
        if(!E || !E->IsPoolActive() || FVector::DistSquared(GetActorLocation(),E->GetActorLocation())>FMath::Square(ScanRadius)) return false;
        FHitResult Wall;return !GetWorld()->LineTraceSingleByObjectType(Wall,GetActorLocation(),E->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic));
    };
    if(TargetPriority==2 && MarkedEnemy.IsValid() && MarkedEnemy->Generation==MarkedGeneration && Visible(MarkedEnemy.Get())) return MarkedEnemy.Get();
    if(TargetPriority==1)
    {
        AMVPEnemy* Best=nullptr;
        for(APooledEnemy* A:Deck->EnemySpawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);Visible(E) && (!Best || E->MaxHealth>Best->MaxHealth)) Best=E;
        if(Best) return Best;
    }
    return Deck->EnemySpawner->FindNearestEnemy(GetActorLocation(),ScanRadius,true);
}

AMVPEnemy::AMVPEnemy()
{
    CreateDefaultSubobject<UForbiddenEnemyBrain>(TEXT("CombatBrain"));
    PrimaryActorTick.bStartWithTickEnabled = true;
    USphereComponent* HitShape = CreateDefaultSubobject<USphereComponent>(TEXT("EnemyHitShape"));
    HitShape->InitSphereRadius(34.f);
    SetRootComponent(HitShape);
    Terrain=CreateDefaultSubobject<UMortisTerrain>(TEXT("Terrain"));
    HitShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitShape->SetCollisionObjectType(MVP::EnemyChannel);
    HitShape->SetCollisionResponseToAllChannels(ECR_Ignore);
    HitShape->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    // Object queries used by projectiles still detect this shape; no crowd blocking.
    HitShape->SetGenerateOverlapEvents(false);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnemyVisual"));
    Visual->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    MVP::SetupVisual(Visual, Shape.Object, FVector(0.65f, 0.65f, 0.9f));
    CharacterArt = CreateDefaultSubobject<UNecromancyVisualComponent>(TEXT("RevenantArt"));
    CharacterArt->SetupAttachment(RootComponent);
    CharacterArt->SetLightingChannels(true,true,false);
    Visual->SetLightingChannels(true,true,false);
}

void AMVPEnemy::BeginPlay()
{
    Super::BeginPlay();
    ApplyEnemyArchetype(0);
}

void AMVPEnemy::OnPoolActivated_Implementation()
{
    ++Generation;
    SetCombatHitFlash(FLinearColor::Black,0);
    if(auto* Game=MVP::Mode(this);Game && !bTrainingTarget && !(Game->TestingDirector && Game->TestingDirector->IsActive()))
    {
        const bool Boss=(Game->RunSeconds>=360.f || Game->bBossRequested) && !Game->bBroadcastBossSpawned && !Game->bSuppressBossSelection;
        int32 Choice=CyberEnemies::Select(Game->RunSeconds,FMath::FRand());
        if(!Boss && Game->RunSeconds>=180.f && ForbiddenSettings::Get(this,"District",0)>0.5f && FMath::FRand()<0.12f) Choice=FMath::RandRange(6,8);
        ApplyEnemyArchetype(Boss ? 9 : Choice);
        if(Boss) Game->bBroadcastBossSpawned=true;
    }
    Terrain->ResetPath();
    UMortisTerrain::Ground(this);
    Health = FMath::Max(1.f, MaxHealth);
    CachedChaseDirection = FVector::ZeroVector;
    DirectionUpdateRemaining = FMath::FRandRange(0.f, 0.05f);
    StaggerRemaining=0;
    Target = Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (Target.IsValid()) CachedChaseDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    MVP::Tint(Visual, FLinearColor(0.65f, 0.08f, 0.17f));
    Visual->SetVisibility(!CharacterArt->IsArtReady());
    CharacterArt->SetDormant(false);
    if(auto* Brain=FindComponentByClass<UForbiddenEnemyBrain>()) Brain->ResetLife();
    SetActorTickEnabled(true);
}
void AMVPEnemy::OnPoolReturned_Implementation()
{
    bTrainingTarget=false;
    Tags.Remove("BroadcastNode");
    if(auto* Brain=FindComponentByClass<UForbiddenEnemyBrain>()) Brain->ResetLife();
    SetCombatHitFlash(FLinearColor::Black,0);
    Target.Reset();
    CharacterArt->SetDormant(true);
}

void AMVPEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bTrainingTarget) return;
    if (!IsPoolActive()) return;
    if(StaggerRemaining>0) {if(auto* Brain=FindComponentByClass<UForbiddenEnemyBrain>()) Brain->CancelChannel();StaggerRemaining=FMath::Max(0.f,StaggerRemaining-DeltaSeconds);return;}
    if (!Target.IsValid()) Target = Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Target.IsValid()) return;
    if(Target->Challenges) Target->Challenges->ResolveOrbContact(this);
    if(Target->Health<=0) return;
    const FVector ToPlayer = Target->GetActorLocation() - GetActorLocation();
    // Only pose evaluation is budgeted. Collision, targeting and enemy combat stay at actor rate.
    CharacterArt->SetComponentTickInterval(EnemyTier>=3 || ToPlayer.SizeSquared2D()<FMath::Square(900.f) ? 0.f : ToPlayer.SizeSquared2D()<FMath::Square(1700.f) ? 1.f/30.f : 1.f/15.f);
    DirectionUpdateRemaining -= DeltaSeconds;
    if (DirectionUpdateRemaining <= 0.f || CachedChaseDirection.IsNearlyZero())
    {
        CachedChaseDirection = ToPlayer.GetSafeNormal2D();
        DirectionUpdateRemaining = 0.05f;
    }
    const float ContactRange=CastChecked<USphereComponent>(RootComponent)->GetScaledSphereRadius()+34.f;
    float ChaseSpeed=BaseMovementSpeed*(Target->Challenges ? Target->Challenges->GetPursuerSpeedMultiplier() : 1.f);
    FVector ChaseGoal=Target->GetActorLocation();
    const auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>();
    if(Rider && Rider->DecoyGoal(this,ChaseGoal)) {if(auto* Brain=FindComponentByClass<UForbiddenEnemyBrain>()) Brain->CancelChannel();}
    else if(auto* Brain=FindComponentByClass<UForbiddenEnemyBrain>();Brain && Brain->Advance(DeltaSeconds,Target.Get(),ChaseGoal,ChaseSpeed)) return;
    CharacterArt->UpdateLocomotion(ToPlayer, ToPlayer.SizeSquared2D() <= FMath::Square(ContactRange) ? 0.f : ChaseSpeed);
    if (ToPlayer.SizeSquared2D() <= FMath::Square(ContactRange) && FMath::Abs(ToPlayer.Z)<150.f && UMortisTerrain::ClearSight(this,Target->GetActorLocation()))
    {
        if(auto* G=MVP::Mode(this)) G->LastDamageCause=CyberEnemies::Roster()[FMath::Clamp(EnemyArchetype,0,9)].Name;
        if(EnemyArchetype>=3) Target->ReceiveContactDamage(ContactDamage);
    }
    else
        Terrain->MoveTowards(ChaseGoal,ChaseSpeed,DeltaSeconds);
}

void AMVPEnemy::ReceiveSpellDamage(float Amount) { TakeSpellDamage(Amount, FVector::ZeroVector); }
void AMVPEnemy::TakeSpellDamage(float Amount, FVector ImpactVelocity, FName ProgramID,bool TriggerEcho)
{
    if (!IsPoolActive() || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    auto* ArenaMode=MVP::Mode(this);
    auto* Contracts=ArenaMode && ArenaMode->GetArenaPlayer() ? ArenaMode->GetArenaPlayer()->Challenges.Get() : nullptr;
    if(!bTrainingTarget && Contracts && ProgramID!="TalismanGuard") Amount*=Contracts->GetDamageMultiplier();
    auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>();
    if(Rider) Amount=Rider->ModifyDamage(this,Amount,ProgramID,TriggerEcho);
    if(auto* Brain=FindComponentByClass<UForbiddenEnemyBrain>()) Amount=Brain->ModifyIncoming(Amount,ImpactVelocity,ProgramID);
    const float Actual=FMath::Min(Amount,FMath::Max(0.f,Health));
    if(ArenaMode) ArenaMode->RecordProgramDamage(ProgramID,Actual);
    Health -= Amount;
    if(Rider) Rider->OnDamageConfirmed(this,ProgramID,TriggerEcho,Actual);
    if(!bTrainingTarget && Contracts && TriggerEcho) Contracts->OnConfirmedSpellHit(GetActorLocation(),FMath::Min(Actual,250.f));
    if(bTrainingTarget)
    {
        if(auto* Game=MVP::Mode(this);Game && Game->TestingDirector) Game->TestingDirector->RecordDamage(this,Actual,Health<=0);
        if(Health<=0) ReturnToPool();
        return;
    }
    if (Health > 0.f) return;
    const FVector DeathPosition = GetActorLocation();
    ReturnToPool(); // Inactivate before callbacks; death cannot pay twice.
    if(Contracts) Contracts->OnEnemyKilled(this,Generation);
    PlayHighDetailDeathVFX(DeathPosition, ImpactVelocity.ContainsNaN() ? FVector::ZeroVector : ImpactVelocity);
    if (AMVPArenaGameMode* Game = MVP::Mode(this)) {Game->EnemyDefeated(DeathPosition,EssenceReward);Game->OnArchetypeKilled(EnemyArchetype,DeathPosition);}
}

AMVPImpactPulse::AMVPImpactPulse()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlastPulse"));
    SetRootComponent(Visual);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    MVP::SetupVisual(Visual,Shape.Object,FVector(0.01f));
    SetActorHiddenInGame(true);
}
void AMVPImpactPulse::Activate(FVector Position,float Radius)
{
    SetActorLocation(FVector(Position.X,Position.Y,4.f));
    TargetRadius = Radius;
    Age = 0.f;
    bActive = true;
    Visual->SetWorldScale3D(FVector(0.01f));
    MVP::Tint(Visual,FLinearColor(1.f,0.02f,0.55f));
    SetActorHiddenInGame(false);
    SetActorTickEnabled(true);
}
void AMVPImpactPulse::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age += DeltaSeconds;
    if (Age >= 0.2f)
    {
        bActive=false;
        SetActorHiddenInGame(true);
        SetActorTickEnabled(false);
        return;
    }
    const float T=Age/0.2f;
    Visual->SetWorldScale3D(FVector(TargetRadius*T/50.f,TargetRadius*T/50.f,0.025f));
    MVP::Tint(Visual,FLinearColor(1.f,0.02f,0.55f)*(1.f-T));
}
void AMVPArenaGameMode::EmitImpactPulse(FVector Position,float Radius)
{
    if (ExplosionDirector && ExplosionDirector->HasAuthoredEffects()) { ExplosionDirector->TriggerExplosion(Position,Radius); return; }
    for (AMVPImpactPulse* Pulse : ImpactPulses)
        if (IsValid(Pulse) && !Pulse->bActive) { Pulse->Activate(Position,Radius); return; }
}

AMVPGem::AMVPGem()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gem"));
    SetRootComponent(Visual);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    MVP::SetupVisual(Visual, Shape.Object, FVector(0.14f));
    Aura=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EssenceAura"));
    Aura->SetupAttachment(Visual);
    MVP::SetupVisual(Aura,Shape.Object,FVector(2.4f));
    SetActorHiddenInGame(true);
}
void AMVPGem::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Core=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Spells/M_EssenceOrb.M_EssenceOrb"))) Visual->SetMaterial(0,Core);
    if(auto* Halo=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Spells/M_EssenceAura.M_EssenceAura"))) Aura->SetMaterial(0,Halo);
}
void AMVPGem::Activate(FVector Position, int32 Value)
{
    Position.Z -= 25.f;
    SetActorLocation(Position);
    SetActorRotation(FRotator(25.f, 45.f, 20.f));
    XPValue = Value; PulseAge=FMath::FRandRange(0.f,6.f);
    bActive = true;
    SetActorHiddenInGame(false);
    SetActorTickEnabled(true);
}
void AMVPGem::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AMVPArenaGameMode* Game = MVP::Mode(this);
    AMVPPlayer* Player = Game ? Game->GetArenaPlayer() : nullptr;
    if (!bActive || !Player) return;
    FVector Delta = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Delta.Size();
    if (Distance < 65.f)
    {
        bActive = false;
        SetActorHiddenInGame(true);
        SetActorTickEnabled(false);
        Game->AddXP(XPValue);
    }
    else if (Distance < 260.f)
        AddActorWorldOffset(Delta.GetSafeNormal() * FMath::Min(800.f * DeltaSeconds, Distance));
    PulseAge+=DeltaSeconds;
    Visual->SetWorldScale3D(FVector(0.14f*(1.f+0.12f*FMath::Sin(PulseAge*4.f))));
}

AMVPBoltSpell::AMVPBoltSpell() { TargetRange = 1200.f;BoltColor=NeonSpellPalette::Color(ENeonSpellStyle::Pulse); }
AMVPEmberSpell::AMVPEmberSpell()
{
    AttackSound=TEXT("Plasma");
    SpellStyle=ENeonSpellStyle::Prism;
    ShotType=EProjectileType::ConcentratedElemental;
    BoltColor = FLinearColor(1.f, 0.015f, 0.5f);
    Modifiers.ProjectileCount = 3;
    Modifiers.ExplodeOnImpact = true;
}
AMVPLightningSpell::AMVPLightningSpell() { SpellStyle=ENeonSpellStyle::Kinetic;AttackSound=TEXT("Blaster"); ShotType=EProjectileType::PhysicalMesh; BoltColor=NeonSpellPalette::Color(SpellStyle); Modifiers.ExplodeOnImpact=true; }
AMVPBloodSpell::AMVPBloodSpell() { SpellStyle=ENeonSpellStyle::Photon;AttackSound=TEXT("Laser"); ShotType=EProjectileType::ConcentratedElemental; BoltColor=NeonSpellPalette::Color(SpellStyle); Modifiers.PiercingCount=3; }
AMVPFrostSpell::AMVPFrostSpell()
{
    AttackSound=TEXT("Blaster");
    SpellStyle=ENeonSpellStyle::Halo;BoltColor=NeonSpellPalette::Color(SpellStyle);
    Modifiers.ProjectileCount = 8;
    Modifiers.PiercingCount = 2;
    Modifiers.NovaPattern = true;
}
void AMVPBoltSpell::FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot)
{
    AMVPArenaGameMode* Game = MVP::Mode(this);
    if (!Game || !IsValid(GetOwner()) || !IsValid(Target) || !Target->IsPoolActive()) return;
    const FVector Origin = GetOwner()->GetActorLocation();
    if(Game->AudioDirector) Game->AudioDirector->PlayAttack(AttackSound,Origin);
    const FVector Aim = (Target->GetActorLocation() - Origin).GetSafeNormal();
    if (auto* Player = Cast<AMVPPlayer>(GetOwner()))
    {
        Player->WeaponMuzzle->SetWorldRotation(Aim.Rotation());
        Player->OnWeaponAim(Origin, Aim);
        Player->TriggerSpellCastVisual();
    }
    auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>();
    FSpellModifiers Pattern=Rider ? Rider->ModifySnapshot(Snapshot) : Snapshot;float Power=Damage;
    if(Snapshot.ProgramID=="HaloNova" && Snapshot.Evolution==2)
    {
        if(auto* Fields=GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()) Fields->QueueField("HaloNova",ENeonSpellStyle::Halo,Origin,Damage*1.8f,350.f*Pattern.RadiusMultiplier,0,1,0.65f,true);
        return;
    }
    if(UsesProgramProgression())
    {
        if(Snapshot.ProgramID=="PulseDart" && Snapshot.Evolution==2) {Pattern.NovaPattern=true;Pattern.ProjectileCount=8;Power*=0.1875f;}
        if(Snapshot.ProgramID=="PrismBurst" && Snapshot.Evolution==2) {Pattern.NovaPattern=true;Pattern.ProjectileCount=8;Power*=0.5f;}
        if(Snapshot.ProgramID=="KineticCards" && Snapshot.Evolution==2) {Pattern.ProjectileCount=3;Power*=0.6f;Pattern.ExplodeOnImpact=false;Pattern.PiercingCount=2;}
        if(Snapshot.ProgramID=="PhotonLance" && Snapshot.Evolution==2) {Pattern.ProjectileCount=2;Power*=0.6f;}
    }
    const int32 FirstShotBonus=Rider && !Pattern.NovaPattern && Snapshot.ProgramID!="HaloNova" ? Rider->ProjectileCast() : 0;
    for (int32 Index = 0; Index < Pattern.ProjectileCount; ++Index)
    {
        const float Angle = Pattern.NovaPattern ? 360.f * Index / Pattern.ProjectileCount : Snapshot.ProgramID=="PhotonLance" && Snapshot.Evolution==2 ? Index*90.f : (Index - (Pattern.ProjectileCount - 1) * 0.5f) * 9.f;
        FVector LaunchOrigin=Origin;
        if(Snapshot.ProgramID=="PhotonLance" && Snapshot.Evolution==2 && Index==1)
        {
            const FVector Cross=Aim.RotateAngleAxis(90.f,FVector::UpVector);
            LaunchOrigin=Target->GetActorLocation()-Cross*350.f;
            FHitResult Wall;
            if(GetWorld()->LineTraceSingleByObjectType(Wall,Target->GetActorLocation(),LaunchOrigin,FCollisionObjectQueryParams(ECC_WorldStatic))) LaunchOrigin=Wall.ImpactPoint+Cross*10.f;
        }
        FSpellModifiers Shot=Pattern;if(Index==0) Shot.PiercingCount+=FirstShotBonus;
        Game->EmitProjectile(LaunchOrigin, Aim.RotateAngleAxis(Angle, FVector::UpVector), Power, Shot, BoltColor, ShotType, bBlood,SpellStyle);
    }
}
APooledEnemy* AMVPBoltSpell::ResolveTarget() const
{
    const auto* Player=Cast<AMVPPlayer>(GetOwner());return Player ? Cast<APooledEnemy>(Player->GetClosestEnemy(TargetRange)) : Super::ResolveTarget();
}

AMVPController::AMVPController()
{
    PrimaryActorTick.bTickEvenWhenPaused = true;
    bShouldPerformFullTickWhenPaused = true;
    bShowMouseCursor = true;
    bEnableClickEvents = true;
}
void AMVPController::BeginPlay()
{
    Super::BeginPlay();
    if(UGameViewportClient* Viewport=GetWorld()->GetGameViewport())
        if(TSharedPtr<SWindow> Window=Viewport->GetWindow()) Window->SetTitle(FText::FromString(ForbiddenCode::Title));
    SetMenuInput(false);
    InstallReleaseValidation(this);
    InstallForbiddenValidation(this);
    InstallForbiddenSystemsValidation(this);
    InstallForbiddenHelpValidation(this);
    InstallNeonMenuAudioValidation(this);
    extern void InstallMusicPlayerValidation(AMVPController* Controller);
    InstallMusicPlayerValidation(this);
    extern void InstallRiderSelectionValidation(AMVPController* Controller);
    InstallRiderSelectionValidation(this);
    extern void InstallSettingsReviewValidation(AMVPController* Controller);
    InstallSettingsReviewValidation(this);
#if !UE_BUILD_SHIPPING
    bSmokeTest = FParse::Param(FCommandLine::Get(), TEXT("MVPSmoke"));
#endif
}
void AMVPController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bSmokeTest) TickSmokeTest();
}
void AMVPController::SetMenuInput(bool bMenu)
{
    // Game-only keeps numeric keyboard input routed to the controller while paused.
    // HUD cards are native canvas hit regions, not focusable Slate widgets.
    FInputModeGameOnly Input;
    Input.SetConsumeCaptureMouseDown(false);
    SetInputMode(Input);
    bShowMouseCursor = true;
    SetIgnoreMoveInput(bMenu);
    if (!bMenu) ResetIgnoreMoveInput();
    if (AMVPPlayer* ArenaPlayer = Cast<AMVPPlayer>(GetPawn())) ArenaPlayer->Movement->StopMovementImmediately();
}
void AMVPController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMVPController::ChooseOne).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMVPController::ChooseTwo).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AMVPController::ChooseThree).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AMVPController::Click).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AMVPController::RestartRun).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AMVPController::BeginSelectedRun).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMVPController::TestExplosion);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AMVPController::InteractChallenge);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &AMVPController::OpenHideout).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AMVPController::OpenHelp).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::L, IE_Pressed, this, &AMVPController::OpenLeaderboard).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AMVPController::OpenLeaderboard).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AMVPController::ToggleSettings).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMVPController::ToggleSettings).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&AMVPController::ScrollMusicUp).bExecuteWhenPaused=true;
    InputComponent->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&AMVPController::ScrollMusicDown).bExecuteWhenPaused=true;
}
void AMVPController::ScrollMusicUp() {if(auto* H=Cast<AMVPHUD>(GetHUD())) H->ScrollMusicPlaylist(-1);}
void AMVPController::ScrollMusicDown() {if(auto* H=Cast<AMVPHUD>(GetHUD())) H->ScrollMusicPlaylist(1);}
void AMVPController::TestExplosion()
{
    if (auto* Game=MVP::Mode(this); Game && Game->ExplosionDirector && !Game->bInCodex && GetPawn())
        Game->EmitImpactPulse(GetPawn()->GetActorLocation()+FVector(350,0,0),450);
}
bool AMVPController::IsChallengeOpen() const
{
    const auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn());return ArenaPlayer && ArenaPlayer->Challenges && ArenaPlayer->Challenges->OfferWidget!=nullptr;
}
void AMVPController::InteractChallenge()
{
    if(HelpWidget || SettingsWidget || ScoreWidget) return;
    if(auto* G=MVP::Mode(this);G && G->bInCodex) {OpenHideout();return;}
    if(auto* G=MVP::Mode(this);G && G->DistrictDirector && G->DistrictDirector->Interact()) return;
    if(auto* P=Cast<AMVPPlayer>(GetPawn());P && P->Challenges) P->Challenges->Interact();
}
void AMVPController::OpenHideout() {if(!HelpWidget) if(auto* G=MVP::Mode(this);G && G->bInCodex) {if(G->AudioDirector) G->AudioDirector->PlayMenuFeedback();G->OpenHideout();}}
void AMVPController::OpenLeaderboard()
{
    auto* G=MVP::Mode(this);if(!G || HelpWidget || ScoreWidget || IsSettingsOpen() || G->DecisionWidget || G->bChoosingCard || IsChallengeOpen()) return;
    ScoreWidget=CreateWidget<UForbiddenScoreWidget>(this,UForbiddenScoreWidget::StaticClass());if(!ScoreWidget) return;
    if(G->AudioDirector) G->AudioDirector->PlayMenuFeedback();
    ScoreWidget->WasPaused=IsPaused();SetPause(true);SetMenuInput(true);ScoreWidget->AddToViewport(90);
    FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);Input.SetWidgetToFocus(ScoreWidget->TakeWidget());SetInputMode(Input);
}
void AMVPController::BeginSelectedRun() { if (!HelpWidget && !SettingsWidget && !ScoreWidget) if (auto* Game = MVP::Mode(this)) {if(Game->bInCodex && Game->bCharacterSelectionOpen) Game->ContinueToDeck();else Game->StartSelectedRun();} }
void AMVPController::ChooseOne() { if (!HelpWidget && !IsSettingsOpen() && !IsChallengeOpen()) if (auto* Game = MVP::Mode(this)) Game->ChooseCard(0); }
void AMVPController::ChooseTwo() { if (!HelpWidget && !IsSettingsOpen() && !IsChallengeOpen()) if (auto* Game = MVP::Mode(this)) Game->ChooseCard(1); }
void AMVPController::ChooseThree() { if (!HelpWidget && !IsSettingsOpen() && !IsChallengeOpen()) if (auto* Game = MVP::Mode(this)) Game->ChooseCard(2); }
void AMVPController::Click()
{
    float X, Y;
    if (GetMousePosition(X, Y)) if (auto* HUD = Cast<AMVPHUD>(GetHUD())) HUD->ClickAt(X, Y);
}
void AMVPController::RestartRun()
{
    if(HelpWidget || IsSettingsOpen() || ScoreWidget) return;
    auto* Game = MVP::Mode(this);
    if(Game && Game->bChoosingCard && !Game->DecisionWidget)
    {
        UCodexDeckComponent* Deck=Game->GetArenaPlayer()->Deck;
        if(Deck->RerollHand() && Game->SelectionWidget) Game->SelectionWidget->InitializeSelectionScreen(Deck->DrawLevelUpHand());return;
    }
    if (IsSettingsOpen() || !Game || (!Game->bRunEnded && !Game->bChoosingCard)) return;
    SetPause(false);
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

AMVPArenaGameMode::AMVPArenaGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AMortisCharacter::StaticClass();
    CardSelectionWidgetClass = UCodexCardSelectionWidget::StaticClass();
    PlayerControllerClass = AMVPController::StaticClass();
    HUDClass = AMVPHUD::StaticClass();
}
void AMVPArenaGameMode::BeginPlay()
{
    Super::BeginPlay();
    BuildArena();
    // Wait for possession/local viewport to exist; avoids order dependencies with player spawn.
    GetWorldTimerManager().SetTimer(InitializeTimer, this, &AMVPArenaGameMode::InitializeRun, 0.1f, true);
}
AMVPPlayer* AMVPArenaGameMode::GetArenaPlayer() const
{
    return Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
}

void AMVPArenaGameMode::InitializeRun()
{
    AMVPPlayer* Player = GetArenaPlayer();
    if (!Player || bRunInitialized) return;
    for(const auto& Profile:CyberEnemies::Roster())
        for(const FString& Asset:{FString(TEXT("SK_"))+Profile.ID,FString(TEXT("A_Walk")),FString(TEXT("A_Run")),FString(TEXT("A_Attack"))})
        {
            const FString Path=FString(TEXT("/Game/Art/Characters/"))+Profile.ID+TEXT("/")+Asset+TEXT(".")+Asset;
            if(auto* Loaded=LoadObject<UObject>(nullptr,*Path)) TierArtAssets.Add(Loaded);
        }
    SpearMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    FireballFX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_Fireball.NS_Fireball"));
    BloodFX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_BloodOrb.NS_BloodOrb"));
    LightningFX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_Lightning.NS_Lightning"));
    // Legacy blood painting is excluded from energy runs.
    BloodDirector=nullptr;
    bRunInitialized = true;
    GetWorldTimerManager().ClearTimer(InitializeTimer);
    Player->SetActorLocation(FVector(0.f, 0.f, 55.f));
    AudioDirector=GetWorld()->SpawnActor<ANeonAudioDirector>();
    CardVFXDirector=GetWorld()->SpawnActor<AKineticCardVFXDirector>();
    SpellImpactDirector=GetWorld()->SpawnActor<ANeonSpellImpactDirector>();
    Spawner = GetWorld()->SpawnActorDeferred<AEnemySpawner>(AEnemySpawner::StaticClass(), FTransform::Identity);
    Spawner->EnemyClass = AMVPEnemy::StaticClass();
    Spawner->bAutoStart = false;
    Spawner->PoolSize = 240;
    Spawner->MaxActiveEnemies = 180;
    Spawner->ReservedSlots = 1;
    int32 StressCount=0;
    if (FParse::Value(FCommandLine::Get(),TEXT("StressEnemies="),StressCount))
    {
        Spawner->PoolSize=FMath::Clamp(StressCount,1,1000);
        Spawner->MaxActiveEnemies=Spawner->PoolSize;
    }
    Spawner->EnemiesPerWave = 8;
    Spawner->WaveInterval = 1.4f;
    Spawner->ArenaZ = 55.f;
    Spawner->bUseNavigation=true;
    Spawner->MinimumSpawnRadius = 0.f;
    Spawner->CameraMargin = 100.f;
    Spawner->SpawnRingWidth = 120.f;
    Spawner->FinishSpawning(FTransform::Identity);
    Player->Deck->EnemySpawner = Spawner;
    Player->Deck->bOfferOwnedSpellUpgrades = true;
    Player->Deck->MaximumActiveWeapons = 6;
    for (int32 Index = 0; Index < 256; ++Index) Projectiles.Add(GetWorld()->SpawnActor<AMVPProjectile>());
    for (int32 Index = 0; Index < 400; ++Index) Gems.Add(GetWorld()->SpawnActor<AMVPGem>());
    for (int32 Index = 0; Index < 32; ++Index) ImpactPulses.Add(GetWorld()->SpawnActor<AMVPImpactPulse>());
    // Defensive talismans replace summoned skeletons in the energy library.
    if (GetWorld()->GetMapName().Contains(TEXT("NeonDistrict")) || FParse::Param(FCommandLine::Get(),TEXT("CinematicFX")) || FParse::Param(FCommandLine::Get(),TEXT("CinematicTest")))
        ExplosionDirector=GetWorld()->SpawnActor<ACinematicExplosionDirector>();

    auto AddCard = [this](const TCHAR* ID, const TCHAR* Name, const TCHAR* Description, EMagicSchool School,
        float Damage, float Cooldown, TSubclassOf<ASpellBase> Class)
    {
        UCardDataAsset* Card = NewObject<UCardDataAsset>(this);
        Card->CardID = ID;
        Card->Name = FText::FromString(Name);
        Card->Description = FText::FromString(Description);
        Card->MagicSchool = School;
        Card->BaseDamage = Damage;
        Card->Cooldown = Cooldown;
        Card->SpellClass = Class;
        const FString ArtPath = FString::Printf(TEXT("/Game/UI/Codex/T_%s.T_%s"), ID, ID);
        Card->Icon = LoadObject<UTexture>(nullptr, *ArtPath);
        const TCHAR* Schools[] = {TEXT("Necromancy"),TEXT("Fire"),TEXT("Blood"),TEXT("Curses"),TEXT("Frost"),TEXT("Lightning"),TEXT("Pulse"),TEXT("Prism"),TEXT("Kinetic"),TEXT("Barrier")};
        Card->SchoolIcon = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/UI/Codex/T_School_%s.T_School_%s"), Schools[int32(School)], Schools[int32(School)]));
        Card->Rarity = School == EMagicSchool::Barrier ? ECardRarity::Epic : School == EMagicSchool::Prism ? ECardRarity::Rare : ECardRarity::Common;
        Card->RarityColor = Card->Rarity == ECardRarity::Epic ? FLinearColor(0.65f,0.35f,0.95f) : Card->Rarity == ECardRarity::Rare ? FLinearColor(0.25f,0.6f,1.f) : FLinearColor(0.68f,0.62f,0.43f);
        ForbiddenPrograms::Configure(Card);
        Cards.Add(Card);
    };
    AddCard(TEXT("PulseDart"),TEXT("PULSE DART"),TEXT("Rapid neon energy bolts.\nA precise, compact kinetic spark."),EMagicSchool::Pulse,30.f,0.55f,AMVPBoltSpell::StaticClass());
    AddCard(TEXT("PrismBurst"),TEXT("PRISM BURST"),TEXT("Three pink plasma charges.\nColliding sparks bloom on impact."),EMagicSchool::Prism,20.f,1.15f,AMVPEmberSpell::StaticClass());
    AddCard(TEXT("HaloNova"),TEXT("HALO NOVA"),TEXT("Eight radiant energy shards.\nA piercing 360-degree pulse."),EMagicSchool::Prism,18.f,1.5f,AMVPFrostSpell::StaticClass());
    AddCard(TEXT("KineticCards"),TEXT("KINETIC CARDS"),TEXT("Spin electric-gold holographic cards.\nShatter into charged geometric sparks."),EMagicSchool::Kinetic,36.f,0.85f,AMVPLightningSpell::StaticClass());
    AddCard(TEXT("PhotonLance"),TEXT("PHOTON LANCE"),TEXT("A focused cyan energy lance.\nPasses through four enemies."),EMagicSchool::Pulse,32.f,0.75f,AMVPBloodSpell::StaticClass());
    AddCard(TEXT("TalismanGuard"),TEXT("TALISMAN GUARD"),TEXT("Gain 3 protective orbiting cards.\nContact kills a foe and consumes a card.\nRepeat drafts add 2; no auto recharge."),EMagicSchool::Barrier,0.f,1.f,ATalismanGuardSpell::StaticClass());
    for(const auto& Profile:NeonArsenal::Profiles()) AddCard(*Profile.ID.ToString(),Profile.Name,Profile.Description,Profile.School,Profile.Damage,Profile.Cooldown,ANeonArsenalSpell::StaticClass());
    ArsenalDirector=GetWorld()->SpawnActor<ANeonArsenalDirector>();
    DistrictDirector=GetWorld()->SpawnActor<AForbiddenDistrictDirector>();
    SupportSpirit=GetWorld()->SpawnActor<AForbiddenSpirit>();DecoySpirit=GetWorld()->SpawnActor<AForbiddenSpirit>();
    for(int32 I=0;I<8;++I)
    {
        auto* Spirit=GetWorld()->SpawnActor<AForbiddenSpirit>();
        const float X=I<4 ? -1900.f : 1700.f;const float Y=-1200.f+(I%4)*620.f;
        Spirit->SetPedestrianRoute(FVector(X,Y,55),FVector(X+750,Y,55));
    }
    Player->Deck->UnlockedCards = Cards;
    Player->Deck->EquippedDeck = Cards;
    LoadStartingDeck();
    if (auto* PC = Cast<AMVPController>(Player->GetController()))
    {
        PC->SetMenuInput(true);
        PC->SetPause(true);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark"))) GetWorld()->SpawnActor<AExplosionBenchmark>();
}

void AMVPArenaGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bRunInitialized || bInCodex || bRunEnded || bChoosingCard || (TestingDirector && TestingDirector->IsActive())) return;
    RunSeconds += DeltaSeconds;
    CapturedRoutineRemaining=FMath::Max(0.f,CapturedRoutineRemaining-DeltaSeconds);
    if(CapturedRoutineRemaining<=0) CapturedRoutine=INDEX_NONE;
    if(PendingCorruptRewards>0 && !DecisionWidget) ChooseCorruptPerk();
    if (Spawner) Spawner->EnemiesPerWave = FMath::Min(22, 8 + 2*FMath::FloorToInt(RunSeconds / 30.f));
}

void AMVPArenaGameMode::EmitProjectile(FVector Origin, FVector Direction, float Damage, const FSpellModifiers& Snapshot, FLinearColor Color, EProjectileType Type, bool bBlood,ENeonSpellStyle Style)
{
    if (bRunEnded || bChoosingCard) return;
    if(TestingDirector && TestingDirector->IsActive()) {TestingDirector->EmitProjectile(Origin,Direction,Damage,Snapshot,Color,Type,Style);return;}
    const AMVPPlayer* Caster=GetArenaPlayer();
    const FSpellModifiers Effective=Caster && Caster->Challenges ? Caster->Challenges->ModifySpellSnapshot(Snapshot) : Snapshot;
    for (AMVPProjectile* Projectile : Projectiles)
        if (IsValid(Projectile) && !Projectile->bActive)
        {
            Projectile->bBloodSpell=bBlood;
            Projectile->SetOwner(GetArenaPlayer());
            Projectile->SetSpellStyle(Type==EProjectileType::PhysicalMesh ? ENeonSpellStyle::Kinetic : Style);
            Projectile->SetupProjectileVisuals(Type,SpearMesh,Type==EProjectileType::ContinuousBeam ? LightningFX : bBlood ? BloodFX : Type==EProjectileType::ConcentratedElemental ? FireballFX : nullptr);
            if(Type==EProjectileType::ContinuousBeam) Projectile->FireContinuousBeam(Origin,Origin+Direction.GetSafeNormal()*1200.f,Damage,Effective);
            else Projectile->Launch(Origin,Direction,Damage,Effective,Color);
            return;
        }
    ++DroppedProjectiles;
}

void AMVPArenaGameMode::EnemyDefeated(FVector Position,int32 Reward)
{
    if(TestingDirector && TestingDirector->IsActive()) return;
    if (bRunEnded) return;
    if(auto* Player=GetArenaPlayer();Player && Player->Challenges) Reward=Player->Challenges->ModifyEssenceReward(Reward);
    ++Kills;
    for (TActorIterator<AMortisProgressionManager> It(GetWorld()); It; ++It) It->OnEnemyKilled();
    AMVPGem* Nearest = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    for (AMVPGem* Gem : Gems)
    {
        if (!IsValid(Gem)) continue;
        if (!Gem->bActive) { Gem->Activate(Position, FMath::Max(1,Reward)); return; }
        const double Distance = FVector::DistSquared(Position, Gem->GetActorLocation());
        if (Distance < BestDistance) { Nearest = Gem; BestDistance = Distance; }
    }
    // Preserve earned XP when the fixed gem pool fills by merging into a nearby gem.
    if (Nearest) Nearest->XPValue+=FMath::Max(1,Reward);
}

void AMVPArenaGameMode::AddXP(int32 Amount)
{
    if(TestingDirector && TestingDirector->IsActive()) return;
    if (bInCodex || bRunEnded || Amount <= 0) return;
    const float BonusRate=(CorruptPerks.Contains("Collector") ? 0.25f : 0.f)+(ForbiddenSettings::Get(this,"District",0)>0.5f ? 0.2f : 0.f);
    if(BonusRate>0) {EssenceRemainder+=Amount*BonusRate;const int32 Bonus=FMath::FloorToInt(EssenceRemainder);EssenceRemainder-=Bonus;Amount+=Bonus;}
    XP += Amount;
    if(auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>()) Rider->OnEssence(Amount);
    while (XP >= GetXPRequirement())
    {
        XP -= GetXPRequirement();
        ++Level;
        ++PendingLevels;
    }
    OpenNextOffer();
}

void AMVPArenaGameMode::AwardChallengeReward(int32 BonusEssence)
{
    if(bInCodex || bRunEnded || (TestingDirector && TestingDirector->IsActive())) return;
    ++PendingLevels;
    if(BonusEssence>0) AddXP(BonusEssence);else OpenNextOffer();
}

void AMVPArenaGameMode::OpenNextOffer()
{
    if (bChoosingCard || bRunEnded || DecisionWidget || PendingLevels <= 0) return;
    AMVPPlayer* Player = GetArenaPlayer();
    if (!Player) return;
    Player->Deck->DrawLevelUpHand(); // An exhausted library offers recovery instead of stalling progression.
    bChoosingCard = true;
    if(AudioDirector) AudioDirector->PlayUI(TEXT("UI_Confirm"));
    if (AMVPController* PC = Cast<AMVPController>(Player->GetController()))
    {
        PC->SetMenuInput(true);
        PC->SetPause(true);
        SelectionWidget = CreateWidget<UCodexCardSelectionWidget>(PC, CardSelectionWidgetClass);
        if (SelectionWidget)
        {
            SelectionWidget->AddToViewport(20);
            TArray<UCardDataAsset*> Hand;
            for (UCardDataAsset* Card : Player->Deck->CurrentHand) Hand.Add(Card);
            SelectionWidget->InitializeSelectionScreen(Hand);
            FInputModeGameAndUI Input;
            Input.SetHideCursorDuringCapture(false);
            Input.SetWidgetToFocus(SelectionWidget->TakeWidget());
            PC->SetInputMode(Input);
        }
    }
}

bool AMVPArenaGameMode::ChooseCard(int32 Index)
{
    if(const auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0)); PC && (PC->HelpWidget || PC->IsSettingsOpen())) return false;
    AMVPPlayer* Player = GetArenaPlayer();
    if (!bChoosingCard || bRunEnded || DecisionWidget || !Player) return false;
    if (Player->Deck->CurrentHand.IsEmpty()) { if (Index != 0) return false; }
    else
    {
        if(Player->Deck->CurrentHand.IsValidIndex(Index))
        {
            UCardDataAsset* Selected=Player->Deck->CurrentHand[Index];
            if(!Player->Deck->FindOwnedSpell(Selected->CardID) && Player->Deck->MaximumActiveWeapons>0 && Player->Deck->ActiveWeapons.Num()>=Player->Deck->MaximumActiveWeapons)
            {
                TArray<FName> IDs;TArray<FString> Choices;
                for(ASpellBase* Spell:Player->Deck->ActiveWeapons) if(Spell && Spell->CardDefinition) {IDs.Add(Spell->CardDefinition->CardID);Choices.Add(TEXT("REPLACE ")+Spell->CardDefinition->Name.ToString());}
                Choices.Add(TEXT("KEEP CURRENT LOADOUT / RETURN TO DRAFT"));
                ShowDecision(TEXT("SIX ACTIVE PROGRAMS / CHOOSE A REPLACEMENT"),Choices,[this,IDs,Selected](int32 Pick)
                {
                    if(IDs.IsValidIndex(Pick) && GetArenaPlayer()->Deck->ReplaceSelectedSpell(Selected,IDs[Pick]))
                    {if(IDs[Pick]=="TalismanGuard") GetArenaPlayer()->ClearTalismans();ChooseCard(0);}
                });
                return false;
            }
        }
        if (!Player->Deck->CurrentHand.IsValidIndex(Index)
            || !Player->Deck->AddSelectedCardSpell(Player->Deck->CurrentHand[Index])) return false;
    }
    --PendingLevels;
    if(AudioDirector) AudioDirector->PlayUI(TEXT("UI_Select"));
    bChoosingCard = false;
    Player->Health = FMath::Min(Player->MaxHealth, Player->Health + 12.f);
    if (SelectionWidget) { SelectionWidget->RemoveFromParent(); SelectionWidget = nullptr; }
    if (PendingLevels > 0) OpenNextOffer();
    if (!bChoosingCard)
        if (AMVPController* PC = Cast<AMVPController>(Player->GetController()))
        {
            PC->SetPause(false);
            PC->SetMenuInput(false);
        }
    return true;
}

void AMVPArenaGameMode::FinishRun(bool Victory)
{
    if (bRunEnded) return;
    if(auto* Rider=GetWorld()->GetSubsystem<UForbiddenRiderSubsystem>()) Rider->EndRun();
    if(DecisionWidget) {DecisionWidget->RemoveFromParent();DecisionWidget=nullptr;}
    bVictory=Victory;
    RunRecord.ID=RunRecord.ID.IsEmpty() ? FGuid::NewGuid().ToString(EGuidFormats::Digits) : RunRecord.ID;
    RunRecord.DateUTC=FDateTime::UtcNow().ToIso8601();RunRecord.Victory=Victory;
    RunRecord.Seconds=RunSeconds;RunRecord.Kills=Kills;RunRecord.Level=Level;RunRecord.Contracts=CompletedContracts;
    RunRecord.DeathCause=Victory ? TEXT("Broadcast Sovereign defeated") : LastDamageCause;
    if(auto* P=GetArenaPlayer()) for(ASpellBase* Spell:P->Deck->ActiveWeapons) if(Spell && Spell->CardDefinition)
    {
        RunRecord.Programs.AddUnique(Spell->CardDefinition->CardID);
        if(Spell->Modifiers.Evolution>0) RunRecord.Evolutions.AddUnique(*FString::Printf(TEXT("%s.%d"),*Spell->CardDefinition->CardID.ToString(),Spell->Modifiers.Evolution));
    }
    if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>())
    {
        Profile->RecordRun(RunRecord,(TestingDirector && TestingDirector->IsActive()) || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")));
        if(Profile->Data && !Profile->Data->History.IsEmpty() && Profile->Data->History[0].ID==RunRecord.ID) RunRecord=Profile->Data->History[0];
    }
    bRunEnded = true;
    if(auto* Scores=GetGameInstance()->GetSubsystem<UForbiddenScoreService>()) Scores->FinishRun(RunRecord);
    if(auto* Player=GetArenaPlayer();Player && Player->Challenges) Player->Challenges->AbortContract();
    if (SelectionWidget) { SelectionWidget->RemoveFromParent(); SelectionWidget = nullptr; }
    for (ASkeletonMinion* Minion : Minions) if (IsValid(Minion)) Minion->Deactivate();
    if (Spawner) Spawner->StopWaves();
    if (AMVPPlayer* Player = GetArenaPlayer())
    {
        Player->ClearTalismans();
        for (ASpellBase* Spell : Player->Deck->ActiveWeapons) if (IsValid(Spell)) Spell->StopCasting();
        if (AMVPController* PC = Cast<AMVPController>(Player->GetController()))
        {
            PC->SetMenuInput(true);
            PC->SetPause(true);
        }
    }
}
void AMVPArenaGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if (SelectionWidget) { SelectionWidget->RemoveFromParent(); SelectionWidget = nullptr; }
    GetWorldTimerManager().ClearTimer(InitializeTimer);
    Super::EndPlay(Reason);
}

void AMVPArenaGameMode::BuildArena()
{
    for(TActorIterator<ANeonCityDirector> City(GetWorld());City;++City) return;
    const bool Cinematic=(FParse::Param(FCommandLine::Get(),TEXT("CinematicFX")) || FParse::Param(FCommandLine::Get(),TEXT("CinematicTest")));
    AActor* Arena = GetWorld()->SpawnActor<AActor>();
    USceneComponent* Root = NewObject<USceneComponent>(Arena);
    Arena->SetRootComponent(Root);
    Root->RegisterComponent();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Layer = [&](const TCHAR* Name, FLinearColor Color)
    {
        UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(Arena, Name);
        Arena->AddInstanceComponent(Mesh);
        Mesh->SetupAttachment(Root);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->RegisterComponent();
        MVP::Tint(Mesh, Color);
        return Mesh;
    };
    auto* Floor = Layer(TEXT("Floor"), FLinearColor(0.019f, 0.031f, 0.042f));
    Floor->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, 0, -40), FVector(190, 190, 0.5f)));
    auto* Tiles = Layer(TEXT("SlatePavers"), FLinearColor(0.035f, 0.048f, 0.052f));
    if (Cinematic)
    {
        if (auto* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Cinematic/M_StoneTest.M_StoneTest")))
        { Floor->SetMaterial(0,Stone); Tiles->SetMaterial(0,Stone); }
        Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Floor->SetCollisionObjectType(ECC_WorldStatic);
        Floor->SetCollisionResponseToAllChannels(ECR_Block);
    }
    FRandomStream Random(731);
    for (int32 X = -36; X <= 36; ++X)
    {
        for (int32 Y = -36; Y <= 36; ++Y)
            Tiles->AddInstance(FTransform(FRotator(0, Random.FRandRange(-0.7f, 0.7f), 0),
                FVector(X * 250.f, Y * 250.f + (X % 2) * 125.f, -13.f), FVector(2.43f, 2.43f, 0.025f)));
    }
    auto* Sigils = Layer(TEXT("JadeRitualInlay"), FLinearColor(0.045f, 0.24f, 0.16f));
    auto Line = [&](FVector A, FVector B, float Width)
    {
        const FVector Delta = B - A;
        Sigils->AddInstance(FTransform(Delta.Rotation(), (A + B) * 0.5f,
            FVector(Delta.Size() / 100.f, Width / 100.f, 0.015f)));
    };
    for (float Radius : {430.f, 470.f, 950.f})
        for (int32 I = 0; I < 96; ++I)
        {
            const float A = I * 2.f * PI / 96.f, B = (I + 1) * 2.f * PI / 96.f;
            Line(FVector(FMath::Cos(A)*Radius, FMath::Sin(A)*Radius, -9), FVector(FMath::Cos(B)*Radius, FMath::Sin(B)*Radius, -9), 3.f);
        }
    for (int32 I = 0; I < 12; ++I)
    {
        const float A = I * PI / 6.f;
        const FVector D(FMath::Cos(A), FMath::Sin(A), 0), T(-D.Y, D.X, 0);
        Line(D*510.f + FVector(0,0,-9), D*560.f + FVector(0,0,-9), 5.f);
        Line(D*535.f-T*15.f + FVector(0,0,-9), D*555.f+T*15.f + FVector(0,0,-9), 4.f);
    }
    // One instanced component per prop keeps repeated grave furniture inexpensive.
    auto Prop = [&](const TCHAR* ID, float Height)
    {
        const FString Path = FString::Printf(TEXT("/Game/Art/Props/%s/SM_%s.SM_%s"), ID, ID, ID);
        UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!Asset) return (UInstancedStaticMeshComponent*)nullptr;
        auto* Component = Layer(ID, FLinearColor::White);
        Component->SetStaticMesh(Asset);
        Component->SetCastShadow(Cinematic);
        Component->EmptyOverrideMaterials();
        const FBoxSphereBounds B = Asset->GetBounds();
        const float Scale = Height / FMath::Max(1.f, float(B.BoxExtent.Z*2));
        Component->SetRelativeScale3D(FVector(Scale));
        Component->SetRelativeLocation(FVector(0,0,-10.f-(B.Origin.Z-B.BoxExtent.Z)*Scale));
        return Component;
    };
    auto Place = [](UInstancedStaticMeshComponent* C, FVector P, float Yaw)
    {
        if (C) C->AddInstance(FTransform(FRotator(0,Yaw,0), P / C->GetRelativeScale3D().X));
    };
    auto* Graves = Prop(TEXT("Gravestone"), 140.f);
    auto* Obelisks = Prop(TEXT("SoulObelisk"), 270.f);
    auto* Altar = Prop(TEXT("OssuaryAltar"), 110.f);
    Place(Altar, FVector(820,0,0), 180);
    for (int32 I=0; I<8; ++I)
    {
        const float A=I*PI/4.f;
        Place(Obelisks, FVector(FMath::Cos(A)*1100, FMath::Sin(A)*1100,0), FMath::RadiansToDegrees(A)+180);
    }
    for (int32 I=0; I<180; ++I)
    {
        const float A=Random.FRandRange(0,2*PI), R=Random.FRandRange(1250,6800);
        Place(Graves, FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0), Random.FRandRange(-25,25));
    }
    // Two shadowless directional lights provide predictable PBR readability without Lumen.
    for (int32 I=0; I<2; ++I)
    {
        auto* Light=NewObject<UDirectionalLightComponent>(Arena);
        Arena->AddInstanceComponent(Light);
        Light->SetupAttachment(Root);
        Light->SetIntensity(I==0 ? 5.f : 2.f);
        Light->SetLightColor(I==0 ? FLinearColor(0.74f,0.86f,1.f) : FLinearColor(0.35f,1.f,0.65f));
        Light->SetCastShadows(Cinematic && I==0);
        Light->ForwardShadingPriority = I == 0 ? 1 : 0;
        Light->SetRelativeRotation(FRotator(I==0 ? -55.f : -35.f,I==0 ? -40.f : 140.f,0));
        Light->RegisterComponent();
    }
    auto* Boundary = Layer(TEXT("Boundary"), FLinearColor(0.2f, 0.32f, 0.3f));
    for (float Sign : {-1.f, 1.f})
    {
        Boundary->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Sign * MVP::ArenaLimit, 0, 0), FVector(0.12f, 140.f, 0.08f)));
        Boundary->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, Sign * MVP::ArenaLimit, 0), FVector(140.f, 0.12f, 0.08f)));
    }
}

