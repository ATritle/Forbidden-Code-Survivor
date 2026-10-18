#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeonChallengeWidget.generated.h"
class UNeonChallengeComponent;
class UNeonChallengeDefinition;
class UTextBlock;
class UButton;
class UMaterialInstanceDynamic;

UCLASS()
class DECKBUILDER_API UNeonChallengeWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeOffer(UNeonChallengeComponent* Owner,UNeonChallengeDefinition* Definition);
    void ShowOfferError(const FString& Message);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry,float Dt) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    TWeakObjectPtr<UNeonChallengeComponent> ContractOwner;
    UPROPERTY() TObjectPtr<UTextBlock> Title;
    UPROPERTY() TObjectPtr<UTextBlock> Reward;
    UPROPERTY() TObjectPtr<UTextBlock> Risk;
    UPROPERTY() TObjectPtr<UTextBlock> Duration;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PanelMaterial;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ActionMaterials;
    UPROPERTY() TArray<TObjectPtr<UButton>> ActionButtons;
    float HoverAmount[2]={};
    UFUNCTION() void Accept();
    UFUNCTION() void Decline();
};
