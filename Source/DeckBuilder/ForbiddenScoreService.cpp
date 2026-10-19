#include "ForbiddenScoreService.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Internationalization/Regex.h"

namespace {const FString ScoreURL=TEXT("https://forbidden-code-survivor-scores.brisk-cloud-6489.chatgpt.site");}
void UForbiddenScoreService::Send(const FString& Route,TSharedPtr<FJsonObject> Payload,TFunction<void(TSharedPtr<FJsonObject>,FString)> Completion)
{
    auto Request=FHttpModule::Get().CreateRequest();Request->SetURL(ScoreURL+Route);Request->SetVerb(Payload ? TEXT("POST") : TEXT("GET"));Request->SetTimeout(10.f);
    Request->SetHeader(TEXT("Content-Type"),TEXT("application/json"));Request->SetHeader(TEXT("Origin"),TEXT("https://atritle.github.io"));
    if(Payload) {FString JSON;FJsonSerializer::Serialize(Payload.ToSharedRef(),TJsonWriterFactory<>::Create(&JSON));Request->SetContentAsString(JSON);}
    auto Callback=MakeShared<TFunction<void(TSharedPtr<FJsonObject>,FString)>>(MoveTemp(Completion));auto Done=MakeShared<bool>(false);
    Request->OnProcessRequestComplete().BindLambda([Callback,Done](FHttpRequestPtr,FHttpResponsePtr Response,bool Success)
    {
        if(*Done) return;*Done=true;TSharedPtr<FJsonObject> JSON;
        if(!Success || !Response || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),JSON) || !JSON.IsValid()) {(*Callback)(nullptr,TEXT("Score service unavailable. Your run remains in local history."));return;}
        if(Response->GetResponseCode()<200 || Response->GetResponseCode()>=300) {FString Error;if(!JSON->TryGetStringField(TEXT("error"),Error)) Error=TEXT("The score service rejected this request.");(*Callback)(nullptr,Error.Left(200));return;}
        (*Callback)(JSON,FString());
    });
    if(!Request->ProcessRequest() && !*Done) {*Done=true;(*Callback)(nullptr,TEXT("Network request could not start."));}
}
void UForbiddenScoreService::BeginRun()
{
    ++Epoch;RunID.Reset();RunToken.Reset();Eligible=false;Submitted=Posting=false;Result=FForbiddenRunRecord();
    if(FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) || FParse::Param(FCommandLine::Get(),TEXT("ReleaseSmoke"))) {Status=TEXT("Practice / validation run: leaderboard submission disabled.");return;}
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    auto JSON=MakeShared<FJsonObject>();JSON->SetStringField(TEXT("playerToken"),Profile->Data->PlayerToken.ToLower());JSON->SetStringField(TEXT("edition"),TEXT("windows"));
    const TWeakObjectPtr<UForbiddenScoreService> WeakThis(this);const int32 RequestEpoch=Epoch;
    Send(TEXT("/v1/runs"),JSON,[WeakThis,RequestEpoch](TSharedPtr<FJsonObject> Reply,FString Error)
    {
        auto* Self=WeakThis.Get();if(!Self || Self->Epoch!=RequestEpoch) return;
        if(!Reply) {Self->Status=Error;++Self->Revision;return;}
        if(Reply->TryGetStringField(TEXT("runId"),Self->RunID) && Reply->TryGetStringField(TEXT("runToken"),Self->RunToken)) {Self->Eligible=Self->Result.ID.IsEmpty() || Self->Result.Seconds>=2;Self->Status=TEXT("Windows run registered.");++Self->Revision;}
    });
}
void UForbiddenScoreService::FinishRun(const FForbiddenRunRecord& Record)
{
    Result=Record;if(Record.Seconds<2) {Eligible=false;Status=TEXT("Survive at least two seconds to submit a score.");}++Revision;
}
void UForbiddenScoreService::FetchBoard()
{
    if(Loading) return;Loading=true;const TWeakObjectPtr<UForbiddenScoreService> WeakThis(this);
    Send(TEXT("/v1/leaderboard?edition=windows"),nullptr,[WeakThis](TSharedPtr<FJsonObject> Reply,FString Error)
    {
        auto* Self=WeakThis.Get();if(!Self) return;Self->Loading=false;
        if(!Reply) {Self->Status=Error;++Self->Revision;return;}
        const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
        if(Reply->TryGetArrayField(TEXT("entries"),Rows))
        {
            Self->Entries.Reset();for(const auto& Row:*Rows)
            {
                auto Object=Row->AsObject();if(!Object.IsValid() || Self->Entries.Num()>=10) continue;
                FForbiddenScoreEntry Entry;double Value=0;
                if(!Object->TryGetStringField(TEXT("username"),Entry.Username) || !Object->TryGetNumberField(TEXT("score"),Value)) continue;
                Entry.Score=int32(FMath::Clamp(int64(Value),int64(0),int64(MAX_int32)));Entry.Username=Entry.Username.Left(20);
                Entry.Rank=Self->Entries.Num()+1;if(Object->TryGetNumberField(TEXT("kills"),Value)) Entry.Kills=FMath::Max(0,int32(Value));Self->Entries.Add(Entry);
            }
        }
        Self->Status=TEXT("Windows edition / top 10 / up to 3 scores per local player identity.");++Self->Revision;
    });
}
bool UForbiddenScoreService::Submit(const FString& Input)
{
    if(!Eligible || RunID.IsEmpty() || Result.ID.IsEmpty()) {Status=TEXT("This run was not registered online, or is a practice run. Local history is retained.");++Revision;return false;}
    if(Posting || Submitted) {Status=Submitted ? TEXT("This run has already been submitted.") : TEXT("Submission in progress...");++Revision;return false;}
    const FString Name=Input.TrimStartAndEnd();FRegexMatcher Match(FRegexPattern(TEXT("^[A-Za-z0-9][A-Za-z0-9 _.\\-]{2,19}$")),Name);
    if(!Match.FindNext()) {Status=TEXT("Use 3–20 letters, numbers, spaces, dots, underscores or hyphens.");++Revision;return false;}
    auto JSON=MakeShared<FJsonObject>();JSON->SetStringField(TEXT("runId"),RunID);JSON->SetStringField(TEXT("runToken"),RunToken);JSON->SetStringField(TEXT("username"),Name);
    JSON->SetNumberField(TEXT("kills"),Result.Kills);JSON->SetNumberField(TEXT("level"),Result.Level);JSON->SetNumberField(TEXT("seconds"),FMath::FloorToInt(Result.Seconds));JSON->SetNumberField(TEXT("challenges"),Result.Contracts);JSON->SetBoolField(TEXT("won"),Result.Victory);
    Posting=true;Status=TEXT("Submitting your Windows score...");++Revision;
    const TWeakObjectPtr<UForbiddenScoreService> WeakThis(this);const int32 RequestEpoch=Epoch;
    Send(TEXT("/v1/scores"),JSON,[WeakThis,RequestEpoch,Name](TSharedPtr<FJsonObject> Reply,FString Error)
    {
        auto* Self=WeakThis.Get();if(!Self || Self->Epoch!=RequestEpoch) return;Self->Posting=false;
        if(!Reply) {Self->Status=Error;++Self->Revision;return;}
        Self->Submitted=true;Self->Status=TEXT("Score submitted. Only your three highest distinct runs are retained.");++Self->Revision;
        if(auto* Profile=Self->GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) {Profile->Data->Username=Name;Profile->Save();}
        Self->FetchBoard();
    });return true;
}
