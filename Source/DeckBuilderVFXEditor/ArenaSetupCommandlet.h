#pragma once
#include "Commandlets/Commandlet.h"
#include "ArenaSetupCommandlet.generated.h"
UCLASS()
class UArenaSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UArenaSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
