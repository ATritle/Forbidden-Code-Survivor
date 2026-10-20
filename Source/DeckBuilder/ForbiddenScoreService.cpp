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
#include "ForbiddenRiderRoster.h"

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
    ++Epoch;RunID.Reset();RunToken.Reset();Eligible=false;Submitted=Posting=false;SubmissionSummary.Reset();Result=FForbiddenRunRecord();
    if(FParse::Param(FCommandLine::Get(),TEXT("AllCardsPlaytest")) || FParse::Param(FCommandLine::Get(),TEXT("ForbiddenQA")) || FParse::Param(FCommandLine::Get(),TEXT("MVPSmoke")) || FParse::Param(FCommandLine::Get(),TEXT("ExplosionBenchmark")) || FParse::Param(FCommandLine::Get(),TEXT("ReleaseSmoke"))) {Status=TEXT("Practice / validation run: leaderboard submission disabled.");return;}
    auto* Profile=GetGameInstance()->GetSubsystem<UForbiddenProfile>();if(!Profile || !Profile->Data) return;
    auto JSON=MakeShared<FJsonObject>();JSON->SetStringField(TEXT("playerToken"),Profile->Data->PlayerToken.ToLower());JSON->SetStringField(TEXT("edition"),TEXT("windows"));JSON->SetStringField(TEXT("riderId"),Profile->Data->SelectedRider.ToString());
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
    if(FParse::Param(FCommandLine::Get(),TEXT("LeaderboardSmoke"))) return;
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
                FForbiddenScoreEntry Entry;
                if(ParseEntry(Object,Entry)) {Entry.Rank=Self->Entries.Num()+1;Self->Entries.Add(Entry);}
            }
        }
        Self->Status=TEXT("TOP 10 / Your best 3 runs per saved player profile. Higher scores automatically replace your lowest.");
        if(!Self->SubmissionSummary.IsEmpty()) Self->Status+=TEXT("\n")+Self->SubmissionSummary;++Self->Revision;
    });
}
bool UForbiddenScoreService::Submit(const FString& Input)
{
    if(!Eligible || RunID.IsEmpty() || Result.ID.IsEmpty()) {Status=TEXT("This run was not registered online, or is a practice run. Local history is retained.");++Revision;return false;}
    if(Posting || Submitted) {Status=Submitted ? TEXT("This run has already been submitted.") : TEXT("Submission in progress...");++Revision;return false;}
    const FString Name=Input.TrimStartAndEnd();FRegexMatcher Match(FRegexPattern(TEXT("^[A-Za-z0-9][A-Za-z0-9 _.\\-]{2,19}$")),Name);
    if(!Match.FindNext()) {Status=TEXT("Use 3Ã¢â‚¬â€œ20 letters, numbers, spaces, dots, underscores or hyphens.");++Revision;return false;}
    auto JSON=MakeShared<FJsonObject>();JSON->SetStringField(TEXT("runId"),RunID);JSON->SetStringField(TEXT("runToken"),RunToken);JSON->SetStringField(TEXT("username"),Name);
    JSON->SetNumberField(TEXT("kills"),Result.Kills);JSON->SetNumberField(TEXT("level"),Result.Level);JSON->SetNumberField(TEXT("seconds"),FMath::FloorToInt(Result.Seconds));JSON->SetNumberField(TEXT("challenges"),Result.Contracts);JSON->SetBoolField(TEXT("won"),Result.Victory);
    JSON->SetStringField(TEXT("riderId"),Result.RiderID.ToString());TArray<TSharedPtr<FJsonValue>> Cards;
    for(const auto& C:Result.Cards) {auto Card=MakeShared<FJsonObject>();Card->SetStringField(TEXT("id"),C.CardID.ToString());Card->SetNumberField(TEXT("level"),C.Level);Card->SetNumberField(TEXT("evolution"),C.Evolution);Cards.Add(MakeShared<FJsonValueObject>(Card));}
    JSON->SetArrayField(TEXT("cards"),Cards);
    Posting=true;Status=TEXT("Submitting your Windows score...");++Revision;
    const TWeakObjectPtr<UForbiddenScoreService> WeakThis(this);const int32 RequestEpoch=Epoch;
    Send(TEXT("/v1/scores"),JSON,[WeakThis,RequestEpoch,Name](TSharedPtr<FJsonObject> Reply,FString Error)
    {
        auto* Self=WeakThis.Get();if(!Self || Self->Epoch!=RequestEpoch) return;Self->Posting=false;
        if(!Reply) {Self->Status=Error;++Self->Revision;return;}
        Self->Submitted=true;bool Retained=false;double Place=0;Reply->TryGetBoolField(TEXT("retained"),Retained);Reply->TryGetNumberField(TEXT("personalRank"),Place);
        Self->SubmissionSummary=Retained ? FString::Printf(TEXT("Saved as personal best #%d. Your highest three runs are kept."),FMath::Clamp(int32(Place),1,3)) : TEXT("Run submitted; it did not beat your current top three. Your saved scores are unchanged.");
        Self->Status=Self->SubmissionSummary;++Self->Revision;
        if(auto* Profile=Self->GetGameInstance()->GetSubsystem<UForbiddenProfile>();Profile && Profile->Data) {Profile->Data->Username=Name;Profile->Save();}
        Self->FetchBoard();
    });return true;
}

bool UForbiddenScoreService::ParseEntry(const TSharedPtr<FJsonObject>& Object,FForbiddenScoreEntry& Entry)
{
    if(!Object.IsValid()) return false;FForbiddenScoreEntry Parsed;
    auto Number=[&](const TCHAR* Key,int32 Max,int32& Out){double N=0;if(!Object->TryGetNumberField(Key,N) || !FMath::IsFinite(N) || N<0 || N>Max || N!=FMath::FloorToDouble(N)) return false;Out=int32(N);return true;};
    if(!Object->TryGetStringField(TEXT("username"),Parsed.Username) || !Number(TEXT("score"),MAX_int32,Parsed.Score)) return false;
    Parsed.Username=Parsed.Username.Left(20);Object->TryGetStringField(TEXT("id"),Parsed.ID);
    if(!Number(TEXT("kills"),300000,Parsed.Kills) || !Number(TEXT("level"),200,Parsed.Level) || Parsed.Level<1 || !Number(TEXT("seconds"),7200,Parsed.Seconds) || !Number(TEXT("challenges"),3,Parsed.Contracts)) return false;
    Object->TryGetBoolField(TEXT("won"),Parsed.Victory);
    const TSharedPtr<FJsonObject>* Loadout=nullptr;
    if(Object->TryGetObjectField(TEXT("loadout"),Loadout) && Loadout && Loadout->IsValid())
    {
        FString Rider;if(!(*Loadout)->TryGetStringField(TEXT("riderId"),Rider) || !ForbiddenRiders::Find(FName(*Rider))) return false;Parsed.RiderID=FName(*Rider);
        const TArray<TSharedPtr<FJsonValue>>* Cards=nullptr;if(!(*Loadout)->TryGetArrayField(TEXT("cards"),Cards) || Cards->IsEmpty() || Cards->Num()>16) return false;
        TSet<FName> Seen;
        for(const auto& Row:*Cards)
        {
            const auto Card=Row->AsObject();FString ID;double Rank=0,Evolution=0;
            if(!Card || !Card->TryGetStringField(TEXT("id"),ID) || !UForbiddenProfile::ProgramIDs().Contains(FName(*ID)) || Seen.Contains(FName(*ID)) || !Card->TryGetNumberField(TEXT("level"),Rank) || !Card->TryGetNumberField(TEXT("evolution"),Evolution)
                || !FMath::IsFinite(Rank) || !FMath::IsFinite(Evolution) || Rank<1 || Rank>3 || Evolution<0 || Evolution>2 || Rank!=FMath::FloorToDouble(Rank) || Evolution!=FMath::FloorToDouble(Evolution) || (Evolution>0 && Rank!=3)) return false;
            FForbiddenCardSnapshot Snapshot;Snapshot.CardID=FName(*ID);Snapshot.Level=int32(Rank);Snapshot.Evolution=int32(Evolution);Parsed.Cards.Add(Snapshot);Seen.Add(Snapshot.CardID);
        }
    }
    Entry=MoveTemp(Parsed);return true;
}
