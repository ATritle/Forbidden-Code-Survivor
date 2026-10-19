#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "GenericPlatform/GenericWindow.h"
#include "NeonSettingsWidget.generated.h"

class USlider;
class UTextBlock;
class UWidgetSwitcher;
class UBorder;
class UForbiddenChoiceButton;

UCLASS()
class DECKBUILDER_API UNeonSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool SavePreferences();
    void RevertUnconfirmedDisplay();
    bool IsDisplayConfirmationPending() const {return bDisplayPending;}
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY() TObjectPtr<UWidgetSwitcher> Pages;
    UPROPERTY() TArray<TObjectPtr<UForbiddenChoiceButton>> Tabs;
    UPROPERTY() TArray<TObjectPtr<UForbiddenChoiceButton>> QualityButtons;
    UPROPERTY() TObjectPtr<UComboBoxString> WindowMode;
    UPROPERTY() TObjectPtr<UComboBoxString> Resolution;
    UPROPERTY() TObjectPtr<UForbiddenChoiceButton> VSyncButton;
    UPROPERTY() TObjectPtr<UBorder> DisplayConfirmation;
    UPROPERTY() TObjectPtr<UTextBlock> DisplayCountdown;
    UPROPERTY() TObjectPtr<UTextBlock> QualityLabel;
    UPROPERTY() TObjectPtr<USlider> RenderScale;
    UPROPERTY() TObjectPtr<UTextBlock> RenderScaleLabel;
    bool bDisplayPending=false,bRefreshing=false;
    double DisplayDeadline=0;
    uint32 DisplayRequestRevision=0;
    FIntPoint PreviousResolution;
    EWindowMode::Type PreviousMode=EWindowMode::Windowed;
    void SelectPage(int32 Index);
    void RefreshGraphics();
    void RefreshDisplayOptions();
    void QueueDisplayPreview();
    void RestoreWindowedFrame(FIntPoint Size);
    void BuildDisplay(class UVerticalBox* Column);
    void BuildAudio(class UVerticalBox* Column);
    UFUNCTION() void WindowModeChanged(FString Selected,ESelectInfo::Type SelectionType);
    UFUNCTION() void ResolutionChanged(FString Selected,ESelectInfo::Type SelectionType);
    UFUNCTION() void ApplyDisplay();
    UFUNCTION() void KeepDisplay();
    UFUNCTION() void ToggleVSync();
    UFUNCTION() void RenderScaleChanged(float Value);
    UFUNCTION() UWidget* ComboEntry(FString Option);
    UPROPERTY() TObjectPtr<USlider> MusicSlider;
    UPROPERTY() TObjectPtr<USlider> SFXSlider;
    UPROPERTY() TObjectPtr<USlider> ImpactSlider;
    UPROPERTY() TObjectPtr<UTextBlock> ImpactLabel;
    UPROPERTY() TObjectPtr<UTextBlock> MusicLabel;
    UPROPERTY() TObjectPtr<UTextBlock> SFXLabel;
    UPROPERTY() TObjectPtr<UTextBlock> StatusLabel;
    UFUNCTION() void MusicChanged(float Value);
    UFUNCTION() void SFXChanged(float Value);
    UFUNCTION() void ImpactChanged(float Value);
    UFUNCTION() void TestingArea();
    UFUNCTION() void ExitTesting();
    UFUNCTION() void PreviewSFX();
    UFUNCTION() void Close();
    UFUNCTION() void HowToPlay();
    UFUNCTION() void CloseGame();
};
