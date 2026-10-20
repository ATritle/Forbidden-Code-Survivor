#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NeonSpellStyle.h"
#include "ForbiddenCombatSubsystem.generated.h"
class AMVPEnemy;
struct FForbiddenDamageNumber
{
    float Age=-1,Amount=0;
    FVector Position=FVector::ZeroVector;
    FLinearColor Color=FLinearColor::White;
};

/** Fixed gameplay field pool. Particle rejection can never cancel scheduled damage. */
UCLASS()
class DECKBUILDER_API UForbiddenCombatSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Tick(float Dt) override;
    virtual TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(UForbiddenCombatSubsystem,STATGROUP_Tickables);}
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override {return WorldType==EWorldType::Game || WorldType==EWorldType::PIE;}
    bool QueueField(FName Program,ENeonSpellStyle Style,FVector Center,float Damage,float Radius,float Delay,int32 Pulses,float Interval,bool Expanding=false,bool Pull=false);
    void Clear();
    void RecordDamage(FVector Position,float Amount,ENeonSpellStyle Style);
    const TArray<FForbiddenDamageNumber>& GetNumbers() const {return Numbers;}
    UPROPERTY(BlueprintReadOnly) int32 AcceptedFields=0;
    UPROPERTY(BlueprintReadOnly) int32 DroppedFields=0;
private:
    struct FField
    {
        bool Active=false,Expanding=false,Pull=false;
        FName Program;
        ENeonSpellStyle Style=ENeonSpellStyle::Prism;
        FVector Position=FVector::ZeroVector;
        float Power=0,Radius=0,Age=0,Delay=0,Interval=0.35f,VisualTime=0;
        int32 Pulses=0,Done=0;
        TMap<TWeakObjectPtr<AMVPEnemy>,uint32> Seen;
    };
    TArray<FField> Fields;
    TArray<FForbiddenDamageNumber> Numbers;
    int32 NumberCursor=0;
};
