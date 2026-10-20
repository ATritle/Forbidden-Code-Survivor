#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForbiddenRiderPreview.generated.h"
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UAnimSequence;
class UPointLightComponent;
class UStaticMeshComponent;
class UNecromancyVisualComponent;

/** Isolated, lit full-body turntable. Only one model and one capture are active. */
UCLASS()
class DECKBUILDER_API AForbiddenRiderPreview : public AActor
{
    GENERATED_BODY()
public:
    AForbiddenRiderPreview();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    bool SetRider(FName ID);
    void SetClip(int32 Index);
    void RotateBy(float Degrees);
    void ResetView();
    void SetPreviewVisible(bool Visible);
    UTextureRenderTarget2D* GetImage() const {return Image;}
    FName GetRider() const {return Rider;}
    int32 GetClip() const {return ClipIndex;}
    float GetYaw() const {return Yaw;}
    bool IsReady() const {return Ready;}
    bool AutoRotate=true;
private:
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Model;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Image;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Clips;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TArray<TObjectPtr<UNecromancyVisualComponent>> Ghosts;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Shield;
    FName Rider;
    int32 ClipIndex=0;
    float Yaw=335,ClipTime=0,CaptureAge=0;
    double LastTime=0,ManualUntil=0;
    bool Ready=false,Visible=false;
    FVector ModelCenter=FVector::ZeroVector;
};
