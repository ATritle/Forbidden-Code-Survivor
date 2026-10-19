#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForbiddenProfile.h"
#include "ForbiddenScoreService.generated.h"
class FJsonObject;
struct FForbiddenScoreEntry {int32 Rank=0,Score=0,Kills=0;FString Username;};

/** Run credentials exist only in memory. Public submissions require an explicit username action. */
UCLASS()
class DECKBUILDER_API UForbiddenScoreService : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    void BeginRun();
    void FinishRun(const FForbiddenRunRecord& Record);
    void FetchBoard();
    bool Submit(const FString& Username);
    FString Status;
    TArray<FForbiddenScoreEntry> Entries;
    int32 Revision=0;
    bool Eligible=false,Posting=false,Submitted=false,Loading=false;
private:
    FString RunID,RunToken;
    FForbiddenRunRecord Result;
    int32 Epoch=0;
    void Send(const FString& Route,TSharedPtr<FJsonObject> Body,TFunction<void(TSharedPtr<FJsonObject>,FString)> Completion);
};
