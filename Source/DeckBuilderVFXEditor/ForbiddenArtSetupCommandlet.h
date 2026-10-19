#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ForbiddenArtSetupCommandlet.generated.h"
UCLASS()
class UForbiddenArtSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UForbiddenArtSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
