#include "ForbiddenRiderPreview.h"
#include "Engine/StaticMesh.h"
#include "ForbiddenRiderRoster.h"
#include "MVPGame.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NecromancyVisualComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/PackageName.h"

AForbiddenRiderPreview::AForbiddenRiderPreview()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Studio")));
    Model=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Rider"));Model->SetupAttachment(RootComponent);
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);Model->SetCastShadow(true);
    Model->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Model->bEnableUpdateRateOptimizations=false;
    for(int32 I=0;I<2;++I)
    {auto* Ghost=CreateDefaultSubobject<UNecromancyVisualComponent>(*FString::Printf(TEXT("Familiar%d"),I));Ghost->SetupAttachment(RootComponent);Ghost->SetVisibility(false);Ghosts.Add(Ghost);}
    Shield=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WardShield"));Shield->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Shield->SetStaticMesh(Sphere.Object);Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);Shield->SetCastShadow(false);
    Shield->SetRelativeLocation(FVector(0,0,95));Shield->SetRelativeScale3D(FVector(1.25f,1.25f,1.95f));Shield->SetVisibility(false);
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("TurntableCamera"));Capture->SetupAttachment(RootComponent);
    Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
    Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->FOVAngle=32;
    Capture->ShowFlags.SetAtmosphere(false);Capture->ShowFlags.SetFog(false);Capture->ShowFlags.SetMotionBlur(false);
    Capture->ShowFlags.SetDirectionalLights(false);Capture->ShowFlags.SetSkyLighting(false);
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=-1;
    const FVector Locations[]={FVector(220,-180,230),FVector(100,200,160),FVector(-160,0,180)};
    for(int32 I=0;I<3;++I)
    {
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("StudioLight%d"),I));L->SetupAttachment(RootComponent);
        L->SetRelativeLocation(Locations[I]);L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(I==0 ? 350 : 180);L->SetAttenuationRadius(900);
        L->SetCastShadows(I==0);Lights.Add(L);
    }
}
void AForbiddenRiderPreview::BeginPlay()
{
    Super::BeginPlay();
    Image=NewObject<UTextureRenderTarget2D>(this);Image->ClearColor=FLinearColor(.004f,.008f,.017f);
    Image->InitCustomFormat(1000,800,PF_B8G8R8A8,false);Image->UpdateResourceImmediate(true);
    Capture->TextureTarget=Image;Capture->ShowOnlyActors.Add(this);LastTime=FPlatformTime::Seconds();
    Shield->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Characters/RiderExtras/M_RiderShield.M_RiderShield")));
}
bool AForbiddenRiderPreview::SetRider(FName ID)
{
    const auto* D=ForbiddenRiders::Find(ID);if(!D) return false;
    const FString Folder=TEXT("/Game/Art/Characters/")+ID.ToString()+TEXT("/");
    const FString MeshPath=Folder+TEXT("SK_")+ID.ToString();
    if(!FPackageName::DoesPackageExist(MeshPath)) return false;
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*(MeshPath+TEXT(".SK_")+ID.ToString()));if(!Mesh) return false;
    TArray<TObjectPtr<UAnimSequence>> Loaded;
    for(const TCHAR* State:{TEXT("HoverIdle"),TEXT("HoverIdleAlt"),TEXT("Cast"),TEXT("CastBlood")})
    {
        const FString Path=Folder+TEXT("A_")+State;
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Path+TEXT(".A_")+State));
        if(!Clip || Clip->GetSkeleton()!=Mesh->GetSkeleton()) return false;Loaded.Add(Clip);
    }
    Clips=MoveTemp(Loaded);Model->SetSkeletalMesh(Mesh);Model->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    const auto Bounds=Mesh->GetBounds();const float Scale=D->Height/FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));
    Model->SetRelativeScale3D(FVector(Scale));ModelCenter=Bounds.Origin*Scale;
    Model->SetRelativeLocation(-ModelCenter+FVector(0,0,95));Rider=ID;Ready=true;ResetView();SetClip(0);
    Lights[1]->SetLightColor(FLinearColor::LerpUsingHSV(FLinearColor::White,D->Color,.3f));Lights[2]->SetLightColor(D->Color);
    for(int32 I=0;I<Ghosts.Num();++I)
    {
        if(ID=="Mira")
        {
            Ghosts[I]->LoadCharacter(TEXT("DigitalSpirit"),I==0 ? 65.f : 45.f);
            Ghosts[I]->UpdateLocomotion(FVector::ForwardVector,100.f);
            auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Characters/RiderExtras/M_RiderGhost.M_RiderGhost"));
            for(int32 J=0;J<Ghosts[I]->GetNumMaterials();++J) Ghosts[I]->SetMaterial(J,Material);
        }
        Ghosts[I]->SetVisibility(Visible && ID=="Mira");
    }
    // Framing includes the rigid board and leaves head/foot margins at every yaw.
    const float Radius=FMath::Max(100.f,float(Bounds.SphereRadius)*Scale);
    const FVector Eye(FMath::Max(480.f,Radius*4.4f),0,132);
    Capture->SetRelativeLocation(Eye);Capture->SetRelativeRotation((FVector(0,0,95)-Eye).Rotation());
    CaptureAge=1;return true;
}
void AForbiddenRiderPreview::SetClip(int32 Index)
{
    if(!Clips.IsValidIndex(Index)) return;
    ClipIndex=Index;ClipTime=0;Model->PlayAnimation(Clips[Index],true);CaptureAge=1;
    Shield->SetVisibility(Visible && Rider=="Rook" && Index>=2);
}
void AForbiddenRiderPreview::RotateBy(float Degrees)
{Yaw=FMath::Fmod(Yaw+Degrees+360.f,360.f);ManualUntil=FPlatformTime::Seconds()+1.5;CaptureAge=1;}
void AForbiddenRiderPreview::ResetView() {Yaw=245;ManualUntil=FPlatformTime::Seconds()+1;CaptureAge=1;}
void AForbiddenRiderPreview::SetPreviewVisible(bool Value)
{
    if(Visible==Value) return;
    Visible=Value;Model->SetVisibility(Value);for(UPointLightComponent* L:Lights) L->SetVisibility(Value);
    for(UNecromancyVisualComponent* G:Ghosts) G->SetVisibility(Value && Rider=="Mira");
    Shield->SetVisibility(Value && Rider=="Rook" && ClipIndex>=2);
    if(Value) CaptureAge=1;
}
void AForbiddenRiderPreview::Tick(float Dt)
{
    Super::Tick(Dt);const double Now=FPlatformTime::Seconds();const float RealDt=FMath::Clamp(float(Now-LastTime),0.f,.1f);LastTime=Now;
    if(!Visible || !Ready) return;
    const auto* G=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();
    const auto* PC=Cast<AMVPController>(UGameplayStatics::GetPlayerController(this,0));
    if(!G || !G->bInCodex || !G->bCharacterSelectionOpen || (PC && (PC->IsSettingsOpen() || PC->HelpWidget || PC->ScoreWidget)) || G->DecisionWidget) return;
    if(AutoRotate && Now>ManualUntil) Yaw=FMath::Fmod(Yaw+RealDt*18.f,360.f);
    Model->SetRelativeRotation(FRotator(0,Yaw,0));
    // The world is paused in menus; pose evaluation follows real time explicitly.
    ClipTime=FMath::Fmod(ClipTime+RealDt,FMath::Max(.01f,Clips[ClipIndex]->GetPlayLength()));
    Model->SetPosition(ClipTime,false);Model->TickAnimation(0.f,false);Model->RefreshBoneTransforms();
    if(Rider=="Mira") for(int32 I=0;I<Ghosts.Num();++I)
    {
        Ghosts[I]->SetRelativeLocation(FVector(0,I==0 ? -65.f : 65.f,I==0 ? 55.f : 100.f).RotateAngleAxis(Yaw,FVector::UpVector)+FVector(0,0,FMath::Sin(Now*2+I)*5));
        Ghosts[I]->SetRelativeRotation(FRotator(0,Yaw,0));
        Ghosts[I]->TickAnimation(RealDt,false);Ghosts[I]->RefreshBoneTransforms();
    }
    if((CaptureAge+=RealDt)>=1.f/30.f) {CaptureAge=0;Capture->CaptureScene();}
}
