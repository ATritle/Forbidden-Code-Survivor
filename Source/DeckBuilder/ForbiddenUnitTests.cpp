#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CardDataAsset.h"
#include "MVPGame.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "ForbiddenPrograms.h"
#include "ForbiddenProfile.h"

namespace ForbiddenTests
{
    struct FWorld
    {
        UWorld* World;
        FWorld()
        {
            const auto Settings=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Settings);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
        }
        ~FWorld() {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);}
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForbiddenProgramsTest,"Forbidden.Runtime.FinitePrograms",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForbiddenProgramsTest::RunTest(const FString& Parameters)
{
    ForbiddenTests::FWorld W;auto* Owner=W.World->SpawnActor<AActor>();auto* Registry=W.World->SpawnActor<AEnemySpawner>();
    for(FName ID:UForbiddenProfile::ProgramIDs())
    {
        auto* Card=NewObject<UCardDataAsset>();Card->CardID=ID;Card->BaseDamage=30;Card->Cooldown=1;ForbiddenPrograms::Configure(Card);
        TestTrue(TEXT("All 16 programs opt into finite ranks"),Card->bUseProgramProgression);
        TestFalse(TEXT("Distinct authored evolution branches"),Card->EvolutionA.EqualTo(Card->EvolutionB));
        for(int32 Branch=0;Branch<2;++Branch)
        {
            auto* Spell=W.World->SpawnActor<AMVPBoltSpell>();Spell->SetOwner(Owner);Spell->InitializeSpell(Card,Registry);
            for(int32 Rank=1;Rank<3;++Rank)
            {
                const auto Offers=Spell->GetAvailableMutationTags();TestEqual(TEXT("Only the next rank is eligible"),Offers.Num(),1);
                if(Offers.IsEmpty()) return false;
                TestTrue(TEXT("Rank applies"),Spell->ApplyMutation(Offers[0],1));
                TestEqual(TEXT("Rank advances once"),Spell->Modifiers.Rank,Rank+1);
            }
            const auto Offers=Spell->GetAvailableMutationTags();TestEqual(TEXT("Exactly two structural evolutions"),Offers.Num(),2);
            if(Offers.Num()!=2) return false;
            TestTrue(TEXT("Evolution has a readable preview"),!Spell->DescribeMutation(Offers[Branch]).IsEmpty());
            TestTrue(TEXT("Chosen evolution applies"),Spell->ApplyMutation(Offers[Branch],1));
            TestFalse(TEXT("Other evolution cannot stack"),Spell->ApplyMutation(Offers[1-Branch],1));
            TestTrue(TEXT("Fully evolved program leaves draft eligibility"),Spell->GetAvailableMutationTags().IsEmpty());
            TestTrue(TEXT("Finite area bound"),Spell->Modifiers.RadiusMultiplier<=1.5f);
            Spell->InitializeSpell(Card,Registry);TestEqual(TEXT("New run clears evolution"),Spell->Modifiers.Evolution,0);TestEqual(TEXT("New run resets rank"),Spell->Modifiers.Rank,1);Spell->Destroy();
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForbiddenDeckCapacityTest,"Forbidden.Runtime.DeckTransactions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForbiddenDeckCapacityTest::RunTest(const FString& Parameters)
{
    ForbiddenTests::FWorld W;auto* Owner=W.World->SpawnActor<AActor>();auto* Registry=W.World->SpawnActor<AEnemySpawner>();
    auto* Deck=NewObject<UCodexDeckComponent>(Owner);Deck->RegisterComponent();Deck->EnemySpawner=Registry;Deck->MaximumActiveWeapons=2;
    for(int32 I=0;I<4;++I)
    {auto* Card=NewObject<UCardDataAsset>(Deck);Card->CardID=*FString::Printf(TEXT("Test%d"),I);Card->SpellClass=AMVPBoltSpell::StaticClass();Deck->UnlockedCards.Add(Card);}
    for(int32 I=0;I<2;++I) {Deck->EquippedDeck={Deck->UnlockedCards[I]};Deck->DrawLevelUpHand();TestNotNull(TEXT("Starter equips"),Deck->AddSelectedCardSpell(Deck->UnlockedCards[I]));}
    auto* Old=Deck->FindOwnedSpell("Test0");Deck->EquippedDeck={Deck->UnlockedCards[2]};Deck->DrawLevelUpHand();
    TestNull(TEXT("Full capacity requires explicit replacement"),Deck->AddSelectedCardSpell(Deck->UnlockedCards[2]));
    TestEqual(TEXT("Rejected choice preserves old spell"),Deck->FindOwnedSpell("Test0"),Old);
    TestFalse(TEXT("Unknown replacement rejected"),Deck->ReplaceSelectedSpell(Deck->UnlockedCards[2],"Missing"));
    TestTrue(TEXT("Validated replacement commits"),Deck->ReplaceSelectedSpell(Deck->UnlockedCards[2],"Test0"));
    TestNull(TEXT("Replaced spell removed"),Deck->FindOwnedSpell("Test0"));TestEqual(TEXT("Capacity remains exact"),Deck->ActiveWeapons.Num(),2);
    Deck->EquippedDeck=Deck->UnlockedCards;Deck->DrawLevelUpHand();TestTrue(TEXT("First reroll"),Deck->RerollHand());TestTrue(TEXT("Second reroll"),Deck->RerollHand());TestFalse(TEXT("Reroll cannot exceed budget"),Deck->RerollHand());
    if(Deck->CurrentHand.IsEmpty()) return false;const FName Banished=Deck->CurrentHand[0]->CardID;
    TestTrue(TEXT("Banish consumes its budget"),Deck->BanishChoice(0));TestFalse(TEXT("Second banish denied"),Deck->BanishChoice(0));
    TestFalse(TEXT("Banished program excluded"),Deck->CurrentHand.ContainsByPredicate([&](const UCardDataAsset* C){return C && C->CardID==Banished;}));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForbiddenPresetTest,"Forbidden.Runtime.PresetValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForbiddenPresetTest::RunTest(const FString& Parameters)
{
    FForbiddenDeckPreset Deck;const auto& IDs=UForbiddenProfile::ProgramIDs();for(int32 I=0;I<8;++I) Deck.DraftPool.Add(IDs[I]);Deck.Starters={IDs[0],IDs[1]};FString Error;
    TestTrue(TEXT("Eight unique programs, two contained starters"),UForbiddenProfile::ValidateDeck(Deck,IDs,Error));
    Deck.DraftPool[7]=Deck.DraftPool[0];TestFalse(TEXT("Duplicate draft entry rejected"),UForbiddenProfile::ValidateDeck(Deck,IDs,Error));
    Deck.DraftPool[7]=IDs[7];Deck.Starters[1]=IDs[15];TestFalse(TEXT("Starter outside draft pool rejected"),UForbiddenProfile::ValidateDeck(Deck,IDs,Error));
    Deck.Starters={IDs[0],IDs[1]};auto Unlocked=IDs;Unlocked.Remove(IDs[7]);TestFalse(TEXT("Locked card rejected"),UForbiddenProfile::ValidateDeck(Deck,Unlocked,Error));return true;
}
#endif
