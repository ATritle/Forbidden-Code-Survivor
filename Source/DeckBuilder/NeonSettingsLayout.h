#pragma once
#include "CoreMinimal.h"
class UUserWidget;
class UWidget;
class UVerticalBox;
class UTextBlock;
class UForbiddenChoiceButton;

namespace NeonSettingsUI
{
    inline const FLinearColor Cyan(.05f,.85f,1), White(.87f,.94f,1), Muted(.47f,.64f,.74f);
    UTextBlock* Text(UUserWidget* Owner,FName Name,const FString& Copy,int32 Size=16,FLinearColor Color=White);
    UForbiddenChoiceButton* Button(UUserWidget* Owner,FName Name,const FString& Copy,TFunction<void(int32)> Callback={},bool Closing=false);
    UTextBlock* Row(UUserWidget* Owner,UVerticalBox* Column,FName Name,const FString& Title,const FString& Hint,UWidget* Control);
    void Heading(UUserWidget* Owner,UVerticalBox* Column,const FString& Title,const FString& Hint);
}
