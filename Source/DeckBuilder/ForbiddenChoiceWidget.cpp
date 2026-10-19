#include "ForbiddenChoiceWidget.h"
#include "ForbiddenUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "InputCoreTypes.h"

void UForbiddenChoiceButton::Configure(int32 InIndex,TFunction<void(int32)> InCallback)
{Index=InIndex;Callback=MoveTemp(InCallback);OnClicked.AddUniqueDynamic(this,&ThisClass::Clicked);}
void UForbiddenChoiceButton::Clicked() {if(Callback) Callback(Index);}

void UForbiddenChoiceWidget::Configure(const FString& Title,const TArray<FString>& Choices,TFunction<void(int32)> Callback)
{
    OnChoice=MoveTemp(Callback);Consumed=false;Focused=0;Buttons.Reset();SetIsFocusable(true);
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(9.f);
    auto* Placement=Root->AddChildToOverlay(Blur);Placement->SetHorizontalAlignment(HAlign_Fill);Placement->SetVerticalAlignment(VAlign_Fill);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();Frame->SetBrushColor(FLinearColor(0.006f,0.012f,0.035f,0.97f));Frame->SetPadding(FMargin(32));
    ForbiddenUI::Glass(Frame,FVector2D(760,740),FLinearColor(0.05f,0.8f,1));
    Placement=Root->AddChildToOverlay(Frame);Placement->SetHorizontalAlignment(HAlign_Center);Placement->SetVerticalAlignment(VAlign_Center);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(760);Size->SetMaxDesiredHeight(740);Frame->SetContent(Size);
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);Size->SetContent(Scroll);
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Column);
    auto Text=[&](const FString& Copy,int32 Font,FLinearColor Color)
    {
        auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Copy));Label->SetAutoWrapText(true);Label->SetColorAndOpacity(Color);auto Info=Label->GetFont();Info.Size=Font;Label->SetFont(Info);return Label;
    };
    Column->AddChildToVerticalBox(Text(Title,26,FLinearColor(0.04f,0.85f,1.f)))->SetPadding(FMargin(0,0,0,20));
    for(int32 I=0;I<Choices.Num();++I)
    {
        auto* Button=WidgetTree->ConstructWidget<UForbiddenChoiceButton>();Button->Configure(I,[this](int32 Index){Select(Index);});Button->SetBackgroundColor(FLinearColor(0.025f,0.055f,0.09f));
        auto* Border=WidgetTree->ConstructWidget<UBorder>();Border->SetPadding(FMargin(18));Border->SetBrushColor(FLinearColor(0.02f,0.1f,0.16f,0.7f));
        Border->SetContent(Text(FString::Printf(TEXT("[%d]  %s"),I+1,*Choices[I]),17,FLinearColor(0.88f,0.96f,1)));Button->SetContent(Border);
        Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0,4));Buttons.Add(Button);
    }
    Column->AddChildToVerticalBox(Text(TEXT("Arrow keys / D-pad to browse. Enter / A to select."),13,FLinearColor(0.45f,0.65f,0.8f)))->SetPadding(FMargin(0,18,0,0));
    if(!Buttons.IsEmpty()) Buttons[0]->SetKeyboardFocus();
}
void UForbiddenChoiceWidget::Select(int32 Index)
{
    if(Consumed || !Buttons.IsValidIndex(Index)) return;
    Consumed=true;auto Callback=MoveTemp(OnChoice);RemoveFromParent();if(Callback) Callback(Index);
}
FReply UForbiddenChoiceWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.IsRepeat()) return FReply::Handled();const FKey Key=Event.GetKey();
    if(Key==EKeys::Up || Key==EKeys::Gamepad_DPad_Up) {Focused=(Focused+Buttons.Num()-1)%FMath::Max(1,Buttons.Num());if(Buttons.IsValidIndex(Focused)) Buttons[Focused]->SetKeyboardFocus();return FReply::Handled();}
    if(Key==EKeys::Down || Key==EKeys::Gamepad_DPad_Down) {Focused=(Focused+1)%FMath::Max(1,Buttons.Num());if(Buttons.IsValidIndex(Focused)) Buttons[Focused]->SetKeyboardFocus();return FReply::Handled();}
    if(Key==EKeys::Enter || Key==EKeys::Gamepad_FaceButton_Bottom) {Select(Focused);return FReply::Handled();}
    const FKey Numbers[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine};
    for(int32 I=0;I<UE_ARRAY_COUNT(Numbers);++I) if(Key==Numbers[I]) {Select(I);return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
