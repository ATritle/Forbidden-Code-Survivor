#include "CodexCardSelectionWidget.h"
#include "ForbiddenCodeBranding.h"
#include "MVPGame.h"
#include "NeonArsenal.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "NativeGameplayTags.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolNecromancy, "MagicSchool.Necromancy");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolFire, "MagicSchool.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolBlood, "MagicSchool.Blood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolCurses, "MagicSchool.Curses");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolLightning, "MagicSchool.Lightning");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolFrost, "MagicSchool.Frost");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolPulse,"MagicSchool.Pulse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolPrism,"MagicSchool.Prism");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolKinetic,"MagicSchool.Kinetic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(SchoolBarrier,"MagicSchool.Barrier");

void UCodexCardSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    if (!WidgetTree->RootWidget)
    {
        bNativeLayout = true;
        auto* Root = WidgetTree->ConstructWidget<UOverlay>();
        WidgetTree->RootWidget = Root;
        auto* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
        Blur->SetBlurStrength(8.f);
        auto* BlurSlot = Root->AddChildToOverlay(Blur);
        BlurSlot->SetHorizontalAlignment(HAlign_Fill);
        BlurSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Shade = WidgetTree->ConstructWidget<UBorder>();
        Shade->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.03f, 0.85f));
        auto* ShadeSlot = Root->AddChildToOverlay(Shade);
        ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
        ShadeSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        auto* Center = Root->AddChildToOverlay(Column);
        Center->SetHorizontalAlignment(HAlign_Center);
        Center->SetVerticalAlignment(VAlign_Center);
        auto* Heading = WidgetTree->ConstructWidget<UTextBlock>();
        Heading->SetText(FText::FromString(TEXT("FORBIDDEN CODE: SURVIVOR\nEnergy unlocked  /  Choose your upgrade\n")));
        Heading->SetJustification(ETextJustify::Center);
        Column->AddChild(Heading);
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Column->AddChild(Row);
        auto* Settings=WidgetTree->ConstructWidget<UButton>();
        auto* SettingsLabel=WidgetTree->ConstructWidget<UTextBlock>();SettingsLabel->SetText(FText::FromString(TEXT("AUDIO SETTINGS  /  ESC")));SettingsLabel->SetJustification(ETextJustify::Center);Settings->SetContent(SettingsLabel);
        Column->AddChild(Settings);Settings->OnClicked.AddDynamic(this,&ThisClass::OpenSettings);
        CardOption_A = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CardOption_A"));
        CardOption_B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CardOption_B"));
        CardOption_C = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CardOption_C"));
        for (UButton* Button : {CardOption_A.Get(), CardOption_B.Get(), CardOption_C.Get()})
        {
            auto* Size = WidgetTree->ConstructWidget<USizeBox>();
            Size->SetWidthOverride(350.f);
            Size->SetHeightOverride(590.f);
            Size->AddChild(Button);
            Row->AddChildToHorizontalBox(Size)->SetPadding(FMargin(12.f));
        }
    }
    if (CardOption_A) CardOption_A->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickA);
    if (CardOption_B) CardOption_B->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickB);
    if (CardOption_C) CardOption_C->OnClicked.AddUniqueDynamic(this, &ThisClass::ClickC);
}

void UCodexCardSelectionWidget::InitializeSelectionScreen(TArray<UCardDataAsset*> ChoiceCards)
{
    Choices.Reset(); DisplayCards.Reset(); bConsumed = false;
    RevealStartTime = FPlatformTime::Seconds();
    auto* Player = GetOwningPlayerPawn<AMVPPlayer>();
    if (!Player || ChoiceCards.Num() > 3) { SetIsEnabled(false); return; }
    // Reject altered, reordered or stale offers. Never redraw a hand from the UI.
    const auto& Hand = Player->Deck->CurrentHand;
    if (Hand.Num() != ChoiceCards.Num()) { SetIsEnabled(false); return; }
    TSet<FName> IDs;
    for (int32 I = 0; I < Hand.Num(); ++I)
    {
        if (!IsValid(ChoiceCards[I]) || ChoiceCards[I] != Hand[I] || IDs.Contains(ChoiceCards[I]->CardID))
        { SetIsEnabled(false); return; }
        IDs.Add(ChoiceCards[I]->CardID);
        Choices.Add(ChoiceCards[I]);
    }
    SetIsEnabled(true);
    const FGameplayTag Schools[] = {SchoolNecromancy, SchoolFire, SchoolBlood, SchoolCurses, SchoolFrost, SchoolLightning,SchoolPulse,SchoolPrism,SchoolKinetic,SchoolBarrier};
    UButton* Buttons[] = {CardOption_A, CardOption_B, CardOption_C};
    const int32 Count = FMath::Max(1, Choices.Num());
    for (int32 I = 0; I < 3; ++I)
    {
        UButton* Button = Buttons[I];
        if (!Button) continue;
        Button->SetVisibility(I < Count ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (I >= Count) continue;
        FCardDisplayData Data;
        if (Choices.IsEmpty())
        {
            Data.Title = FText::FromString(TEXT("CODEX COMPLETE"));
            Data.Description = FText::FromString(TEXT("Recover 12 vitality and continue."));
        }
        else
        {
            const UCardDataAsset* Card = Choices[I];
            Data.Title = Card->Name; Data.Description = Card->Description;
            Data.RarityColor = Card->RarityColor; Data.Icon = Card->Icon; Data.SchoolIcon = Card->SchoolIcon;
            const TCHAR* Rarities[] = {TEXT("COMMON"),TEXT("RARE"),TEXT("EPIC"),TEXT("LEGENDARY")};
            Data.RarityLabel = FText::FromString(Rarities[FMath::Clamp(int32(Card->Rarity),0,3)]);
            Data.MagicSchoolTag = Schools[FMath::Clamp(int32(Card->MagicSchool), 0, 9)];
            const FGameplayTag MutationTag = Player->Deck->GetOfferedMutation(Card->CardID);
            const FString Mutation = MutationTag.ToString();
            if (MutationTag.IsValid())
            {
                FString Detail;
                if (Mutation == TEXT("Mutation.Nova")) Detail = TEXT("PRISM NOVA\nFire at least 8 bolts in a full circle.");
                else if (Mutation == TEXT("Mutation.ExplodeOnImpact")) Detail = TEXT("ENERGY DETONATION\nHits explode. Nearby foes take 60% damage.");
                else if (Mutation == TEXT("Mutation.PiercingCount")) Detail = TEXT("PHASE PIERCING\nEach projectile hits 2 extra enemies.");
                else if (Mutation == TEXT("Mutation.Minion.BoneArcher")) Detail = TEXT("BONE ARCHERS\nAllies replace melee with ranged bone bolts.");
                else if (Mutation == TEXT("Mutation.Minion.GraveGuard")) Detail = TEXT("GRAVE GUARD\nAllies orbit and defend against nearby threats.");
                else if (Mutation == TEXT("Mutation.Minion.GraveBurst")) Detail = TEXT("GRAVE BURST\nAlly attacks explode on impact.");
                else if(Mutation==TEXT("Mutation.Talisman.Add")) Detail=TEXT("REINFORCE BARRIER\nAdd 2 consumable talismans (maximum 16).\nEach destroys one enemy on contact.");
                else if(Mutation==TEXT("Mutation.ProjectileCount")) Detail=TEXT("ECHO CAST\nAdd one projectile per cast.");
                else if(Mutation==TEXT("Mutation.RadiusMultiplier")) Detail=TEXT("AMPLIFY FIELD\nIncrease field size by 25%.");
                else Detail = Mutation;
                if(NeonArsenal::Find(Card->CardID)!=INDEX_NONE) Detail=NeonArsenal::MutationDescription(Card->CardID,MutationTag);
                Data.Description = FText::FromString(Detail);
            }
        }
        DisplayCards.Add(Data);
        if (bNativeLayout) BuildCardFace(Button, Data, I);
        OnCardDisplayUpdated(I, Data);
    }
}

bool UCodexCardSelectionWidget::SelectChoice(int32 Index)
{
    if (bConsumed || !GetIsEnabled()) return false;
    auto* Game = Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    auto* Player = GetOwningPlayerPawn<AMVPPlayer>();
    if (!Game || Game->SelectionWidget != this || !Player || !Game->bChoosingCard) return false;
    if (Choices.IsEmpty()) { if (Index != 0 || !Player->Deck->CurrentHand.IsEmpty()) return false; }
    else if (!Choices.IsValidIndex(Index) || !Player->Deck->CurrentHand.IsValidIndex(Index)
        || Player->Deck->CurrentHand[Index] != Choices[Index]) return false;
    bConsumed = true;
    // ChooseCard validates/equips through Deck, removes this widget and restores input/pause
    // only after all queued levels are consumed. Never unpause from NativeDestruct.
    if (!Game->ChooseCard(Index)) { bConsumed = false; return false; }
    return true;
}
void UCodexCardSelectionWidget::ClickA() { SelectChoice(0); }
void UCodexCardSelectionWidget::ClickB() { SelectChoice(1); }
void UCodexCardSelectionWidget::ClickC() { SelectChoice(2); }
FReply UCodexCardSelectionWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    if(Key==EKeys::Escape) {if(!Event.IsRepeat()) OpenSettings();return FReply::Handled();}
    const int32 Index = Key == EKeys::One ? 0 : Key == EKeys::Two ? 1 : Key == EKeys::Three ? 2 : INDEX_NONE;
    if (Index != INDEX_NONE) { if (!Event.IsRepeat()) SelectChoice(Index); return FReply::Handled(); }
    return Super::NativeOnKeyDown(Geometry, Event);
}
void UCodexCardSelectionWidget::OpenSettings() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->ToggleSettings();}
