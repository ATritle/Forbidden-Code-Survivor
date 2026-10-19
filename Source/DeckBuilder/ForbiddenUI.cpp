#include "ForbiddenUI.h"
#include "Components/Border.h"
#include "Materials/MaterialInstanceDynamic.h"
void ForbiddenUI::Glass(UBorder* Border,FVector2D Size,FLinearColor Accent)
{
    if(!Border) return;
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/NeonGlass/M_NeonGlass_UMG.M_NeonGlass_UMG"));if(!Base) return;
    auto* Skin=UMaterialInstanceDynamic::Create(Base,Border);Skin->SetVectorParameterValue("PanelSize",FLinearColor(Size.X,Size.Y,0,0));Skin->SetVectorParameterValue("Rect",FLinearColor(0.15f,0.15f,0.7f,0.7f));
    Skin->SetVectorParameterValue("Accent",Accent);Skin->SetVectorParameterValue("Control",FLinearColor(0,1,1,0));Border->SetBrushFromMaterial(Skin);Border->SetBrushColor(FLinearColor::White);
}
