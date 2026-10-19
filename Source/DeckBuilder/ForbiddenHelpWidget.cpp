#include "ForbiddenHelpWidget.h"
#include "ForbiddenChoiceWidget.h"
#include "ForbiddenUI.h"
#include "NeonMenuAudio.h"
#include "MVPGame.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

namespace
{
    const FLinearColor Cyan(.04f,.85f,1), White(.87f,.94f,1), Muted(.55f,.7f,.8f);
    UTextBlock* Label(UWidgetTree* Tree,const FString& Text,int32 Size,FLinearColor Color)
    {
        auto* L=Tree->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(Text));
        auto Font=L->GetFont();Font.Size=Size;L->SetFont(Font);L->SetColorAndOpacity(Color);L->SetAutoWrapText(true);return L;
    }
}

void UForbiddenHelpWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();SetIsFocusable(true);Entries=ForbiddenHelp::BuildEntries(this);
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.002f,.004f,.012f,.97f));
    auto* ShadeSlot=Root->AddChildToOverlay(Shade);ShadeSlot->SetHorizontalAlignment(HAlign_Fill);ShadeSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);
    auto* Placement=Root->AddChildToOverlay(Fit);Placement->SetHorizontalAlignment(HAlign_Fill);Placement->SetVerticalAlignment(VAlign_Fill);Placement->SetPadding(FMargin(30));
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1360);Size->SetHeightOverride(790);Fit->SetContent(Size);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();Frame->SetPadding(FMargin(26));Frame->SetBrushColor(FLinearColor(.012f,.025f,.045f,1));Size->SetContent(Frame);
    ForbiddenUI::Glass(Frame,FVector2D(1360,790),Cyan);
    auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Frame->SetContent(Column);
    auto* Header=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0,0,0,15));
    Header->AddChildToHorizontalBox(Label(WidgetTree,TEXT("HOW TO PLAY  /  RIDER FIELD GUIDE"),27,Cyan))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto MakeButton=[&](const FString& Text,int32 Index,TFunction<void(int32)> Callback,int32 Font=15)
    {
        auto* B=WidgetTree->ConstructWidget<UForbiddenChoiceButton>();B->Configure(Index,MoveTemp(Callback));B->SetBackgroundColor(FLinearColor(.025f,.07f,.12f));
        B->bClosingFeedback=Text.StartsWith(TEXT("RETURN"));
        auto* Pad=WidgetTree->ConstructWidget<UBorder>();Pad->SetBrushColor(FLinearColor::Transparent);Pad->SetPadding(FMargin(12,10));Pad->SetContent(Label(WidgetTree,Text,Font,White));B->SetContent(Pad);return B;
    };
    Header->AddChildToHorizontalBox(MakeButton(TEXT("RETURN  [ESC / B]"),0,[this](int32){Close();}));
    auto* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0,0,0,14));
    const auto& Topics=ForbiddenHelp::Topics();
    for(int32 I=0;I<Topics.Num();++I)
    {
        auto* B=MakeButton(Topics[I],I,[this](int32 Index){SelectTopic(Index);},14);TopicButtons.Add(B);
        auto* TabSlot=Tabs->AddChildToHorizontalBox(B);TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));TabSlot->SetPadding(FMargin(2,0));
    }
    auto* Main=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Main)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* ListSize=WidgetTree->ConstructWidget<USizeBox>();ListSize->SetWidthOverride(266);Main->AddChildToHorizontalBox(ListSize)->SetPadding(FMargin(0,0,22,0));
    EntryScroll=WidgetTree->ConstructWidget<UScrollBox>();EntryScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);ListSize->SetContent(EntryScroll);
    EntryList=WidgetTree->ConstructWidget<UVerticalBox>();EntryScroll->AddChild(EntryList);
    ReadingScroll=WidgetTree->ConstructWidget<UScrollBox>();Main->AddChildToHorizontalBox(ReadingScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Reading=WidgetTree->ConstructWidget<UVerticalBox>();ReadingScroll->AddChild(Reading);
    Heading=Label(WidgetTree,TEXT(""),25,White);Reading->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,0,0,12));
    auto* ImageSize=WidgetTree->ConstructWidget<USizeBox>();ImageSize->SetHeightOverride(290);Reading->AddChildToVerticalBox(ImageSize);
    auto* ImageFit=WidgetTree->ConstructWidget<UScaleBox>();ImageFit->SetStretch(EStretch::ScaleToFit);ImageSize->SetContent(ImageFit);
    Illustration=WidgetTree->ConstructWidget<UImage>();ImageFit->SetContent(Illustration);
    Caption=Label(WidgetTree,TEXT(""),12,Muted);Reading->AddChildToVerticalBox(Caption)->SetPadding(FMargin(0,8,0,15));
    Body=Label(WidgetTree,TEXT(""),18,White);Reading->AddChildToVerticalBox(Body)->SetPadding(FMargin(0,0,14,20));
    auto* Footer=WidgetTree->ConstructWidget<UHorizontalBox>();Column->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0,14,0,0));
    Footer->AddChildToHorizontalBox(MakeButton(TEXT("< PREV"),-1,[this](int32 D){Step(D);}));
    Counter=Label(WidgetTree,TEXT(""),13,Muted);auto* CounterSlot=Footer->AddChildToHorizontalBox(Counter);CounterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CounterSlot->SetPadding(FMargin(18,10));
    Footer->AddChildToHorizontalBox(MakeButton(TEXT("NEXT >"),1,[this](int32 D){Step(D);}));
    SelectTopic(0);
}

void UForbiddenHelpWidget::SelectTopic(int32 Index)
{
    if(!ForbiddenHelp::Topics().IsValidIndex(Index)) return;Topic=Index;EntryList->ClearChildren();EntryButtons.Reset();
    for(int32 I=0;I<TopicButtons.Num();++I) TopicButtons[I]->SetBackgroundColor(I==Topic ? FLinearColor(.02f,.35f,.48f) : FLinearColor(.025f,.07f,.12f));
    int32 First=INDEX_NONE;
    for(int32 I=0;I<Entries.Num();++I) if(Entries[I].Topic==Topic)
    {
        if(First==INDEX_NONE) First=I;
        auto* B=WidgetTree->ConstructWidget<UForbiddenChoiceButton>();B->Configure(I,[this](int32 N){SelectEntry(N);});
        auto* Pad=WidgetTree->ConstructWidget<UBorder>();Pad->SetBrushColor(FLinearColor::Transparent);Pad->SetPadding(FMargin(14,13));
        Pad->SetContent(Label(WidgetTree,Entries[I].Title,15,White));B->SetContent(Pad);
        EntryList->AddChildToVerticalBox(B)->SetPadding(FMargin(0,0,0,5));EntryButtons.Add(B);
    }
    EntryScroll->ScrollToStart();if(First!=INDEX_NONE) SelectEntry(First);
}
void UForbiddenHelpWidget::SelectEntry(int32 Index)
{
    if(!Entries.IsValidIndex(Index)) return;
    if(Entries[Index].Topic!=Topic) SelectTopic(Entries[Index].Topic);
    Selected=Index;const auto& E=Entries[Index];Heading->SetText(FText::FromString(E.Title));Body->SetText(FText::FromString(E.Copy));Caption->SetText(FText::FromString(E.Caption));
    CurrentImage=LoadObject<UTexture>(nullptr,*E.ImagePath);
    Illustration->SetBrushFromTexture(Cast<UTexture2D>(CurrentImage),true);
    ReadingScroll->SetScrollOffset(0);
    int32 Row=0;for(int32 I=0;I<Entries.Num();++I) if(Entries[I].Topic==Topic)
    {
        UForbiddenChoiceButton* B=EntryButtons[Row++];B->SetBackgroundColor(I==Index ? FLinearColor(.015f,.3f,.38f) : FLinearColor(.02f,.055f,.09f));
        if(I==Index) EntryScroll->ScrollWidgetIntoView(B,false);
    }
    Counter->SetText(FText::FromString(FString::Printf(TEXT("%02d / %02d   |   ARROWS: PAGES   LB/RB: TOPICS   PG UP/DN: READ"),Index+1,Entries.Num())));
}
void UForbiddenHelpWidget::Step(int32 Direction) {NeonMenuAudio::Navigate(this);SelectEntry((Selected+Direction+Entries.Num())%FMath::Max(1,Entries.Num()));}
void UForbiddenHelpWidget::Close() {if(auto* PC=GetOwningPlayer<AMVPController>()) PC->CloseHelp();}
FReply UForbiddenHelpWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const FKey Key=Event.GetKey();
    if(Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Right) {if(!Event.IsRepeat()) Close();return FReply::Handled();}
    if(Key==EKeys::Right || Key==EKeys::Down || Key==EKeys::Gamepad_DPad_Right || Key==EKeys::Gamepad_DPad_Down) {Step(1);return FReply::Handled();}
    if(Key==EKeys::Left || Key==EKeys::Up || Key==EKeys::Gamepad_DPad_Left || Key==EKeys::Gamepad_DPad_Up) {Step(-1);return FReply::Handled();}
    if(Key==EKeys::Gamepad_LeftShoulder || Key==EKeys::Gamepad_RightShoulder) {NeonMenuAudio::Navigate(this);SelectTopic((Topic+(Key==EKeys::Gamepad_LeftShoulder ? 6 : 1))%7);return FReply::Handled();}
    if(Key==EKeys::PageDown || Key==EKeys::PageUp) {ReadingScroll->SetScrollOffset(FMath::Max(0.f,ReadingScroll->GetScrollOffset()+(Key==EKeys::PageDown ? 240.f : -240.f)));return FReply::Handled();}
    // Prevent gameplay shortcuts reaching the controller while browsing the guide.
    if(Key==EKeys::Enter || Key==EKeys::One || Key==EKeys::Two || Key==EKeys::Three || Key==EKeys::R || Key==EKeys::H || Key==EKeys::L || Key==EKeys::F1) return FReply::Handled();
    return Super::NativeOnKeyDown(Geometry,Event);
}
