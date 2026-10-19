#include "NeonSettingsWidget.h"
#include "ForbiddenSettingsControls.h"
#include "ForbiddenChoiceWidget.h"
#include "NeonMenuAudio.h"
#include "ForbiddenUI.h"
#include "ForbiddenCodeBranding.h"
#include "NeonAudioDirector.h"
#include "NeonSpellImpactDirector.h"
#include "MortisTestingDirector.h"
#include "MVPGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UNeonSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);
    auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this));
    if(!Game || !Game->AudioDirector) return;
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(10.f);
    auto* BlurSlot=Root->AddChildToOverlay(Blur);BlurSlot->SetHorizontalAlignment(HAlign_Fill);BlurSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(0.005f,0.01f,0.025f,0.9f));
    Root->AddChildToOverlay(Shade);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(640.f);Size->SetMaxDesiredHeight(790);
    auto* Center=Root->AddChildToOverlay(Size);Center->SetHorizontalAlignment(HAlign_Center);Center->SetVerticalAlignment(VAlign_Center);
    auto* Rim=WidgetTree->ConstructWidget<UBorder>();Rim->SetBrushColor(FLinearColor(0.02f,0.8f,1.f));Rim->SetPadding(FMargin(2));Size->SetContent(Rim);
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetBrushColor(FLinearColor(0.012f,0.022f,0.04f));Panel->SetPadding(FMargin(36));Rim->SetContent(Panel);
    ForbiddenUI::Glass(Panel,FVector2D(640,790),FLinearColor(0.05f,0.8f,1));
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);Panel->SetContent(Scroll);
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Column);
    auto Text=[&](const TCHAR* Caption,int32 PointSize,FLinearColor Color)
    {
        auto* Label=WidgetTree->ConstructWidget<UTextBlock>();auto Font=Label->GetFont();Font.Size=PointSize;Label->SetFont(Font);
        Label->SetText(FText::FromString(Caption));Label->SetColorAndOpacity(Color);Label->SetAutoWrapText(true);
        Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0,8));return Label;
    };
    const FLinearColor Cyan(0.05f,0.85f,1.f),White(0.85f,0.94f,1.f),Muted(0.5f,0.65f,0.75f);
    Text(ForbiddenCode::DisplayTitle,22,Cyan);
    Text(TEXT("SETTINGS"),16,Muted);
    auto MenuAction=[&](const TCHAR* Name,const TCHAR* Copy)
    {
        auto* B=WidgetTree->ConstructWidget<UForbiddenChoiceButton>(UForbiddenChoiceButton::StaticClass(),FName(Name));B->Configure(0,{});
        B->bClosingFeedback=FString(Name)==TEXT("QuitGame");
        B->SetBackgroundColor(FLinearColor(.02f,.15f,.23f));auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(Copy));L->SetJustification(ETextJustify::Center);L->SetColorAndOpacity(Cyan);B->SetContent(L);
        Column->AddChildToVerticalBox(B)->SetPadding(FMargin(0,7));return B;
    };
    MenuAction(TEXT("HowToPlay"),TEXT("HOW TO PLAY  /  ILLUSTRATED FIELD GUIDE"))->OnClicked.AddDynamic(this,&ThisClass::HowToPlay);
    MenuAction(TEXT("QuitGame"),TEXT("CLOSE GAME  /  EXIT TO DESKTOP"))->OnClicked.AddDynamic(this,&ThisClass::CloseGame);
    Text(TEXT("AUDIO MIX"),13,Muted);
    auto Slider=[&](FName Name,float Value,FLinearColor Color)
    {
        auto* Control=WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),Name);
        Control->SetMinValue(0);Control->SetMaxValue(1);Control->SetStepSize(0.01f);Control->SetValue(Value);
        Control->SetSliderHandleColor(Color);Control->SetSliderBarColor(FLinearColor(0.12f,0.2f,0.27f));
        auto* Row=WidgetTree->ConstructWidget<USizeBox>();Row->SetHeightOverride(42);Row->SetContent(Control);
        Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,0,0,18));return Control;
    };
    MusicLabel=Text(TEXT(""),18,White);
    MusicSlider=Slider(TEXT("MusicVolumeSlider"),Game->AudioDirector->GetMusicVolume(),Cyan);
    SFXLabel=Text(TEXT(""),18,White);
    SFXSlider=Slider(TEXT("SFXVolumeSlider"),Game->AudioDirector->GetSFXVolume(),FLinearColor(1.f,0.65f,0.04f));
    Text(TEXT("COMBAT READABILITY"),13,Muted);
    ImpactLabel=Text(TEXT(""),18,White);
    ImpactSlider=Slider(TEXT("ImpactIntensitySlider"),0,Cyan);ImpactSlider->SetMaxValue(1.5f);
    ImpactSlider->SetValue(Game->SpellImpactDirector ? Game->SpellImpactDirector->GetImpactIntensity() : 1.f);
    ImpactLabel->SetText(FText::FromString(FString::Printf(TEXT("IMPACT PARTICLES   %d%%"),FMath::RoundToInt(ImpactSlider->GetValue()*100))));
    Text(TEXT("0–150% spark density. Hit markers and damage feedback remain visible."),11,Muted);
    ForbiddenSettings::AddControls(this,Column);
    auto Button=[&](FName Name,const TCHAR* Caption)
    {
        auto* Control=WidgetTree->ConstructWidget<UForbiddenChoiceButton>(UForbiddenChoiceButton::StaticClass(),Name);Control->Configure(0,{});
        Control->bClosingFeedback=Name==TEXT("CloseSettings") || Name==TEXT("ExitTestingArea");
        Control->SetBackgroundColor(FLinearColor(0.03f,0.13f,0.22f));
        auto* Label=WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Caption));
        Label->SetJustification(ETextJustify::Center);Label->SetColorAndOpacity(White);Control->SetContent(Label);
        Column->AddChildToVerticalBox(Control)->SetPadding(FMargin(0,8));return Control;
    };
    Button(TEXT("PreviewSFX"),TEXT("PREVIEW SOUND EFFECT"))->OnClicked.AddDynamic(this,&ThisClass::PreviewSFX);
    Button(TEXT("TestingArea"),TEXT("TESTING AREA  /  CARDS & TARGETS"))->OnClicked.AddDynamic(this,&ThisClass::TestingArea);
    if(Game->TestingDirector && Game->TestingDirector->IsActive()) Button(TEXT("ExitTestingArea"),TEXT("EXIT TESTING & RETURN TO SESSION"))->OnClicked.AddDynamic(this,&ThisClass::ExitTesting);
    StatusLabel=Text(TEXT("Changes apply immediately and save automatically."),12,Muted);
    Button(TEXT("CloseSettings"),TEXT("SAVE & RETURN  /  ESC"))->OnClicked.AddDynamic(this,&ThisClass::Close);
    MusicLabel->SetText(FText::FromString(FString::Printf(TEXT("MUSIC   %d%%"),FMath::RoundToInt(MusicSlider->GetValue()*100))));
    SFXLabel->SetText(FText::FromString(FString::Printf(TEXT("SOUND EFFECTS   %d%%"),FMath::RoundToInt(SFXSlider->GetValue()*100))));
    MusicSlider->OnValueChanged.AddDynamic(this,&ThisClass::MusicChanged);
    SFXSlider->OnValueChanged.AddDynamic(this,&ThisClass::SFXChanged);
    ImpactSlider->OnValueChanged.AddDynamic(this,&ThisClass::ImpactChanged);
}
void UNeonSettingsWidget::ImpactChanged(float Value)
{
    if(auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());Game && Game->SpellImpactDirector)
    {Game->SpellImpactDirector->SetImpactIntensity(Value);ImpactLabel->SetText(FText::FromString(FString::Printf(TEXT("IMPACT PARTICLES   %d%%"),FMath::RoundToInt(Game->SpellImpactDirector->GetImpactIntensity()*100))));}
}
void UNeonSettingsWidget::TestingArea() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->OpenTestingArea();}
void UNeonSettingsWidget::HowToPlay() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->OpenHelp();}
void UNeonSettingsWidget::CloseGame() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->CloseGame();}
void UNeonSettingsWidget::ExitTesting() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->ExitTestingArea();}
void UNeonSettingsWidget::MusicChanged(float Value)
{
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->AudioDirector)
    {Game->AudioDirector->SetMusicVolume(Value);MusicLabel->SetText(FText::FromString(FString::Printf(TEXT("MUSIC   %d%%"),FMath::RoundToInt(Game->AudioDirector->GetMusicVolume()*100))));}
}
void UNeonSettingsWidget::SFXChanged(float Value)
{
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->AudioDirector)
    {Game->AudioDirector->SetSFXVolume(Value);SFXLabel->SetText(FText::FromString(FString::Printf(TEXT("SOUND EFFECTS   %d%%"),FMath::RoundToInt(Game->AudioDirector->GetSFXVolume()*100))));}
}
void UNeonSettingsWidget::PreviewSFX()
{
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->AudioDirector) Game->AudioDirector->PlayMenuFeedback();
}
void UNeonSettingsWidget::Close()
{
    if(auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());Game && Game->SpellImpactDirector && !Game->SpellImpactDirector->SaveVFXSettings())
    {StatusLabel->SetText(FText::FromString(TEXT("Could not save visual settings. Please retry.")));return;}
    if(auto* Game=Cast<AMVPArenaGameMode>(UGameplayStatics::GetGameMode(this)); Game && Game->AudioDirector && !Game->AudioDirector->SaveAudioSettings())
    {StatusLabel->SetText(FText::FromString(TEXT("Could not save. Check disk space, then try again.")));return;}
    if(auto* PC=GetOwningPlayer<AMVPController>()) PC->CloseSettings();
}
FReply UNeonSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(!Event.IsRepeat() && ForbiddenSettings::HandleKey(this,Event.GetKey())) return FReply::Handled();
    if(Event.GetKey()==EKeys::Up || Event.GetKey()==EKeys::Down || Event.GetKey()==EKeys::Gamepad_DPad_Up || Event.GetKey()==EKeys::Gamepad_DPad_Down) NeonMenuAudio::Navigate(this);
    if(Event.GetKey()==EKeys::Escape) {if(!Event.IsRepeat()) Close();return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
