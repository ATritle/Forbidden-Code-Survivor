#include "NeonSettingsWidget.h"
#include "NeonSettingsLayout.h"
#include "ForbiddenSettingsControls.h"
#include "ForbiddenProfile.h"
#include "ForbiddenChoiceWidget.h"
#include "NeonMenuAudio.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Containers/Ticker.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

using namespace NeonSettingsUI;
namespace
{
    const TCHAR* ModeNames[]={TEXT("FULLSCREEN"),TEXT("BORDERLESS"),TEXT("WINDOWED")};
    const TCHAR* QualityNames[]={TEXT("PERFORMANCE"),TEXT("BALANCED"),TEXT("HIGH"),TEXT("CINEMATIC")};
    UComboBoxString* Combo(UUserWidget* Owner,FName Name)
    {
        auto* Box=Owner->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),Name);
        auto Style=Box->GetWidgetStyle();auto& Button=Style.ComboButtonStyle.ButtonStyle;
        Button.Normal.TintColor=FSlateColor(FLinearColor(.008f,.025f,.04f));Button.Hovered.TintColor=FSlateColor(FLinearColor(.018f,.10f,.15f));
        Button.Normal.OutlineSettings.Color=FSlateColor(FLinearColor(.03f,.28f,.35f));Button.Hovered.OutlineSettings.Color=FSlateColor(Cyan);Style.ComboButtonStyle.DownArrowImage.TintColor=FSlateColor(Cyan);
        Box->SetWidgetStyle(Style);Box->SetContentPadding(FMargin(12,10));Box->SetMaxListHeight(260);return Box;
    }
    FIntPoint Desktop(UGameUserSettings* Settings)
    {const auto Size=Settings->GetDesktopResolution();return Size.X>0 && Size.Y>0 ? Size : FIntPoint(1920,1080);}
    FString ResolutionName(FIntPoint Size) {return FString::Printf(TEXT("%d x %d"),Size.X,Size.Y);}
}
UWidget* UNeonSettingsWidget::ComboEntry(FString Option) {return Text(this,NAME_None,Option,15,White);}
void UNeonSettingsWidget::BuildDisplay(UVerticalBox* Column)
{
    auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings) return;
    auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Columns);
    auto* Graphics=WidgetTree->ConstructWidget<UVerticalBox>();auto* Left=Columns->AddChildToHorizontalBox(Graphics);Left->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Left->SetPadding(FMargin(0,0,12,0));
    auto* Display=WidgetTree->ConstructWidget<UVerticalBox>();auto* Right=Columns->AddChildToHorizontalBox(Display);Right->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Right->SetPadding(FMargin(12,0,0,0));
    Heading(this,Graphics,TEXT("RENDER QUALITY"),TEXT("Presets adjust shadows, lighting, reflections, textures, effects and view distance."));
    for(int32 RowIndex=0;RowIndex<2;++RowIndex)
    {
        auto* Pair=WidgetTree->ConstructWidget<UHorizontalBox>();Graphics->AddChildToVerticalBox(Pair)->SetPadding(FMargin(0,0,0,8));
        for(int32 Col=0;Col<2;++Col)
        {
            const int32 Index=RowIndex*2+Col;
            auto* Choice=Button(this,FName(*FString::Printf(TEXT("GraphicsPreset_%d"),Index)),QualityNames[Index],[this,Index](int32)
            {if(!ForbiddenSettings::SetGraphicsPreset(this,Index) && StatusLabel) StatusLabel->SetText(FText::FromString(TEXT("Graphics applied, but the profile could not be saved.")));RefreshGraphics();});
            auto* Cell=Pair->AddChildToHorizontalBox(Choice);Cell->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Cell->SetPadding(FMargin(0,0,Col==0 ? 8 : 0,0));QualityButtons.Add(Choice);
        }
    }
    QualityLabel=Text(this,TEXT("GraphicsPresetStatus"),TEXT(""),12,Muted);Graphics->AddChildToVerticalBox(QualityLabel)->SetPadding(FMargin(0,2,0,12));
    RenderScale=WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),TEXT("RenderScaleSlider"));RenderScale->SetMinValue(50);RenderScale->SetMaxValue(100);RenderScale->SetStepSize(1);
    RenderScale->SetSliderHandleColor(Cyan);RenderScale->SetSliderBarColor(FLinearColor(.13f,.27f,.35f));
    RenderScaleLabel=Row(this,Graphics,TEXT("RenderScaleLabel"),TEXT(""),TEXT("Lower the 3D render resolution. Menus remain sharp."),RenderScale);
    VSyncButton=Button(this,TEXT("VSyncToggle"),TEXT(""),[this](int32){ToggleVSync();});
    Row(this,Graphics,NAME_None,TEXT("Vertical sync"),TEXT("Synchronize frames with the monitor to reduce tearing."),VSyncButton);
    Heading(this,Display,TEXT("SCREEN & WINDOW"),TEXT("Preview a display change, then keep it within 15 seconds. Escape reverts it."));
    WindowMode=Combo(this,TEXT("WindowModeCombo"));WindowMode->OnGenerateWidgetEvent.BindDynamic(this,&ThisClass::ComboEntry);
    for(const TCHAR* Name:ModeNames) WindowMode->AddOption(Name);WindowMode->SetSelectedIndex(int32(Settings->GetFullscreenMode()));
    Row(this,Display,NAME_None,TEXT("Window mode"),TEXT("Fullscreen, borderless desktop, or a movable window."),WindowMode);
    Resolution=Combo(this,TEXT("ResolutionCombo"));Resolution->OnGenerateWidgetEvent.BindDynamic(this,&ThisClass::ComboEntry);
    Row(this,Display,NAME_None,TEXT("Output resolution"),TEXT("Borderless uses the desktop resolution automatically."),Resolution);
    Display->AddChildToVerticalBox(Button(this,TEXT("ApplyDisplay"),TEXT("APPLY DISPLAY CHANGES"),[this](int32){ApplyDisplay();}))->SetPadding(FMargin(0,4,0,12));
    Display->AddChildToVerticalBox(Text(this,NAME_None,TEXT("Quality and render scale apply immediately. Window mode and output resolution change only after Apply."),12,Muted));
    WindowMode->OnSelectionChanged.AddDynamic(this,&ThisClass::WindowModeChanged);RenderScale->OnValueChanged.AddDynamic(this,&ThisClass::RenderScaleChanged);
    RefreshDisplayOptions();RefreshGraphics();
}
void UNeonSettingsWidget::RefreshGraphics()
{
    auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings) return;
    bRefreshing=true;const int32 Preset=FMath::Clamp(FMath::RoundToInt(ForbiddenSettings::Get(this,"Graphics",2)),0,3);
    for(int32 I=0;I<QualityButtons.Num();++I)
    {QualityButtons[I]->SetBackgroundColor(I==Preset ? FLinearColor(.15f,.85f,1) : FLinearColor(.6f,.7f,.8f));if(auto* L=Cast<UTextBlock>(QualityButtons[I]->GetContent())) L->SetColorAndOpacity(I==Preset ? Cyan : Muted);}
    float Normalized,Value,Minimum,Maximum;Settings->GetResolutionScaleInformationEx(Normalized,Value,Minimum,Maximum);
    RenderScale->SetValue(Value);RenderScaleLabel->SetText(FText::FromString(FString::Printf(TEXT("Render scale / %d%%"),FMath::RoundToInt(Value))));
    QualityLabel->SetText(FText::FromString(FString::Printf(TEXT("ACTIVE: %s / %d%% render scale"),QualityNames[Preset],FMath::RoundToInt(Value))));
    if(auto* L=Cast<UTextBlock>(VSyncButton->GetContent())) L->SetText(FText::FromString(Settings->IsVSyncEnabled() ? TEXT("ON") : TEXT("OFF")));
    bRefreshing=false;
}
void UNeonSettingsWidget::RefreshDisplayOptions()
{
    auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings || !Resolution) return;
    const FString Previous=Resolution->GetSelectedOption();const int32 Mode=WindowMode->GetSelectedIndex();
    TArray<FIntPoint> Sizes;const auto Native=Desktop(Settings);
    if(Mode==0) UKismetSystemLibrary::GetSupportedFullscreenResolutions(Sizes);
    else if(Mode==2)
    {
        UKismetSystemLibrary::GetConvenientWindowedResolutions(Sizes);
        for(FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080)}) if(Size.X<=Native.X && Size.Y<=Native.Y) Sizes.AddUnique(Size);
    }
    Sizes.AddUnique(Native);if(Mode==1) Sizes={Native};else Sizes.AddUnique(Settings->GetScreenResolution());
    Sizes.RemoveAll([](FIntPoint Size){return Size.X<800 || Size.Y<600;});Sizes.Sort([](FIntPoint A,FIntPoint B){return A.X==B.X ? A.Y<B.Y : A.X<B.X;});
    Resolution->ClearOptions();for(FIntPoint Size:Sizes) if(Resolution->FindOptionIndex(ResolutionName(Size))==INDEX_NONE) Resolution->AddOption(ResolutionName(Size));
    const FString Preferred=Previous.IsEmpty() ? ResolutionName(Settings->GetScreenResolution()) : Previous;
    Resolution->SetSelectedOption(Resolution->FindOptionIndex(Preferred)!=INDEX_NONE ? Preferred : ResolutionName(Native));Resolution->SetIsEnabled(Mode!=1);
}
void UNeonSettingsWidget::WindowModeChanged(FString Selected,ESelectInfo::Type SelectionType)
{if(!bRefreshing) {RefreshDisplayOptions();if(SelectionType!=ESelectInfo::Direct) NeonMenuAudio::Input(this);}}
void UNeonSettingsWidget::ApplyDisplay()
{
    auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings) return;
    if(bDisplayPending) RevertUnconfirmedDisplay();
    const int32 Mode=WindowMode->GetSelectedIndex();if(Mode<0 || Mode>2) return;
    FIntPoint Size=Desktop(Settings);if(Mode!=1)
    {
        FString X,Y;if(!Resolution->GetSelectedOption().Split(TEXT(" x "),&X,&Y)) return;
        Size=FIntPoint(FCString::Atoi(*X),FCString::Atoi(*Y));if(Size.X<800 || Size.Y<600) return;
    }
    PreviousMode=Settings->GetFullscreenMode();PreviousResolution=Settings->GetScreenResolution();
    if(PreviousMode==Mode && PreviousResolution==Size) {StatusLabel->SetText(FText::FromString(TEXT("This display mode is already active.")));return;}
    Settings->SetFullscreenMode(EWindowMode::Type(Mode));Settings->SetScreenResolution(Size);Settings->ApplyResolutionSettings(false);
    bDisplayPending=true;DisplayDeadline=FPlatformTime::Seconds()+15;DisplayConfirmation->SetVisibility(ESlateVisibility::Visible);
    Pages->SetIsEnabled(false);for(UForbiddenChoiceButton* Tab:Tabs) Tab->SetIsEnabled(false);SetKeyboardFocus();
    DisplayCountdown->SetText(FText::FromString(TEXT("Keep this display mode? Reverting in 15 seconds.")));
    const TWeakObjectPtr<UNeonSettingsWidget> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
    {
        auto* Widget=Weak.Get();if(!Widget || !Widget->bDisplayPending) return false;
        const int32 Seconds=FMath::CeilToInt(Widget->DisplayDeadline-FPlatformTime::Seconds());
        if(Seconds<=0) {Widget->RevertUnconfirmedDisplay();return false;}
        Widget->DisplayCountdown->SetText(FText::FromString(FString::Printf(TEXT("Keep this display mode? Reverting in %d seconds."),Seconds)));return true;
    }));
    // No unconfirmed display state is written to disk.
}
void UNeonSettingsWidget::KeepDisplay()
{
    if(!bDisplayPending) return;auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings) return;
    Settings->ConfirmVideoMode();Settings->SaveSettings();bDisplayPending=false;DisplayConfirmation->SetVisibility(ESlateVisibility::Collapsed);
    Pages->SetIsEnabled(true);for(UForbiddenChoiceButton* Tab:Tabs) Tab->SetIsEnabled(true);
    StatusLabel->SetText(FText::FromString(TEXT("Display mode saved.")));
}
void UNeonSettingsWidget::RevertUnconfirmedDisplay()
{
    if(!bDisplayPending) return;bDisplayPending=false;
    if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {Settings->SetFullscreenMode(PreviousMode);Settings->SetScreenResolution(PreviousResolution);Settings->ApplyResolutionSettings(false);Settings->SaveSettings();}
    if(DisplayConfirmation) DisplayConfirmation->SetVisibility(ESlateVisibility::Collapsed);
    if(Pages) Pages->SetIsEnabled(true);for(UForbiddenChoiceButton* Tab:Tabs) Tab->SetIsEnabled(true);
    if(WindowMode) {bRefreshing=true;WindowMode->SetSelectedIndex(int32(PreviousMode));bRefreshing=false;RefreshDisplayOptions();Resolution->SetSelectedOption(ResolutionName(PreviousResolution));}
    if(StatusLabel) StatusLabel->SetText(FText::FromString(TEXT("Display change reverted. Your previous mode is restored.")));
}
void UNeonSettingsWidget::ToggleVSync()
{
    if(auto* Settings=UGameUserSettings::GetGameUserSettings()) {Settings->SetVSyncEnabled(!Settings->IsVSyncEnabled());Settings->ApplyNonResolutionSettings();if(!bDisplayPending) Settings->SaveSettings();RefreshGraphics();}
}
void UNeonSettingsWidget::RenderScaleChanged(float Value)
{
    if(bRefreshing || !FMath::IsFinite(Value)) return;
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    Profile->Data->Settings.Add("RenderScale",FMath::Clamp(Value,50.f,100.f));ForbiddenSettings::ApplyGraphics(this);
    if(!Profile->Save() && StatusLabel) StatusLabel->SetText(FText::FromString(TEXT("Could not save render scale.")));RefreshGraphics();
}
