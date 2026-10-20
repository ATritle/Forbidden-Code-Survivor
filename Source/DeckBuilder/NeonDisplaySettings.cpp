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
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

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
    FSlateRect WorkArea(UGameUserSettings* Settings)
    {
        if(FSlateApplication::IsInitialized())
        {
            if(const auto Window=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;Window.IsValid())
                return FSlateApplication::Get().GetWorkArea(FSlateRect::FromPointAndExtent(Window->GetPositionInScreen(),Window->GetSizeInScreen()));
            return FSlateApplication::Get().GetPreferredWorkArea();
        }
        const auto Native=Desktop(Settings);return FSlateRect(0,0,Native.X,Native.Y);
    }
    bool Committed(ESelectInfo::Type Type) {return Type==ESelectInfo::OnMouseClick || Type==ESelectInfo::OnKeyPress;}
    void ReturnCaption(UUserWidget* Widget,bool Pending)
    {
        if(auto* B=Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CloseSettings"))))
            if(auto* L=Cast<UTextBlock>(B->GetContent())) L->SetText(FText::FromString(Pending ? TEXT("REVERT & RETURN") : TEXT("RETURN / ESC")));
    }
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
    Heading(this,Display,TEXT("SCREEN & WINDOW"),TEXT("Selecting a mode previews it immediately. Keep within 15 seconds, or Escape to revert."));
    WindowMode=Combo(this,TEXT("WindowModeCombo"));WindowMode->OnGenerateWidgetEvent.BindDynamic(this,&ThisClass::ComboEntry);
    for(const TCHAR* Name:ModeNames) WindowMode->AddOption(Name);WindowMode->SetSelectedIndex(int32(Settings->GetFullscreenMode()));
    Row(this,Display,NAME_None,TEXT("Window mode"),TEXT("Fullscreen, borderless desktop, or a movable window."),WindowMode);
    Resolution=Combo(this,TEXT("ResolutionCombo"));Resolution->OnGenerateWidgetEvent.BindDynamic(this,&ThisClass::ComboEntry);
    Row(this,Display,NAME_None,TEXT("Output resolution"),TEXT("Borderless uses the desktop resolution automatically."),Resolution);
    Display->AddChildToVerticalBox(Button(this,TEXT("ApplyDisplay"),TEXT("APPLY DISPLAY CHANGES"),[this](int32){ApplyDisplay();}))->SetPadding(FMargin(0,4,0,12));
    Display->AddChildToVerticalBox(Text(this,NAME_None,TEXT("Windowed opens a smaller, movable window. Mode and resolution selections preview immediately; KEEP CHANGES saves them."),12,Muted));
    WindowMode->OnSelectionChanged.AddDynamic(this,&ThisClass::WindowModeChanged);Resolution->OnSelectionChanged.AddDynamic(this,&ThisClass::ResolutionChanged);RenderScale->OnValueChanged.AddDynamic(this,&ThisClass::RenderScaleChanged);
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
        for(FIntPoint Size:{FIntPoint(800,600),FIntPoint(1024,768),FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080)}) Sizes.AddUnique(Size);
    }
    Sizes.AddUnique(Native);if(Mode==1) Sizes={Native};else Sizes.AddUnique(Settings->GetScreenResolution());
    const auto Area=WorkArea(Settings);const FIntPoint Limit(FMath::Max(800,FMath::FloorToInt(Area.GetSize().X)-32),FMath::Max(600,FMath::FloorToInt(Area.GetSize().Y)-64));
    Sizes.RemoveAll([Mode,Limit](FIntPoint Size){return Size.X<800 || Size.Y<600 || (Mode==2 && (Size.X>Limit.X || Size.Y>Limit.Y));});
    if(Sizes.IsEmpty()) Sizes.Add(FIntPoint(FMath::Min(1280,Limit.X),FMath::Min(720,Limit.Y)));
    Sizes.Sort([](FIntPoint A,FIntPoint B){return A.X==B.X ? A.Y<B.Y : A.X<B.X;});
    Resolution->ClearOptions();for(FIntPoint Size:Sizes) if(Resolution->FindOptionIndex(ResolutionName(Size))==INDEX_NONE) Resolution->AddOption(ResolutionName(Size));
    const FString Preferred=Previous.IsEmpty() ? ResolutionName(Settings->GetScreenResolution()) : Previous;
    FString Fallback=ResolutionName(Native);
    if(Mode==2)
    {
        const FIntPoint Saved(FMath::RoundToInt(ForbiddenSettings::Get(this,"WindowedWidth",1600)),FMath::RoundToInt(ForbiddenSettings::Get(this,"WindowedHeight",900)));
        Fallback=Resolution->FindOptionIndex(ResolutionName(Saved))!=INDEX_NONE ? ResolutionName(Saved) : Resolution->FindOptionIndex(TEXT("1280 x 720"))!=INDEX_NONE ? TEXT("1280 x 720") : ResolutionName(Sizes.Last());
    }
    Resolution->SetSelectedOption(Resolution->FindOptionIndex(Preferred)!=INDEX_NONE ? Preferred : Fallback);Resolution->SetIsEnabled(Mode!=1);
}
void UNeonSettingsWidget::WindowModeChanged(FString Selected,ESelectInfo::Type SelectionType)
{if(!bRefreshing) {RefreshDisplayOptions();if(Committed(SelectionType)) {NeonMenuAudio::Input(this);QueueDisplayPreview();}}}
void UNeonSettingsWidget::ResolutionChanged(FString Selected,ESelectInfo::Type SelectionType)
{if(!bRefreshing && Committed(SelectionType)) {NeonMenuAudio::Input(this);QueueDisplayPreview();}}
void UNeonSettingsWidget::QueueDisplayPreview()
{
    const uint32 Request=++DisplayRequestRevision;const TWeakObjectPtr<UNeonSettingsWidget> Weak(this);
    // Resize after the combo popup has closed, rather than inside its Slate callback.
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Request](float)
    {if(auto* Widget=Weak.Get();Widget && Widget->DisplayRequestRevision==Request && !Widget->bDisplayPending) Widget->ApplyDisplay();return false;}),.05f);
}
void UNeonSettingsWidget::RestoreWindowedFrame(FIntPoint Size)
{
    const double Until=FPlatformTime::Seconds()+2;
    const TWeakPtr<SWindow> WeakWindow=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWindow,Size,Until](float)
    {
        auto* Settings=UGameUserSettings::GetGameUserSettings();
        if(!Settings || Settings->GetFullscreenMode()!=EWindowMode::Windowed || Settings->GetScreenResolution()!=Size) return false;
        const auto Window=WeakWindow.Pin();
        if(!Window.IsValid() || Window->GetWindowMode()!=EWindowMode::Windowed) return FPlatformTime::Seconds()<Until;
        const bool WasVisible=Window->IsVisible();
        Window->Restore();Window->Resize(FVector2D(Size.X,Size.Y));
        const auto Area=WorkArea(Settings);const FVector2D Extent=Window->GetSizeInScreen();
        Window->MoveWindowTo(FVector2D(Area.Left+FMath::Max(8.f,(float(Area.GetSize().X)-float(Extent.X))*.5f),Area.Top+FMath::Max(8.f,(float(Area.GetSize().Y)-float(Extent.Y))*.5f)));
        if(!WasVisible) Window->HideWindow();
        return false;
    }),.05f);
}
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
    const auto Window=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
    const bool ActualMatches=Window.IsValid() && Window->GetWindowMode()==Mode && GEngine->GameViewport->Viewport->GetWindowMode()==Mode;
    if(PreviousMode==Mode && PreviousResolution==Size && ActualMatches) {if(Mode==2) RestoreWindowedFrame(Size);StatusLabel->SetText(FText::FromString(TEXT("This display mode is already active.")));return;}
    ++DisplayRequestRevision;
    Settings->SetFullscreenMode(EWindowMode::Type(Mode));Settings->SetScreenResolution(Size);Settings->ApplyResolutionSettings(false);
    bDisplayPending=true;DisplayDeadline=FPlatformTime::Seconds()+15;DisplayConfirmation->SetVisibility(ESlateVisibility::Visible);
    if(Mode==2) RestoreWindowedFrame(Size);
    Pages->SetIsEnabled(false);for(UForbiddenChoiceButton* Tab:Tabs) Tab->SetIsEnabled(false);SetKeyboardFocus();
    ReturnCaption(this,true);
    DisplayCountdown->SetText(FText::FromString(FString::Printf(TEXT("%s / %d x %d. Keep changes? Reverting in 15 seconds."),ModeNames[Mode],Size.X,Size.Y)));
    const TWeakObjectPtr<UNeonSettingsWidget> Weak(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
    {
        auto* Widget=Weak.Get();if(!Widget || !Widget->bDisplayPending) return false;
        const int32 Seconds=FMath::CeilToInt(Widget->DisplayDeadline-FPlatformTime::Seconds());
        if(Seconds<=0) {Widget->RevertUnconfirmedDisplay();return false;}
        const auto* ActiveSettings=UGameUserSettings::GetGameUserSettings();const auto ActiveSize=ActiveSettings->GetScreenResolution();
        Widget->DisplayCountdown->SetText(FText::FromString(FString::Printf(TEXT("%s / %d x %d. Keep changes? Reverting in %d seconds."),ModeNames[int32(ActiveSettings->GetFullscreenMode())],ActiveSize.X,ActiveSize.Y,Seconds)));return true;
    }));
    // No unconfirmed display state is written to disk.
}
void UNeonSettingsWidget::KeepDisplay()
{
    if(!bDisplayPending) return;auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings) return;
    Settings->ConfirmVideoMode();Settings->SaveSettings();bDisplayPending=false;DisplayConfirmation->SetVisibility(ESlateVisibility::Collapsed);
    if(Settings->GetFullscreenMode()==EWindowMode::Windowed)
        if(auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data)
        {const auto Size=Settings->GetScreenResolution();Profile->Data->Settings.Add("WindowedWidth",Size.X);Profile->Data->Settings.Add("WindowedHeight",Size.Y);Profile->Save();}
    Pages->SetIsEnabled(true);for(UForbiddenChoiceButton* Tab:Tabs) Tab->SetIsEnabled(true);
    ReturnCaption(this,false);bRefreshing=true;RefreshDisplayOptions();bRefreshing=false;
    StatusLabel->SetText(FText::FromString(TEXT("Display mode saved.")));
}
void UNeonSettingsWidget::RevertUnconfirmedDisplay()
{
    ++DisplayRequestRevision;if(!bDisplayPending) return;bDisplayPending=false;
    if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {Settings->SetFullscreenMode(PreviousMode);Settings->SetScreenResolution(PreviousResolution);Settings->ApplyResolutionSettings(false);Settings->SaveSettings();}
    if(PreviousMode==EWindowMode::Windowed) RestoreWindowedFrame(PreviousResolution);
    if(DisplayConfirmation) DisplayConfirmation->SetVisibility(ESlateVisibility::Collapsed);
    ReturnCaption(this,false);
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
