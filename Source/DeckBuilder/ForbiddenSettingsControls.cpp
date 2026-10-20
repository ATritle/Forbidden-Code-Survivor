#include "ForbiddenSettingsControls.h"
#include "ForbiddenProfile.h"
#include "ForbiddenChoiceWidget.h"
#include "NeonSettingsLayout.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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
void ForbiddenSettings::ApplyGraphics(const UObject* Context)
{
    const int32 Preset=FMath::Clamp(FMath::RoundToInt(Get(Context,"Graphics",2)),0,3);
    const float Scale=FMath::Clamp(Get(Context,"RenderScale",Preset==0 ? 70.f : Preset==1 ? 85.f : 100.f),50.f,100.f);
    if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {
        Settings->SetOverallScalabilityLevel(Preset);Settings->SetResolutionScaleValueEx(Scale);
        Settings->ApplyNonResolutionSettings();Settings->SaveSettings();
    }
}
bool ForbiddenSettings::SetGraphicsPreset(const UObject* Context,int32 Preset)
{
    if(!Context || Preset<0 || Preset>3) return false;
    auto* GI=UGameplayStatics::GetGameInstance(Context);auto* Profile=GI ? GI->GetSubsystem<UForbiddenProfile>() : nullptr;
    if(!Profile || !Profile->Data) return false;
    Profile->Data->Settings.Add("Graphics",float(Preset));Profile->Data->Settings.Add("RenderScale",Preset==0 ? 70.f : Preset==1 ? 85.f : 100.f);
    ApplyGraphics(Context);return Profile->Save();
}
void UForbiddenSettingsSlider::Configure(FName Setting,UForbiddenProfile* InProfile,UTextBlock* Label)
{
    SettingID=Setting;Profile=InProfile;Caption=Label;SetMinValue(0);SetMaxValue(1);SetStepSize(0.01f);
    const float* V=InProfile->Data->Settings.Find(Setting);SetValue(V ? *V : 1.f);
    Caption->SetText(FText::FromString(FString::Printf(TEXT("%s / %d%%"),*FString(Setting=="Flash" ? TEXT("Hit flash") : Setting=="Bloom" ? TEXT("Bloom") : Setting=="Shake" ? TEXT("Screen shake") : TEXT("Damage numbers")),FMath::RoundToInt(GetValue()*100))));OnValueChanged.AddUniqueDynamic(this,&ThisClass::Changed);
}
void UForbiddenSettingsSlider::Changed(float NewValue)
{
    if(!Profile.IsValid() || !Profile->Data || !FMath::IsFinite(NewValue)) return;
    Profile->Data->Settings.Add(SettingID,FMath::Clamp(NewValue,0.f,1.f));
    Caption->SetText(FText::FromString(FString::Printf(TEXT("%s / %d%%"),*FString(SettingID=="Flash" ? TEXT("Hit flash") : SettingID=="Bloom" ? TEXT("Bloom") : SettingID=="Shake" ? TEXT("Screen shake") : TEXT("Damage numbers")),FMath::RoundToInt(NewValue*100))));
    ForbiddenSettings::Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(this,0)));SaveLater(Profile.Get());
}
void ForbiddenSettings::AddControls(UUserWidget* Widget,UVerticalBox* Column)
{AddAccessibilityControls(Widget,Column);AddInputControls(Widget,Column);}
void ForbiddenSettings::AddAccessibilityControls(UUserWidget* Widget,UVerticalBox* Column)
{
    using namespace NeonSettingsUI;
    auto* Profile=Widget->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    Heading(Widget,Column,TEXT("COMBAT READABILITY"),TEXT("Fine-tune visual feedback. Enemy danger markers and hit confirmations remain visible."));
    const TCHAR* Hints[]={TEXT("Enemy hit flashes and short impact lights."),TEXT("Glow around bright energy and neon surfaces."),TEXT("Landing and combat camera-shake strength."),TEXT("Density of floating damage numbers.")};int32 Index=0;
    for(FName Key:{FName("Flash"),FName("Bloom"),FName("Shake"),FName("DamageNumbers")})
    {
        auto* Control=Widget->WidgetTree->ConstructWidget<UForbiddenSettingsSlider>(UForbiddenSettingsSlider::StaticClass(),FName(*(Key.ToString()+TEXT("Slider"))));
        Control->SetSliderHandleColor(Cyan);Control->SetSliderBarColor(FLinearColor(.13f,.27f,.35f));
        auto* Label=Row(Widget,Column,NAME_None,Key.ToString(),Hints[Index++],Control);Control->Configure(Key,Profile,Label);
    }
    const auto Caption=MakeShared<TWeakObjectPtr<UTextBlock>>();
    auto* Reduced=Button(Widget,TEXT("ReducedFlashesToggle"),Get(Widget,"ReducedFlashes",0)>.5f ? TEXT("ON") : TEXT("OFF"),[Widget,Profile,Caption](int32)
    {
        Profile->Data->Settings.Add("ReducedFlashes",Get(Widget,"ReducedFlashes",0)>.5f ? 0.f : 1.f);Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(Widget,0)));
        if(Caption->IsValid()) Caption->Get()->SetText(FText::FromString(Get(Widget,"ReducedFlashes",0)>.5f ? TEXT("ON") : TEXT("OFF")));SaveLater(Profile);
    });
    *Caption=Cast<UTextBlock>(Reduced->GetContent());Row(Widget,Column,NAME_None,TEXT("Reduced flashes"),TEXT("Further limits hit flashes and bloom without hiding hazards."),Reduced);
}
void ForbiddenSettings::AddInputControls(UUserWidget* Widget,UVerticalBox* Column)
{
    using namespace NeonSettingsUI;
    auto* Profile=Widget->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    Heading(Widget,Column,TEXT("MOVEMENT & ACTIONS"),TEXT("Select an action, then press a key. Escape cancels. Menu shortcuts stay reserved."));
    const TArray<FName> Actions={"Forward","Back","Left","Right","Boost","Mark","Interact","Inspect"};
    const auto Defaults=DefaultBindings();
    for(int32 Index=0;Index<Actions.Num();Index+=2)
    {
        auto* Pair=Widget->WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Pair)->SetPadding(FMargin(0,0,0,12));
        for(int32 Col=0;Col<2;++Col)
        {
            const FName Action=Actions[Index+Col];const FKey Key=Profile->Data->Bindings.Contains(Action) ? Profile->Data->Bindings[Action] : Defaults[Action];
            auto* B=Button(Widget,FName(*(TEXT("Rebind_")+Action.ToString())),TEXT(""));
            auto* Label=Text(Widget,FName(*(TEXT("BindLabel_")+Action.ToString())),Action.ToString()+TEXT(" / ")+Key.GetDisplayName().ToString(),15);
            Label->SetAutoWrapText(false);Label->SetJustification(ETextJustify::Center);B->SetContent(Label);
            B->Configure(0,[Widget,Action,Label](int32){RebindingWidget=Widget;RebindingAction=Action;RebindingCaption=Label;Label->SetText(FText::FromString(TEXT("PRESS A KEY / ESC TO CANCEL")));Widget->SetKeyboardFocus();});
            auto* Slot=Pair->AddChildToHorizontalBox(B);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(0,0,Col==0 ? 12 : 0,0));
        }
    }
    Column->AddChildToVerticalBox(Button(Widget,TEXT("ResetInput"),TEXT("RESTORE DEFAULT CONTROLS"),[Widget,Profile,Actions](int32)
    {
        Profile->Data->Bindings=DefaultBindings();Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(Widget,0)),true);SaveLater(Profile);
        RebindingWidget.Reset();RebindingCaption.Reset();RebindingAction=NAME_None;
        for(FName Action:Actions) if(auto* Label=Cast<UTextBlock>(Widget->WidgetTree->FindWidget(FName(*(TEXT("BindLabel_")+Action.ToString())))))
            Label->SetText(FText::FromString(Action.ToString()+TEXT(" / ")+Profile->Data->Bindings[Action].GetDisplayName().ToString()));
    }))->SetPadding(FMargin(0,8,0,0));
}
bool ForbiddenSettings::HandleKey(UUserWidget* Widget,FKey Key)
{
    if(RebindingWidget.Get()!=Widget || RebindingAction.IsNone()) return false;
    if(Key==EKeys::Escape)
    {
        const auto* P=Widget->GetGameInstance()->GetSubsystem<UForbiddenProfile>();const auto Defaults=DefaultBindings();
        const FKey Current=P && P->Data && P->Data->Bindings.Contains(RebindingAction) ? P->Data->Bindings[RebindingAction] : Defaults[RebindingAction];
        if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(RebindingAction.ToString()+TEXT(" / ")+Current.GetDisplayName().ToString()));
        RebindingWidget.Reset();RebindingCaption.Reset();RebindingAction=NAME_None;return true;
    }
    auto* Profile=Widget->GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data || !Key.IsValid()) return true;
    auto Bindings=DefaultBindings();for(const auto& Pair:Profile->Data->Bindings) Bindings.Add(Pair.Key,Pair.Value);
    for(const auto& Pair:Bindings) if(Pair.Key!=RebindingAction && Pair.Value==Key) {if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(TEXT("KEY ALREADY USED / CHOOSE ANOTHER")));return true;}
    if((Key==EKeys::Tab && RebindingAction!="Inspect") || Key==EKeys::F1 || Key==EKeys::One || Key==EKeys::Two || Key==EKeys::Three || Key==EKeys::R || Key==EKeys::H || Key==EKeys::T || Key==EKeys::L || Key==EKeys::Enter || Key==EKeys::LeftMouseButton || Key.IsGamepadKey() || Key==EKeys::MouseScrollUp || Key==EKeys::MouseScrollDown)
    {if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(TEXT("RESERVED MENU KEY / CHOOSE ANOTHER")));return true;}
    Profile->Data->Bindings.Add(RebindingAction,Key);if(RebindingCaption.IsValid()) RebindingCaption->SetText(FText::FromString(RebindingAction.ToString()+TEXT(" / ")+Key.GetDisplayName().ToString()));
    RebindingWidget.Reset();RebindingAction=NAME_None;Apply(Cast<AMVPPlayer>(UGameplayStatics::GetPlayerPawn(Widget,0)),true);SaveLater(Profile);return true;
}
