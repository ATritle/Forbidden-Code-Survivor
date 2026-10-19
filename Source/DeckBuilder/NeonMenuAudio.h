#pragma once
#include "CoreMinimal.h"
namespace NeonMenuAudio
{
    void Input(const UObject* Context);
    void Exit(const UObject* Context);
    void Navigate(const UObject* Context);
    void BeginPointerFrame(const UObject* HUD);
    void Hover(const UObject* HUD,FName Key);
}
