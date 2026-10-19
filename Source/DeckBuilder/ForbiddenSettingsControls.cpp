#include "ForbiddenSettingsControls.h"
#include "ForbiddenProfile.h"
#include "ForbiddenChoiceWidget.h"
#include "MVPGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Containers/Ticker.h"

namespace
{
    TWeakObjectPtr<UUserWidget> RebindingWidget;
    TWeakObjectPtr<UTextBlock> RebindingCaption;
    FName RebindingAction;
    TWeakObjectPtr<UForbiddenProfile> DirtyProfile;
    double SaveAt=0;
    bool SavePending=false;
    void SaveLater(UForbiddenProfile* Profile)
    {
        DirtyProfile=Profile;SaveAt=FPlatformTime::Seconds()+0.4;
        if(SavePending) return;SavePending=true;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            if(FPlatformTime::Seconds()<SaveAt) return true;
            if(DirtyProfile.IsValid()) DirtyProfile->Save();SavePending=false;DirtyProfile.Reset();return false;
        }));
    }
}
TMap<FName,FKey> ForbiddenSettings::DefaultBindings()
{
    return {{"Forward",EKeys::W},{"Back",EKeys::S},{"Left",EKeys::A},{"Right",EKeys::D},{"Boost",EKeys::LeftShift},{"Mark",EKeys::MiddleMouseButton},{"Interact",EKeys::E},{"Inspect",EKeys::Tab}};
}
float ForbiddenSettings::Get(const UObject* Context,FName Key,float Default)
{
    if(!Context) return Default;const auto* GI=UGameplayStatics::GetGameInstance(Context);const auto* P=GI ? GI->GetSubsystem<UForbiddenProfile>() : nullptr;
    const auto* V=P && P->Data ? P->Data->Settings.Find(Key) : nullptr;return V && FMath::IsFinite(*V) ? *V : Default;
}
void ForbiddenSettings::Apply(AMVPPlayer* Player,bool Rebind)
{
    if(!Player) return;
    const bool Reduced=Get(Player,"ReducedFlashes",0)>0.5f;
    Player->FollowCamera->PostProcessSettings.bOverride_BloomIntensity=true;
    Player->FollowCamera->PostProcessSettings.BloomIntensity=Get(Player,"Bloom",0.7f)*(Reduced ? 0.15f : 0.65f);
    const int32 Preset=FMath::Clamp(FMath::RoundToInt(Get(Player,"Graphics",2)),0,3);
    if(auto* Quality=UGameUserSettings::GetGameUserSettings();Quality && Quality->GetOverallScalabilityLevel()!=Preset)
    {Quality->SetOverallScalabilityLevel(Preset);Quality->SetResolutionScaleValueEx(Preset==0 ? 70.f : Preset==1 ? 85.f : 100.f);Quality->ApplySettings(false);}
    auto* Profile=Player->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    auto* Inputs=GetMutableDefault<UInputSettings>();const auto OldMappings=Inputs->GetAxisMappings();
    for(const auto& M:OldMappings) if((M.AxisName=="MoveForward" || M.AxisName=="MoveRight") && !M.Key.IsGamepadKey()) Inputs->RemoveAxisMapping(M,false);
    auto Bindings=DefaultBindings();for(const auto& Pair:Profile->Data->Bindings) if(Pair.Value.IsValid()) Bindings.Add(Pair.Key,Pair.Value);
    Inputs->AddAxisMapping(FInputAxisKeyMapping("MoveForward",Bindings["Forward"],1),false);Inputs->AddAxisMapping(FInputAxisKeyMapping("MoveForward",Bindings["Back"],-1),false);
    Inputs->AddAxisMapping(FInputAxisKeyMapping("MoveRight",Bindings["Right"],1),false);Inputs->AddAxisMapping(FInputAxisKeyMapping("MoveRight",Bindings["Left"],-1),false);
    Inputs->ForceRebuildKeymaps();
    if(Rebind && Player->InputComponent)
    {
        Player->InputComponent->KeyBindings.Reset();Player->InputComponent->AxisBindings.Reset();Player->SetupPlayerInputComponent(Player->InputComponent);
    }
}
void UForbiddenSettingsSlider::Configure(FName Setting,UForbiddenProfile* InProfile,UTextBlock* Label)
{
    SettingID=Setting;Profile=InProfile;Caption=Label;SetMinValue(0);SetMaxValue(1);SetStepSize(0.01f);
    const float* V=InProfile->Data->Settings.Find(Setting);SetValue(V ? *V : 1.f);
    Caption->SetText(FText::FromString(FString::Printf(TEXT("%s / %d%%"),*Setting.ToString(),FMath::RoundToInt(GetValue()*100))));OnValueChanged.AddUniqueDynamic(this,&ThisClass::Changed);
}
void UForbiddenSettingsSlider::Changed(float NewValue)
{
    if(!Profile.IsValid() || !Profile->Data || !FMath::IsFinite(NewValue)) return;
    Profile->Data->Settings.Add(SettingID,FMath::Clamp(NewValue,0.f,1.f));
    Caption->SetText(FText::FromString(FString::Printf(TEXT("%s / %d%%"),*SettingID.ToString(),FMath::RoundToInt(NewValue*100))));
    ForbiddenSettings::Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this,0)));SaveLater(Profile.Get());
}
void ForbiddenSettings::AddControls(UUserWidget* Widget,UVerticalBox* Column)
{
    auto* Profile=Widget->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    auto Text=[&](const FString& Copy)
    {auto* T=Widget->WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Copy));T->SetColorAndOpacity(FLinearColor(0.5f,0.85f,1));T->SetAutoWrapText(true);Column->AddChildToVerticalBox(T)->SetPadding(FMargin(0,9));return T;};
    Text(TEXT("ACCESSIBILITY / DANGER MARKERS ALWAYS VISIBLE"));
    for(FName Key:{FName("Flash"),FName("Bloom"),FName("Shake"),FName("DamageNumbers")})
    {
        auto* Label=Text(Key.ToString());auto* Control=Widget->WidgetTree->ConstructWidget<UForbiddenSettingsSlider>();Control->Configure(Key,Profile,Label);Control->SetSliderHandleColor(FLinearColor(0.05f,0.8f,1));Column->AddChildToVerticalBox(Control)->SetPadding(FMargin(0,4,0,14));
    }
    auto Button=[&](const FString& Copy,TFunction<void(int32)> Callback)
    {
        auto* B=Widget->WidgetTree->ConstructWidget<UForbiddenChoiceButton>();auto* T=Widget->WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Copy));T->SetColorAndOpacity(FLinearColor(0.85f,0.95f,1));B->SetContent(T);B->SetBackgroundColor(FLinearColor(0.02f,0.08f,0.15f));B->Configure(0,MoveTemp(Callback));Column->AddChildToVerticalBox(B)->SetPadding(FMargin(0,7));return T;
    };
    const auto ReducedCaption=MakeShared<TWeakObjectPtr<UTextBlock>>();
    auto* ReducedLabel=Button(Get(Widget,"ReducedFlashes",0)>0.5f ? TEXT("REDUCED FLASHES / ON") : TEXT("REDUCED FLASHES / OFF"),[Widget,Profile,ReducedCaption](int32)
    {Profile->Data->Settings.Add("ReducedFlashes",Get(Widget,"ReducedFlashes",0)>0.5f ? 0.f : 1.f);Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(Widget,0)));if(ReducedCaption->IsValid()) ReducedCaption->Get()->SetText(FText::FromString(Get(Widget,"ReducedFlashes",0)>0.5f ? TEXT("REDUCED FLASHES / ON") : TEXT("REDUCED FLASHES / OFF")));SaveLater(Profile);});
    *ReducedCaption=ReducedLabel;
    ReducedLabel->SetToolTipText(FText::FromString(TEXT("Limits flash strength and bloom; telegraphs and hit cues stay enabled.")));
    Text(TEXT("GRAPHICS PRESETS"));
    const TCHAR* Names[]={TEXT("PERFORMANCE"),TEXT("BALANCED"),TEXT("HIGH"),TEXT("CINEMATIC")};
    for(int32 I=0;I<4;++I) Button(Names[I],[Widget,Profile,I](int32)
    {
        Profile->Data->Settings.Add("Graphics",float(I));if(auto* Settings=UGameUserSettings::GetGameUserSettings()) {Settings->SetOverallScalabilityLevel(I);Settings->SetResolutionScaleValueEx(I==0 ? 70.f : I==1 ? 85.f : 100.f);Settings->ApplySettings(false);}
        SaveLater(Profile);
    });
    Text(TEXT("REMAP CONTROLS / SELECT AN ACTION, THEN PRESS A KEY"));
    for(const auto& Pair:DefaultBindings())
    {
        const FName Action=Pair.Key;const FKey Current=Profile->Data->Bindings.Contains(Action) ? Profile->Data->Bindings[Action] : Pair.Value;
        auto* B=Widget->WidgetTree->ConstructWidget<UForbiddenChoiceButton>();auto* Label=Widget->WidgetTree->ConstructWidget<UTextBlock>();Label->SetText(FText::FromString(Action.ToString()+TEXT(" / ")+Current.ToString()));B->SetContent(Label);
        B->Configure(0,[Widget,Action,Label](int32){RebindingWidget=Widget;RebindingAction=Action;RebindingCaption=Label;Label->SetText(FText::FromString(TEXT("PRESS A NEW KEY / ESC TO CANCEL")));Widget->SetKeyboardFocus();});Column->AddChildToVerticalBox(B)->SetPadding(FMargin(0,5));
    }
    Button(TEXT("RESET INPUT TO DEFAULT"),[Widget,Profile](int32){Profile->Data->Bindings=DefaultBindings();Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(Widget,0)),true);SaveLater(Profile);});
}
bool ForbiddenSettings::HandleKey(UUserWidget* Widget,FKey Key)
{
    if(RebindingWidget.Get()!=Widget || RebindingAction.IsNone()) return false;
    if(Key==EKeys::Escape) {if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(TEXT("CANCELLED / REOPEN TO REFRESH")));RebindingWidget.Reset();RebindingAction=NAME_None;return true;}
    auto* Profile=Widget->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data || !Key.IsValid()) return true;
    auto Bindings=DefaultBindings();for(const auto& Pair:Profile->Data->Bindings) Bindings.Add(Pair.Key,Pair.Value);
    for(const auto& Pair:Bindings) if(Pair.Key!=RebindingAction && Pair.Value==Key) {if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(TEXT("KEY ALREADY USED / CHOOSE ANOTHER")));return true;}
    if(Key==EKeys::One || Key==EKeys::Two || Key==EKeys::Three || Key==EKeys::R || Key==EKeys::H || Key==EKeys::T || Key==EKeys::L || Key==EKeys::Enter || Key==EKeys::LeftMouseButton || Key.IsGamepadKey() || Key==EKeys::MouseScrollUp || Key==EKeys::MouseScrollDown)
    {if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(TEXT("RESERVED MENU KEY / CHOOSE ANOTHER")));return true;}
    Profile->Data->Bindings.Add(RebindingAction,Key);if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(RebindingAction.ToString()+TEXT(" / ")+Key.ToString()));
    RebindingWidget.Reset();RebindingAction=NAME_None;Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(Widget,0)),true);SaveLater(Profile);return true;
}
