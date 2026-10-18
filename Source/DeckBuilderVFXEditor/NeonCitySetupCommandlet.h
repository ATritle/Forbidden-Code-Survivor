#pragma once
#include "Commandlets/Commandlet.h"
#include "NeonCitySetupCommandlet.generated.h"
UCLASS()
class UNeonCitySetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UNeonCitySetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
