#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeonSettingsWidget.generated.h"

class USlider;
class UTextBlock;

UCLASS()
class DECKBUILDER_API UNeonSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
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
