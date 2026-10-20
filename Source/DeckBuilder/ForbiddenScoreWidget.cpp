#include "ForbiddenScoreWidget.h"
#include "NeonMenuAudio.h"
#include "ForbiddenUI.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenScoreService.h"
#include "MVPGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "InputCoreTypes.h"

void UForbiddenScoreWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);Scores=GetGameInstance()->GetSubsystem<UForbiddenScoreService>();
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(10);
    auto* Placement=Root->AddChildToOverlay(Blur);Placement->SetHorizontalAlignment(HAlign_Fill);Placement->SetVerticalAlignment(VAlign_Fill);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();Frame->SetBrushColor(FLinearColor(0.006f,0.012f,0.03f,0.98f));Frame->SetPadding(FMargin(32));
    ForbiddenUI::Glass(Frame,FVector2D(720,700),FLinearColor(0.05f,0.8f,1));
    Placement=Root->AddChildToOverlay(Frame);Placement->SetHorizontalAlignment(HAlign_Center);Placement->SetVerticalAlignment(VAlign_Center);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(720);Frame->SetContent(Size);auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Size->SetContent(Column);
    auto Label=[&](const FString& Copy,int32 Font)
    {auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Copy));T->SetAutoWrapText(true);auto F=T->GetFont();F.Size=Font;T->SetFont(F);T->SetColorAndOpacity(FLinearColor(0.3f,0.88f,1));Column->AddChildToVerticalBox(T)->SetPadding(FMargin(0,8));return T;};
    Label(TEXT("FORBIDDEN CODE / WINDOWS LEADERBOARD"),22);Board=Label(TEXT("Loading..."),16);Status=Label(TEXT(""),12);
    auto* Game=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());
    if(Game && Game->bRunEnded)
    {
        Label(FString::Printf(TEXT("YOUR RUN / %d POINTS"),Game->RunRecord.Score),18);
        Username=WidgetTree->ConstructWidget<UEditableTextBox>();Username->SetHintText(FText::FromString(TEXT("USERNAME / 3–20 characters / public when submitted")));
        if(auto* P=GetGameInstance()->GetSubsystem<UForbiddenProfile>();P && P->Data) Username->SetText(FText::FromString(P->Data->Username));Column->AddChild(Username);
    }
    auto Button=[&](const TCHAR* Copy)
    {auto* B=WidgetTree->ConstructWidget<UForbiddenChoiceButton>();B->Configure(0,{});B->bClosingFeedback=FString(Copy).StartsWith(TEXT("RETURN"));auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Copy));T->SetJustification(ETextJustify::Center);B->SetContent(T);B->SetBackgroundColor(FLinearColor(0.02f,0.12f,0.2f));Column->AddChildToVerticalBox(B)->SetPadding(FMargin(0,8));return B;};
    if(Username) Button(TEXT("SUBMIT SCORE"))->OnClicked.AddDynamic(this,&ThisClass::Submit);
    Button(TEXT("REFRESH TOP 10"))->OnClicked.AddDynamic(this,&ThisClass::Refresh);Button(TEXT("RETURN / SKIP SUBMISSION / ESC"))->OnClicked.AddDynamic(this,&ThisClass::Close);Refresh();
}
void UForbiddenScoreWidget::NativeTick(const FGeometry& Geometry,float Dt)
{
    Super::NativeTick(Geometry,Dt);if(!Scores || SeenRevision==Scores->Revision) return;SeenRevision=Scores->Revision;
    FString Text;for(const auto& E:Scores->Entries) Text+=FString::Printf(TEXT("%02d   %s    /    %d POINTS    /    %d KILLS\n"),E.Rank,*E.Username,E.Score,E.Kills);
    if(Text.IsEmpty()) Text=TEXT("No Windows scores yet. Finish a run to post yours.");Board->SetText(FText::FromString(Text));Status->SetText(FText::FromString(Scores->Status));
}
void UForbiddenScoreWidget::Submit() {if(Scores && Username) Scores->Submit(Username->GetText().ToString());}
void UForbiddenScoreWidget::Refresh() {if(Scores) Scores->FetchBoard();}
void UForbiddenScoreWidget::Close()
{
    NeonMenuAudio::Exit(this);
    auto* PC=GetOwningPlayer<AMVPController>();auto* G=Cast<AMVPArenaGameMode>(GetWorld()->GetAuthGameMode());RemoveFromParent();
    if(PC) {PC->ScoreWidget=nullptr;PC->SetMenuInput(!G || G->bInCodex || G->bRunEnded || G->bChoosingCard);PC->SetPause(WasPaused || !G || G->bInCodex || G->bRunEnded || G->bChoosingCard);}
}
FReply UForbiddenScoreWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right) {if(!Event.IsRepeat()) Close();return FReply::Handled();}return Super::NativeOnKeyDown(Geometry,Event);}

FReply UForbiddenScoreWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right) {if(!Event.IsRepeat()) {Close();}return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
