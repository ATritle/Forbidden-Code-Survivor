#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "ForbiddenScoreWidget.generated.h"
class UTextBlock;
class UEditableTextBox;
class UVerticalBox;
class UForbiddenScoreService;

UCLASS()
class DECKBUILDER_API UForbiddenScoreRowButton : public UButton
{
    GENERATED_BODY()
public:
    void Configure(int32 Index,TFunction<void(int32)> Callback);
private:
    int32 EntryIndex=0;
    TFunction<void(int32)> OnInspect;
    UFUNCTION() void Hovered();
    UFUNCTION() void Clicked();
};

UCLASS()
class DECKBUILDER_API UForbiddenScoreWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool WasPaused=false;
    UFUNCTION() void Close();
    void InspectEntry(int32 Index);
    FString DetailSignature;
    int32 InspectedIndex=INDEX_NONE;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UPROPERTY() TObjectPtr<UVerticalBox> Board;
    UPROPERTY() TObjectPtr<UVerticalBox> Detail;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UEditableTextBox> Username;
    UPROPERTY() TObjectPtr<UForbiddenScoreService> Scores;
    UPROPERTY() TArray<TObjectPtr<UForbiddenScoreRowButton>> Rows;
    int32 SeenRevision=-1;
    void RebuildBoard();
    UFUNCTION() void Submit();
    UFUNCTION() void Refresh();
};
