#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForbiddenHelpWidget.generated.h"

class UVerticalBox;
class UScrollBox;
class UTextBlock;
class UImage;
class UTexture;
class UForbiddenChoiceButton;

struct FForbiddenHelpEntry
{
    int32 Topic=0;
    FString Title, Copy, ImagePath, Caption;
};

namespace ForbiddenHelp
{
    TArray<FForbiddenHelpEntry> BuildEntries(const UObject* Context);
    const TArray<FString>& Topics();
}

/** Illustrated, read-only field guide. Only the selected illustration is held in memory. */
UCLASS()
class DECKBUILDER_API UForbiddenHelpWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SelectTopic(int32 Index);
    void SelectEntry(int32 Index);
    void Close();
    int32 EntryCount() const {return Entries.Num();}
    int32 CurrentEntry() const {return Selected;}
    const TArray<FForbiddenHelpEntry>& GetEntries() const {return Entries;}
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    TArray<FForbiddenHelpEntry> Entries;
    int32 Topic=0,Selected=0;
    UPROPERTY() TObjectPtr<UVerticalBox> EntryList;
    UPROPERTY() TObjectPtr<UScrollBox> EntryScroll;
    UPROPERTY() TObjectPtr<UScrollBox> ReadingScroll;
    UPROPERTY() TObjectPtr<UTextBlock> Heading;
    UPROPERTY() TObjectPtr<UTextBlock> Body;
    UPROPERTY() TObjectPtr<UTextBlock> Caption;
    UPROPERTY() TObjectPtr<UTextBlock> Counter;
    UPROPERTY() TObjectPtr<UImage> Illustration;
    UPROPERTY() TObjectPtr<UTexture> CurrentImage;
    UPROPERTY() TArray<TObjectPtr<UForbiddenChoiceButton>> TopicButtons;
    UPROPERTY() TArray<TObjectPtr<UForbiddenChoiceButton>> EntryButtons;
    void Step(int32 Direction);
};
