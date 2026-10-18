#include "MVPGame.h"
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
    LoadCameraPreference();
    for(TActorIterator<ANeonCityDirector> City(GetWorld());City;++City)
    {CameraBoom->SetRelativeRotation(FRotator(-55.f,-35.f,0));break;}
    LandingDust->SetAsset(LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_LandingDust.NS_LandingDust")));
    if (CharacterArt->LoadCharacter(TEXT("NeonExorcist"), 170.f)) {Visual->SetVisibility(false);InitializeNeonVFX();}
    else if (CharacterArt->LoadCharacter(TEXT("Necromancer"),145.f)) Visual->SetVisibility(false);
    CharacterArt->SetCastShadow(GetWorld()->GetMapName().Contains(TEXT("NeonDistrict")) || FParse::Param(FCommandLine::Get(), TEXT("CinematicFX")) || FParse::Param(FCommandLine::Get(), TEXT("CinematicTest")));
}

void AMVPPlayer::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"), this, &AMVPPlayer::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &AMVPPlayer::MoveRight);
    Input->BindAxis(TEXT("CameraZoom"), this, &AMVPPlayer::ZoomCamera);
}
void AMVPPlayer::MoveForward(float Value) { AddMovementInput(FRotator(0,CameraBoom->GetComponentRotation().Yaw,0).Vector(),Value); }
void AMVPPlayer::MoveRight(float Value) { AddMovementInput(FRotationMatrix(FRotator(0,CameraBoom->GetComponentRotation().Yaw,0)).GetUnitAxis(EAxis::Y),Value); }

void AMVPPlayer::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, PreferredCameraDistance, DeltaSeconds, 10.f);
    if (bZoomDirty && (ZoomSaveDelay -= DeltaSeconds) <= 0.f) { SaveCameraPreference(); ZoomSaveDelay = 5.f; }
    InvulnerabilityRemaining = FMath::Max(0.f, InvulnerabilityRemaining - DeltaSeconds);
    // A finite arena keeps both the player and follow camera on the floor.
    FVector Position = GetActorLocation();
    const auto* ArenaGame=MVP::Mode(this);
    const float Limit=ArenaGame && ArenaGame->TestingDirector && ArenaGame->TestingDirector->IsActive() ? 4300.f : MVP::ArenaLimit;
    Position.X = FMath::Clamp(Position.X, -Limit, Limit);
    Position.Y = FMath::Clamp(Position.Y, -Limit, Limit);

    SetActorLocation(Position,true);
    UpdateGroundPhysics(DeltaSeconds);
    const bool bVisible = InvulnerabilityRemaining <= 0.f || FMath::Fmod(InvulnerabilityRemaining, 0.15f) < 0.09f;
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
    if (!Game || Game->bInCodex || Game->bRunEnded || Game->bChoosingCard || InvulnerabilityRemaining > 0.f || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    Health = FMath::Max(0.f, Health - Amount*(Challenges ? Challenges->GetIncomingDamageMultiplier() : 1.f));
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
    return FMath::IsFinite(ScanRadius) && ScanRadius > 0.f && Deck && IsValid(Deck->EnemySpawner)
        ? Deck->EnemySpawner->FindNearestEnemy(GetActorLocation(), ScanRadius) : nullptr;
}

AMVPEnemy::AMVPEnemy()
{
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
    if(auto* Game=MVP::Mode(this);Game && !bTrainingTarget)
    {
        const bool Boss=Game->RunSeconds>=360.f && !Game->bBroadcastBossSpawned;
        ApplyEnemyArchetype(Boss ? 9 : CyberEnemies::Select(Game->RunSeconds,FMath::FRand()));
        if(Boss) Game->bBroadcastBossSpawned=true;
    }
    Terrain->ResetPath();
    UMortisTerrain::Ground(this);
    Health = FMath::Max(1.f, MaxHealth);
    CachedChaseDirection = FVector::ZeroVector;
    DirectionUpdateRemaining = FMath::FRandRange(0.f, 0.05f);
    Target = Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (Target.IsValid()) CachedChaseDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    MVP::Tint(Visual, FLinearColor(0.65f, 0.08f, 0.17f));
    Visual->SetVisibility(!CharacterArt->IsArtReady());
    CharacterArt->SetDormant(false);
    SetActorTickEnabled(true);
}
void AMVPEnemy::OnPoolReturned_Implementation()
{
    SetCombatHitFlash(FLinearColor::Black,0);
    Target.Reset();
    CharacterArt->SetDormant(true);
}

void AMVPEnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bTrainingTarget) return;
    if (!IsPoolActive()) return;
    if (!Target.IsValid()) Target = Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Target.IsValid()) return;
    if(Target->Challenges) Target->Challenges->ResolveOrbContact(this);
    if(Target->Health<=0) return;
    const FVector ToPlayer = Target->GetActorLocation() - GetActorLocation();
    DirectionUpdateRemaining -= DeltaSeconds;
    if (DirectionUpdateRemaining <= 0.f || CachedChaseDirection.IsNearlyZero())
    {
        CachedChaseDirection = ToPlayer.GetSafeNormal2D();
        DirectionUpdateRemaining = 0.05f;
    }
    const float ContactRange=CastChecked<USphereComponent>(RootComponent)->GetScaledSphereRadius()+34.f;
    const float ChaseSpeed=BaseMovementSpeed*(Target->Challenges ? Target->Challenges->GetPursuerSpeedMultiplier() : 1.f);
    CharacterArt->UpdateLocomotion(ToPlayer, ToPlayer.SizeSquared2D() <= FMath::Square(ContactRange) ? 0.f : ChaseSpeed);
    if (ToPlayer.SizeSquared2D() <= FMath::Square(ContactRange) && FMath::Abs(ToPlayer.Z)<150.f && UMortisTerrain::ClearSight(this,Target->GetActorLocation()))
    {
        Target->ReceiveContactDamage(ContactDamage);
    }
    else
        Terrain->MoveTowards(Target->GetActorLocation(),ChaseSpeed,DeltaSeconds);
}

void AMVPEnemy::ReceiveSpellDamage(float Amount) { TakeSpellDamage(Amount, FVector::ZeroVector); }
void AMVPEnemy::TakeSpellDamage(float Amount, FVector ImpactVelocity)
{
    if (!IsPoolActive() || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    auto* ArenaMode=MVP::Mode(this);
    auto* Contracts=ArenaMode && ArenaMode->GetArenaPlayer() ? ArenaMode->GetArenaPlayer()->Challenges.Get() : nullptr;
    if(!bTrainingTarget && Contracts) Amount*=Contracts->GetDamageMultiplier();
    Health -= Amount;
    if(!bTrainingTarget && Contracts) Contracts->OnConfirmedSpellHit(GetActorLocation(),Amount);
    if(bTrainingTarget)
    {
        const float Actual=FMath::Max(0.f,FMath::Min(Amount,Health+Amount));
        if(auto* Game=MVP::Mode(this);Game && Game->TestingDirector) Game->TestingDirector->RecordDamage(this,Actual,Health<=0);
        if(Health<=0) ReturnToPool();
        return;
    }
    if (Health > 0.f) return;
    const FVector DeathPosition = GetActorLocation();
    ReturnToPool(); // Inactivate before callbacks; death cannot pay twice.
    if(Contracts) Contracts->OnEnemyKilled(this,Generation);
    PlayHighDetailDeathVFX(DeathPosition, ImpactVelocity.ContainsNaN() ? FVector::ZeroVector : ImpactVelocity);
    if (AMVPArenaGameMode* Game = MVP::Mode(this)) Game->EnemyDefeated(DeathPosition,EssenceReward);
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
    for (int32 Index = 0; Index < Snapshot.ProjectileCount; ++Index)
    {
        const float Angle = Snapshot.NovaPattern ? 360.f * Index / Snapshot.ProjectileCount : (Index - (Snapshot.ProjectileCount - 1) * 0.5f) * 9.f;
        Game->EmitProjectile(Origin, Aim.RotateAngleAxis(Angle, FVector::UpVector), Damage, Snapshot, BoltColor, ShotType, bBlood,SpellStyle);
    }
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
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMVPController::InteractChallenge);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AMVPController::ToggleSettings).bExecuteWhenPaused=true;
}
void AMVPController::TestExplosion()
{
    if (auto* Game=MVP::Mode(this); Game && Game->ExplosionDirector && !Game->bInCodex && GetPawn())
        Game->EmitImpactPulse(GetPawn()->GetActorLocation()+FVector(350,0,0),450);
}
bool AMVPController::IsChallengeOpen() const
{
    const auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn());return ArenaPlayer && ArenaPlayer->Challenges && ArenaPlayer->Challenges->OfferWidget!=nullptr;
}
void AMVPController::InteractChallenge() {if(auto* ArenaPlayer=Cast<AMVPPlayer>(GetPawn());ArenaPlayer && ArenaPlayer->Challenges) ArenaPlayer->Challenges->Interact();}
void AMVPController::BeginSelectedRun() { if (auto* Game = MVP::Mode(this)) Game->StartSelectedRun(); }
void AMVPController::ChooseOne() { if (!IsSettingsOpen() && !IsChallengeOpen()) if (auto* Game = MVP::Mode(this)) Game->ChooseCard(0); }
void AMVPController::ChooseTwo() { if (!IsSettingsOpen() && !IsChallengeOpen()) if (auto* Game = MVP::Mode(this)) Game->ChooseCard(1); }
void AMVPController::ChooseThree() { if (!IsSettingsOpen() && !IsChallengeOpen()) if (auto* Game = MVP::Mode(this)) Game->ChooseCard(2); }
void AMVPController::Click()
{
    float X, Y;
    if (GetMousePosition(X, Y)) if (auto* HUD = Cast<AMVPHUD>(GetHUD())) HUD->ClickAt(X, Y);
}
void AMVPController::RestartRun()
{
    const auto* Game = MVP::Mode(this);
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
        for(const FString& Asset:{FString(TEXT("SK_"))+Profile.ID,FString(TEXT("A_Walk")),FString(TEXT("A_Run"))})
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
            Projectile->SetSpellStyle(Type==EProjectileType::PhysicalMesh ? ENeonSpellStyle::Kinetic : Style);
            Projectile->SetupProjectileVisuals(Type,SpearMesh,Type==EProjectileType::ContinuousBeam ? LightningFX : bBlood ? BloodFX : Type==EProjectileType::ConcentratedElemental ? FireballFX : nullptr);
            if(Type==EProjectileType::ContinuousBeam) Projectile->FireContinuousBeam(Origin,Origin+Direction.GetSafeNormal()*1200.f,Damage,Effective);
            else Projectile->Launch(Origin,Direction,Damage,Effective,Color);
            return;
        }
    // Fixed capacity: drop excess shots instead of allocating during combat.
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
    XP += Amount;
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
    if (bChoosingCard || bRunEnded || PendingLevels <= 0) return;
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
    if(const auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0)); PC && PC->IsSettingsOpen()) return false;
    AMVPPlayer* Player = GetArenaPlayer();
    if (!bChoosingCard || bRunEnded || !Player) return false;
    if (Player->Deck->CurrentHand.IsEmpty()) { if (Index != 0) return false; }
    else
    {
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

void AMVPArenaGameMode::FinishRun()
{
    if (bRunEnded) return;
    bRunEnded = true;
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

