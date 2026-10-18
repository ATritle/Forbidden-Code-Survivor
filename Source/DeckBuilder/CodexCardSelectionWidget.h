#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "CodexCardSelectionWidget.generated.h"

class UCardDataAsset;
class UButton;
class UTexture;

USTRUCT(BlueprintType)
struct DECKBUILDER_API FCardDisplayData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FText Title;
    UPROPERTY(BlueprintReadOnly) FText Description;
    UPROPERTY(BlueprintReadOnly) FGameplayTag MagicSchoolTag;
    UPROPERTY(BlueprintReadOnly) FLinearColor RarityColor = FLinearColor::White;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture> Icon;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<UTexture> SchoolIcon;
    UPROPERTY(BlueprintReadOnly) FText RarityLabel;
};

/** Native playable layout; Blueprint children may supply the three named buttons.
 * GameMode owns pause/progression so queued drafts never resume combat prematurely. */
UCLASS(Blueprintable)
class DECKBUILDER_API UCodexCardSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Codex")
    void InitializeSelectionScreen(TArray<UCardDataAsset*> ChoiceCards);
    UFUNCTION(BlueprintCallable, Category="Codex")
    bool SelectChoice(int32 Index);
    UPROPERTY(BlueprintReadOnly, Category="Codex")
    TArray<FCardDisplayData> DisplayCards;
    UFUNCTION(BlueprintImplementableEvent, Category="Codex")
    void OnCardDisplayUpdated(int32 Index, const FCardDisplayData& Data);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> CardOption_A;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> CardOption_B;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> CardOption_C;
private:
    UFUNCTION() void OpenSettings();
    UFUNCTION() void ClickA();
    UFUNCTION() void ClickB();
    UFUNCTION() void ClickC();
    UPROPERTY(Transient) TArray<TObjectPtr<UCardDataAsset>> Choices;
    bool bConsumed = false;
    bool bNativeLayout = false;
    double RevealStartTime = 0.0;
    void BuildCardFace(UButton* Button, const FCardDisplayData& Data, int32 Index);
};
