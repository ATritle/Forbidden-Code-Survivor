#include "ForbiddenCombatSubsystem.h"
#include "MVPGame.h"
#include "EnemySpawner.h"
#include "ForbiddenEnemyBrain.h"
#include "NeonSpellImpactDirector.h"
#include "Kismet/GameplayStatics.h"
#include "MortisTerrain.h"

void UForbiddenCombatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);Fields.SetNum(64);for(auto& F:Fields) F.Seen.Reserve(256);Numbers.SetNum(128);
}
void UForbiddenCombatSubsystem::Clear() {for(auto& F:Fields) {F.Active=false;F.Seen.Reset();}}
void UForbiddenCombatSubsystem::RecordDamage(FVector Position,float Amount,ENeonSpellStyle Style)
{
    if(!FMath::IsFinite(Amount) || Amount<=0) return;
    for(auto& N:Numbers) if(N.Age>=0 && N.Age<0.12f && FVector::DistSquared(N.Position,Position)<FMath::Square(45.f)) {N.Amount+=Amount;return;}
    auto& N=Numbers[NumberCursor++%Numbers.Num()];N.Age=0;N.Position=Position;N.Amount=Amount;N.Color=NeonSpellPalette::Color(Style);
}
bool UForbiddenCombatSubsystem::QueueField(FName ID,ENeonSpellStyle Style,FVector Center,float Power,float Radius,float Delay,int32 Pulses,float Interval,bool Expanding,bool Pull)
{
    if(Center.ContainsNaN() || !FMath::IsFinite(Power) || Power<=0 || !FMath::IsFinite(Radius) || Radius<=0) return false;
    for(auto& F:Fields) if(!F.Active)
    {
        F.Active=true;F.Program=ID;F.Style=Style;F.Position=Center;F.Power=Power;F.Radius=FMath::Clamp(Radius,40.f,500.f);
        F.Delay=FMath::Max(0.f,Delay);F.Interval=FMath::Clamp(Interval,0.1f,1.f);F.Age=F.VisualTime=0;F.Pulses=FMath::Clamp(Pulses,1,8);F.Done=0;F.Expanding=Expanding;F.Pull=Pull;F.Seen.Reset();++AcceptedFields;return true;
    }
    ++DroppedFields;return false;
}
void UForbiddenCombatSubsystem::Tick(float Dt)
{
    auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());if(!G || !G->Spawner || G->bInCodex || G->bRunEnded || G->bChoosingCard || UGameplayStatics::IsGamePaused(this)) return;
    for(auto& N:Numbers) if(N.Age>=0) {N.Age+=Dt;if(N.Age>=0.8f) N.Age=-1;}
    TArray<APooledEnemy*,TInlineAllocator<256>> Enemies;for(APooledEnemy* A:G->Spawner->GetActiveEnemies()) Enemies.Add(A);
    for(auto& F:Fields)
    {
        if(!F.Active) continue;F.Age+=Dt;F.VisualTime-=Dt;
        if(F.Age<F.Delay) continue;
        const float Radius=F.Expanding ? F.Radius*FMath::Clamp((F.Age-F.Delay)/0.65f,0.f,1.f) : F.Radius;
        const bool Pulse=F.Expanding || F.Age>=F.Delay+F.Done*F.Interval;
        if(Pulse)
        {
            for(APooledEnemy* A:Enemies) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive() && FVector::DistSquared(E->GetActorLocation(),F.Position)<FMath::Square(Radius) && UMortisTerrain::ClearSight(E,F.Position))
            {
                const auto* Prior=F.Seen.Find(E);if(F.Expanding && Prior && *Prior==E->Generation) continue;
                F.Seen.Add(E,E->Generation);const float Before=E->Health;E->TakeSpellDamage(F.Power,FVector::ZeroVector,F.Program,false);
                if(E->IsPoolActive()) if(auto* B=E->FindComponentByClass<UForbiddenEnemyBrain>()) B->ApplySlow(F.Expanding ? 0.25f : 0.1f,1.f);
                if(F.Pull && E->IsPoolActive() && E->EnemyTier<3) E->AddActorWorldOffset((F.Position-E->GetActorLocation()).GetSafeNormal2D()*(E->EnemyTier<2 ? 50.f : 12.f),true);
                if(G->SpellImpactDirector) G->SpellImpactDirector->ConfirmDamage(F.Style,E,FMath::Max(0.f,Before-FMath::Max(0.f,E->Health)));
            }
            if(!F.Expanding) ++F.Done;
        }
        if(F.VisualTime<=0 && G->SpellImpactDirector)
        {
            for(int32 I=0;I<8;++I) {const float A=I*2*PI/8+F.Age;G->SpellImpactDirector->PlayFlightTrail(F.Style,F.Position+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,8));}
            F.VisualTime=0.15f;
        }
        if((F.Expanding && F.Age>=F.Delay+0.7f) || (!F.Expanding && F.Done>=F.Pulses)) F.Active=false;
    }
}
