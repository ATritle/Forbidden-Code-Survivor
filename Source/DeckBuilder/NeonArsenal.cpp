#include "NeonArsenal.h"
#include "MVPGame.h"
#include "NeonChallenge.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"

const TArray<FNeonArsenalProfile>& NeonArsenal::Profiles()
{
    static const TArray<FNeonArsenalProfile> Data={
        {TEXT("ArcRelay"),TEXT("ARC RELAY"),TEXT("22 damage. Chains to 4 targets.\n320cm jumps; 18% decay per hop."),EMagicSchool::Pulse,22,1.3f,1100,320,4,FLinearColor(0.02f,0.55f,1)},
        {TEXT("GhostLink"),TEXT("GHOST LINK"),TEXT("14 damage. Six linked targets.\n240cm jumps; 10% decay per hop."),EMagicSchool::Barrier,14,1.9f,1000,240,6,FLinearColor(0.65f,0.2f,1)},
        {TEXT("NeonCataclysm"),TEXT("NEON CATACLYSM"),TEXT("78 area damage within 300cm.\nA pink charge detonates after 0.45s."),EMagicSchool::Prism,78,3.8f,1000,300,1,FLinearColor(1,0.01f,0.38f)},
        {TEXT("OrbitRazor"),TEXT("ORBIT RAZOR"),TEXT("Two returning crescent blades.\n28 damage; each pierces 4 targets."),EMagicSchool::Kinetic,28,1.5f,1000,0,2,FLinearColor(0.85f,1,0.08f)},
        {TEXT("RicochetSeal"),TEXT("RICOCHET SEAL"),TEXT("24 damage per impact.\nA jade seal rebounds to 3 more foes."),EMagicSchool::Kinetic,24,0.95f,1100,350,1,FLinearColor(0.12f,1,0.3f)},
        {TEXT("StrobeRail"),TEXT("STROBE RAIL"),TEXT("46 damage along a 1200cm rail.\nA 32cm wide flash pierces the line."),EMagicSchool::Pulse,46,2.2f,1200,32,1,FLinearColor(0.65f,0.9f,1)},
        {TEXT("EchoPetals"),TEXT("ECHO PETALS"),TEXT("Six spiraling coral petals.\n16 damage each; one target per petal."),EMagicSchool::Prism,16,1.4f,1000,0,6,FLinearColor(1,0.22f,0.12f)},
        {TEXT("EventHorizon"),TEXT("EVENT HORIZON"),TEXT("Four 16-damage pulses over 1.4s.\nA 230cm violet energy field."),EMagicSchool::Barrier,16,3.2f,1000,230,4,FLinearColor(0.3f,0.04f,1)},
        {TEXT("ShatterSigil"),TEXT("SHATTER SIGIL"),TEXT("34 damage. Splits on contact\ninto five 12-damage amber fragments."),EMagicSchool::Kinetic,34,1.8f,1100,0,5,FLinearColor(1,0.38f,0.025f)},
        {TEXT("SatelliteChoir"),TEXT("SATELLITE CHOIR"),TEXT("Three homing aquamarine orbs.\n18 damage each after a short orbit."),EMagicSchool::Pulse,18,1.6f,1150,0,3,FLinearColor(0.015f,1,0.72f)}
    };return Data;
}
int32 NeonArsenal::Find(FName ID) {return Profiles().IndexOfByPredicate([ID](const auto& P){return P.ID==ID;});}
FString NeonArsenal::MutationDescription(FName ID,const FGameplayTag& Tag)
{
    const int32 P=Find(ID);const FString Kind=Tag.ToString();
    if(Kind==TEXT("Mutation.RadiusMultiplier")) return TEXT("AMPLIFY FIELD\nIncrease area size or link reach by 25%.");
    if(Kind==TEXT("Mutation.ProjectileCount")) return P<=1 ? TEXT("EXTEND NETWORK\nChain to one additional target (up to 12).") : P==2 || P==7 ? TEXT("DUPLICATE FIELD\nAdd another overlapping charge or pulse field.") : TEXT("ECHO CAST\nAdd one projectile or parallel rail to each cast.");
    if(Kind==TEXT("Mutation.PiercingCount")) return P<=1 || P==4 ? TEXT("EXTEND NETWORK\nStrike two additional targets.") : TEXT("PHASE PIERCING\nEach projectile hits two extra enemies.");
    return P<=1 || P==5 ? TEXT("ENERGY DETONATION\nHits burst for 30% area damage.") : TEXT("ENERGY DETONATION\nHits burst for 40% area damage.");
}
void ANeonArsenalSpell::InitializeSpell(UCardDataAsset* Card,AEnemySpawner* Spawner)
{
    ProfileIndex=Card ? NeonArsenal::Find(Card->CardID) : INDEX_NONE;
    if(ProfileIndex!=INDEX_NONE) TargetRange=NeonArsenal::Profiles()[ProfileIndex].Range;
    Super::InitializeSpell(Card,Spawner);
}
TArray<FGameplayTag> ANeonArsenalSpell::GetAvailableMutationTags() const
{
    TArray<FGameplayTag> MutationTags;
    auto Add=[&](const TCHAR* Name){MutationTags.Add(FGameplayTag::RequestGameplayTag(Name));};
    if(Modifiers.ProjectileCount<8) Add(TEXT("Mutation.ProjectileCount"));
    if(ProfileIndex==0 || ProfileIndex==1 || ProfileIndex==4 || ProfileIndex==3 || ProfileIndex==6 || ProfileIndex==8 || ProfileIndex==9)
        if(Modifiers.PiercingCount<12) Add(TEXT("Mutation.PiercingCount"));
    if((ProfileIndex==0 || ProfileIndex==1 || ProfileIndex==2 || ProfileIndex==4 || ProfileIndex==5 || ProfileIndex==7) && Modifiers.RadiusMultiplier<20.f) Add(TEXT("Mutation.RadiusMultiplier"));
    if(ProfileIndex!=2 && ProfileIndex!=7 && !Modifiers.ExplodeOnImpact) Add(TEXT("Mutation.ExplodeOnImpact"));
    return MutationTags;
}
void ANeonArsenalSpell::FireSpell_Implementation(APooledEnemy* Target,const FSpellModifiers& Snapshot)
{
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    const auto* Caster=Cast<AMVPPlayer>(GetOwner());
    const FSpellModifiers Effective=Caster && Caster->Challenges ? Caster->Challenges->ModifySpellSnapshot(Snapshot) : Snapshot;
    if(Game && Game->ArsenalDirector && ProfileIndex!=INDEX_NONE && IsValid(GetOwner()))
        Game->ArsenalDirector->FirePattern(ProfileIndex,GetOwner(),Cast<AMVPEnemy>(Target),Damage,Effective);
}
ANeonArsenalDirector::ANeonArsenalDirector()
{
    PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    for(int32 P=0;P<10;++P)
    {
        auto Make=[&](const TCHAR* Prefix){auto* M=CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("%s%d"),Prefix,P));M->SetupAttachment(RootComponent);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCastShadow(false);M->bReceivesDecals=false;return M;};
        Bodies.Add(Make(TEXT("Bodies")));Streaks.Add(Make(TEXT("Streaks")));
        for(int32 I=0;I<4;++I) {auto* FX=CreateDefaultSubobject<UNiagaraComponent>(*FString::Printf(TEXT("Sparks%d_%d"),P,I));FX->SetupAttachment(RootComponent);FX->SetAutoActivate(false);Sparks.Add(FX);}
    }
    for(int32 I=0;I<2;++I) {auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("PinkFlash%d"),I));L->SetupAttachment(RootComponent);L->SetCastShadows(false);L->SetVisibility(false);L->SetAttenuationRadius(500);Flashes.Add(L);}
}
void ANeonArsenalDirector::BeginPlay()
{
    Super::BeginPlay();Shots.SetNum(10*Capacity);
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    for(int32 P=0;P<10;++P)
    {
        const auto& Profile=NeonArsenal::Profiles()[P];
        const bool Ball=P==2 || P==9;
        Bodies[P]->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Ball ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Plane.Plane")));
        Streaks[P]->SetStaticMesh(Cylinder);
        auto* Mat=LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Art/Arsenal/M_%s.M_%s"),*Profile.ID.ToString(),*Profile.ID.ToString()));
        Bodies[P]->SetMaterial(0,Mat);
        auto* Beam=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Feedback/M_NeonBeam.M_NeonBeam")),this);
        if(Beam) Beam->SetVectorParameterValue(TEXT("Color"),Profile.Color*3);
        Streaks[P]->SetMaterial(0,Beam);
        for(int32 I=0;I<Capacity;++I) {Bodies[P]->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));Streaks[P]->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector));Shots[P*Capacity+I].HitLives.Reserve(16);}
        auto* System=LoadObject<UNiagaraSystem>(nullptr,*FString::Printf(TEXT("/Game/Art/Arsenal/NS_%s.NS_%s"),*Profile.ID.ToString(),*Profile.ID.ToString()));
        for(int32 I=0;I<4;++I) Sparks[P*4+I]->SetAsset(System);
    }
}
bool ANeonArsenalDirector::IsReady() const {for(int32 I=0;I<10;++I) if(!Bodies[I]->GetMaterial(0) || !Sparks[I*4]->GetAsset()) return false;return Shots.Num()==640;}
int32 ANeonArsenalDirector::ActiveCount() const {int32 N=0;for(const auto& S:Shots) N+=S.Active;return N;}
ANeonArsenalDirector::FShot* ANeonArsenalDirector::Rent(int32 P,int32 Mode)
{
    const int32 First=P*Capacity+(Mode==1 ? Capacity/2 : 0);
    for(int32 I=First;I<First+Capacity/2;++I) if(!Shots[I].Active)
    {
        auto& S=Shots[I];S.HitLives.Reset();S.Active=true;S.Fragment=false;S.Profile=P;S.Mode=Mode;S.Age=0;S.TrailAge=0;S.Life=1.5f;S.Remaining=0;S.Pulses=0;S.Power=0;S.Radius=0;S.Owner.Reset();S.Modifiers=FSpellModifiers();return &S;
    }
    ++DroppedVisuals;return nullptr;
}
bool ANeonArsenalDirector::Clear(FVector A,FVector B) const
{
    FHitResult Wall;return !GetWorld()->LineTraceSingleByObjectType(Wall,A,B,FCollisionObjectQueryParams(ECC_WorldStatic));
}
AMVPEnemy* ANeonArsenalDirector::Nearest(FVector Origin,float Range,const TMap<TWeakObjectPtr<AMVPEnemy>,uint32>& Excluded) const
{
    TArray<FOverlapResult> Found;GetWorld()->OverlapMultiByObjectType(Found,Origin,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(Range));
    AMVPEnemy* Best=nullptr;float Distance=Range*Range;
    for(const auto& R:Found) if(auto* E=Cast<AMVPEnemy>(R.GetActor());E && E->IsPoolActive())
    {
        const auto* Life=Excluded.Find(E);const float D=FVector::DistSquared(Origin,E->GetActorLocation());
        if((!Life || *Life!=E->Generation) && D<Distance && Clear(Origin,E->GetActorLocation())) {Distance=D;Best=E;}
    }return Best;
}
void ANeonArsenalDirector::DamageEnemy(int32 P,AMVPEnemy* Enemy,float Damage,FVector Direction)
{
    if(IsValid(Enemy) && Enemy->IsPoolActive())
    {
        const float Before=Enemy->Health;++Hits[P];Enemy->TakeSpellDamage(Damage,Direction*350);
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector)
            G->SpellImpactDirector->ConfirmDamage(ENeonSpellStyle(6+P),Enemy,FMath::Max(0.f,Before-FMath::Max(0.f,Enemy->Health)));
    }
}
void ANeonArsenalDirector::Burst(int32 P,FVector Position)
{
    const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if(Player && FVector::DistSquared(Position,Player->GetActorLocation())>FMath::Square(2600.f)) return;
    const auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());const float Intensity=G && G->SpellImpactDirector ? G->SpellImpactDirector->GetImpactIntensity() : 1.f;
    if(Intensity>0) for(int32 I=P*4;I<P*4+4;++I) if(SparkBusy[I]<=0) {Sparks[I]->SetWorldLocation(Position);Sparks[I]->SetVariableInt(TEXT("User.SparkCount"),FMath::RoundToInt((P==2 ? 96 : P==7 ? 40 : 28)*Intensity));Sparks[I]->Activate(true);SparkBusy[I]=0.7f;break;}
    if(auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());Game && Game->AudioDirector) Game->AudioDirector->PlayImpact(Position);
    if(P==2) for(int32 I=0;I<2;++I) if(FlashLife[I]<=0) {FlashLife[I]=0.2f;Flashes[I]->SetWorldLocation(Position+FVector(0,0,60));Flashes[I]->SetLightColor(NeonArsenal::Profiles()[P].Color);Flashes[I]->SetVisibility(true);break;}
}
void ANeonArsenalDirector::Area(int32 P,FVector Position,float Radius,float Damage)
{
    TArray<FOverlapResult> Found;GetWorld()->OverlapMultiByObjectType(Found,Position,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(Radius));
    TSet<AMVPEnemy*> Seen;
    for(const auto& R:Found) if(auto* E=Cast<AMVPEnemy>(R.GetActor());E && E->IsPoolActive() && !Seen.Contains(E) && Clear(Position,E->GetActorLocation())) {Seen.Add(E);++AreaHits;DamageEnemy(P,E,Damage,(E->GetActorLocation()-Position).GetSafeNormal());}
}
void ANeonArsenalDirector::Streak(int32 P,FVector A,FVector B,float Lifetime)
{
    if(auto* S=Rent(P,1)) {S->Start=A;S->End=B;S->Life=Lifetime;}
}
void ANeonArsenalDirector::Chain(int32 P,FVector Origin,AMVPEnemy* First,float Damage,int32 Count,float Jump,float Falloff,const FSpellModifiers& Snapshot)
{
    TMap<TWeakObjectPtr<AMVPEnemy>,uint32> Seen;
    AMVPEnemy* Current=First;
    for(int32 Hop=0;Hop<Count && IsValid(Current) && Current->IsPoolActive();++Hop)
    {
        const FVector End=Current->GetActorLocation();if(!Clear(Origin,End)) break;
        Seen.Add(Current,Current->Generation);
        const FVector Side=FVector::CrossProduct((End-Origin).GetSafeNormal(),FVector::UpVector);
        FVector A=Origin;
        for(int32 J=1;J<=3;++J) {FVector B=FMath::Lerp(Origin,End,J/3.f);if(J<3) B+=Side*FMath::FRandRange(-24.f,24.f)+FVector(0,0,FMath::FRandRange(-10.f,20.f));Streak(P,A,B,P==0 ? 0.26f : 0.38f);A=B;}
        ++ChainLinks;DamageEnemy(P,Current,Damage,(End-Origin).GetSafeNormal());Burst(P,End);
        if(Snapshot.ExplodeOnImpact) Area(P,End,90*Snapshot.RadiusMultiplier,Damage*0.3f);
        Origin=End;Damage*=Falloff;Current=Nearest(Origin,Jump,Seen);
    }
}
void ANeonArsenalDirector::FirePattern(int32 P,AActor* Caster,AMVPEnemy* Target,float Damage,const FSpellModifiers& Snapshot)
{
    if(!NeonArsenal::Profiles().IsValidIndex(P) || !IsValid(Caster) || !IsValid(Target) || !Target->IsPoolActive()) return;
    const auto& Profile=NeonArsenal::Profiles()[P];const FVector Origin=Caster->GetActorLocation(),Goal=Target->GetActorLocation(),Aim=(Goal-Origin).GetSafeNormal();
    if(!Clear(Origin,Goal)) return;
    static const FName Animations[]={TEXT("CastStorm"),TEXT("HoverIdleAlt"),TEXT("CastBlood"),TEXT("Cast"),TEXT("Cast"),TEXT("CastStorm"),TEXT("CastBlood"),TEXT("HoverIdleAlt"),TEXT("Cast"),TEXT("CastStorm")};
    ++Casts[P];if(auto* Player=Cast<AMVPPlayer>(Caster)) {Player->TriggerSpellCastVisual(Animations[P]);Player->WeaponMuzzle->SetWorldRotation(Aim.Rotation());Player->OnWeaponAim(Origin,Aim);}
    if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->AudioDirector) G->AudioDirector->PlayAttack(P==2 ? TEXT("Plasma") : P==5 ? TEXT("Laser") : TEXT("Pulse"),Origin);
    const int32 Extra=FMath::Clamp(Snapshot.ProjectileCount-1,0,7);
    if(P<=1) {Chain(P,Origin,Target,Damage,FMath::Min(12,Profile.Count+Extra+Snapshot.PiercingCount),Profile.Radius*Snapshot.RadiusMultiplier,P==0 ? 0.82f : 0.9f,Snapshot);return;}
    if(P==5)
    {
        for(int32 J=0;J<=Extra;++J)
        {
            FVector End=Origin+Aim.RotateAngleAxis((J-Extra*0.5f)*5,FVector::UpVector)*Profile.Range;
            FHitResult Wall;if(GetWorld()->LineTraceSingleByObjectType(Wall,Origin,End,FCollisionObjectQueryParams(ECC_WorldStatic))) End=Wall.ImpactPoint;
            TArray<FHitResult> Found;GetWorld()->SweepMultiByObjectType(Found,Origin,End,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(Profile.Radius*0.5f*Snapshot.RadiusMultiplier));
            TSet<AMVPEnemy*> Seen;for(const auto& H:Found) if(auto* E=Cast<AMVPEnemy>(H.GetActor());E && E->IsPoolActive() && !Seen.Contains(E)) {Seen.Add(E);const FVector Hit=E->GetActorLocation();DamageEnemy(P,E,Damage,Aim);Burst(P,Hit);if(Snapshot.ExplodeOnImpact) Area(P,Hit,100,Damage*0.3f);}
            Streak(P,Origin,End,0.22f);
        }return;
    }
    if(P==2 || P==7)
    {
        for(int32 J=0;J<=Extra;++J) if(auto* S=Rent(P,P==2 ? 2 : 3)) {S->Position=Goal+FVector(0,(J-Extra*0.5f)*100,0);S->Start=Origin;S->Power=Damage;S->Radius=Profile.Radius*Snapshot.RadiusMultiplier;S->Life=P==2 ? 0.45f : 1.4f;S->Remaining=Profile.Count;S->Owner=Caster;}
        return;
    }
    const int32 Count=FMath::Min(12,(P==8 ? 1 : Profile.Count)+Extra);
    for(int32 J=0;J<Count;++J) if(auto* S=Rent(P,0))
    {
        const float Angle=P==6 ? J*360.f/Count : (J-(Count-1)*0.5f)*12;
        S->Position=S->Start=Origin;S->Direction=Aim.RotateAngleAxis(Angle,FVector::UpVector);S->Power=Damage;S->Owner=Caster;S->Life=P==3 ? 1.35f : 1.7f;S->Modifiers=Snapshot;
        S->Remaining=(P==3 ? 4 : P==4 ? 4 : 1)+Snapshot.PiercingCount;S->Radius=J*2.f*PI/FMath::Max(1,Count);
    }
}
void ANeonArsenalDirector::ProjectileStep(FShot& S,float Dt)
{
    const int32 P=S.Profile;
    if(P==9 && S.Owner.IsValid() && S.Age<0.28f) {S.Position=S.Owner->GetActorLocation()+FVector(FMath::Cos(S.Radius+S.Age*12)*65,FMath::Sin(S.Radius+S.Age*12)*65,20);return;}
    if(P==3 && S.Age>0.65f && S.Owner.IsValid()) S.Direction=(S.Owner->GetActorLocation()-S.Position).GetSafeNormal();
    if(P==6) S.Direction=S.Direction.RotateAngleAxis(75*Dt,FVector::UpVector);
    if(P==9) if(auto* E=Nearest(S.Position,900,S.HitLives)) S.Direction=FMath::VInterpNormalRotationTo(S.Direction,(E->GetActorLocation()-S.Position).GetSafeNormal(),Dt,400);
    const FVector Previous=S.Position;FVector Next=Previous+S.Direction*(P==3 ? 950 : P==4 ? 1100 : P==9 ? 800 : 1050)*Dt;
    FHitResult Wall;const bool Blocked=GetWorld()->LineTraceSingleByObjectType(Wall,Previous,Next,FCollisionObjectQueryParams(ECC_WorldStatic));if(Blocked) Next=Wall.ImpactPoint;
    TArray<FHitResult> Found;GetWorld()->SweepMultiByObjectType(Found,Previous,Next,FQuat::Identity,FCollisionObjectQueryParams(ECC_GameTraceChannel1),FCollisionShape::MakeSphere(P==3 ? 22 : 13));
    Found.Sort([](const FHitResult& A,const FHitResult& B){return A.Time<B.Time;});
    for(const auto& H:Found) if(auto* E=Cast<AMVPEnemy>(H.GetActor());E && E->IsPoolActive())
    {
        const auto* Life=S.HitLives.Find(E);if(Life && *Life==E->Generation) continue;
        S.HitLives.Add(E,E->Generation);const FVector Impact=E->GetActorLocation();
        DamageEnemy(P,E,S.Power,S.Direction);Burst(P,Impact);--S.Remaining;
        if(S.Modifiers.ExplodeOnImpact) Area(P,Impact,100*S.Modifiers.RadiusMultiplier,S.Power*0.4f);
        if(P==8 && !S.Fragment)
        {
            for(int32 J=0;J<5;++J) if(auto* Child=Rent(P,0)) {Child->Fragment=true;Child->Position=Impact+S.Direction*40;Child->Start=Child->Position;Child->Direction=S.Direction.RotateAngleAxis((J-2)*25,FVector::UpVector);Child->Power=S.Power*(12.f/34.f);Child->Remaining=1;Child->Life=0.6f;Child->HitLives.Append(S.HitLives);}
        }
        if(S.Remaining<=0) {S.Active=false;break;}
        if(P==4) {if(auto* New=Nearest(Impact,NeonArsenal::Profiles()[P].Radius*S.Modifiers.RadiusMultiplier,S.HitLives)) {S.Direction=(New->GetActorLocation()-Impact).GetSafeNormal();Next=Impact+S.Direction*40;}else S.Active=false;break;}
    }
    S.TrailAge+=Dt;
    if(S.TrailAge>=0.075f)
    {
        Streak(P,Previous-S.Direction*65,Next,0.18f);S.TrailAge=0;
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector) G->SpellImpactDirector->PlayFlightTrail(ENeonSpellStyle(6+P),Next);
    }
    S.Position=Next;if(Blocked)
    {
        Burst(P,Next);S.Active=false;
        if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector) G->SpellImpactDirector->PlayImpact(ENeonSpellStyle(6+P),Next);
    }
}
void ANeonArsenalDirector::Tick(float Dt)
{
    Super::Tick(Dt);for(float& Busy:SparkBusy) Busy=FMath::Max(0.f,Busy-Dt);
    for(int32 I=0;I<2;++I) {FlashLife[I]=FMath::Max(0.f,FlashLife[I]-Dt);Flashes[I]->SetVisibility(FlashLife[I]>0);Flashes[I]->SetIntensity(18000*FMath::Square(FlashLife[I]/0.2f));}
    bool Dirty[10]={};
    for(int32 I=0;I<Shots.Num();++I)
    {
        auto& S=Shots[I];if(!S.Active) continue;const int32 P=S.Profile,Instance=I%Capacity;Dirty[P]=true;S.Age+=Dt;
        if(S.Mode==0) ProjectileStep(S,Dt);
        if(S.Mode==2 && S.Age>=S.Life) {Area(P,S.Position,S.Radius,S.Power);Burst(P,S.Position);S.Mode=4;S.Age=0;S.Life=0.28f;}
        if(S.Mode==3)
        {
            while(S.Pulses<S.Remaining && S.Age>=S.Pulses*0.35f) {Area(P,S.Position,S.Radius,S.Power);Burst(P,S.Position);++S.Pulses;}
        }
        if(S.Age>=S.Life) S.Active=false;
        FTransform Body(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),Trail=Body;
        if(S.Active)
        {
            if(S.Mode==1)
            {
                const FVector Jitter=FVector(0,0,FMath::Sin(S.Age*70+Instance)*5);
                const FVector Delta=S.End-S.Start;const float Width=(P==5 ? 0.12f : 0.06f)*(0.4f+0.6f*(1-S.Age/S.Life));
                Trail=FTransform(FRotationMatrix::MakeFromZ(Delta).ToQuat(),(S.Start+S.End)*0.5f+Jitter,FVector(Width,Width,Delta.Size()/100.f));
            }
            else
            {
                FVector Pos=S.Position,Scale(0.22f);FRotator Rotation(0,S.Age*720,0);
                if(S.Mode==2) {Pos=FMath::Lerp(S.Start,S.Position,FMath::Clamp(S.Age/S.Life,0.f,1.f));Scale=FVector(0.22f+0.14f*S.Age/S.Life);}
                else if(S.Mode==3) {Scale=FVector(S.Radius/50.f,S.Radius/50.f,1);Rotation=FRotator(0,S.Age*30,0);}
                else if(S.Mode==4) Scale=FVector(S.Radius/50.f*(S.Age/S.Life),S.Radius/50.f*(S.Age/S.Life),0.07f);
                else if(P==3) Scale=FVector(0.62f,0.62f,1);
                else if(P==4) Scale=FVector(0.44f,0.44f,1);
                else if(P==6) {Scale=FVector(0.55f,0.28f,1);Rotation=S.Direction.Rotation();}
                else if(P==8) Scale=FVector(S.Fragment ? 0.3f : 0.58f);
                else if(P==9) Scale=FVector(0.24f*(1.f+0.08f*FMath::Sin(S.Age*35)));
                if(auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());G && G->SpellImpactDirector)
                {
                    if(S.Mode==0 || S.Mode==2) Scale*=G->SpellImpactDirector->ReadabilityScale;
                    if(S.Mode==2 || S.Mode==3)
                    {
                        S.TrailAge+=Dt;
                        if(S.TrailAge>=0.12f)
                        {
                            const FVector Emit=S.Mode==3 ? Pos+FVector(FMath::Cos(S.Age*8)*S.Radius*.7f,FMath::Sin(S.Age*8)*S.Radius*.7f,20) : Pos;
                            G->SpellImpactDirector->PlayFlightTrail(ENeonSpellStyle(6+P),Emit);S.TrailAge=0;
                        }
                    }
                }
                Body=FTransform(Rotation,Pos,Scale);
            }
        }
        Bodies[P]->UpdateInstanceTransform(Instance,Body,true,false,true);Streaks[P]->UpdateInstanceTransform(Instance,Trail,true,false,true);
    }
    for(int32 P=0;P<10;++P) if(Dirty[P]) {Bodies[P]->MarkRenderStateDirty();Streaks[P]->MarkRenderStateDirty();}
}
