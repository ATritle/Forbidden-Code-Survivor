#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EnemyVisualRow.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimInstance;

UENUM(BlueprintType)
enum class EEnemyVisualType : uint8 { Static, Skeletal };

/** One row per imported monster. Soft references must be cooked and preloaded. */
USTRUCT(BlueprintType)
struct CODEXRUNTIME_API FEnemyVisualRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    EEnemyVisualType VisualType = EEnemyVisualType::Skeletal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<UStaticMesh> StaticMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftClassPtr<UAnimInstance> AnimationClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FTransform VisualOffset = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gameplay", meta=(ClampMin="1"))
    float MaxHealth = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gameplay", meta=(ClampMin="0"))
    float MoveSpeed = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gameplay", meta=(ClampMin="1"))
    float CollisionRadius = 35.f;
};
