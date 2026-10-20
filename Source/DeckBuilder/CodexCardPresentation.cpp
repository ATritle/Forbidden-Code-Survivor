#include "CodexCardSelectionWidget.h"
#include "ForbiddenUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture.h"

void UCodexCardSelectionWidget::BuildCardFace(UButton* Button, const FCardDisplayData& Data, int32 Index)
{
    FButtonStyle Style = Button->GetStyle();
    Style.Normal.TintColor = FSlateColor(FLinearColor(0.015f,0.02f,0.025f));
    Style.Hovered.TintColor = FSlateColor(FLinearColor(0.08f,0.06f,0.04f));
    Style.Pressed.TintColor = FSlateColor(FLinearColor(0.12f,0.09f,0.04f));
    Style.NormalPadding = FMargin(0); Style.PressedPadding = FMargin(0);
    Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
    Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
    Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
    Button->SetStyle(Style);
    Button->SetBackgroundColor(FLinearColor::White);
    auto* Frame = WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrushColor(Data.RarityColor);
    ForbiddenUI::Glass(Frame,FVector2D(350,590),Data.RarityColor);
    Frame->SetPadding(FMargin(2.f));
    Button->SetContent(Frame);
    if (auto* ButtonContentSlot = Cast<UButtonSlot>(Frame->Slot))
    {
        ButtonContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ButtonContentSlot->SetVerticalAlignment(VAlign_Fill);
    }
    auto* Inner = WidgetTree->ConstructWidget<UBorder>();
    Inner->SetBrushColor(FLinearColor(0.006f,0.014f,0.025f,0.38f));
    Inner->SetPadding(FMargin(16.f)); Frame->SetContent(Inner);
    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Inner->SetContent(Column);
    auto Text = [&](const FString& Value, int32 Size, FLinearColor Color)
    {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        auto Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
        Label->SetText(FText::FromString(Value)); Label->SetColorAndOpacity(Color);
        Label->SetAutoWrapText(true);
        Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,5));
        return Label;
    };
    Text(FString::Printf(TEXT("%02d   /   %s"),Index+1,*Data.RarityLabel.ToString()),12,Data.RarityColor);
    auto* ArtSize = WidgetTree->ConstructWidget<USizeBox>();
    ArtSize->SetHeightOverride(235.f);
    Column->AddChildToVerticalBox(ArtSize)->SetHorizontalAlignment(HAlign_Center);
    ArtSize->SetWidthOverride(235.f);
    auto* Art = WidgetTree->ConstructWidget<UImage>();
    FSlateBrush ArtBrush; ArtBrush.SetResourceObject(Data.Icon); ArtBrush.ImageSize = FVector2D(235,235);
    // Square original remains intact; modest inset border frames the complete composition.
    Art->SetBrush(ArtBrush); ArtSize->AddChild(Art);
    auto* SchoolRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(SchoolRow)->SetPadding(FMargin(0,8));
    if (Data.SchoolIcon)
    {
        auto* Icon = WidgetTree->ConstructWidget<UImage>();
        FSlateBrush Brush; Brush.SetResourceObject(Data.SchoolIcon); Brush.ImageSize = FVector2D(32);
        Icon->SetBrush(Brush); SchoolRow->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(0,0,8,0));
    }
    FString School = Data.MagicSchoolTag.IsValid() ? Data.MagicSchoolTag.ToString() : TEXT("RECOVERY");
    School.RemoveFromStart(TEXT("MagicSchool."));
    auto* SchoolLabel = WidgetTree->ConstructWidget<UTextBlock>();
    auto Font = SchoolLabel->GetFont(); Font.Size = 12; SchoolLabel->SetFont(Font);
    SchoolLabel->SetText(FText::FromString(School.ToUpper()));
    SchoolLabel->SetColorAndOpacity(FLinearColor(0.72f,0.68f,0.55f));
    SchoolRow->AddChildToHorizontalBox(SchoolLabel)->SetVerticalAlignment(VAlign_Center);
    Text(Data.Title.ToString(),19,FLinearColor(0.9f,0.96f,1.f));
    auto* Rule = WidgetTree->ConstructWidget<UBorder>();
    Rule->SetBrushColor(Data.RarityColor * 0.5f); Rule->SetPadding(FMargin(0,1));
    Column->AddChild(Rule);
    Text(Data.Description.ToString(),14,FLinearColor(0.66f,0.72f,0.69f));
    auto* Spacer = WidgetTree->ConstructWidget<USizeBox>();
    Column->AddChildToVerticalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Text(TEXT("+12 VITALITY   /   INSTALL MODULE"),11,FLinearColor(1.f,0.15f,0.6f))->SetJustification(ETextJustify::Center);
    Button->SetRenderTransformPivot(FVector2D(0.5f,0.5f));
    Button->SetRenderOpacity(0.f);
}

void UCodexCardSelectionWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    if (!bNativeLayout) return;
    UButton* Buttons[] = {CardOption_A,CardOption_B,CardOption_C};
    const double Elapsed = FPlatformTime::Seconds() - RevealStartTime;
    for (int32 I=0; I<3; ++I)
    {
        UButton* Button = Buttons[I]; if (!Button) continue;
        const float T = FMath::Clamp(float((Elapsed-I*0.1)/0.3),0.f,1.f);
        const float Ease = 1.f-FMath::Pow(1.f-T,3.f);
        if(Button->IsHovered() || Button->HasKeyboardFocus()) FocusedChoice=I;
        const bool Focus=Button->IsHovered() || Button->HasKeyboardFocus();
        const float Hover = Focus ? 1.025f : 1.f;
        Button->SetRenderOpacity(Ease);
        Button->SetRenderScale(FVector2D(FMath::Lerp(0.94f,Hover,Ease)));
        Button->SetRenderTranslation(FVector2D(0,(1.f-Ease)*28.f-(Button->IsHovered()?5.f:0.f)));
    }
}

