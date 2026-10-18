#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeonTestingWidget.generated.h"
class UCheckBox;
class USpinBox;
class UTextBlock;
class UBorder;
class UOverlay;
class UButton;
class UCardDataAsset;

UCLASS()
class DECKBUILDER_API UNeonTestingWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UPROPERTY() TArray<TObjectPtr<UCheckBox>> AttackToggles;
    UPROPERTY() TArray<TObjectPtr<UCheckBox>> Enabled;
    UPROPERTY() TArray<TObjectPtr<USpinBox>> Counts;
    UPROPERTY() TObjectPtr<UCheckBox> AutoReset;
    UPROPERTY() TObjectPtr<USpinBox> Spacing;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    TArray<FName> IDs;
    UPROPERTY() TObjectPtr<UBorder> MainBody;
    UPROPERTY() TObjectPtr<UOverlay> WarningLayer;
    UPROPERTY() TObjectPtr<UTextBlock> WarningText;
    UPROPERTY() TObjectPtr<UButton> ContinueButton;
    TArray<FName> PendingCards;
    TArray<int32> PendingCounts;
    bool bPendingReset=true;
    float PendingSpacing=220;
    void Commit();
    UWidget* MakeTooltip(const UCardDataAsset* Card);
    UFUNCTION() void Start();
    UFUNCTION() void ContinueWarning();
    UFUNCTION() void CancelWarning();
    UFUNCTION() void SelectAll();
    UFUNCTION() void ClearAll();
    UFUNCTION() void Back();
    UFUNCTION() void Exit();
};
