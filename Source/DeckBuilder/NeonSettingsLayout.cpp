#include "NeonSettingsLayout.h"
#include "ForbiddenChoiceWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UTextBlock* NeonSettingsUI::Text(UUserWidget* Owner,FName Name,const FString& Copy,int32 Size,FLinearColor Color)
{
    auto* Label=Owner->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
    auto Font=Label->GetFont();Font.Size=Size;Label->SetFont(Font);
    Label->SetText(FText::FromString(Copy));Label->SetColorAndOpacity(Color);Label->SetAutoWrapText(true);
    return Label;
}
UForbiddenChoiceButton* NeonSettingsUI::Button(UUserWidget* Owner,FName Name,const FString& Copy,TFunction<void(int32)> Callback,bool Closing)
{
    auto* Button=Owner->WidgetTree->ConstructWidget<UForbiddenChoiceButton>(UForbiddenChoiceButton::StaticClass(),Name);
    Button->Configure(0,MoveTemp(Callback));Button->bClosingFeedback=Closing;
    auto Style=Button->GetStyle();Style.Normal.TintColor=FSlateColor(FLinearColor(.008f,.025f,.04f));
    Style.Hovered.TintColor=FSlateColor(FLinearColor(.018f,.10f,.15f));Style.Pressed.TintColor=FSlateColor(FLinearColor(.02f,.18f,.23f));
    Style.Normal.OutlineSettings.Color=FSlateColor(FLinearColor(.03f,.28f,.35f));Style.Hovered.OutlineSettings.Color=FSlateColor(Cyan);Style.Pressed.OutlineSettings.Color=FSlateColor(Cyan);
    Style.NormalPadding=FMargin(12,9);Style.PressedPadding=FMargin(12,10,12,8);Button->SetStyle(Style);
    auto* Label=Text(Owner,NAME_None,Copy,15);Label->SetAutoWrapText(false);Label->SetJustification(ETextJustify::Center);Button->SetContent(Label);
    return Button;
}
UTextBlock* NeonSettingsUI::Row(UUserWidget* Owner,UVerticalBox* Column,FName Name,const FString& Title,const FString& Hint,UWidget* Control)
{
    auto* Back=Owner->WidgetTree->ConstructWidget<UBorder>();Back->SetBrushColor(FLinearColor(.012f,.027f,.043f));Back->SetPadding(FMargin(18,10));
    auto* Minimum=Owner->WidgetTree->ConstructWidget<USizeBox>();Minimum->SetMinDesiredHeight(44);Back->SetContent(Minimum);
    auto* Line=Owner->WidgetTree->ConstructWidget<UHorizontalBox>();Minimum->SetContent(Line);
    auto* Copy=Owner->WidgetTree->ConstructWidget<UVerticalBox>();auto* Left=Line->AddChildToHorizontalBox(Copy);
    Left->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Left->SetVerticalAlignment(VAlign_Center);Left->SetPadding(FMargin(0,0,28,0));
    auto* Label=Text(Owner,Name,Title,16);Copy->AddChildToVerticalBox(Label);
    if(!Hint.IsEmpty()) Copy->AddChildToVerticalBox(Text(Owner,NAME_None,Hint,12,Muted))->SetPadding(FMargin(0,5,0,0));
    auto* Right=Line->AddChildToHorizontalBox(Control);Right->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Right->SetVerticalAlignment(VAlign_Center);
    Column->AddChildToVerticalBox(Back)->SetPadding(FMargin(0,0,0,8));return Label;
}
void NeonSettingsUI::Heading(UUserWidget* Owner,UVerticalBox* Column,const FString& Title,const FString& Hint)
{
    Column->AddChildToVerticalBox(Text(Owner,NAME_None,Title,20,Cyan))->SetPadding(FMargin(0,0,0,6));
    Column->AddChildToVerticalBox(Text(Owner,NAME_None,Hint,13,Muted))->SetPadding(FMargin(0,0,0,18));
}
