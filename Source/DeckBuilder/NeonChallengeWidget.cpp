#include "NeonChallengeWidget.h"
#include "NeonChallenge.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"

void UNeonChallengeWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);
    const FLinearColor Cyan(0.05f,0.8f,1),Pink(1,0.025f,0.45f),Amber(1,0.6f,0.04f),White(0.87f,0.95f,1);
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(10);
    auto* BlurSlot=Root->AddChildToOverlay(Blur);BlurSlot->SetHorizontalAlignment(HAlign_Fill);BlurSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(0.005f,0.008f,0.02f,0.6f));
    auto* ShadeSlot=Root->AddChildToOverlay(Shade);ShadeSlot->SetHorizontalAlignment(HAlign_Fill);ShadeSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(830);
    auto* Center=Root->AddChildToOverlay(Size);Center->SetHorizontalAlignment(HAlign_Center);Center->SetVerticalAlignment(VAlign_Center);
    auto* Rim=WidgetTree->ConstructWidget<UBorder>();Rim->SetBrushColor(Cyan);Rim->SetPadding(FMargin(2));Size->SetContent(Rim);
    auto* Body=WidgetTree->ConstructWidget<UBorder>();Body->SetBrushColor(FLinearColor(0.008f,0.014f,0.028f,0.97f));Body->SetPadding(FMargin(34));Rim->SetContent(Body);
    auto* Glass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/NeonGlass/M_NeonGlass_UMG.M_NeonGlass_UMG"));
    if(Glass)
    {
        Rim->SetBrushColor(FLinearColor::Transparent);Rim->SetPadding(FMargin(0));
        PanelMaterial=UMaterialInstanceDynamic::Create(Glass,this);
        PanelMaterial->SetVectorParameterValue(TEXT("PanelSize"),FLinearColor(830,540,0,0));
        PanelMaterial->SetVectorParameterValue(TEXT("Accent"),Cyan);PanelMaterial->SetVectorParameterValue(TEXT("Control"),FLinearColor(0,0,0,0.15f));
        Body->SetBrushFromMaterial(PanelMaterial);Body->SetBrushColor(FLinearColor::White);
    }
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Body->SetContent(Column);
    auto Text=[&](FName Name,const TCHAR* Caption,int32 Points,FLinearColor Color)
    {
        auto* T=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);T->SetText(FText::FromString(Caption));
        auto F=T->GetFont();F.Size=Points;T->SetFont(F);T->SetColorAndOpacity(Color);T->SetWrapTextAt(755);
        Column->AddChildToVerticalBox(T)->SetPadding(FMargin(0,7));return T;
    };
    Text(TEXT("SignTitle"),TEXT("NO PAIN - NO GAIN"),34,Pink);
    Text(TEXT("ContractIntro"),TEXT("FORBIDDEN PROTOCOL / VOLUNTARY CONTRACT"),12,Cyan);
    Title=Text(TEXT("ChallengeTitle"),TEXT(""),24,White);
    Duration=Text(TEXT("ChallengeDuration"),TEXT(""),12,Cyan);
    Text(TEXT("RewardLabel"),TEXT("THE REWARD"),13,Cyan);
    Reward=Text(TEXT("ChallengeReward"),TEXT(""),16,White);
    Text(TEXT("RiskLabel"),TEXT("THE PRICE"),13,Amber);
    Risk=Text(TEXT("ChallengeRisk"),TEXT(""),16,Amber);
    Text(TEXT("PauseNotice"),TEXT("Combat is paused. One contract at a time. Accepting consumes this station for the run."),11,FLinearColor(0.48f,0.64f,0.75f));
    auto* Actions=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0,20,0,0));
    auto Button=[&](FName Name,const TCHAR* Caption,FLinearColor Color)
    {
        auto* B=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
        auto Style=B->GetStyle();Style.Normal.TintColor=FLinearColor(0.02f,0.065f,0.09f);Style.Hovered.TintColor=FLinearColor(0.04f,0.18f,0.23f);Style.Pressed.TintColor=Color*0.3f;B->SetStyle(Style);
        if(Glass)
        {
            auto* M=UMaterialInstanceDynamic::Create(Glass,this);M->SetVectorParameterValue(TEXT("PanelSize"),FLinearColor(370,48,0,0));
            M->SetVectorParameterValue(TEXT("Accent"),Color);M->SetVectorParameterValue(TEXT("Control"),FLinearColor(0,0,2,0));
            FSlateBrush Brush;Brush.SetResourceObject(M);Brush.DrawAs=ESlateBrushDrawType::Image;Brush.TintColor=FLinearColor::White;
            Style.Normal=Brush;Style.Hovered=Brush;Style.Pressed=Brush;Style.NormalPadding=FMargin(16,14);Style.PressedPadding=FMargin(16,14);B->SetStyle(Style);
            ActionMaterials.Add(M);ActionButtons.Add(B);
        }
        auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Caption));T->SetColorAndOpacity(Color);T->SetJustification(ETextJustify::Center);auto Font=T->GetFont();Font.Size=15;T->SetFont(Font);B->SetContent(T);
        auto* Slot=Actions->AddChildToHorizontalBox(B);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(0,0,12,0));return B;
    };
    Button(TEXT("AcceptChallenge"),TEXT("ACCEPT CHALLENGE"),Pink)->OnClicked.AddDynamic(this,&ThisClass::Accept);
    Button(TEXT("DeclineChallenge"),TEXT("DECLINE / ESC"),Cyan)->OnClicked.AddDynamic(this,&ThisClass::Decline);
}
void UNeonChallengeWidget::NativeTick(const FGeometry& Geometry,float Dt)
{
    Super::NativeTick(Geometry,Dt);
    for(int32 I=0;I<ActionButtons.Num();++I)
    {
        HoverAmount[I]=FMath::FInterpTo(HoverAmount[I],ActionButtons[I]->IsHovered() || ActionButtons[I]->HasKeyboardFocus() ? 1.f : 0.f,Dt,12);
        ActionMaterials[I]->SetVectorParameterValue(TEXT("Control"),FLinearColor(HoverAmount[I],0,2,0));
    }
}
void UNeonChallengeWidget::InitializeOffer(UNeonChallengeComponent* Owner,UNeonChallengeDefinition* Definition)
{
    ContractOwner=Owner;if(!Definition) return;
    Title->SetText(Definition->Title);Title->SetColorAndOpacity(Definition->Accent);
    Reward->SetText(Definition->Reward);Risk->SetText(Definition->Risk);
    Duration->SetText(FText::FromString(FString::Printf(TEXT("DURATION  %.0f SECONDS / NO COST TO DECLINE"),Definition->Duration)));
}
void UNeonChallengeWidget::Accept() {if(ContractOwner.IsValid()) ContractOwner->AcceptOffer();}
void UNeonChallengeWidget::ShowOfferError(const FString& Message) {Duration->SetText(FText::FromString(Message));Duration->SetColorAndOpacity(FLinearColor(1,0.5f,0.05f));}
void UNeonChallengeWidget::Decline() {if(ContractOwner.IsValid()) ContractOwner->DeclineOffer();}
FReply UNeonChallengeWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape) {Decline();return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
