#pragma once
#include "CoreMinimal.h"
#include "Components/Slider.h"
#include "ForbiddenSettingsControls.generated.h"
class UTextBlock;
class UForbiddenProfile;
class UUserWidget;
class UVerticalBox;
class AMVPPlayer;

UCLASS()
class DECKBUILDER_API UForbiddenSettingsSlider : public USlider
{
    GENERATED_BODY()
public:
    void Configure(FName Setting,UForbiddenProfile* Profile,UTextBlock* Label);
private:
    FName SettingID;
    TWeakObjectPtr<UForbiddenProfile> Profile;
    UPROPERTY() TObjectPtr<UTextBlock> Caption;
    UFUNCTION() void Changed(float NewValue);
};
namespace ForbiddenSettings
{
    DECKBUILDER_API float Get(const UObject* Context,FName Key,float Default=1.f);
    DECKBUILDER_API void Apply(AMVPPlayer* Player,bool Rebind=false);
    DECKBUILDER_API void AddControls(UUserWidget* Widget,UVerticalBox* Column);
    DECKBUILDER_API void AddAccessibilityControls(UUserWidget* Widget,UVerticalBox* Column);
    DECKBUILDER_API void AddInputControls(UUserWidget* Widget,UVerticalBox* Column);
    DECKBUILDER_API void ApplyGraphics(const UObject* Context);
    DECKBUILDER_API bool SetGraphicsPreset(const UObject* Context,int32 Preset);
    DECKBUILDER_API bool HandleKey(UUserWidget* Widget,FKey Key);
    DECKBUILDER_API TMap<FName,FKey> DefaultBindings();
}
