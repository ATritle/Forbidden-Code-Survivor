#include "MVPGame.h"
#include "NecromancyVisualComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/World.h"
#include "CinematicExplosionDirector.h"
#include "NeonSpellImpactDirector.h"
#include "CodexDeckComponent.h"
#include "ForbiddenCombatSubsystem.h"
#include "ForbiddenSettingsControls.h"

void AMVPPlayer::TriggerSpellCastVisual(FName Style) { CharacterArt->TriggerRiderCast(Style); }

void AMVPPlayer::InitializeNeonVFX()
{
    NeonHoverLight=NewObject<UPointLightComponent>(this);AddInstanceComponent(NeonHoverLight);
    NeonHoverLight->SetupAttachment(RootComponent);NeonHoverLight->SetRelativeLocation(FVector(0,0,-25));
    NeonHoverLight->SetMobility(EComponentMobility::Movable);NeonHoverLight->SetIntensityUnits(ELightUnits::Lumens);
    NeonHoverLight->SetLightColor(FLinearColor(0.01f,0.6f,1.f));NeonHoverLight->SetIntensity(110.f);
    NeonHoverLight->SetAttenuationRadius(135.f);NeonHoverLight->SetCastShadows(false);NeonHoverLight->RegisterComponent();
    NeonWake=NewObject<UNiagaraComponent>(this);AddInstanceComponent(NeonWake);NeonWake->SetupAttachment(RootComponent);
    NeonWake->SetAutoActivate(false);NeonWake->SetAsset(LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_HoverWake.NS_HoverWake")));NeonWake->RegisterComponent();
    auto* Plane=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Characters/NeonExorcist/M_NeonTalisman.M_NeonTalisman"));
    if(!Plane || !Base) return;
    TalismanCharges.Init(false,MaxTalismans);
    TalismanPrevious.Init(FVector::ZeroVector,MaxTalismans);
    for(int32 I=0;I<MaxTalismans;++I)
    {
        auto* Card=NewObject<UStaticMeshComponent>(this);AddInstanceComponent(Card);
        Card->SetupAttachment(RootComponent);Card->SetStaticMesh(Plane);
        Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);Card->SetCastShadow(false);
        Card->SetRelativeScale3D(FVector(0.16f,0.3f,1.f));Card->SetVisibility(false);Card->RegisterComponent();
        auto* Material=UMaterialInstanceDynamic::Create(Base,this);
        Material->SetVectorParameterValue(TEXT("NeonColor"),NeonSpellPalette::Color(ENeonSpellStyle::Barrier));
        Card->SetMaterial(0,Material);NeonTalismans.Add(Card);
    }
}
int32 AMVPPlayer::GetTalismanCount() const
{
    int32 Count=0;for(bool Charge:TalismanCharges) if(Charge) ++Count;return Count;
}
FVector AMVPPlayer::GetTalismanLocation(int32 Index) const
{
    if(!NeonTalismans.IsValidIndex(Index)) return GetActorLocation();
    const float A=NeonPhase*2.2f+Index*2.399963f;
    return GetActorLocation()+FVector(FMath::Cos(A)*110.f,FMath::Sin(A)*110.f,20.f+FMath::Sin(NeonPhase*3.f+Index)*10.f);
}
int32 AMVPPlayer::AddTalismans(int32 Count)
{
    if(Health<=0.f || Count<=0) return 0;
    if(Deck) if(const auto* Spell=Deck->FindOwnedSpell("TalismanGuard");Spell && Spell->UsesProgramProgression()) Count=FMath::Min(Count,FMath::Max(0,8-GetTalismanCount()));
    int32 Added=0;
    for(int32 I=0;I<TalismanCharges.Num() && Added<Count;++I)
        if(!TalismanCharges[I])
        {
            TalismanCharges[I]=true;++Added;
            TalismanPrevious[I]=GetTalismanLocation(I);
            NeonTalismans[I]->SetWorldLocation(TalismanPrevious[I]);
            NeonTalismans[I]->SetVisibility(true);
        }
    return Added;
}
void AMVPPlayer::ClearTalismans()
{
    for(int32 I=0;I<TalismanCharges.Num();++I) {TalismanCharges[I]=false;NeonTalismans[I]->SetVisibility(false);}
}
void AMVPPlayer::UpdateNeonVFX(float Dt,bool bVisible)
{
    NeonPhase+=Dt;
    const bool bAlive=bVisible && Health>0.f;
    const bool Spectral=ForbiddenSettings::Get(this,"Board",0)>0.5f;
    const FVector4 Protocol=Spectral ? FVector4(3.f,1.3f,0.65f,1.f) : FVector4(1,1,1,1);
    CharacterArt->SetCustomPrimitiveDataVector4(4,Protocol);
    if(NeonWake) NeonWake->SetCustomPrimitiveDataVector4(4,Protocol);
    if(NeonHoverLight) {NeonHoverLight->SetVisibility(bAlive);NeonHoverLight->SetLightColor(Spectral ? FLinearColor(0.2f,1.f,0.85f) : FLinearColor(0.01f,0.6f,1.f));NeonHoverLight->SetIntensity(100.f+10.f*FMath::Sin(NeonPhase*5.f));}
    if(NeonWake)
    {
        const bool bMoving=bAlive && Movement->Velocity.SizeSquared2D()>FMath::Square(40.f);
        if(bMoving)
        {
            const FVector Head=GetActorLocation()+FVector(0,0,-28.f);
            NeonWake->SetVariableVec3(TEXT("User.BeamStart"),Head);
            NeonWake->SetVariableVec3(TEXT("User.BeamEnd"),Head-Movement->Velocity.GetSafeNormal2D()*100.f);
            NeonWake->SetWorldScale3D(FVector(Spectral ? 1.2f : 1.f));
            if(!NeonWake->IsActive() || (NeonWakeRefresh-=Dt)<=0.f) {NeonWake->Activate(true);NeonWakeRefresh=1.f;}
        }
        else {NeonWake->DeactivateImmediate();NeonWakeRefresh=0.f;}
    }
    for(int32 I=0;I<NeonTalismans.Num();++I)
    {
        UStaticMeshComponent* Card=NeonTalismans[I];
        const FVector Position=GetTalismanLocation(I);
        Card->SetWorldLocation(Position);
        Card->SetWorldRotation(FRotationMatrix::MakeFromZ(FollowCamera->GetComponentLocation()-Card->GetComponentLocation()).Rotator());
        Card->SetVisibility(bVisible && Health>0.f && TalismanCharges[I]);
        if(TalismanCharges[I] && Health>0.f)
        {
            TArray<FHitResult> Hits;
            // Clamp sweep after a teleport; never cut across the arena on a pooled charge.
            const FVector Start=FVector::DistSquared(TalismanPrevious[I],Position)<FMath::Square(200.f) ? TalismanPrevious[I] : Position;
            GetWorld()->SweepMultiByObjectType(Hits,Start,Position,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(20.f));
            Hits.Sort([](const FHitResult& A,const FHitResult& B){return A.Time<B.Time;});
            for(const auto& Hit:Hits)
                if(auto* Enemy=Cast<AMVPEnemy>(Hit.GetActor());Enemy && Enemy->IsPoolActive() && Enemy->Health>0.f)
                {
                    FHitResult Wall;
                    if(GetWorld()->LineTraceSingleByObjectType(Wall,GetActorLocation(),Enemy->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic))) continue;
                    // Consume before damage callbacks can recycle the enemy or award XP.
                    TalismanCharges[I]=false;Card->SetVisibility(false);
                    const float Before=Enemy->Health;
                    const int32 Cost=Enemy->EnemyTier>=3 ? 4 : Enemy->EnemyTier>=2 ? 3 : Enemy->EnemyTier>=1 ? 2 : 1;
                    int32 Paid=1;
                    for(int32 J=0;J<TalismanCharges.Num() && Paid<Cost;++J) if(TalismanCharges[J]) {TalismanCharges[J]=false;NeonTalismans[J]->SetVisibility(false);++Paid;}
                    const float Damage=Enemy->EnemyTier>=3 ? Enemy->MaxHealth*0.08f*Paid/Cost : Paid==Cost ? Before : Enemy->MaxHealth*0.3f*Paid/Cost;
                    Enemy->StaggerRemaining=Enemy->EnemyTier>=3 ? 0.25f : 0.8f;
                    Enemy->TakeSpellDamage(Damage,FVector::UpVector*250.f,TEXT("TalismanGuard"),false);
                    if(const auto* Guard=Deck->FindOwnedSpell("TalismanGuard");Guard && Guard->Modifiers.Evolution==2)
                        if(auto* Fields=GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>()) Fields->QueueField("TalismanGuard",ENeonSpellStyle::Barrier,Position,45.f,100.f,0.05f,1,0.1f);
                    if(auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());Game && Game->SpellImpactDirector)
                        Game->SpellImpactDirector->ConfirmDamage(ENeonSpellStyle::Barrier,Enemy,FMath::Max(0.f,Before-FMath::Max(0.f,Enemy->Health)));
                    break;
                }
        }
        TalismanPrevious[I]=Position;
    }
}
