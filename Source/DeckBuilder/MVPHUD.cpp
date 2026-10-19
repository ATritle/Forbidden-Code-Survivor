#include "MVPGame.h"
#include "ForbiddenCodeBranding.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "EnemySpawner.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MortisTestingDirector.h"
#include "CyberEnemyRoster.h"
#include "TestingCardInfo.h"
#include "ForbiddenDistrictDirector.h"
#include "ForbiddenCombatSubsystem.h"
#include "ForbiddenEnemyBrain.h"
#include "ForbiddenSettingsControls.h"
#include "NeonMenuAudio.h"

namespace Palette
{
    const FLinearColor Ink(0.035f, 0.055f, 0.075f, 0.97f);
    const FLinearColor Panel(0.065f, 0.09f, 0.11f, 1.f);
    const FLinearColor White(0.88f, 0.92f, 0.88f);
    const FLinearColor Muted(0.44f, 0.57f, 0.59f);
    const FLinearColor Mint(0.08f, 0.85f, 1.f);
    const FLinearColor Gold(1.f, 0.18f, 0.65f);
}

namespace
{
    bool InChamfer(float X,float Y,float Left,float Top,float Width,float Height)
    {
        const float PX=X-Left,PY=Y-Top;
        return PX>=0 && PY>=0 && PX<=Width && PY<=Height && PX-PY<=Width-18.f && PY-PX<=Height-18.f;
    }
}

void AMVPHUD::Box(float X, float Y, float W, float H, FLinearColor Color)
{
    if(GlassMaterial && H>15 && Color.A>0.7f && Color.R+Color.G+Color.B<0.32f)
    {Glass(*FString::Printf(TEXT("HUD_%d_%d_%d_%d"),int32(X),int32(Y),int32(W),int32(H)),X,Y,W,H,InterfaceAccent);return;}
    DrawRect(Color, OffsetX + X * UIScale, OffsetY + Y * UIScale, W * UIScale, H * UIScale);
}
void AMVPHUD::Text(const FString& Value, float X, float Y, float Size, FLinearColor Color)
{
    if(NeonFont)
    {
        const float Ratio=GEngine->GetMediumFont()->GetMaxCharHeight()/FMath::Max(1.f,NeonFont->GetMaxCharHeight());
        FCanvasTextItem Item(FVector2D(OffsetX+X*UIScale,OffsetY+Y*UIScale),FText::FromString(Value),NeonFont,Color);
        Item.Scale=FVector2D(Size*UIScale*1.5f*Ratio);
        Item.FontRenderInfo.GlowInfo.bEnableGlow=Size>=1.f;
        Item.FontRenderInfo.GlowInfo.GlowColor=FLinearColor(Color.R*0.45f,Color.G*0.45f,Color.B*0.45f,Color.A*0.45f);
        Item.FontRenderInfo.GlowInfo.GlowOuterRadius=FVector2D(0.45f,0.5f);Item.FontRenderInfo.GlowInfo.GlowInnerRadius=FVector2D(0.5f,0.51f);
        Canvas->DrawItem(Item);return;
    }
    DrawText(Value, Color, OffsetX + X * UIScale, OffsetY + Y * UIScale,
        GEngine->GetMediumFont(), Size * UIScale * 1.5f, false);
}
void AMVPHUD::CenteredText(const FString& Value, float Y, float Size, FLinearColor Color)
{
    float Width, Height;
    Canvas->StrLen(GEngine->GetMediumFont(), Value, Width, Height);
    Text(Value, 800.f - Width * Size * 1.5f * 0.5f, Y, Size, Color);
}

void AMVPHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    AMVPArenaGameMode* Game = Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    AMVPPlayer* Player = Game ? Game->GetArenaPlayer() : nullptr;
    if (!Game || !Player) return;
    UIScale = FMath::Min(Canvas->SizeX / 1600.f, Canvas->SizeY / 900.f);
    OffsetX = (Canvas->SizeX - 1600.f * UIScale) * 0.5f;
    OffsetY = (Canvas->SizeY - 900.f * UIScale) * 0.5f;
    PrepareNeonInterface();
    HoveredTestingCard=NAME_None;
    if(Game->TestingDirector && Game->TestingDirector->IsActive())
    {
        const auto* Range=Game->TestingDirector.Get();
        Box(28,24,820,124,Palette::Ink);Text(TEXT("TESTING AREA"),50,36,1.35f,Palette::Mint);
        Text(Range->CardCaption,50,76,1.f,NeonSpellPalette::Color(NeonSpellPalette::ForCard(Range->CardID)));
        Text(FString::Printf(TEXT("%d TARGETS   /   %d HITS   /   %.1f DAMAGE   /   %d DEFEATED"),Range->GetTargetCount(),Range->HitCount,Range->TotalDamage,Range->Defeats),50,112,0.75f,Palette::White);
        DrawSettingsButton();Box(28,806,1544,70,Palette::Ink);
        Text(TEXT("WASD move  /  Wheel zoom  /  ESC: intensity, card & targets  /  Exit Testing returns to your session"),50,827,0.77f,Palette::Muted);
        auto WorldText=[&](const FString& Caption,FVector Position,float Size,FLinearColor Color)
        {
            FVector2D Screen;if(!PlayerOwner->ProjectWorldLocationToScreen(Position,Screen)) return;
            const float X=(Screen.X-OffsetX)/UIScale,Y=(Screen.Y-OffsetY)/UIScale;
            if(X<25 || X>1550 || Y<155 || Y>800) return;
            Text(Caption,X-42,Y,Size,Color);
        };
        static const TCHAR* Tiers[]={TEXT("B"),TEXT("M"),TEXT("E"),TEXT("BOSS")};
        for(const auto& Target:Range->GetTargets())
        {
            const auto* Enemy=Target.Enemy.Get();const auto& Profile=CyberEnemies::Roster()[Target.Archetype];
            const FVector Position=Target.bAlive && Enemy ? Enemy->GetCombatVFXLocation()+FVector(0,0,Profile.Height*.5f) : Target.Position+FVector(0,0,100);
            WorldText(Target.bAlive && Enemy ? FString::Printf(TEXT("%s  %.0f / %.0f"),Tiers[Profile.Tier],FMath::Max(0.f,Enemy->Health),Enemy->MaxHealth) : Range->bAutoReset ? TEXT("REBOOTING") : TEXT("DEFEATED"),Position,0.5f,Palette::White);
        }
        const FLinearColor Color=Range->SelectedCards.Num()==1 ? NeonSpellPalette::Color(NeonSpellPalette::ForCard(Range->CardID)) : FLinearColor(0.9f,0.97f,1.f);
        for(const auto& Number:Range->GetDamageNumbers()) if(Number.Age>=0)
            WorldText(FString::Printf(TEXT("-%.1f"),Number.Amount),Number.Position+FVector(0,0,Number.Age*100),0.85f,FLinearColor(Color.R,Color.G,Color.B,1-Number.Age/0.85f));
        UCardDataAsset* Hovered=nullptr;int32 AttackIndex=0;
        const int32 Rows=FMath::DivideAndRoundUp(Player->Deck->EquippedDeck.Num(),8);
        for(UCardDataAsset* Card:Player->Deck->EquippedDeck) if(Card)
        {
            const float X=28+(AttackIndex%8)*194.f,Y=790-Rows*38+(AttackIndex/8)*38;
            const FLinearColor Accent=NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID));
            const float H=Hover(Card->CardID,X,Y,182,32);Glass(Card->CardID,X,Y,182,32,Accent,H);
            Text(Card->Name.ToString(),X+8,Y+8,0.65f,Accent);
            if(Cursor.X>=X && Cursor.X<=X+182 && Cursor.Y>=Y && Cursor.Y<=Y+32) Hovered=Card;
            ++AttackIndex;
        }
        if(Hovered)
        {
            HoveredTestingCard=Hovered->CardID;
            TArray<FString> Source,Lines;TestingCardInfo::Describe(Hovered).ParseIntoArrayLines(Source,false);
            for(FString Line:Source)
            {
                while(Line.Len()>62)
                {
                    int32 Split=62;while(Split>0 && Line[Split]!=' ') --Split;if(Split==0) Split=62;
                    Lines.Add(Line.Left(Split));Line=Line.Mid(Split).TrimStart();
                }
                Lines.Add(Line);
            }
            const float Height=Lines.Num()*19.f+24,X=FMath::Clamp(float(Cursor.X)+18,28.f,1010.f),Y=FMath::Clamp(float(Cursor.Y)-Height-12,160.f,790.f-Height);
            Box(X,Y,560,Height,FLinearColor(0.006f,0.012f,0.025f,0.98f));Box(X,Y,560,2,NeonSpellPalette::Color(NeonSpellPalette::ForCard(Hovered->CardID)));
            for(int32 I=0;I<Lines.Num();++I) Text(Lines[I],X+14,Y+12+I*19,0.67f,I==0 ? NeonSpellPalette::Color(NeonSpellPalette::ForCard(Hovered->CardID)) : Palette::White);
        }
        return;
    }
    if (Game->bInCodex) { DrawCodex(Game);return; }
    DrawHealthAlert(Player);
    DrawChallengeHUD(Player);
    if(Game->DistrictDirector)
    {
        Text(Game->DistrictDirector->GetTracker(),460,28,0.6f,Palette::Mint);
        Text(Game->DistrictDirector->GetPrompt(),570,666,0.85f,Palette::White);
    }
    DrawSettingsButton();
    if(!Game->bRunEnded) {Glass(TEXT("CombatBoard"),1360,186,210,34,Palette::Mint,Hover(TEXT("CombatBoard"),1360,186,210,34));Text(TEXT("LEADERBOARD [L]"),1375,194,0.67f,Palette::Mint);}

    Box(28, 24, 400, 132, Palette::Ink);
    Box(28, 24, 4, 132, Palette::Mint);
    Text(ForbiddenCode::DisplayTitle, 50, 39, 0.76f, Palette::White);
    Text(TEXT("CYBERPUNK SURVIVAL  /  ENERGY DECK"), 50, 68, 0.72f, Palette::Muted);
    Text(FString::Printf(TEXT("VITALITY  %d / %d"), FMath::CeilToInt(Player->Health), FMath::CeilToInt(Player->MaxHealth)), 50, 101, 0.8f, Palette::White);
    Box(50, 130, 350, 6, Palette::Panel);
    Box(50, 130, 350 * Player->Health / FMath::Max(1.f, Player->MaxHealth), 6, FLinearColor(0.88f, 0.24f, 0.34f));

    Box(1200, 24, 372, 108, Palette::Ink);
    Text(FString::Printf(TEXT("%02d:%02d"), FMath::FloorToInt(Game->RunSeconds) / 60, FMath::FloorToInt(Game->RunSeconds) % 60), 1224, 36, 1.7f, Palette::Gold);
    Text(FString::Printf(TEXT("%d BANISHED    /    %d APPROACHING"), Game->Kills, Game->Spawner ? Game->Spawner->GetActiveEnemyCount() : 0), 1224, 88, 0.72f, Palette::Muted);

    Box(28, 786, 1544, 90, Palette::Ink);
    Text(FString::Printf(TEXT("LV %02d"), Game->Level), 50, 800, 1.3f, Palette::Gold);
    Text(FString::Printf(TEXT("ESSENCE  %d / %d"), Game->XP, Game->GetXPRequirement()), 182, 806, 0.82f, Palette::White);
    Box(50, 850, 1500, 5, Palette::Panel);
    Box(50, 850, 1500.f * Game->XP / Game->GetXPRequirement(), 5, Palette::Mint);
    Text(TEXT("MOVE / LEFT STICK   |   BOOST / SHIFT / A   |   T: PRIORITY   |   TAB: INSPECT"), 580, 807, 0.65f, Palette::Muted);
    if(Player->Deck->FindOwnedSpell("TalismanGuard")) Text(FString::Printf(TEXT("TALISMAN CHARGES  %d / 8"),Player->GetTalismanCount()),50,165,0.8f,Palette::Gold);
    const TCHAR* Priorities[]={TEXT("NEAREST"),TEXT("STRONGEST"),TEXT("MARKED")};
    Text(FString::Printf(TEXT("BOOST %s  /  TARGET %s"),Player->BoostCooldown>0 ? *FString::Printf(TEXT("%.1fs"),Player->BoostCooldown) : TEXT("READY"),Priorities[FMath::Clamp(Player->TargetPriority,0,2)]),50,200,0.62f,Palette::Mint);
    int32 WeaponIndex = 0;
    const auto* Profile=Game->GetGameInstance()->GetSubsystem<UForbiddenProfile>();
    const FKey InspectKey=Profile && Profile->Data && Profile->Data->Bindings.Contains("Inspect") ? Profile->Data->Bindings["Inspect"] : EKeys::Tab;
    const bool Inspect=PlayerOwner->IsInputKeyDown(InspectKey) && !Game->bChoosingCard && !Game->bRunEnded;
    if(Inspect) Box(400,240,800,445,FLinearColor(0.005f,0.012f,0.025f,0.97f));
    for (UCardDataAsset* Card : Player->Deck->EquippedDeck)
    {
        ASpellBase* Spell=Card ? Player->Deck->FindOwnedSpell(Card->CardID) : nullptr;if(!Spell) continue;
        const float X=500+WeaponIndex*158.f,Y=712.f;
        const FLinearColor Accent=NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID));
        const float H=Hover(Card->CardID,X,Y,146,64);Glass(Card->CardID,X,Y,146,64,Accent,H);
        SpellArt(*FString::Printf(TEXT("CombatArt%d"),WeaponIndex),Card->Icon,X+4,Y+5,46,46,Accent,H);
        Text(Card->Name.ToString(),X+52,Y+12,0.43f,Accent);Text(FString::Printf(TEXT("R%d %s"),Spell->Modifiers.Rank,Spell->Modifiers.Evolution>0 ? TEXT("EVOLVED") : TEXT("")),X+52,Y+32,0.48f,Palette::White);
        Box(X+5,Y+58,136*Spell->GetCooldownFraction(),3,Accent);
        if(Inspect)
        {
            const FString Branch=Spell->Modifiers.Evolution==1 ? Card->EvolutionA.ToString() : Spell->Modifiers.Evolution==2 ? Card->EvolutionB.ToString() : TEXT("NO EVOLUTION");
            Text(FString::Printf(TEXT("%s / R%d / %s"),*Card->Name.ToString(),Spell->Modifiers.Rank,*Branch),425,258+WeaponIndex*65,0.72f,Accent);
            Text(FString::Printf(TEXT("%.1f DAMAGE  /  %.2fs  /  %d VOLLEY MOD  /  %d PIERCE  /  %.2fx FIELD"),Spell->Damage*Game->PerkDamageMultiplier(),Spell->Cooldown*Player->Deck->GlobalCooldownScale,Spell->Modifiers.ProjectileCount,Spell->Modifiers.PiercingCount,Spell->Modifiers.RadiusMultiplier),425,285+WeaponIndex*65,0.58f,Palette::White);
        }
        ++WeaponIndex;
    }
    if(!Game->bChoosingCard && !Game->bRunEnded && !Game->DecisionWidget && Game->Spawner)
    {
        if(const auto* Combat=GetWorld()->GetSubsystem<UForbiddenCombatSubsystem>();Combat && ForbiddenSettings::Get(this,"DamageNumbers",1)>0.01f)
        {
            const int32 Stride=FMath::Max(1,FMath::RoundToInt(1.f/FMath::Max(0.01f,ForbiddenSettings::Get(this,"DamageNumbers",1))));int32 N=0;
            for(const auto& Number:Combat->GetNumbers()) if(Number.Age>=0 && N++%Stride==0)
            {
                FVector2D Screen;if(!PlayerOwner->ProjectWorldLocationToScreen(Number.Position+FVector(0,0,40+Number.Age*85),Screen)) continue;
                const float X=(Screen.X-OffsetX)/UIScale,Y=(Screen.Y-OffsetY)/UIScale;
                if(X>30 && X<1550 && Y>230 && Y<690) Text(FString::Printf(TEXT("%.0f"),Number.Amount),X,Y,0.65f,FLinearColor(Number.Color.R,Number.Color.G,Number.Color.B,1-Number.Age/0.8f));
            }
        }
        for(APooledEnemy* A:Game->Spawner->GetActiveEnemies()) if(auto* E=Cast<AMVPEnemy>(A);E && E->IsPoolActive() && (E->EnemyTier>=2 || E->Tags.Contains("BroadcastNode") || Player->MarkedEnemy.Get()==E))
        {
            FVector2D Screen;const auto& P=CyberEnemies::Roster()[FMath::Clamp(E->EnemyArchetype,0,9)];
            if(!PlayerOwner->ProjectWorldLocationToScreen(E->GetCombatVFXLocation()+FVector(0,0,P.Height*.45f),Screen)) continue;
            const float X=(Screen.X-OffsetX)/UIScale-44,Y=(Screen.Y-OffsetY)/UIScale;
            if(X<20 || X>1480 || Y<200 || Y>680) continue;
            Box(X,Y,88,5,Palette::Ink);Box(X,Y,88*FMath::Clamp(E->Health/E->MaxHealth,0.f,1.f),5,E->EnemyTier==3 ? Palette::Gold : Palette::Mint);
            if(E->EnemyTier==3) Text(FString::Printf(TEXT("SOVEREIGN / %.0f HP"),E->Health),X-42,Y-24,0.6f,Palette::Gold);
            else if(E->Tags.Contains("BroadcastNode")) Text(TEXT("SHIELD TRANSMITTER"),X-25,Y-20,0.5f,Palette::Mint);
        }
    }

    if (Game->bRunEnded)
    {
        DrawRect(FLinearColor(0.008f, 0.015f, 0.025f, 0.87f), 0, 0, Canvas->SizeX, Canvas->SizeY);
        DrawSettingsButton();
        Box(440, 240, 720, 390, Palette::Ink);
        Box(440, 240, 720, 3, Palette::Gold);
        Text(Game->bVictory ? TEXT("TRANSMISSION TERMINATED") : TEXT("SIGNAL LOST"), 505, 290, 1.45f, Palette::White);
        Text(FString::Printf(TEXT("Level %d    /    %d enemies banished"), Game->Level, Game->Kills), 505, 362, 1.f, Palette::Muted);
        Text(FString::Printf(TEXT("Survived %02d:%02d"), static_cast<int32>(Game->RunSeconds) / 60, static_cast<int32>(Game->RunSeconds) % 60), 505, 409, 1.f, Palette::Gold);
        Text(FString::Printf(TEXT("SCORE %d  /  +%d CODE FRAGMENTS  /  %d CONTRACTS"),Game->RunRecord.Score,Game->RunRecord.Currency,Game->CompletedContracts),505,455,0.68f,Palette::Mint);
        Text(Game->RunRecord.DeathCause,505,486,0.65f,Palette::Muted);
        Glass(TEXT("SubmitScore"),490,580,620,36,Palette::Mint,Hover(TEXT("SubmitScore"),490,580,620,36));
        Text(TEXT("POST SCORE / WINDOWS TOP 10  [L]"),550,590,0.72f,Palette::Mint);
        Box(490, 510, 620, 60, Palette::Panel);
        Text(TEXT("RETURN TO CODEX  /  Click or press R"), 505, 524, 1.05f, Palette::Mint);
        return;
    }
    if (!Game->bChoosingCard || Game->SelectionWidget) return;

    DrawRect(FLinearColor(0.008f, 0.015f, 0.025f, 0.88f), 0, 0, Canvas->SizeX, Canvas->SizeY);
    CenteredText(TEXT("A NEW PAGE AWAKENS"), 170, 1.9f, Palette::White);
    CenteredText(TEXT("Choose a spell. Combat is paused. Each choice restores 12 vitality."), 226, 0.9f, Palette::Muted);
    for (int32 Index = 0; Index < Player->Deck->CurrentHand.Num(); ++Index)
    {
        UCardDataAsset* Card = Player->Deck->CurrentHand[Index];
        if (!Card) continue;
        const float X = 200.f + Index * 410.f;
        const bool bOwned = Player->Deck->FindOwnedSpell(Card->CardID) != nullptr;
        const FLinearColor Accent = Card->MagicSchool == EMagicSchool::Prism ? FLinearColor(1.f, 0.08f, 0.6f)
            : Card->MagicSchool == EMagicSchool::Frost ? FLinearColor(0.4f, 0.7f, 1.f) : Palette::Mint;
        Box(X, 300, 380, 340, Palette::Panel);
        Box(X, 300, 380, 4, Accent);
        Box(X + 24, 326, 46, 42, Palette::Ink);
        Text(FString::FromInt(Index + 1), X + 40, 331, 1.25f, Accent);
        const FGameplayTag Mutation = Player->Deck->GetOfferedMutation(Card->CardID);
        const FString Kind = Mutation.ToString();
        const FString Title = Kind==TEXT("Mutation.Talisman.Add") ? TEXT("REINFORCE BARRIER") : Kind == TEXT("Mutation.Minion.BoneArcher") ? TEXT("BONE ARCHERS")
            : Kind == TEXT("Mutation.Minion.GraveGuard") ? TEXT("GRAVE GUARD")
            : Kind == TEXT("Mutation.Minion.GraveBurst") ? TEXT("GRAVE BURST")
            : Kind == TEXT("Mutation.Nova") ? TEXT("NOVA RITE")
            : Kind == TEXT("Mutation.ExplodeOnImpact") ? TEXT("VOLATILE RITE") : TEXT("PIERCING RITE");
        const FString Detail = Kind==TEXT("Mutation.Talisman.Add") ? TEXT("Gain 2 consumable orbiting cards.\nMaximum 16. No automatic recharge.") : Kind == TEXT("Mutation.Minion.BoneArcher") ? TEXT("Allies fire bone bolts at range.\nReplaces their melee strikes.")
            : Kind == TEXT("Mutation.Minion.GraveGuard") ? TEXT("Allies orbit and defend you.\nThey only pursue nearby threats.")
            : Kind == TEXT("Mutation.Minion.GraveBurst") ? TEXT("Ally attacks explode on impact.\nCombines with melee or ranged.")
            : Kind == TEXT("Mutation.Nova") ? TEXT("Fire at least 8 bolts in a full circle.\nReplaces the aimed spread.")
            : Kind == TEXT("Mutation.ExplodeOnImpact") ? TEXT("Every hit bursts in a 120cm radius.\nNearby foes take 60% spell damage.")
            : TEXT("Each bolt hits 2 extra enemies.\nKeeps its other mutations.");
        Text(bOwned ? TEXT("MUTATE SPELL") : TEXT("NEW SPELL"), X + 90, 338, 0.85f, Palette::Muted);
        Text(bOwned ? Title : Card->Name.ToString(), X + 24, 383, 1.3f, Palette::White);
        if (bOwned) Text(Card->Name.ToString(), X + 24, 419, 0.8f, Accent);
        TArray<FString> Lines;
        (bOwned ? Detail : Card->Description.ToString()).ParseIntoArrayLines(Lines);
        for (int32 Line = 0; Line < Lines.Num(); ++Line) Text(Lines[Line], X + 24, 451 + Line * 28, 0.85f, Palette::Muted);
        Text(FString::Printf(TEXT("%.2fs COOLDOWN"), Card->Cooldown), X + 24, 542, 0.8f, Accent);
        Box(X + 24, 584, 332, 35, Palette::Ink);
        Text(TEXT("CLICK OR PRESS ") + FString::FromInt(Index + 1), X + 73, 591, 0.8f, Accent);
    }
    if (Player->Deck->CurrentHand.IsEmpty())
    {
        Box(200,300,380,340,Palette::Panel);
        Box(200,300,380,4,Palette::Mint);
        Text(TEXT("CODEX COMPLETE"),224,338,0.9f,Palette::Gold);
        Text(TEXT("RECOVER VITALITY"),224,395,1.25f,Palette::White);
        Text(TEXT("All available rites are mastered."),224,451,0.85f,Palette::Muted);
        Text(TEXT("Restore 12 vitality and continue."),224,480,0.85f,Palette::Muted);
        Text(TEXT("CLICK OR PRESS 1"),265,591,0.85f,Palette::Mint);
    }
    CenteredText(FString::Printf(TEXT("%d choice%s remaining"), Game->PendingLevels, Game->PendingLevels == 1 ? TEXT("") : TEXT("s")), 685, 0.9f, Palette::Gold);
}

void AMVPHUD::ClickAt(float X, float Y)
{
    auto* PC=Cast<AMVPController>(PlayerOwner);
    if(PC && (PC->HelpWidget || PC->IsSettingsOpen() || PC->IsChallengeOpen() || PC->ScoreWidget)) return;
    AMVPArenaGameMode* Game = Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if (!Game || Game->DecisionWidget || UIScale <= 0.f) return;
    const float LocalX = (X - OffsetX) / UIScale;
    const float LocalY = (Y - OffsetY) / UIScale;
    if(!Game->bInCodex && InChamfer(LocalX,LocalY,1360,142,190,36)) {if(PC) PC->ToggleSettings();return;}
    if((!Game->bInCodex && InChamfer(LocalX,LocalY,1360,186,190,36)) || (Game->bInCodex && InChamfer(LocalX,LocalY,920,182,230,46)) || (Game->bRunEnded && InChamfer(LocalX,LocalY,490,580,620,36))) {if(PC) PC->OpenLeaderboard();return;}
    if (Game->bInCodex)
    {
        if(InChamfer(LocalX,LocalY,80,145,255,34)) {if(PC) PC->OpenHelp();return;}
        if(InChamfer(LocalX,LocalY,350,145,255,34)) {if(PC) PC->ToggleSettings();return;}
        if(InChamfer(LocalX,LocalY,620,145,205,34)) {if(PC) PC->CloseGame();return;}
        if (LocalY >= 182 && LocalY <= 228)
            for (int32 I=0; I<5; ++I)
                if (LocalX >= 80+I*160 && LocalX <= 230+I*160) { NeonMenuAudio::Input(this);Game->SchoolFilter = I==0 ? -1 : I+5; Game->LibraryPage=0; return; }
        const auto Library = Game->GetLibraryCards();
        const int32 LastPage=FMath::Max(0,(Library.Num()-1)/3);
        if (LocalY>=182 && LocalY<=228)
        {
            if (InChamfer(LocalX,LocalY,1240,182,120,46)) {NeonMenuAudio::Input(this);Game->LibraryPage=FMath::Max(0,Game->LibraryPage-1);}
            if (InChamfer(LocalX,LocalY,1380,182,120,46)) {NeonMenuAudio::Input(this);Game->LibraryPage=FMath::Min(LastPage,Game->LibraryPage+1);}
        }
        if (LocalY >= 270 && LocalY <= 570)
            for (int32 I=0; I<3 && Library.IsValidIndex(Game->LibraryPage*3+I); ++I)
                if (LocalX >= 80+I*485 && LocalX <= 540+I*485) { NeonMenuAudio::Input(this);Game->OpenProgramEditor(Library[Game->LibraryPage*3+I]->CardID); return; }
        if (LocalY >= 640 && LocalY <= 696)
            for (int32 I=0; I<Game->StartingCards.Num(); ++I)
                if (LocalX >= 80+I*400 && LocalX <= 460+I*400) { NeonMenuAudio::Input(this);Game->OpenProgramEditor(Game->StartingCards[I]); return; }
        if (LocalY >= 776 && LocalY <= 840)
        {
            if (InChamfer(LocalX,LocalY,80,776,290,64)) Game->SaveStartingDeck();
            if (InChamfer(LocalX,LocalY,410,776,330,64)) {if(PC) PC->OpenHideout();}
            if (InChamfer(LocalX,LocalY,1080,776,440,64)) Game->StartSelectedRun();
        }
        return;
    }
    if (Game->bRunEnded)
    {
        if (LocalX >= 490 && LocalX <= 1110 && LocalY >= 510 && LocalY <= 570)
        {
            PlayerOwner->SetPause(false);
            UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
        }
        return;
    }
    if (!Game->bChoosingCard || Game->SelectionWidget) return;
    if (LocalY < 300 || LocalY > 640) return;
    for (int32 Index = 0; Index < 3; ++Index)
        if (LocalX >= 200 + Index * 410 && LocalX <= 580 + Index * 410)
        {
            Game->ChooseCard(Index);
            return;
        }
}

void AMVPHUD::DrawSettingsButton()
{
    const float H=Hover(TEXT("SettingsButton"),1360,142,190,36);
    Glass(TEXT("SettingsButton"),1360,142,190,36,InterfaceAccent,H,false,2);
    Text(TEXT("SETTINGS  /  ESC"),1375,151-H*2,0.76f,InterfaceAccent);
}

