#include "BloodSurfaceDirector.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ABloodSurfaceDirector::ABloodSurfaceDirector() { PrimaryActorTick.bCanEverTick=true; }
void ABloodSurfaceDirector::BeginPlay()
{
    Super::BeginPlay();
    MaskTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Art/Spells/T_BloodSplatter.T_BloodSplatter"));
    PoolTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Art/Spells/T_BloodPool.T_BloodPool"));
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Spells/M_BloodFloor.M_BloodFloor"));
    if (!Base) return;
    // Index zero is the common ground; each raised surface owns an isolated mask.
    FBloodSurface Floor; Floor.Origin=FVector2D(-9500,-9500);
    Surfaces.Add(Floor);
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        if (!It->ActorHasTag(TEXT("MortisTerrainSurface"))) continue;
        if (auto* Mesh=It->FindComponentByClass<UStaticMeshComponent>())
        {
            FBloodSurface S; S.Surface=Mesh;
            const FBox B=Mesh->Bounds.GetBox(); S.Origin=FVector2D(B.Min.X,B.Min.Y); S.Size=FVector2D(B.Max.X-B.Min.X,B.Max.Y-B.Min.Y);
            Surfaces.Add(S);
        }
    }
    for (int32 I=0;I<Surfaces.Num();++I)
    {
        auto& S=Surfaces[I]; const int32 Resolution=I==0 ? 2048 : 512;
        S.Mask=UKismetRenderingLibrary::CreateRenderTarget2D(this,Resolution,Resolution,RTF_RGBA8,FLinearColor::Black,false);
        S.Material=UMaterialInstanceDynamic::Create(Base,this);
        S.Material->SetTextureParameterValue(TEXT("BloodMask"),S.Mask);
        S.Material->SetVectorParameterValue(TEXT("PaintBounds"),FLinearColor(S.Origin.X,S.Origin.Y,S.Size.X,S.Size.Y));
        S.Material->SetVectorParameterValue(TEXT("PaintSize"),FLinearColor(S.Size.X,S.Size.Y,0,0));
        if (S.Surface) S.Surface->SetMaterial(0,S.Material);
    }
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
        for (auto* Mesh:Meshes)
            if (Mesh->GetName()==TEXT("Floor") || Mesh->GetName()==TEXT("SlatePavers")) Mesh->SetMaterial(0,Surfaces[0].Material);
    }
    auto* Effect=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Art/Spells/NS_BloodImpact.NS_BloodImpact"));
    for (int32 I=0;I<12;++I)
    {
        auto* FX=NewObject<UNiagaraComponent>(this); AddInstanceComponent(FX);
        FX->SetAutoActivate(false); FX->SetAsset(Effect); FX->RegisterComponent();
        Bursts.Add(FX); Ages.Add(0.f);
    }
}
void ABloodSurfaceDirector::Splatter(FVector Impact)
{
    if (!Impact.ContainsNaN() && Pending.Num()<32) Pending.Add(Impact);
    for (int32 I=0;I<Bursts.Num();++I) if (Ages[I]<=0.f)
    { Bursts[I]->SetWorldLocation(Impact); Bursts[I]->Activate(true); Ages[I]=1.5f; break; }
}
void ABloodSurfaceDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    for (int32 I=0;I<Ages.Num();++I) if (Ages[I]>0.f && (Ages[I]-=Dt)<=0.f) Bursts[I]->DeactivateImmediate();
    for (int32 I=0;I<4 && !Pending.IsEmpty();++I) Paint(Pending.Pop(EAllowShrinking::No));
}
void ABloodSurfaceDirector::Paint(FVector Impact)
{
    if (Surfaces.IsEmpty() || !MaskTexture) return;
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,Impact+FVector(0,0,10),Impact-FVector(0,0,2000),FCollisionObjectQueryParams(ECC_WorldStatic)) || Hit.ImpactNormal.Z<0.65f) return;
    int32 Index=Surfaces.IndexOfByPredicate([&](const FBloodSurface& S){return S.Surface==Hit.GetComponent();});
    if (Index==INDEX_NONE) Index=0;
    auto& S=Surfaces[Index];
    UCanvas* Canvas=nullptr; FVector2D Size; FDrawToRenderTargetContext Context;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this,S.Mask,Canvas,Size,Context);
    if (Canvas)
    {
        const FVector2D UV=(FVector2D(Hit.ImpactPoint.X,Hit.ImpactPoint.Y)-S.Origin)/S.Size;
        const FVector2D Extent=FVector2D(220,220)/S.Size*Size;
        Canvas->K2_DrawTexture(MaskTexture,UV*Size-Extent*0.5,Extent,FVector2D::ZeroVector,FVector2D(1,1),FLinearColor::White,BLEND_Additive,FMath::FRandRange(0.f,360.f),FVector2D(0.5,0.5));
        if(PoolTexture) Canvas->K2_DrawTexture(PoolTexture,UV*Size-Extent*0.35,Extent*0.7,FVector2D::ZeroVector,FVector2D(1,1),FLinearColor::White,BLEND_Additive,0.f,FVector2D(0.5,0.5));
        ++PaintedImpacts;
    }
    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this,Context);
}
