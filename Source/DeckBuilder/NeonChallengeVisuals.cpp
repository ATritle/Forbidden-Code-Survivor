#include "NeonChallenge.h"
#include "MVPGame.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ANeonChallengeStation::ANeonChallengeStation()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=0.05f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ShrineRoot")));
    MeshyHousing=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshySignHousing"));MeshyHousing->SetupAttachment(RootComponent);
    MeshyHousing->SetCollisionEnabled(ECollisionEnabled::NoCollision);MeshyHousing->SetCanEverAffectNavigation(false);
    auto* Body=CreateDefaultSubobject<UBoxComponent>(TEXT("SolidHousing"));Body->SetupAttachment(RootComponent);
    Body->SetRelativeLocation(FVector(0,0,225));Body->SetBoxExtent(FVector(43,165,225));
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Body->SetCollisionObjectType(ECC_WorldStatic);
    Body->SetCollisionResponseToAllChannels(ECR_Block);Body->SetGenerateOverlapEvents(false);
    // The narrow footprint is placed off the validated approach route. No runtime nav rebuild.
    Body->SetCanEverAffectNavigation(false);
    StatusText=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ContractCaption"));StatusText->SetupAttachment(RootComponent);
    SignLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("NeonSpill"));SignLight->SetupAttachment(RootComponent);
    SignLight->SetRelativeLocation(FVector(90,0,240));SignLight->SetIntensityUnits(ELightUnits::Lumens);
    SignLight->SetIntensity(350);SignLight->SetAttenuationRadius(480);SignLight->SetSourceRadius(45);
    SignLight->SetLightColor(FLinearColor(1,0.025f,0.3f));SignLight->SetCastShadows(false);
    SignLight->SetAffectGlobalIllumination(false);
    Tags.Add(TEXT("NoPainNoGain"));
}
void ANeonChallengeStation::BeginPlay() {Super::BeginPlay();BuildSign();SetContractState(State);}
void ANeonChallengeStation::BuildSign()
{
    if(auto* Asset=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/Challenges/Meshy/SM_ChallengeSign.SM_ChallengeSign")))
    {MeshyHousing->SetStaticMesh(Asset);bUsingMeshyHousing=true;}
    else MeshyHousing->SetVisibility(false);
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Art/City/SM_CityPanel.SM_CityPanel"));
    if(!Cube) Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    TMap<FString,UInstancedStaticMeshComponent*> Groups;
    auto Part=[&](const TCHAR* Material,UStaticMesh* Mesh,FVector P,FVector Dimensions,FRotator R=FRotator::ZeroRotator)
    {
        const bool bNeon=FString(Material).Contains(TEXT("Pink")) || FString(Material).Contains(TEXT("Cyan")) || FString(Material).Contains(TEXT("Amber"));
        if(bUsingMeshyHousing && !bNeon) return;
        const FString Key=FString(Material)+Mesh->GetName();
        auto** Found=Groups.Find(Key);UInstancedStaticMeshComponent* Group=Found ? *Found : nullptr;
        if(!Group)
        {
            Group=NewObject<UInstancedStaticMeshComponent>(this);AddInstanceComponent(Group);Group->SetupAttachment(RootComponent);
            Group->SetMobility(EComponentMobility::Movable);Group->SetStaticMesh(Mesh);
            const FString Path=FString(TEXT("/Game/Art/City/"))+Material+TEXT(".")+Material;
            Group->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*Path));Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Group->SetCanEverAffectNavigation(false);Group->SetCastShadow(!FString(Material).Contains(TEXT("Pink")) && !FString(Material).Contains(TEXT("Cyan")) && !FString(Material).Contains(TEXT("Amber")));
            Group->RegisterComponent();Groups.Add(Key,Group);Parts.Add(Group);
        }
        Group->AddInstance(FTransform(R,P,Dimensions/100.f));
    };
    auto Pipe=[&](FVector A,FVector B,float Width,const TCHAR* Mat)
    {Part(Mat,Cylinder,(A+B)*0.5f,FVector(Width,Width,FVector::Distance(A,B)),FRotationMatrix::MakeFromZ(B-A).Rotator());};
    Part(TEXT("MI_Trim"),Cube,FVector(0,0,14),FVector(110,365,28));
    Part(TEXT("MI_Panel"),Cube,FVector(0,0,30),FVector(92,340,15));
    for(float Y:{-126.f,126.f})
    {
        Part(TEXT("MI_Trim"),Cube,FVector(0,Y,125),FVector(42,25,190));
        Part(TEXT("MI_Rust"),Cube,FVector(23,Y,115),FVector(3,16,130));
        Pipe(FVector(3,Y,48),FVector(3,Y,218),9,TEXT("MI_Panel"));
        Part(TEXT("MI_Cyan"),Cube,FVector(29,Y,149),FVector(4,5,72));
    }
    Part(TEXT("MI_Trim"),Cube,FVector(0,0,330),FVector(58,340,258));
    Part(TEXT("MI_Panel"),Cube,FVector(31,0,330),FVector(7,316,235));
    for(float Y:{-153.f,153.f})
    {
        Part(TEXT("MI_Cyan"),Cube,FVector(37,Y,330),FVector(4,3,197));
        Part(TEXT("MI_Trim"),Cube,FVector(38,Y,442),FVector(9,26,20),FRotator(0,0,25));
        Part(TEXT("MI_Trim"),Cube,FVector(38,Y,218),FVector(9,26,20),FRotator(0,0,-25));
    }
    for(float Z:{211.f,449.f}) Part(TEXT("MI_Trim"),Cube,FVector(38,0,Z),FVector(12,302,13));
    for(float Y:{-145.f,145.f}) for(float Z:{222.f,438.f})
        Part(TEXT("MI_Rust"),Cylinder,FVector(47,Y,Z),FVector(12,12,6),FRotator(90,0,0));
    Pipe(FVector(0,-115,457),FVector(0,-115,480),12,TEXT("MI_Panel"));
    Pipe(FVector(0,-115,480),FVector(0,-50,480),12,TEXT("MI_Panel"));
    Pipe(FVector(0,-50,480),FVector(0,-50,457),12,TEXT("MI_Panel"));
    // Protective energy-core ring under the sign. Sixteen short pipes form a supported loop.
    for(int32 I=0;I<16;++I)
    {
        const float A=I*2*PI/16,B=(I+1)*2*PI/16;
        Pipe(FVector(7,FMath::Cos(A)*43,153+FMath::Sin(A)*43),FVector(7,FMath::Cos(B)*43,153+FMath::Sin(B)*43),9,TEXT("MI_Trim"));
    }
    Part(TEXT("MI_Pink"),Sphere,FVector(10,0,153),FVector(42));
    Pipe(FVector(6,0,196),FVector(6,0,204),8,TEXT("MI_Pink"));
    Pipe(FVector(6,-43,153),FVector(6,-113,153),7,TEXT("MI_Panel"));
    Pipe(FVector(6,43,153),FVector(6,113,153),7,TEXT("MI_Panel"));
    Part(TEXT("MI_Trim"),Cube,FVector(30,0,82),FVector(62,80,38),FRotator(0,0,0));
    Part(TEXT("MI_Cyan"),Cube,FVector(63,0,87),FVector(3,58,20));
    for(float Y:{-140.f,0.f,140.f}) Part(TEXT("MI_Cyan"),Cube,FVector(57,Y,14),FVector(3,22,8));
    auto Text=[&](const TCHAR* Caption,float Z,float Size,FColor Color)
    {
        auto* T=NewObject<UTextRenderComponent>(this);AddInstanceComponent(T);T->SetupAttachment(RootComponent);
        T->SetRelativeLocation(FVector(42,0,Z));T->SetHorizontalAlignment(EHTA_Center);T->SetVerticalAlignment(EVRTA_TextCenter);
        T->SetWorldSize(Size);T->SetText(FText::FromString(Caption));T->SetTextRenderColor(Color);
        T->SetTextMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Challenges/M_ChallengeText.M_ChallengeText")));
        T->SetCastShadow(false);T->RegisterComponent();Parts.Add(T);return T;
    };
    Text(TEXT("NO PAIN"),388,57,FColor(255,7,99));
    Text(TEXT("-"),333,32,FColor(10,210,255));
    // A physical neon tube keeps the separator readable at the furthest gameplay zoom.
    Part(TEXT("MI_Cyan"),Cube,FVector(58,0,333),FVector(4,64,6));
    Text(TEXT("NO GAIN"),279,57,FColor(255,166,9));
    StatusText->SetRelativeLocation(FVector(45,0,230));StatusText->SetWorldSize(13);
    StatusText->SetHorizontalAlignment(EHTA_Center);StatusText->SetVerticalAlignment(EVRTA_TextCenter);
    StatusText->SetTextRenderColor(FColor(20,220,255));
    StatusText->SetTextMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Challenges/M_ChallengeText.M_ChallengeText")));
    StatusText->SetCastShadow(false);
}
void ANeonChallengeStation::Configure(UNeonChallengeDefinition* InDefinition) {Definition=InDefinition;SetContractState(State);}
void ANeonChallengeStation::SetContractState(ENeonContractState NewState)
{
    State=NewState;
    const FString Caption=State==ENeonContractState::Succeeded ? TEXT("CONTRACT COMPLETE") : State==ENeonContractState::Failed ? TEXT("CONTRACT FAILED") : State==ENeonContractState::Aborted ? TEXT("CONTRACT CLOSED") : State==ENeonContractState::Active ? TEXT("CONTRACT ACTIVE") : Definition ? Definition->Title.ToString() : TEXT("DANGER CONTRACT");
    StatusText->SetText(FText::FromString(Caption));
}
bool ANeonChallengeStation::CanInteract(const AMVPPlayer* Player) const
{
    if(!Player || (State!=ENeonContractState::Ready && State!=ENeonContractState::Offered && State!=ENeonContractState::Active) || !Definition) return false;
    const FVector D=Player->GetActorLocation()-GetActorLocation();
    if(D.SizeSquared2D()>FMath::Square(InteractRadius) || FMath::Abs(D.Z-55)>130) return false;
    FHitResult Wall;FCollisionQueryParams Q(SCENE_QUERY_STAT(ContractInteract),false,this);Q.AddIgnoredActor(Player);
    return !GetWorld()->LineTraceSingleByObjectType(Wall,Player->GetActorLocation(),GetActorLocation()+FVector(0,0,65),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
}
void ANeonChallengeStation::Tick(float Dt)
{
    Super::Tick(Dt);
    const float T=GetWorld()->GetTimeSeconds();
    const bool bAvailable=State==ENeonContractState::Ready || State==ENeonContractState::Offered;
    SignLight->SetIntensity((bAvailable ? 350.f : 120.f)*(0.9f+0.1f*FMath::Sin(T*2)));
}

ANeonChallengeEffect::ANeonChallengeEffect()
{
    PrimaryActorTick.bCanEverTick=false;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EffectRoot")));
    Core=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnergyCore"));Core->SetupAttachment(RootComponent);
    Marker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReadableDangerRing"));Marker->SetupAttachment(RootComponent);
    Tether=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnergyTether"));Tether->SetupAttachment(RootComponent);
    Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreSpill"));Light->SetupAttachment(RootComponent);
    Light->SetCastShadows(false);Light->SetAffectGlobalIllumination(false);Light->SetAttenuationRadius(280);
    Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(180);Light->SetLightColor(FLinearColor(1,0.02f,0.4f));
    for(auto* Mesh:{Core.Get(),Marker.Get(),Tether.Get()})
    {Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCanEverAffectNavigation(false);Mesh->SetCastShadow(false);}
}
void ANeonChallengeEffect::BeginPlay()
{
    Super::BeginPlay();
    Core->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    CoreMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Challenges/M_ChallengeOrb.M_ChallengeOrb")),this);
    ShieldMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Challenges/M_ChallengeShield.M_ChallengeShield")),this);
    Core->SetMaterial(0,CoreMaterial);
    Marker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    RingMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Challenges/M_ChallengeRing.M_ChallengeRing")),this);
    Marker->SetMaterial(0,RingMaterial);
    Tether->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    Tether->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/City/MI_Cyan.MI_Cyan")));
    HideEffect();
}
void ANeonChallengeEffect::ShowOrb(FVector Position,FVector PlayerPosition,float Age)
{
    SetActorHiddenInGame(false);SetActorLocation(Position);Core->SetVisibility(true);Tether->SetVisibility(true);Light->SetVisibility(true);
    Core->SetMaterial(0,CoreMaterial);if(CoreMaterial) CoreMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(1,0.012f,0.32f));
    Core->SetRelativeLocation(FVector::ZeroVector);
    Core->SetRelativeScale3D(FVector(0.64f*(1+0.06f*FMath::Sin(Age*7))));
    Marker->SetRelativeScale3D(FVector(0.95f));Marker->SetWorldRotation(FRotator::ZeroRotator);Marker->SetRelativeLocation(FVector::ZeroVector);
    if(RingMaterial) {RingMaterial->SetScalarParameterValue(TEXT("Charge"),0);RingMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor::Black);}
    const FVector D=PlayerPosition-Position;Tether->SetWorldLocation((Position+PlayerPosition)*0.5f);
    Tether->SetWorldRotation(FRotationMatrix::MakeFromZ(D).Rotator());Tether->SetWorldScale3D(FVector(0.012f,0.012f,D.Size()/100.f));
}
void ANeonChallengeEffect::ShowEcho(FVector Position,float Radius,float Charge,bool bDetonated)
{
    SetActorHiddenInGame(false);SetActorLocation(Position);Core->SetVisibility(false);Tether->SetVisibility(false);Light->SetVisibility(bDetonated);
    Marker->SetRelativeScale3D(FVector(Radius*2/100.f));Marker->SetRelativeLocation(FVector(0,0,4));Marker->SetWorldRotation(FRotator::ZeroRotator);
    if(RingMaterial) {RingMaterial->SetScalarParameterValue(TEXT("Charge"),FMath::Clamp(Charge,0.f,1.f));RingMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor::Black);}
}
void ANeonChallengeEffect::ShowObjective(FVector Position,float Radius,FLinearColor Color,bool bBeacon)
{
    SetActorHiddenInGame(false);SetActorLocation(Position);Core->SetVisibility(bBeacon);Tether->SetVisibility(false);Light->SetVisibility(bBeacon);
    Core->SetMaterial(0,CoreMaterial);if(CoreMaterial) CoreMaterial->SetVectorParameterValue(TEXT("Tint"),Color);
    Core->SetRelativeLocation(FVector(0,0,bBeacon ? 75.f : 0.f));Core->SetRelativeScale3D(FVector(0.48f));
    Marker->SetRelativeLocation(FVector(0,0,6));Marker->SetWorldRotation(FRotator::ZeroRotator);Marker->SetRelativeScale3D(FVector(Radius*2/100));
    if(RingMaterial) {RingMaterial->SetVectorParameterValue(TEXT("Tint"),Color);RingMaterial->SetScalarParameterValue(TEXT("Charge"),0.4f);}
    Light->SetLightColor(Color);
}
void ANeonChallengeEffect::ShowPlayerShell(FVector Position,bool bGlass,float Pulse)
{
    SetActorHiddenInGame(false);SetActorLocation(Position);Tether->SetVisibility(false);Light->SetVisibility(false);
    Core->SetVisibility(bGlass);Core->SetMaterial(0,ShieldMaterial);Core->SetRelativeLocation(FVector(0,0,40));Core->SetRelativeScale3D(FVector(1.75f,1.75f,2.45f));
    Marker->SetRelativeLocation(FVector(0,0,-42));Marker->SetWorldRotation(FRotator::ZeroRotator);Marker->SetRelativeScale3D(FVector(bGlass ? 1.8f : 1.3f+Pulse*0.3f));
    if(RingMaterial) {RingMaterial->SetVectorParameterValue(TEXT("Tint"),bGlass ? FLinearColor(0.3f,0.8f,1) : FLinearColor(0.15f,1,0.3f)*(1+Pulse));RingMaterial->SetScalarParameterValue(TEXT("Charge"),0.75f);}
}
void ANeonChallengeEffect::HideEffect() {SetActorHiddenInGame(true);Light->SetVisibility(false);}
