#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForbiddenScoreWidget.generated.h"
class UTextBlock;
class UEditableTextBox;
class UForbiddenScoreService;
UCLASS()
class DECKBUILDER_API UForbiddenScoreWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool WasPaused=false;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<UTextBlock> Board;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UEditableTextBox> Username;
    UPROPERTY() TObjectPtr<UForbiddenScoreService> Scores;
    int32 SeenRevision=-1;
    UFUNCTION() void Submit();
    UFUNCTION() void Refresh();
    UFUNCTION() void Close();
};
