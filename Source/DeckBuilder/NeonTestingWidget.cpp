#include "NeonTestingWidget.h"
#include "MVPGame.h"
#include "MortisTestingDirector.h"
#include "TestingCardInfo.h"
#include "CyberEnemyRoster.h"
#include "CodexDeckComponent.h"
#include "CardDataAsset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "InputCoreTypes.h"

void UNeonTestingWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());auto* Player=Game ? Game->GetArenaPlayer() : nullptr;if(!Player) return;
    const FLinearColor Cyan(0.05f,0.85f,1),White(0.85f,0.94f,1),Muted(0.5f,0.65f,0.75f),Ink(0.012f,0.022f,0.04f);
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(8);auto* BS=Root->AddChildToOverlay(Blur);BS->SetHorizontalAlignment(HAlign_Fill);BS->SetVerticalAlignment(VAlign_Fill);
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(0.005f,0.01f,0.025f,0.6f));auto* SS=Root->AddChildToOverlay(Shade);SS->SetHorizontalAlignment(HAlign_Fill);SS->SetVerticalAlignment(VAlign_Fill);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1280);Size->SetMaxDesiredHeight(820);
    auto* Center=Root->AddChildToOverlay(Size);Center->SetHorizontalAlignment(HAlign_Center);Center->SetVerticalAlignment(VAlign_Center);
    auto* Rim=WidgetTree->ConstructWidget<UBorder>();Rim->SetBrushColor(Cyan);Rim->SetPadding(FMargin(2));Size->SetContent(Rim);
    MainBody=WidgetTree->ConstructWidget<UBorder>();MainBody->SetBrushColor(Ink);MainBody->SetPadding(FMargin(24));Rim->SetContent(MainBody);
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();MainBody->SetContent(Column);
    auto Label=[&](const FString& Caption,int32 Points,FLinearColor Color)
    {
        auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Caption));auto Font=T->GetFont();Font.Size=Points;T->SetFont(Font);T->SetColorAndOpacity(Color);return T;
    };
    auto Text=[&](UVerticalBox* Parent,const FString& Caption,int32 Points,FLinearColor Color)
    {auto* T=Label(Caption,Points,Color);Parent->AddChildToVerticalBox(T)->SetPadding(FMargin(0,5));return T;};
    auto Button=[&](UVerticalBox* Parent,FName Name,const TCHAR* Caption)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);B->SetBackgroundColor(FLinearColor(0.025f,0.15f,0.22f));auto* T=Label(Caption,14,Cyan);T->SetJustification(ETextJustify::Center);B->SetContent(T);Parent->AddChildToVerticalBox(B)->SetPadding(FMargin(0,5));return B;
    };
    Text(Column,TEXT("TESTING AREA  /  NEON PROVING GROUND"),23,Cyan);
    Text(Column,TEXT("Select one or more base attacks. Hover attack names for descriptions and stats. Your original session stays preserved."),12,Muted)->SetWrapTextAt(1200);
    auto* Body=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Body)->SetPadding(FMargin(0,8));
    auto MakeColumn=[&](float Width)
    {
        auto* S=WidgetTree->ConstructWidget<USizeBox>();S->SetWidthOverride(Width);S->SetMaxDesiredHeight(535);
        auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();S->SetContent(Scroll);auto* V=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(V);Body->AddChildToHorizontalBox(S)->SetPadding(FMargin(0,0,22,0));return V;
    };
    auto* Attacks=MakeColumn(540);auto* Targets=MakeColumn(640);
    Text(Attacks,TEXT("ATTACKS  /  SELECT ANY COMBINATION"),13,Cyan);
    for(UCardDataAsset* Card:Player->Deck->UnlockedCards) if(Card)
    {
        const int32 I=IDs.Add(Card->CardID);auto* Toggle=WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),*FString::Printf(TEXT("AttackToggle%d"),I));
        Toggle->SetIsChecked(Game->TestingDirector ? Game->TestingDirector->SelectedCards.Contains(Card->CardID) : Card->CardID==TEXT("PulseDart"));
        Toggle->SetContent(Label(Card->Name.ToString(),14,NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID))));Toggle->SetToolTip(MakeTooltip(Card));
        Attacks->AddChildToVerticalBox(Toggle)->SetPadding(FMargin(0,3));AttackToggles.Add(Toggle);
    }
    auto* Bulk=WidgetTree->ConstructWidget<UHorizontalBox>();Attacks->AddChildToVerticalBox(Bulk)->SetPadding(FMargin(0,5));
    auto BulkButton=[&](FName Name,const TCHAR* Caption)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);B->SetBackgroundColor(FLinearColor(0.025f,0.15f,0.22f));auto* T=Label(Caption,13,Cyan);T->SetJustification(ETextJustify::Center);B->SetContent(T);
        auto* Slot=Bulk->AddChildToHorizontalBox(B);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(0,0,6,0));return B;
    };
    BulkButton(TEXT("SelectAllAttacks"),TEXT("SELECT ALL"))->OnClicked.AddDynamic(this,&ThisClass::SelectAll);
    BulkButton(TEXT("ClearAllAttacks"),TEXT("CLEAR ALL"))->OnClicked.AddDynamic(this,&ThisClass::ClearAll);
    Text(Targets,TEXT("STATIC TARGETS  /  STEPS OF 5  /  200 TOTAL MAX"),13,Cyan);
    auto SpinStyle=[&](USpinBox* Spin)
    {
        auto Style=Spin->GetWidgetStyle();Style.BackgroundBrush.TintColor=FLinearColor(0.025f,0.08f,0.12f);Style.ActiveBackgroundBrush.TintColor=FLinearColor(0.02f,0.18f,0.24f);Style.HoveredBackgroundBrush.TintColor=FLinearColor(0.03f,0.22f,0.3f);
        Style.ActiveFillBrush.TintColor=FLinearColor(0.02f,0.18f,0.24f);Style.InactiveFillBrush.TintColor=FLinearColor(0.02f,0.09f,0.14f);Style.HoveredFillBrush.TintColor=FLinearColor(0.03f,0.2f,0.27f);Style.ForegroundColor=Cyan;Spin->SetWidgetStyle(Style);Spin->SetForegroundColor(Cyan);
    };
    static const TCHAR* Tiers[]={TEXT("BASIC"),TEXT("MEDIUM"),TEXT("ELITE"),TEXT("BOSS")};
    for(int32 I=0;I<10;++I)
    {
        const auto& P=CyberEnemies::Roster()[I];auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Targets->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,4));
        auto* Toggle=WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),*FString::Printf(TEXT("EnemyToggle%d"),I));
        const int32 Count=Game->TestingDirector ? Game->TestingDirector->EnemyCounts[I] : 5;Toggle->SetIsChecked(Count>0);
        Toggle->SetContent(Label(FString::Printf(TEXT("%s / %s / %.0f HP"),P.Name,Tiers[P.Tier],P.Health),12,White));Row->AddChildToHorizontalBox(Toggle)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Enabled.Add(Toggle);
        auto* CountBox=WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(),*FString::Printf(TEXT("EnemyCount%d"),I));CountBox->SetMinValue(5);CountBox->SetMaxValue(50);CountBox->SetMinSliderValue(5);CountBox->SetMaxSliderValue(50);CountBox->SetDelta(5);CountBox->SetAlwaysUsesDeltaSnap(true);CountBox->SetMinFractionalDigits(0);CountBox->SetMaxFractionalDigits(0);CountBox->SetValue(FMath::Max(5,Count));SpinStyle(CountBox);
        auto* Width=WidgetTree->ConstructWidget<USizeBox>();Width->SetWidthOverride(80);Width->SetContent(CountBox);Row->AddChildToHorizontalBox(Width)->SetPadding(FMargin(10,0));Counts.Add(CountBox);
    }
    AutoReset=WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("RebootTargets"));AutoReset->SetIsChecked(!Game->TestingDirector || Game->TestingDirector->bAutoReset);AutoReset->SetContent(Label(TEXT("Reboot defeated targets after 1.5 seconds"),12,White));Targets->AddChildToVerticalBox(AutoReset)->SetPadding(FMargin(0,10));
    Text(Targets,TEXT("GRID SPACING (cm) / closer targets allow chain attacks"),12,Muted);
    Spacing=WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(),TEXT("GridSpacing"));Spacing->SetMinValue(200);Spacing->SetMaxValue(500);Spacing->SetDelta(20);Spacing->SetAlwaysUsesDeltaSnap(true);Spacing->SetMaxFractionalDigits(0);Spacing->SetValue(Game->TestingDirector ? Game->TestingDirector->GridSpacing : 220);SpinStyle(Spacing);Targets->AddChildToVerticalBox(Spacing)->SetPadding(FMargin(0,5));
    Status=Text(Column,TEXT("WASD move / wheel zoom / hover attack names in the range for stats. Apply resets selected attacks, targets and totals."),12,Muted);Status->SetWrapTextAt(1200);
    Button(Column,TEXT("StartTesting"),TEXT("APPLY SETUP & ENTER RANGE"))->OnClicked.AddDynamic(this,&ThisClass::Start);
    Button(Column,TEXT("BackToSettings"),TEXT("BACK TO SETTINGS / ESC"))->OnClicked.AddDynamic(this,&ThisClass::Back);
    if(Game->TestingDirector && Game->TestingDirector->IsActive()) Button(Column,TEXT("ExitTesting"),TEXT("EXIT TESTING & RETURN TO ORIGINAL SESSION"))->OnClicked.AddDynamic(this,&ThisClass::Exit);

    WarningLayer=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("ChainSpacingWarning"));auto* WS=Root->AddChildToOverlay(WarningLayer);WS->SetHorizontalAlignment(HAlign_Fill);WS->SetVerticalAlignment(VAlign_Fill);
    auto* Dim=WidgetTree->ConstructWidget<UBorder>();Dim->SetBrushColor(FLinearColor(0,0,0,0.8f));auto* DS=WarningLayer->AddChildToOverlay(Dim);DS->SetHorizontalAlignment(HAlign_Fill);DS->SetVerticalAlignment(VAlign_Fill);
    auto* WarnSize=WidgetTree->ConstructWidget<USizeBox>();WarnSize->SetWidthOverride(690);auto* WC=WarningLayer->AddChildToOverlay(WarnSize);WC->SetHorizontalAlignment(HAlign_Center);WC->SetVerticalAlignment(VAlign_Center);
    auto* WarnRim=WidgetTree->ConstructWidget<UBorder>();WarnRim->SetBrushColor(FLinearColor(1,0.55f,0.04f));WarnRim->SetPadding(FMargin(2));WarnSize->SetContent(WarnRim);
    auto* WarnBody=WidgetTree->ConstructWidget<UBorder>();WarnBody->SetBrushColor(Ink);WarnBody->SetPadding(FMargin(24));WarnRim->SetContent(WarnBody);auto* WarnColumn=WidgetTree->ConstructWidget<UVerticalBox>();WarnBody->SetContent(WarnColumn);
    Text(WarnColumn,TEXT("CHAIN RANGE WARNING"),23,FLinearColor(1,0.65f,0.1f));WarningText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ChainWarningText"));WarningText->SetColorAndOpacity(White);auto WarnFont=WarningText->GetFont();WarnFont.Size=14;WarningText->SetFont(WarnFont);WarningText->SetWrapTextAt(630);WarnColumn->AddChildToVerticalBox(WarningText)->SetPadding(FMargin(0,10));
    ContinueButton=Button(WarnColumn,TEXT("ConfirmChainSpacing"),TEXT("CONTINUE WITH THIS SPACING"));ContinueButton->OnClicked.AddDynamic(this,&ThisClass::ContinueWarning);
    Button(WarnColumn,TEXT("CancelChainSpacing"),TEXT("BACK & ADJUST TARGET SPACING"))->OnClicked.AddDynamic(this,&ThisClass::CancelWarning);
    WarningLayer->SetVisibility(ESlateVisibility::Collapsed);
}
UWidget* UNeonTestingWidget::MakeTooltip(const UCardDataAsset* Card)
{
    auto* Rim=WidgetTree->ConstructWidget<UBorder>();Rim->SetBrushColor(NeonSpellPalette::Color(NeonSpellPalette::ForCard(Card->CardID)));Rim->SetPadding(FMargin(2));
    auto* Body=WidgetTree->ConstructWidget<UBorder>();Body->SetBrushColor(FLinearColor(0.01f,0.02f,0.035f));Body->SetPadding(FMargin(16));Rim->SetContent(Body);
    auto* Text=WidgetTree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(TestingCardInfo::Describe(Card)));Text->SetColorAndOpacity(FLinearColor(0.88f,0.96f,1));Text->SetWrapTextAt(440);auto Font=Text->GetFont();Font.Size=13;Text->SetFont(Font);Body->SetContent(Text);return Rim;
}
void UNeonTestingWidget::Start()
{
    PendingCards.Reset();for(int32 I=0;I<IDs.Num();++I) if(AttackToggles[I]->IsChecked()) PendingCards.Add(IDs[I]);
    PendingCounts.Reset();int32 Total=0;for(int32 I=0;I<10;++I) {const int32 Count=Enabled[I]->IsChecked() ? FMath::Clamp(FMath::RoundToInt(Counts[I]->GetValue()/5)*5,5,50) : 0;PendingCounts.Add(Count);Total+=Count;}
    if(PendingCards.IsEmpty() || Total<5 || Total>200) {Status->SetText(FText::FromString(TEXT("Select at least one attack and 5–200 targets in total.")));return;}
    Status->SetText(FText::FromString(TEXT("Hover attack names for stats. Apply resets selected attacks, targets and damage totals.")));
    PendingSpacing=Spacing->GetValue();bPendingReset=AutoReset->IsChecked();const FString Warning=TestingCardInfo::SpacingWarning(PendingCards,PendingSpacing);
    if(!Warning.IsEmpty()) {WarningText->SetText(FText::FromString(Warning));MainBody->SetIsEnabled(false);WarningLayer->SetVisibility(ESlateVisibility::Visible);ContinueButton->SetKeyboardFocus();return;}
    Commit();
}
void UNeonTestingWidget::Commit()
{
    if(auto* PC=GetOwningPlayer<AMVPController>();PC && !PC->StartTestingCards(PendingCards,PendingCounts,bPendingReset,PendingSpacing))
    {CancelWarning();Status->SetText(FText::FromString(TEXT("Could not apply setup. Check attacks, target counts and spacing.")));}
}
void UNeonTestingWidget::ContinueWarning() {if(WarningLayer->GetVisibility()==ESlateVisibility::Visible && !PendingCards.IsEmpty()) Commit();}
void UNeonTestingWidget::CancelWarning() {WarningLayer->SetVisibility(ESlateVisibility::Collapsed);MainBody->SetIsEnabled(true);PendingCards.Reset();PendingCounts.Reset();Spacing->SetKeyboardFocus();}
void UNeonTestingWidget::SelectAll() {for(UCheckBox* Toggle:AttackToggles) Toggle->SetIsChecked(true);}
void UNeonTestingWidget::ClearAll() {for(UCheckBox* Toggle:AttackToggles) Toggle->SetIsChecked(false);}
void UNeonTestingWidget::Back() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->CloseTestingConfiguration();}
void UNeonTestingWidget::Exit() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->ExitTestingArea();}
FReply UNeonTestingWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape) {if(!Event.IsRepeat()) {RequestBack();}return FReply::Handled();}return Super::NativeOnKeyDown(Geometry,Event);
}

FReply UNeonTestingWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right) {if(!Event.IsRepeat()) {RequestBack();}return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}

void UNeonTestingWidget::RequestBack()
{
    if(WarningLayer && WarningLayer->GetVisibility()==ESlateVisibility::Visible) {CancelWarning();return;}
    Back();
}
