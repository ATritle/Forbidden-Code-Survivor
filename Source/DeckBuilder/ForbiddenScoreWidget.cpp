#include "ForbiddenScoreWidget.h"
#include "NeonMenuAudio.h"
#include "ForbiddenUI.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenScoreService.h"
#include "ForbiddenRiderRoster.h"
#include "CardDataAsset.h"
#include "CodexDeckComponent.h"
#include "MVPGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/Border.h"
#include "Engine/Texture.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"

namespace
{
    const FLinearColor Cyan(.08f,.87f,1),White(.87f,.94f,1),Muted(.48f,.64f,.76f),Gold(1,.68f,.08f);
    UTextBlock* Label(UWidgetTree* Tree,const FString& Copy,int32 Size,FLinearColor Color=White)
    {
        auto* Text=Tree->ConstructWidget<UTextBlock>();Text->SetText(FText::FromString(Copy));Text->SetAutoWrapText(true);
        auto Font=Text->GetFont();Font.Size=Size;Text->SetFont(Font);Text->SetColorAndOpacity(Color);return Text;
    }
    USizeBox* Sized(UWidgetTree* Tree,float Width,float Height=0)
    {auto* Box=Tree->ConstructWidget<USizeBox>();Box->SetWidthOverride(Width);if(Height>0) Box->SetHeightOverride(Height);return Box;}
    UImage* Art(UWidgetTree* Tree,UTexture* Texture,FVector2D Size)
    {auto* Image=Tree->ConstructWidget<UImage>();FSlateBrush Brush;Brush.SetResourceObject(Texture);Brush.ImageSize=Size;Image->SetBrush(Brush);return Image;}
}
void UForbiddenScoreRowButton::Configure(int32 Index,TFunction<void(int32)> Callback)
{
    EntryIndex=Index;OnInspect=MoveTemp(Callback);OnHovered.AddUniqueDynamic(this,&ThisClass::Hovered);OnClicked.AddUniqueDynamic(this,&ThisClass::Clicked);
}
void UForbiddenScoreRowButton::Hovered() {if(OnInspect) OnInspect(EntryIndex);}
void UForbiddenScoreRowButton::Clicked() {NeonMenuAudio::Input(this);Hovered();}

void UForbiddenScoreWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);Scores=GetGameInstance()->GetSubsystem<UForbiddenScoreService>();
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Blur=WidgetTree->ConstructWidget<UBackgroundBlur>();Blur->SetBlurStrength(10);auto* Placement=Root->AddChildToOverlay(Blur);Placement->SetHorizontalAlignment(HAlign_Fill);Placement->SetVerticalAlignment(VAlign_Fill);
    auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);Placement=Root->AddChildToOverlay(Fit);Placement->SetPadding(FMargin(22));Placement->SetHorizontalAlignment(HAlign_Fill);Placement->SetVerticalAlignment(VAlign_Fill);
    auto* Size=Sized(WidgetTree,1200,680);Fit->SetContent(Size);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();Frame->SetPadding(FMargin(20));ForbiddenUI::Glass(Frame,FVector2D(1200,680),Cyan);Size->SetContent(Frame);
    auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();Frame->SetContent(Columns);
    auto* LeftSize=Sized(WidgetTree,560);Columns->AddChildToHorizontalBox(LeftSize)->SetPadding(FMargin(0,0,24,0));
    auto* Left=WidgetTree->ConstructWidget<UVerticalBox>();LeftSize->SetContent(Left);
    Left->AddChildToVerticalBox(Label(WidgetTree,TEXT("WINDOWS / HIGH SCORES"),22,Cyan))->SetPadding(FMargin(0,0,0,8));
    Left->AddChildToVerticalBox(Label(WidgetTree,TEXT("Hover a run to inspect its rider and final build.\nTOP 10 GLOBAL / YOUR BEST 3 RUNS"),12,Muted))->SetPadding(FMargin(0,0,0,12));
    auto* Heading=WidgetTree->ConstructWidget<UHorizontalBox>();Left->AddChildToVerticalBox(Heading);
    for(const auto& Cell:TArray<TPair<FString,float>>{{TEXT("#"),36},{TEXT("PLAYER"),210},{TEXT("SCORE"),155},{TEXT("KILLS"),95}})
    {auto* CellSize=Sized(WidgetTree,Cell.Value);CellSize->SetContent(Label(WidgetTree,Cell.Key,11,Cyan));Heading->AddChildToHorizontalBox(CellSize);}
    auto* BoardSize=Sized(WidgetTree,560,346);Board=WidgetTree->ConstructWidget<UVerticalBox>();BoardSize->SetContent(Board);Left->AddChildToVerticalBox(BoardSize)->SetPadding(FMargin(0,6));
    Status=Label(WidgetTree,TEXT("Loading..."),11,Muted);Left->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,2,0,10));
    if(auto* Game=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();Game && Game->bRunEnded)
    {
        Left->AddChildToVerticalBox(Label(WidgetTree,FString::Printf(TEXT("YOUR RUN / %d POINTS"),Game->RunRecord.Score),15,Gold));
        Username=WidgetTree->ConstructWidget<UEditableTextBox>();Username->SetHintText(FText::FromString(TEXT("Username (3–20 characters)")));
        auto InputStyle=Username->GetWidgetStyle();InputStyle.SetFont(FCoreStyle::GetDefaultFontStyle("Regular",14));InputStyle.SetForegroundColor(White);
        InputStyle.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor(.008f,.025f,.05f)));InputStyle.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(.015f,.07f,.11f)));InputStyle.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(.02f,.12f,.17f)));InputStyle.Padding=FMargin(10,7);Username->SetWidgetStyle(InputStyle);
        if(auto* P=GetGameInstance()->GetSubsystem<UForbiddenProfile>();P && P->Data) Username->SetText(FText::FromString(P->Data->Username));Left->AddChildToVerticalBox(Username)->SetPadding(FMargin(0,5));
    }
    auto* Controls=WidgetTree->ConstructWidget<UHorizontalBox>();Left->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0,8));
    auto Button=[&](const TCHAR* Copy,float Width)
    {
        auto* B=WidgetTree->ConstructWidget<UForbiddenChoiceButton>();B->Configure(0,{});B->bClosingFeedback=FString(Copy).StartsWith(TEXT("RETURN"));
        auto* T=Label(WidgetTree,Copy,12,Cyan);T->SetAutoWrapText(false);T->SetJustification(ETextJustify::Center);B->SetContent(T);B->SetBackgroundColor(FLinearColor(.02f,.12f,.2f));
        auto* BSize=Sized(WidgetTree,Width,34);BSize->SetContent(B);Controls->AddChildToHorizontalBox(BSize)->SetPadding(FMargin(0,0,8,0));return B;
    };
    if(Username) Button(TEXT("SUBMIT SCORE"),170)->OnClicked.AddDynamic(this,&ThisClass::Submit);
    Button(TEXT("REFRESH"),130)->OnClicked.AddDynamic(this,&ThisClass::Refresh);Button(TEXT("RETURN / ESC"),170)->OnClicked.AddDynamic(this,&ThisClass::Close);
    auto* DetailSize=Sized(WidgetTree,576);Columns->AddChildToHorizontalBox(DetailSize);
    auto* DetailFrame=WidgetTree->ConstructWidget<UBorder>();DetailFrame->SetPadding(FMargin(16));DetailFrame->SetBrushColor(FLinearColor(.006f,.014f,.03f,.95f));DetailSize->SetContent(DetailFrame);
    Detail=WidgetTree->ConstructWidget<UVerticalBox>();DetailFrame->SetContent(Detail);
    Detail->AddChildToVerticalBox(Label(WidgetTree,TEXT("RUN PROFILE / SELECT A SCORE"),20,Cyan));Refresh();
}
void UForbiddenScoreWidget::RebuildBoard()
{
    if(!Board || !Scores) return;Board->ClearChildren();Rows.Reset();InspectedIndex=INDEX_NONE;
    if(Scores->Entries.IsEmpty())
    {
        Board->AddChildToVerticalBox(Label(WidgetTree,TEXT("No Windows scores yet.\nFinish a run and submit yours to start the new board."),15,Muted))->SetPadding(FMargin(8,30));
        Detail->ClearChildren();Detail->AddChildToVerticalBox(Label(WidgetTree,TEXT("RUN PROFILE / NO SCORES YET"),20,Cyan));Detail->AddChildToVerticalBox(Label(WidgetTree,TEXT("New entries include their rider portrait, run stats and all equipped card levels and evolutions."),14,Muted))->SetPadding(FMargin(0,18));DetailSignature.Reset();return;
    }
    for(int32 I=0;I<Scores->Entries.Num() && I<10;++I)
    {
        const auto& E=Scores->Entries[I];auto* Button=WidgetTree->ConstructWidget<UForbiddenScoreRowButton>();const TWeakObjectPtr<UForbiddenScoreWidget> Weak(this);
        Button->Configure(I,[Weak](int32 Index){if(auto* Self=Weak.Get()) Self->InspectEntry(Index);});
        auto* Cells=WidgetTree->ConstructWidget<UHorizontalBox>();Button->SetContent(Cells);
        const TArray<TPair<FString,float>> Values={{FString::Printf(TEXT("%02d"),E.Rank),36},{E.Username,210},{FString::FromInt(E.Score),155},{FString::FromInt(E.Kills),95}};
        for(int32 C=0;C<Values.Num();++C) {auto* Box=Sized(WidgetTree,Values[C].Value);auto* T=Label(WidgetTree,Values[C].Key,C==2 ? 14 : 13,C==2 ? Gold : White);T->SetAutoWrapText(false);Box->SetContent(T);Cells->AddChildToHorizontalBox(Box);}
        auto* Height=Sized(WidgetTree,560,31);Height->SetContent(Button);Board->AddChildToVerticalBox(Height)->SetPadding(FMargin(0,1));Rows.Add(Button);
    }
    InspectEntry(0);
}
void UForbiddenScoreWidget::InspectEntry(int32 Index)
{
    if(!Scores || !Scores->Entries.IsValidIndex(Index) || Index==InspectedIndex || !Detail) return;
    InspectedIndex=Index;const auto& E=Scores->Entries[Index];Detail->ClearChildren();
    for(int32 I=0;I<Rows.Num();++I) Rows[I]->SetBackgroundColor(I==Index ? FLinearColor(.015f,.24f,.31f) : FLinearColor(.008f,.035f,.07f));
    Detail->AddChildToVerticalBox(Label(WidgetTree,E.Username+TEXT(" / RUN PROFILE"),17,Cyan))->SetPadding(FMargin(0,0,0,10));
    const auto* Rider=ForbiddenRiders::Find(E.RiderID);
    if(!Rider || E.Cards.IsEmpty()) {DetailSignature=TEXT("LEGACY / BUILD UNAVAILABLE");Detail->AddChildToVerticalBox(Label(WidgetTree,DetailSignature,16,Muted));return;}
    auto* Profile=WidgetTree->ConstructWidget<UHorizontalBox>();Detail->AddChildToVerticalBox(Profile);
    auto* PortraitSize=Sized(WidgetTree,74,112);Profile->AddChildToHorizontalBox(PortraitSize)->SetPadding(FMargin(0,0,16,0));
    if(auto* Owner=GetOwningPlayer<AMVPController>()) if(auto* HUD=Owner->GetHUD<AMVPHUD>()) if(auto* Texture=HUD->GetWorkbenchPortrait(E.RiderID)) PortraitSize->SetContent(Art(WidgetTree,Texture,FVector2D(74,111)));
    auto* RiderText=WidgetTree->ConstructWidget<UVerticalBox>();auto* RiderWidth=Sized(WidgetTree,430);RiderWidth->SetContent(RiderText);Profile->AddChildToHorizontalBox(RiderWidth);
    RiderText->AddChildToVerticalBox(Label(WidgetTree,Rider->Name,23,Rider->Color));RiderText->AddChildToVerticalBox(Label(WidgetTree,Rider->Role,12,Cyan))->SetPadding(FMargin(0,4));
    RiderText->AddChildToVerticalBox(Label(WidgetTree,Rider->Description,11,Muted));
    Detail->AddChildToVerticalBox(Label(WidgetTree,FString::Printf(TEXT("PLAYER LV %d / %02d:%02d / %d KILLS / %s\n%d POINTS / %d CONTRACTS"),E.Level,E.Seconds/60,E.Seconds%60,E.Kills,E.Victory ? TEXT("VICTORY") : TEXT("DEFEAT"),E.Score,E.Contracts),12,White))->SetPadding(FMargin(0,8));
    Detail->AddChildToVerticalBox(Label(WidgetTree,FString::Printf(TEXT("FINAL LOADOUT / %d CARDS"),E.Cards.Num()),12,Cyan))->SetPadding(FMargin(0,0,0,6));
    auto* Grid=WidgetTree->ConstructWidget<UUniformGridPanel>();Grid->SetSlotPadding(FMargin(3));Detail->AddChildToVerticalBox(Grid);
    auto* Game=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();TArray<UCardDataAsset*> Library;if(Game && Game->GetArenaPlayer() && Game->GetArenaPlayer()->Deck) for(UCardDataAsset* Definition:Game->GetArenaPlayer()->Deck->UnlockedCards) Library.Add(Definition);
    DetailSignature=E.RiderID.ToString();
    for(int32 I=0;I<E.Cards.Num();++I)
    {
        const auto& C=E.Cards[I];UCardDataAsset* Card=nullptr;for(auto* Candidate:Library) if(Candidate && Candidate->CardID==C.CardID) {Card=Candidate;break;}
        auto* Tile=WidgetTree->ConstructWidget<UBorder>();Tile->SetPadding(FMargin(5));Tile->SetBrushColor(FLinearColor(.015f,.045f,.075f));
        auto* TileSize=Sized(WidgetTree,120,82);Tile->SetContent(TileSize);auto* Stack=WidgetTree->ConstructWidget<UVerticalBox>();TileSize->SetContent(Stack);
        if(Card && Card->Icon) {auto* ImageSize=Sized(WidgetTree,120,36);auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);ImageSize->SetContent(Fit);Fit->SetContent(Art(WidgetTree,Card->Icon,FVector2D(64)));Stack->AddChildToVerticalBox(ImageSize);}
        auto* CardName=Label(WidgetTree,Card ? Card->Name.ToString() : C.CardID.ToString(),10,White);Stack->AddChildToVerticalBox(CardName);
        Stack->AddChildToVerticalBox(Label(WidgetTree,FString::Printf(TEXT("LV %d%s"),C.Level,C.Evolution>0 ? TEXT(" / EVOLVED") : TEXT("")),11,C.Evolution>0 ? Gold : Cyan));
        const FString Evolution=Card && C.Evolution>0 ? (C.Evolution==1 ? Card->EvolutionA.ToString() : Card->EvolutionB.ToString()) : TEXT("BASE PROGRAM");Tile->SetToolTipText(FText::FromString((Card ? Card->Name.ToString() : C.CardID.ToString())+FString::Printf(TEXT(" / LEVEL %d\n"),C.Level)+Evolution));
        Grid->AddChildToUniformGrid(Tile,I/4,I%4);DetailSignature+=FString::Printf(TEXT(" / %s:%d:%d"),*C.CardID.ToString(),C.Level,C.Evolution);
    }
}
void UForbiddenScoreWidget::NativeTick(const FGeometry& Geometry,float Dt)
{
    Super::NativeTick(Geometry,Dt);if(!Scores) return;
    if(SeenRevision!=Scores->Revision) {SeenRevision=Scores->Revision;RebuildBoard();Status->SetText(FText::FromString(Scores->Status));}
    for(int32 I=0;I<Rows.Num();++I) if(Rows[I]->HasKeyboardFocus()) {InspectEntry(I);break;}
}
void UForbiddenScoreWidget::Submit() {if(Scores && Username) Scores->Submit(Username->GetText().ToString());}
void UForbiddenScoreWidget::Refresh() {if(Scores) Scores->FetchBoard();}
void UForbiddenScoreWidget::Close()
{
    NeonMenuAudio::Exit(this);auto* PC=GetOwningPlayer<AMVPController>();auto* G=GetWorld()->GetAuthGameMode<AMVPArenaGameMode>();RemoveFromParent();
    if(PC) {PC->ScoreWidget=nullptr;PC->SetMenuInput(!G || G->bInCodex || G->bRunEnded || G->bChoosingCard);PC->SetPause(WasPaused || !G || G->bInCodex || G->bRunEnded || G->bChoosingCard);}
}
FReply UForbiddenScoreWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right) {if(!Event.IsRepeat()) Close();return FReply::Handled();}
    if((!Username || !Username->HasKeyboardFocus()) && !Rows.IsEmpty() && (Event.GetKey()==EKeys::Up || Event.GetKey()==EKeys::Down || Event.GetKey()==EKeys::Gamepad_DPad_Up || Event.GetKey()==EKeys::Gamepad_DPad_Down))
    {const bool Up=Event.GetKey()==EKeys::Up || Event.GetKey()==EKeys::Gamepad_DPad_Up;const int32 Next=(FMath::Max(0,InspectedIndex)+(Up ? Rows.Num()-1 : 1))%Rows.Num();Rows[Next]->SetKeyboardFocus();InspectEntry(Next);return FReply::Handled();}
    return Super::NativeOnKeyDown(Geometry,Event);
}
FReply UForbiddenScoreWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right) {if(!Event.IsRepeat()) Close();return FReply::Handled();}return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
