#include "CodexTestSpell.h"
#include "PooledEnemy.h"

void ACodexTestSpell::FireSpell_Implementation(APooledEnemy* Target, const FSpellModifiers& Snapshot)
{
    ++Shots; LastShot = Snapshot; LastTarget = Target;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EnemySpawner.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "TimerManager.h"

namespace CodexTests
{
    struct FTestWorld
    {
        UWorld* World = nullptr;
        FTestWorld()
        {
            const UWorld::InitializationValues Settings = UWorld::InitializationValues().AllowAudioPlayback(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &Settings);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
        }
        ~FTestWorld()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexPoolTest, "Codex.Runtime.PoolLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCodexPoolTest::RunTest(const FString& Parameters)
{
    CodexTests::FTestWorld Context;
    AEnemySpawner* Spawner = Context.World->SpawnActor<AEnemySpawner>();
    Spawner->EnemyClass = APooledEnemy::StaticClass();
    Spawner->PoolSize = 2; Spawner->MaxActiveEnemies = 1;
    TestTrue(TEXT("Prewarm succeeds"), Spawner->InitializePool());
    APooledEnemy* First = Spawner->ActivateEnemyFromPool(FTransform(FVector(100, 0, 0)));
    if (!TestNotNull(TEXT("First activation"), First)) return false;
    TestTrue(TEXT("Active and visible"), First->IsPoolActive() && !First->IsHidden());
    TestNull(TEXT("Cap enforced"), Spawner->ActivateEnemyFromPool(FTransform::Identity));
    TestTrue(TEXT("Return accepted"), Spawner->ReturnEnemyToPool(First));
    TestFalse(TEXT("Double return rejected"), Spawner->ReturnEnemyToPool(First));
    TestTrue(TEXT("Dormant collision off"), First->IsHidden() && !First->GetActorEnableCollision());
    TestEqual(TEXT("Actor reused"), Spawner->ActivateEnemyFromPool(FTransform::Identity), First);
    Spawner->MaxActiveEnemies = 10;
    TestNotNull(TEXT("Second pool slot"), Spawner->ActivateEnemyFromPool(FTransform::Identity));
    TestNull(TEXT("Exhaustion does not allocate"), Spawner->ActivateEnemyFromPool(FTransform::Identity));
    First->Destroy();
    TestEqual(TEXT("External destruction removes active entry"), Spawner->GetActiveEnemyCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCodexDeckTest, "Codex.Runtime.DeckAndMutations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCodexDeckTest::RunTest(const FString& Parameters)
{
    CodexTests::FTestWorld Context;
    AActor* Player = Context.World->SpawnActor<AActor>();
    AEnemySpawner* Spawner = Context.World->SpawnActor<AEnemySpawner>();
    UCodexDeckComponent* Deck = NewObject<UCodexDeckComponent>(Player);
    Deck->RegisterComponent();
    Deck->EnemySpawner = Spawner;
    for (int32 Index = 0; Index < 4; ++Index)
    {
        UCardDataAsset* Card = NewObject<UCardDataAsset>(Deck);
        Card->CardID = FName(*FString::Printf(TEXT("Card%d"), Index));
        Card->SpellClass = ACodexTestSpell::StaticClass();
        Deck->UnlockedCards.Add(Card); Deck->EquippedDeck.Add(Card);
    }
    UCardDataAsset* RepeatedCard = Deck->EquippedDeck[0];
    Deck->EquippedDeck.Add(RepeatedCard);
    UCardDataAsset* DuplicateID = NewObject<UCardDataAsset>(Deck);
    DuplicateID->CardID = Deck->EquippedDeck[0]->CardID;
    DuplicateID->SpellClass = ACodexTestSpell::StaticClass();
    Deck->EquippedDeck.Add(DuplicateID);
    const TArray<UCardDataAsset*> Hand = Deck->DrawLevelUpHand();
    TestEqual(TEXT("Three choices"), Hand.Num(), 3);
    TSet<FName> IDs;
    for (UCardDataAsset* Card : Hand) IDs.Add(Card->CardID);
    TestEqual(TEXT("Unique IDs"), IDs.Num(), Hand.Num());
    TestTrue(TEXT("Repeat draw preserves pending offer"), Hand == Deck->DrawLevelUpHand());
    if (Hand.IsEmpty()) return false;
    ACodexTestSpell* Spell = Cast<ACodexTestSpell>(Deck->AddSelectedCardSpell(Hand[0]));
    if (!TestNotNull(TEXT("Selected weapon added"), Spell)) return false;
    TestNull(TEXT("Consumed choice cannot be replayed"), Deck->AddSelectedCardSpell(Hand[0]));
    TestEqual(TEXT("Active list updated"), Deck->ActiveWeapons.Num(), 1);
    TestTrue(TEXT("Projectile mutation"), Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.ProjectileCount")), 7.f));
    Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.PiercingCount")), 2.f);
    Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.RadiusMultiplier")), 2.f);
    Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.ExplodeOnImpact")), 1.f);
    TestEqual(TEXT("Eight projectiles"), Spell->Modifiers.ProjectileCount, 8);
    TestFalse(TEXT("Unknown tag rejected"), Spell->ApplyMutation(FGameplayTag(), 1.f));
    TestFalse(TEXT("Invalid radius rejected"), Spell->ApplyMutation(FGameplayTag::RequestGameplayTag(TEXT("Mutation.RadiusMultiplier")), -1.f));
    Deck->DiscardHand();
    Deck->EquippedDeck.Reset();
    Deck->EquippedDeck.Add(Hand[0]);
    TestTrue(TEXT("Already owned card excluded"), Deck->DrawLevelUpHand().IsEmpty());
    Deck->EquippedDeck.Reset();
    for (UCardDataAsset* Card : Deck->UnlockedCards)
        if (Card != Hand[0]) { Deck->EquippedDeck.Add(Card); break; }
    TestEqual(TEXT("Small deck produces smaller hand"), Deck->DrawLevelUpHand().Num(), 1);
    Deck->DiscardHand();
    Spell->InitializeSpell(Hand[0], Spawner);
    Deck->EquippedDeck = {Hand[0]};
    Deck->bOfferOwnedSpellUpgrades = true;
    TSet<FGameplayTag> Applied;
    const float OriginalDamage = Spell->Damage;
    for (int32 I=0; I<3; ++I)
    {
        const auto Offer = Deck->DrawLevelUpHand();
        if (!TestEqual(TEXT("Owned spell offers a transformation"), Offer.Num(), 1)) return false;
        const auto Tag = Deck->GetOfferedMutation(Hand[0]->CardID);
        TestTrue(TEXT("Mutation tag valid"), Tag.IsValid());
        TestFalse(TEXT("Mutation not offered twice"), Applied.Contains(Tag));
        Deck->DrawLevelUpHand();
        TestEqual(TEXT("Pending mutation remains stable"), Deck->GetOfferedMutation(Hand[0]->CardID), Tag);
        TestEqual(TEXT("Mutation preserves weapon instance"), Deck->AddSelectedCardSpell(Hand[0]), static_cast<ASpellBase*>(Spell));
        TestNull(TEXT("Mutation selection cannot replay"), Deck->AddSelectedCardSpell(Hand[0]));
        Applied.Add(Tag);
    }
    TestTrue(TEXT("Nova enabled"), Spell->Modifiers.NovaPattern);
    TestEqual(TEXT("Nova has eight shots"), Spell->Modifiers.ProjectileCount, 8);
    TestEqual(TEXT("Pierces two additional enemies"), Spell->Modifiers.PiercingCount, 2);
    TestTrue(TEXT("Explosive enabled"), Spell->Modifiers.ExplodeOnImpact);
    TestEqual(TEXT("Mutations do not multiply damage"), Spell->Damage, OriginalDamage);
    TestTrue(TEXT("Fully mutated spell excluded"), Deck->DrawLevelUpHand().IsEmpty());
    Spell->InitializeSpell(Hand[0],Spawner);
    TestFalse(TEXT("New run resets nova"),Spell->Modifiers.NovaPattern);
    TestFalse(TEXT("New run resets explosions"),Spell->Modifiers.ExplodeOnImpact);
    return true;
}
#endif
