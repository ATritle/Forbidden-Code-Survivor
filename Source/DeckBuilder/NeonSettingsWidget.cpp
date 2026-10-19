#include "NeonSettingsWidget.h"
#include "NeonSettingsLayout.h"
#include "ForbiddenSettingsControls.h"
#include "ForbiddenProfile.h"
#include "ForbiddenChoiceWidget.h"
#include "NeonMenuAudio.h"
#include "ForbiddenCodeBranding.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "MortisTestingDirector.h"
#include "MVPGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

using namespace NeonSettingsUI;
namespace
{
    USizeBox* SliderBox(UUserWidget* Owner,USlider* Slider)
    {
        Slider->SetSliderHandleColor(Cyan);Slider->SetSliderBarColor(FLinearColor(.13f,.27f,.35f));
        auto* Size=Owner->WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(32);Size->SetContent(Slider);return Size;
    }
}
void UNeonSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));if(!Game || !Game->AudioDirector) return;
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(8);
    auto* Place=Root->AddChildToOverlay(Blur);Place->SetHorizontalAlignment(HAlign_Fill);Place->SetVerticalAlignment(VAlign_Fill);
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.003f,.006f,.015f,.75f));
    Place=Root->AddChildToOverlay(Shade);Place->SetHorizontalAlignment(HAlign_Fill);Place->SetVerticalAlignment(VAlign_Fill);
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetStretchDirection(EStretchDirection::DownOnly);
    Place=Root->AddChildToOverlay(Scale);Place->SetPadding(FMargin(24));Place->SetHorizontalAlignment(HAlign_Fill);Place->SetVerticalAlignment(VAlign_Fill);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1080);Size->SetHeightOverride(760);Scale->SetContent(Size);
    auto* Rim=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SettingsFrame"));Rim->SetBrushColor(FLinearColor(.03f,.48f,.58f));Rim->SetPadding(FMargin(1));Size->SetContent(Rim);
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(.009f,.018f,.032f,.99f));Panel->SetPadding(FMargin(28,22));Rim->SetContent(Panel);
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Panel->SetContent(Column);
    Column->AddChildToVerticalBox(Text(this,TEXT("SettingsTitle"),TEXT("SETTINGS"),28,Cyan));
    Column->AddChildToVerticalBox(Text(this,NAME_None,ForbiddenCode::DisplayTitle,12,Muted))->SetPadding(FMargin(0,4,0,18));
    auto* Nav=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Nav)->SetPadding(FMargin(0,0,0,18));
    const TCHAR* Titles[]={TEXT("DISPLAY"),TEXT("AUDIO & EFFECTS"),TEXT("ACCESSIBILITY"),TEXT("CONTROLS")};
    for(int32 I=0;I<4;++I)
    {
        auto* Tab=Button(this,FName(*FString::Printf(TEXT("SettingsTab_%d"),I)),Titles[I],[this,I](int32){SelectPage(I);});
        auto* Cell=Nav->AddChildToHorizontalBox(Tab);Cell->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Cell->SetPadding(FMargin(0,0,I<3 ? 8 : 0,0));Tabs.Add(Tab);
    }
    Pages=WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(),TEXT("SettingsPages"));
    auto* Body=Column->AddChildToVerticalBox(Pages);Body->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for(int32 I=0;I<4;++I)
    {
        auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
        Scroll->SetScrollbarThickness(FVector2D(5,5));Scroll->SetScrollbarPadding(FMargin(8,0,0,0));Pages->AddChild(Scroll);
        auto* Page=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Page);
        if(I==0) BuildDisplay(Page);else if(I==1) BuildAudio(Page);
        else if(I==2) ForbiddenSettings::AddAccessibilityControls(this,Page);else ForbiddenSettings::AddInputControls(this,Page);
    }
    DisplayConfirmation=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("DisplayConfirmation"));
    DisplayConfirmation->SetBrushColor(FLinearColor(.10f,.045f,.025f));DisplayConfirmation->SetPadding(FMargin(12,10));DisplayConfirmation->SetVisibility(ESlateVisibility::Collapsed);
    Column->AddChildToVerticalBox(DisplayConfirmation)->SetPadding(FMargin(0,8,0,0));
    auto* ConfirmRow=WidgetTree->ConstructWidget<UHorizontalBox>();DisplayConfirmation->SetContent(ConfirmRow);
    DisplayCountdown=Text(this,NAME_None,TEXT(""),14);auto* Caption=ConfirmRow->AddChildToHorizontalBox(DisplayCountdown);Caption->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Caption->SetVerticalAlignment(VAlign_Center);
    ConfirmRow->AddChildToHorizontalBox(Button(this,TEXT("KeepDisplay"),TEXT("KEEP CHANGES"),[this](int32){KeepDisplay();}))->SetPadding(FMargin(12,0,0,0));
    ConfirmRow->AddChildToHorizontalBox(Button(this,TEXT("RevertDisplay"),TEXT("REVERT"),[this](int32){RevertUnconfirmedDisplay();},true))->SetPadding(FMargin(8,0,0,0));
    StatusLabel=Text(this,TEXT("SettingsStatus"),TEXT("Quality, audio, effects and controls save automatically. Display changes require confirmation."),12,Muted);
    Column->AddChildToVerticalBox(StatusLabel)->SetPadding(FMargin(0,14,0,12));
    auto* Footer=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Footer);
    auto Action=[&](FName Name,const FString& Copy,TFunction<void(int32)> Callback,bool Closing=false)
    {
        auto* Cell=Footer->AddChildToHorizontalBox(Button(this,Name,Copy,MoveTemp(Callback),Closing));Cell->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Cell->SetPadding(FMargin(0,0,8,0));
    };
    Action(TEXT("HowToPlay"),TEXT("HOW TO PLAY"),[this](int32){HowToPlay();});
    Action(TEXT("TestingArea"),TEXT("TESTING AREA"),[this](int32){TestingArea();});
    if(Game->TestingDirector && Game->TestingDirector->IsActive()) Action(TEXT("ExitTestingArea"),TEXT("EXIT TESTING"),[this](int32){ExitTesting();},true);
    Action(TEXT("QuitGame"),TEXT("CLOSE GAME"),[this](int32){CloseGame();},true);
    Action(TEXT("CloseSettings"),TEXT("RETURN / ESC"),[this](int32){Close();},true);
    SelectPage(0);
}
void UNeonSettingsWidget::SelectPage(int32 Index)
{
    if(!Pages || Index<0 || Index>=Tabs.Num()) return;Pages->SetActiveWidgetIndex(Index);
    for(int32 I=0;I<Tabs.Num();++I)
    {
        Tabs[I]->SetBackgroundColor(I==Index ? FLinearColor(.12f,.75f,.90f) : FLinearColor(.55f,.65f,.75f));
        if(auto* Label=Cast<UTextBlock>(Tabs[I]->GetContent())) Label->SetColorAndOpacity(I==Index ? Cyan : Muted);
    }
}
void UNeonSettingsWidget::BuildAudio(UVerticalBox* Column)
{
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    Heading(this,Column,TEXT("AUDIO & COMBAT EFFECTS"),TEXT("Music continues while menus and card drafts are open. Each channel has its own saved volume."));
    auto Slider=[&](FName Name,float Value,float Maximum=1.f)
    {
        auto* Control=WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),Name);Control->SetMinValue(0);Control->SetMaxValue(Maximum);Control->SetStepSize(.01f);Control->SetValue(Value);return Control;
    };
    MusicSlider=Slider(TEXT("MusicVolumeSlider"),Game->AudioDirector->GetMusicVolume());
    MusicLabel=Row(this,Column,TEXT("MusicLabel"),TEXT(""),TEXT("Main menu and gameplay music. Raising this also unmutes Signal Dock."),SliderBox(this,MusicSlider));
    SFXSlider=Slider(TEXT("SFXVolumeSlider"),Game->AudioDirector->GetSFXVolume());
    SFXLabel=Row(this,Column,TEXT("SFXLabel"),TEXT(""),TEXT("Attacks, impacts, enemies and menu clicks."),SliderBox(this,SFXSlider));
    ImpactSlider=Slider(TEXT("ImpactIntensitySlider"),Game->SpellImpactDirector ? Game->SpellImpactDirector->GetImpactIntensity() : 1,1.5f);
    ImpactLabel=Row(this,Column,TEXT("ImpactLabel"),TEXT(""),TEXT("Spark density from 0 to 150%. Hit markers and damage feedback stay visible."),SliderBox(this,ImpactSlider));
    MusicLabel->SetText(FText::FromString(FString::Printf(TEXT("Music / %d%%%s"),FMath::RoundToInt(MusicSlider->GetValue()*100),Game->AudioDirector->IsMusicMuted() ? TEXT(" / muted") : TEXT(""))));
    SFXLabel->SetText(FText::FromString(FString::Printf(TEXT("Sound effects / %d%%"),FMath::RoundToInt(SFXSlider->GetValue()*100))));
    ImpactLabel->SetText(FText::FromString(FString::Printf(TEXT("Impact particles / %d%%"),FMath::RoundToInt(ImpactSlider->GetValue()*100))));
    Row(this,Column,NAME_None,TEXT("Sound check"),TEXT("Plays a menu click at the current sound-effects volume."),Button(this,TEXT("PreviewSFX"),TEXT("PREVIEW SOUND"),[this](int32){PreviewSFX();}));
    MusicSlider->OnValueChanged.AddDynamic(this,&ThisClass::MusicChanged);SFXSlider->OnValueChanged.AddDynamic(this,&ThisClass::SFXChanged);ImpactSlider->OnValueChanged.AddDynamic(this,&ThisClass::ImpactChanged);
}
void UNeonSettingsWidget::ImpactChanged(float Value)
{
    if(auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());Game && Game->SpellImpactDirector)
    {Game->SpellImpactDirector->SetImpactIntensity(Value);ImpactLabel->SetText(FText::FromString(FString::Printf(TEXT("Impact particles / %d%%"),FMath::RoundToInt(Game->SpellImpactDirector->GetImpactIntensity()*100))));}
}
void UNeonSettingsWidget::TestingArea() {RevertUnconfirmedDisplay();if(auto* PC=GetOwningPlayer<AMVPController>()) PC->OpenTestingArea();}
void UNeonSettingsWidget::HowToPlay() {RevertUnconfirmedDisplay();if(auto* PC=GetOwningPlayer<AMVPController>()) PC->OpenHelp();}
void UNeonSettingsWidget::CloseGame() {if(SavePreferences()) if(auto* PC=GetOwningPlayer<AMVPController>()) PC->CloseGame();}
void UNeonSettingsWidget::ExitTesting() {RevertUnconfirmedDisplay();if(auto* PC=GetOwningPlayer<AMVPController>()) PC->ExitTestingArea();}
void UNeonSettingsWidget::MusicChanged(float Value)
{
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));Game && Game->AudioDirector)
    {Game->AudioDirector->SetMusicVolume(Value);MusicLabel->SetText(FText::FromString(FString::Printf(TEXT("Music / %d%%"),FMath::RoundToInt(Game->AudioDirector->GetMusicVolume()*100))));}
}
void UNeonSettingsWidget::SFXChanged(float Value)
{
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));Game && Game->AudioDirector)
    {Game->AudioDirector->SetSFXVolume(Value);SFXLabel->SetText(FText::FromString(FString::Printf(TEXT("Sound effects / %d%%"),FMath::RoundToInt(Game->AudioDirector->GetSFXVolume()*100))));}
}
void UNeonSettingsWidget::PreviewSFX()
{if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));Game && Game->AudioDirector) Game->AudioDirector->PlayMenuFeedback();}
bool UNeonSettingsWidget::SavePreferences()
{
    RevertUnconfirmedDisplay();auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();
    const bool Saved=(!Profile || Profile->Save()) && (!Game || !Game->SpellImpactDirector || Game->SpellImpactDirector->SaveVFXSettings()) && (!Game || !Game->AudioDirector || Game->AudioDirector->SaveAudioSettings());
    if(!Saved && StatusLabel) StatusLabel->SetText(FText::FromString(TEXT("Could not save preferences. Check disk space and try again.")));return Saved;
}
void UNeonSettingsWidget::Close() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->CloseSettings();}
void UNeonSettingsWidget::NativeDestruct() {RevertUnconfirmedDisplay();Super::NativeDestruct();}
FReply UNeonSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(!Event.IsRepeat() && ForbiddenSettings::HandleKey(this,Event.GetKey())) return FReply::Handled();
    if(Event.GetKey()==EKeys::Escape) {if(!Event.IsRepeat()) {if(bDisplayPending) RevertUnconfirmedDisplay();else Close();}return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
