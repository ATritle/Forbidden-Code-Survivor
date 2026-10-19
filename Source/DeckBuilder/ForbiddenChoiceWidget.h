#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ForbiddenChoiceWidget.generated.h"

UCLASS()
class DECKBUILDER_API UForbiddenChoiceButton : public UButton
{
    GENERATED_BODY()
public:
    void Configure(int32 InIndex,TFunction<void(int32)> InCallback);
private:
    int32 Index=0;
    TFunction<void(int32)> Callback;
    UFUNCTION() void Clicked();
};

/** Keyboard/controller-accessible decision surface shared by replacement, capture and rewards. */
UCLASS()
class DECKBUILDER_API UForbiddenChoiceWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(const FString& Title,const TArray<FString>& Choices,TFunction<void(int32)> Callback);
    void Select(int32 Index);
protected:
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UPROPERTY() TArray<TObjectPtr<UForbiddenChoiceButton>> Buttons;
    TFunction<void(int32)> OnChoice;
    int32 Focused=0;
    bool Consumed=false;
};
