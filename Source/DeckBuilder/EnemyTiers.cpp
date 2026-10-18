#include "MVPGame.h"
#include "CyberEnemyRoster.h"
#include "NecromancyVisualComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

int32 AMVPEnemy::SelectTimedTier(float Seconds,float Roll)
{
    if(Seconds>=360.f && Roll<0.02f) return 3;
    if(Seconds>=180.f && Roll<0.2f) return 2;
    if(Seconds>=75.f && Roll<0.55f) return 1;
    return 0;
}
void AMVPEnemy::ApplyEnemyTier(int32 Tier)
{
    const int32 FirstByTier[]={0,4,6,9};ApplyEnemyArchetype(FirstByTier[FMath::Clamp(Tier,0,3)]);
}
void AMVPEnemy::ApplyEnemyArchetype(int32 Index)
{
    Index=FMath::Clamp(Index,0,9);const auto& P=CyberEnemies::Roster()[Index];
    if(EnemyArchetype!=Index)
    {
        CharacterArt->LoadCharacter(P.ID,P.Height);
        CharacterArt->AddLocalOffset(FVector(0,0,55-(P.CollisionRadius+21)));
    }
    EnemyArchetype=Index;EnemyTier=P.Tier;MaxHealth=P.Health;Health=P.Health;BaseMovementSpeed=P.Speed;
    ContactDamage=P.ContactDamage;EssenceReward=P.Essence;
    CastChecked<USphereComponent>(RootComponent)->SetSphereRadius(P.CollisionRadius);
    Visual->SetWorldScale3D(FVector(P.Height/150.f));
    Visual->SetVisibility(!CharacterArt->IsArtReady());
}
FVector AMVPEnemy::GetCombatVFXLocation() const
{
    const auto& P=CyberEnemies::Roster()[FMath::Clamp(EnemyArchetype,0,9)];
    return GetActorLocation()+FVector(0,0,P.Height*0.55f-(P.CollisionRadius+21));
}
void AMVPEnemy::SetCombatHitFlash(FLinearColor Color,float Strength)
{
    CharacterArt->SetCustomPrimitiveDataVector4(0,FVector4(Color.R,Color.G,Color.B,FMath::Clamp(Strength,0.f,1.f)));
}
