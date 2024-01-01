// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "InputSequenceCoreEditor.h"

#include "Kismet/KismetTextLibrary.h"

#include "Factories.h"
#include "AssetTypeActions.h"
#include "AssetTypeCategories.h"
#include "InputSequence.h"

#include "InputAction.h"

#include "InputSequenceGraphSchema.h"
#include "InputSequenceGraphNodes.h"
#include "EdGraphNode_Comment.h"

#include "ConnectionDrawingPolicy.h"
#include "SLevelOfDetailBranchNode.h"
#include "SPinTypeSelector.h"
#include "SGraphActionMenu.h"
#include "KismetPins/SGraphPinExec.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "EdGraphUtilities.h"
#include "GraphEditorActions.h"
#include "Settings/EditorStyleSettings.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Commands/GenericCommands.h"

#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include <Styling/StyleColors.h>

#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "FInputSequenceCoreEditor"

const float InputFocusRadius = 2.f;
const float InputFocusThickness = 0.5f;
const FVector2D Icon8x8(8.0f, 8.0f);
const auto extention = TEXT(".png");

const FSpinBoxStyle spinBoxStyle = FSpinBoxStyle()
.SetBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, InputFocusRadius, FStyleColors::InputOutline, InputFocusThickness))
.SetHoveredBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, InputFocusRadius, FStyleColors::Hover, InputFocusThickness))
.SetActiveBackgroundBrush(FSlateRoundedBoxBrush(FStyleColors::Input, InputFocusRadius, FStyleColors::Primary, InputFocusThickness))
.SetActiveFillBrush(FSlateRoundedBoxBrush(FStyleColors::Primary, InputFocusRadius, FLinearColor::Transparent, InputFocusThickness))
.SetHoveredFillBrush(FSlateRoundedBoxBrush(FStyleColors::Hover, InputFocusRadius, FLinearColor::Transparent, InputFocusThickness))
.SetInactiveFillBrush(FSlateRoundedBoxBrush(FStyleColors::Dropdown, InputFocusRadius, FLinearColor::Transparent, InputFocusThickness))
.SetArrowsImage(FSlateNoResource())
.SetForegroundColor(FStyleColors::ForegroundHover)
.SetTextPadding(FMargin(3.f, 1.f, 3.f, 1.f))
.SetInsetPadding(FMargin(0.5));

const FCheckBoxStyle checkBoxStyle = FCheckBoxStyle()
.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
.SetUncheckedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox" + extention, Icon8x8))
.SetUncheckedHoveredImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox" + extention, Icon8x8))
.SetUncheckedPressedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Hovered" + extention, Icon8x8, FLinearColor(0.5f, 0.5f, 0.5f)))
.SetCheckedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Checked_Hovered" + extention, Icon8x8))
.SetCheckedHoveredImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Checked_Hovered" + extention, Icon8x8, FLinearColor(0.5f, 0.5f, 0.5f)))
.SetCheckedPressedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Checked" + extention, Icon8x8))
.SetUndeterminedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Undetermined" + extention, Icon8x8))
.SetUndeterminedHoveredImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Undetermined_Hovered" + extention, Icon8x8))
.SetUndeterminedPressedImage(FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate") / "Common/CheckBox_Undetermined_Hovered" + extention, Icon8x8, FLinearColor(0.5f, 0.5f, 0.5f)));

const FSlateFontInfo pinFontInfo(FCoreStyle::GetDefaultFont(), 6, "Regular");
const FSlateFontInfo pinFontInfo_Selected(FCoreStyle::GetDefaultFont(), 6, "Bold");
const FSlateFontInfo inputEventFontInfo(FCoreStyle::GetDefaultFont(), 10, "Regular");
const FSlateFontInfo inputEventFontInfo_Selected(FCoreStyle::GetDefaultFont(), 10, "Bold");
const float padding = 2;

const FName NAME_NoBorder("NoBorder");
const FString trueFlag("1");

template<class T>
void GetAssetsFromAssetRegistry(TArray<FAssetData>& outAssetDatas)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter filter;
	filter.ClassPaths.Add(T::StaticClass()->GetClassPathName());
	filter.bRecursiveClasses = true;
	filter.bRecursivePaths = true;

	AssetRegistryModule.Get().GetAssets(filter, outAssetDatas);
}

void AddPinToDynamicNode(UEdGraphNode* graphNode, FName category, FName pinName, TObjectPtr<UInputAction> inputAction)
{
	const FScopedTransaction Transaction(LOCTEXT("Transaction_AddPinToDynamicNode", "Add Pin"));
	
	graphNode->Modify();
	UEdGraphPin* graphPin = graphNode->CreatePin(EGPD_Output, category, pinName, UEdGraphNode::FCreatePinParams());
	
	if (graphPin->DefaultObject = inputAction)
	{
		UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
		UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

		inputState->Modify();
		inputState->InputActionInfos.Add(FSoftObjectPath(inputAction), FInputActionInfo());
	}

	Cast<UInputSequenceGraphNode_Dynamic>(graphNode)->OnUpdateGraphNode.ExecuteIfBound();
}

//------------------------------------------------------
// SInputSequenceGraphNode_Dynamic
//------------------------------------------------------

class SToolTip_Dummy : public SLeafWidget, public IToolTip
{
public:

	SLATE_BEGIN_ARGS(SToolTip_Dummy) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override { return LayerId; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

	virtual TSharedRef<class SWidget> AsWidget() { return SNullWidget::NullWidget; }
	virtual TSharedRef<SWidget> GetContentWidget() { return SNullWidget::NullWidget; }
	virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget) override {}
	virtual bool IsEmpty() const override { return false; }
	virtual bool IsInteractive() const { return false; }
	virtual void OnOpening() override {}
	virtual void OnClosed() override {}
};

//------------------------------------------------------
// SInputSequenceGraphNode_Dynamic
//------------------------------------------------------

class SInputSequenceGraphNode_Dynamic : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SInputSequenceGraphNode_Dynamic) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	virtual ~SInputSequenceGraphNode_Dynamic();

protected:

	virtual void CreatePinWidgets() override;
};

void SInputSequenceGraphNode_Dynamic::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	SetCursor(EMouseCursor::CardinalCross);

	GraphNode = InNode;

	Cast<UInputSequenceGraphNode_Dynamic>(GraphNode)->OnUpdateGraphNode.BindLambda([&]() { UpdateGraphNode(); });

	UpdateGraphNode();
}

SInputSequenceGraphNode_Dynamic::~SInputSequenceGraphNode_Dynamic()
{
	Cast<UInputSequenceGraphNode_Dynamic>(GraphNode)->OnUpdateGraphNode.Unbind();
}

void SInputSequenceGraphNode_Dynamic::CreatePinWidgets()
{
	// Create Pin widgets for each of the pins.
	for (int32 PinIndex = 0; PinIndex < GraphNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		if (CurPin->PinType.PinCategory == UInputSequenceGraphSchema::PC_Input) continue;

		if (!ensureMsgf(CurPin->GetOuter() == GraphNode
			, TEXT("Graph node ('%s' - %s) has an invalid %s pin: '%s'; (with a bad %s outer: '%s'); skiping creation of a widget for this pin.")
			, *GraphNode->GetNodeTitle(ENodeTitleType::ListView).ToString()
			, *GraphNode->GetPathName()
			, (CurPin->Direction == EEdGraphPinDirection::EGPD_Input) ? TEXT("input") : TEXT("output")
			, CurPin->PinFriendlyName.IsEmpty() ? *CurPin->PinName.ToString() : *CurPin->PinFriendlyName.ToString()
			, CurPin->GetOuter() ? *CurPin->GetOuter()->GetClass()->GetName() : TEXT("UNKNOWN")
			, CurPin->GetOuter() ? *CurPin->GetOuter()->GetPathName() : TEXT("NULL")))
		{
			continue;
		}

		CreateStandardPinWidget(CurPin);
	}
}

//------------------------------------------------------
// SInputSequenceParameterMenu
//------------------------------------------------------

class SInputSequenceParameterMenu : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetSectionTitle, int32);

	SLATE_BEGIN_ARGS(SInputSequenceParameterMenu) : _AutoExpandMenu(false) {}

		SLATE_ARGUMENT(bool, AutoExpandMenu)
		SLATE_EVENT(FGetSectionTitle, OnGetSectionTitle)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		this->bAutoExpandMenu = InArgs._AutoExpandMenu;

		ChildSlot
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("Menu.Background")).Padding(5)
					[
						SNew(SBox)
							.MinDesiredWidth(300)
							.MaxDesiredHeight(700) // Set max desired height to prevent flickering bug for menu larger than screen
							[
								SAssignNew(GraphMenu, SGraphActionMenu)
									.OnCollectStaticSections(this, &SInputSequenceParameterMenu::OnCollectStaticSections)
									.OnGetSectionTitle(this, &SInputSequenceParameterMenu::OnGetSectionTitle)
									.OnCollectAllActions(this, &SInputSequenceParameterMenu::CollectAllActions)
									.OnActionSelected(this, &SInputSequenceParameterMenu::OnActionSelected)
									.SortItemsRecursively(false)
									.AlphaSortItems(false)
									.AutoExpandActionMenu(bAutoExpandMenu)
									.ShowFilterTextBox(true)
							]
					]
			];
	}

	TSharedPtr<SEditableTextBox> GetSearchBox() { return GraphMenu->GetFilterTextBox(); }

protected:

	virtual void OnCollectStaticSections(TArray<int32>& StaticSectionIDs) = 0;

	virtual FText OnGetSectionTitle(int32 InSectionID) = 0;

	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) = 0;

	virtual void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) = 0;

private:

	bool bAutoExpandMenu;

	TSharedPtr<SGraphActionMenu> GraphMenu;
};

//------------------------------------------------------
// SInputSequenceParameterMenu_Pin
//------------------------------------------------------

class SInputSequenceParameterMenu_Pin : public SInputSequenceParameterMenu
{
public:
	SLATE_BEGIN_ARGS(SInputSequenceParameterMenu_Pin)
		: _AutoExpandMenu(false)
		{}
		//~ Begin Required Args
		SLATE_ARGUMENT(UEdGraphNode*, Node)
		//~ End Required Args
		SLATE_ARGUMENT(bool, AutoExpandMenu)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		this->Node = InArgs._Node;

		SInputSequenceParameterMenu::FArguments SuperArgs;
		SuperArgs._AutoExpandMenu = InArgs._AutoExpandMenu;
		SInputSequenceParameterMenu::Construct(SuperArgs);
	}

protected:

	virtual void OnCollectStaticSections(TArray<int32>& StaticSectionIDs) override { StaticSectionIDs.Add(1); }

	virtual FText OnGetSectionTitle(int32 InSectionID) override { return LOCTEXT("SInputSequenceParameterMenu_Pin_SectionTitle", "Input Actions"); }

	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override
	{
		TArray<FAssetData> assetDatas;
		GetAssetsFromAssetRegistry<UInputAction>(assetDatas);

		TArray<UInputAction*> inputActions;
		
		for (const FAssetData& assetData : assetDatas)
		{
			if (UInputAction* inputAction = Cast<UInputAction>(assetData.GetAsset()))
			{
				inputActions.Add(inputAction);
			}
		}

		for (const UEdGraphPin* pin : Node->Pins)
		{
			if (UInputAction* inputAction = Cast<UInputAction>(pin->DefaultObject))
			{
				inputActions.Remove(inputAction);
			}
		}

		inputActions.Sort([](UInputAction& a, UInputAction& b)->bool { return FNameLexicalLess().operator()(a.GetFName(), b.GetFName()); });

		TArray<TSharedPtr<FEdGraphSchemaAction>> schemaActions;

		for (UInputAction* inputAction : inputActions)
		{
			const FName inputActionName = inputAction->GetFName();

			const FText tooltip = FText::Format(LOCTEXT("SInputSequenceParameterMenu_Pin_Tooltip", "Add {0} for {1}"), FText::FromString("Action pin"), FText::FromName(inputActionName));

			TSharedPtr<FInputSequenceGraphSchemaAction_AddPin> schemaAction(
				new FInputSequenceGraphSchemaAction_AddPin(
					FText::GetEmpty()
					, FText::FromName(inputActionName)
					, tooltip
					, 0
					, 1
				)
			);

			schemaAction->InputAction = inputAction;
			schemaActions.Add(schemaAction);
		}

		for (TSharedPtr<FEdGraphSchemaAction> schemaAction : schemaActions)
		{
			TSharedPtr<FInputSequenceGraphSchemaAction_AddPin> addPinAction = StaticCastSharedPtr<FInputSequenceGraphSchemaAction_AddPin>(schemaAction);
			OutAllActions.AddAction(schemaAction);
		}
	}

	virtual void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) override
	{
		if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress)
		{
			for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
			{
				FSlateApplication::Get().DismissAllMenus();
				SelectedActions[ActionIndex]->PerformAction(Node->GetGraph(), Node->FindPin(NAME_None, EGPD_Input), FVector2D::ZeroVector);
			}
		}
	}

private:

	UEdGraphNode* Node;
};

//------------------------------------------------------
// SGraphPin_Input
//------------------------------------------------------

bool bIsSliderValueChange = 0;

class SGraphPin_Input : public SGridPanel
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_Input) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	FReply OnClicked_RemovePin() const;

	FReply OnClicked_Started() const { return SetTriggerEvent(ETriggerEvent::Started); }

	FReply OnClicked_Triggered() const { return SetTriggerEvent(ETriggerEvent::Triggered); }

	FReply OnClicked_Completed() const { return SetTriggerEvent(ETriggerEvent::Completed); }

	FReply SetTriggerEvent(const ETriggerEvent triggerEvent) const;

	FSlateColor GetTriggerEventForegroundColor_Started() const { return GetTriggerEventForegroundColor(ButtonStartedPtr, ETriggerEvent::Started); }

	FSlateColor GetTriggerEventForegroundColor_Triggered() const { return GetTriggerEventForegroundColor(ButtonTriggeredPtr, ETriggerEvent::Triggered); }

	FSlateColor GetTriggerEventForegroundColor_Completed() const { return GetTriggerEventForegroundColor(ButtonCompletedPtr, ETriggerEvent::Completed); }

	FSlateColor GetTriggerEventForegroundColor(const TSharedPtr<SButton>& buttonPtr, const ETriggerEvent triggerEvent) const;

	FSlateFontInfo GetTriggerEventFont_Started() const { return GetTriggerEventFont(ETriggerEvent::Started); }

	FSlateFontInfo GetTriggerEventFont_Triggered() const { return GetTriggerEventFont(ETriggerEvent::Triggered); }

	FSlateFontInfo GetTriggerEventFont_Completed() const { return GetTriggerEventFont(ETriggerEvent::Completed); }

	FSlateFontInfo GetTriggerEventFont(const ETriggerEvent triggerEvent) const;

	EVisibility GetTriggerEventStrongMatchVisibility_Started() const { return GetTriggerEventStrongMatchVisibility(ETriggerEvent::Started); }

	EVisibility GetTriggerEventStrongMatchVisibility_Triggered() const { return GetTriggerEventStrongMatchVisibility(ETriggerEvent::Triggered); }

	EVisibility GetTriggerEventStrongMatchVisibility_Completed() const { return GetTriggerEventStrongMatchVisibility(ETriggerEvent::Completed); }

	EVisibility GetTriggerEventStrongMatchVisibility(const ETriggerEvent triggerEvent) const;

	EVisibility GetTriggerEventPreciseMatchVisibility_Started() const { return GetTriggerEventPreciseMatchVisibility(ETriggerEvent::Started); }

	EVisibility GetTriggerEventPreciseMatchVisibility_Triggered() const { return GetTriggerEventPreciseMatchVisibility(ETriggerEvent::Triggered); }

	EVisibility GetTriggerEventPreciseMatchVisibility_Completed() const { return GetTriggerEventPreciseMatchVisibility(ETriggerEvent::Completed); }

	EVisibility GetTriggerEventPreciseMatchVisibility(const ETriggerEvent triggerEvent) const;

	FText ToolTipText_Started() const { return ToolTipText(ETriggerEvent::Started); }

	FText ToolTipText_Triggered() const { return ToolTipText(ETriggerEvent::Triggered); }

	FText ToolTipText_Completed() const { return ToolTipText(ETriggerEvent::Completed); }

	FText ToolTipText(const ETriggerEvent triggerEvent) const;

	FSlateColor GetPinTextColor() const;

	FText GetPinFriendlyName() const;

	void SetWaitTimeValue(const float NewValue) const;

	void SetWaitTimeValueCommited(const float NewValue, ETextCommit::Type CommitType) const { SetWaitTimeValue(NewValue); }

	TOptional<float> GetWaitTimeValue() const;

	void OnBeginSliderMovement() const;

	void OnEndSliderMovement(const float NewValue) const;

	TSharedPtr<SButton> ButtonStartedPtr;
	TSharedPtr<SButton> ButtonTriggeredPtr;
	TSharedPtr<SButton> ButtonCompletedPtr;

	UEdGraphPin* PinObject;
};

void SGraphPin_Input::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
	SGridPanel::Construct(SGridPanel::FArguments());

	PinObject = InPin;

	SetRowFill(0, 0);
	SetRowFill(1, 0);
	SetColumnFill(0, 1);
	SetColumnFill(1, 0);
	SetColumnFill(2, 0);

	AddSlot(0, 0).RowSpan(2).Padding(2 * padding, padding)
		[
			SNew(SBox).MinDesiredWidth(64).VAlign(VAlign_Center).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text_Raw(this, &SGraphPin_Input::GetPinFriendlyName).Font(pinFontInfo).ColorAndOpacity(this, &SGraphPin_Input::GetPinTextColor).AutoWrapText(true)
				]
		];

	AddSlot(1, 0).VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1).Padding(0, padding, 0, 0)
			[
				SAssignNew(ButtonStartedPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText_Raw(this, &SGraphPin_Input::ToolTipText_Started)
					.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_Started)
					[
						SNew(SGridPanel).FillColumn(0, 0).FillColumn(1, 0)

							+ SGridPanel::Slot(0, 0)
							[
								SNew(STextBlock).Text(FText::FromString("S"))
									.Font_Raw(this, &SGraphPin_Input::GetTriggerEventFont_Started)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Started)
							]

							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Top).HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("~"))
									.Font(pinFontInfo_Selected)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Started)
									.Visibility_Raw(this, &SGraphPin_Input::GetTriggerEventStrongMatchVisibility_Started)
							]

							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Top).HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("!"))
									.Font(pinFontInfo_Selected)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Started)
									.Visibility_Raw(this, &SGraphPin_Input::GetTriggerEventPreciseMatchVisibility_Started)
							]
					]
			]

			+ SHorizontalBox::Slot().FillWidth(1).Padding(padding, padding, 0, 0)
			[
				SAssignNew(ButtonTriggeredPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText_Raw(this, &SGraphPin_Input::ToolTipText_Triggered)
					.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_Triggered)
					[
						SNew(SGridPanel).FillColumn(0, 0).FillColumn(1,0)

							+ SGridPanel::Slot(0, 0)
							[
								SNew(STextBlock).Text(FText::FromString("T"))
									.Font_Raw(this, &SGraphPin_Input::GetTriggerEventFont_Triggered)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Triggered)
							]

							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Top).HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("~"))
									.Font(pinFontInfo_Selected)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Triggered)
									.Visibility_Raw(this, &SGraphPin_Input::GetTriggerEventStrongMatchVisibility_Triggered)
							]

							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Top).HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("!"))
									.Font(pinFontInfo_Selected)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Triggered)
									.Visibility_Raw(this, &SGraphPin_Input::GetTriggerEventPreciseMatchVisibility_Triggered)
							]
					]
			]

			+ SHorizontalBox::Slot().FillWidth(1).Padding(padding, padding, 0, 0)
			[
				SAssignNew(ButtonCompletedPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
					.Cursor(EMouseCursor::Hand)
					.ToolTipText_Raw(this, &SGraphPin_Input::ToolTipText_Completed)
					.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_Completed)
					[
						SNew(SGridPanel).FillColumn(0, 0).FillColumn(1, 0)

						+SGridPanel::Slot(0, 0)
							[
								SNew(STextBlock).Text(FText::FromString("C"))
									.Font_Raw(this, &SGraphPin_Input::GetTriggerEventFont_Completed)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Completed)
							]

							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Top).HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("~"))
									.Font(pinFontInfo_Selected)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Completed)
									.Visibility_Raw(this, &SGraphPin_Input::GetTriggerEventStrongMatchVisibility_Completed)
							]

							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Top).HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("!"))
									.Font(pinFontInfo_Selected)
									.ColorAndOpacity_Raw(this, &SGraphPin_Input::GetTriggerEventForegroundColor_Completed)
									.Visibility_Raw(this, &SGraphPin_Input::GetTriggerEventPreciseMatchVisibility_Completed)
							]
					]
			]
		];

	AddSlot(2, 0).RowSpan(2).VAlign(VAlign_Center).Padding(2 * padding, padding)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
				.ForegroundColor(FLinearColor::White)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_RemovePin", "Click to remove pin"))
				.OnClicked_Raw(this, &SGraphPin_Input::OnClicked_RemovePin)
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
				]
		];

	AddSlot(1, 1).VAlign(VAlign_Center).Padding(0, 0, 0, padding)
		[
			SNew(SNumericEntryBox<float>).SpinBoxStyle(&spinBoxStyle)
				.Font(pinFontInfo)
				.ToolTipText(LOCTEXT("SGraphPin_Input_TooltipText_WaitTime", "Wait Time"))
				.AllowSpin(true)
				.MinValue(0)
				.MaxValue(10)
				.MinSliderValue(0)
				.MaxSliderValue(10)
				.Delta(0.01f)
				.OnBeginSliderMovement(this, &SGraphPin_Input::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &SGraphPin_Input::OnEndSliderMovement)
				.Value(this, &SGraphPin_Input::GetWaitTimeValue)
				.OnValueChanged(this, &SGraphPin_Input::SetWaitTimeValue)
				.OnValueCommitted(this, &SGraphPin_Input::SetWaitTimeValueCommited)
		];

	SetToolTip(SNew(SToolTip_Dummy));
}

FReply SGraphPin_Input::OnClicked_RemovePin() const
{
	UEdGraphPin* graphPin = PinObject;
	UEdGraphNode* graphNode = graphPin->GetOwningNode();
	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
	UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

	const FScopedTransaction Transaction(LOCTEXT("Transaction_SGraphPin_Input::OnClicked_RemovePin", "Remove Pin"));

	////// TODO Maybe store zombie?
	//////for (TMap<FSoftObjectPath, FInputActionInfo>::TIterator It(inputState->InputActionInfos); It; ++It)
	//////{
	//////	if ((*It).Key.TryLoad() == nullptr)
	//////	{
	//////		It.RemoveCurrent();
	//////	}
	//////}

	inputState->Modify();
	inputState->InputActionInfos.Remove(Cast<UInputAction>(graphPin->DefaultObject));

	graphNode->Modify();
	graphPin->Modify();
	graphNode->RemovePin(graphPin);

	Cast<UInputSequenceGraphNode_Dynamic>(graphNode)->OnUpdateGraphNode.ExecuteIfBound();

	return FReply::Handled();
}

FReply SGraphPin_Input::SetTriggerEvent(const ETriggerEvent triggerEvent) const
{
	if (PinObject && PinObject->DefaultObject)
	{
		UEdGraphNode* graphNode = PinObject->GetOwningNode();
		UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
		UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

		const FScopedTransaction Transaction(LOCTEXT("Transaction_SGraphPin_Input_SetTriggerEvent", "Trigger Event"));

		inputState->Modify();

		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].TriggerEvent != triggerEvent)
		{
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch = false;
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch = false;

			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].TriggerEvent = triggerEvent;
		}
		else if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch)
		{
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch = false;
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch = false;
		}
		else if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch)
		{
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch = true;
		}
		else
		{
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch = true;
		}
	}

	return FReply::Handled();
}

FSlateColor SGraphPin_Input::GetTriggerEventForegroundColor(const TSharedPtr<SButton>& buttonPtr, const ETriggerEvent triggerEvent) const
{
	const bool isHovered = buttonPtr->IsHovered();

	FLinearColor color = isHovered ? FLinearColor::White : FLinearColor(1, 1, 1, 0.2f);

	if (PinObject && PinObject->DefaultObject)
	{
		UEdGraphNode* graphNode = PinObject->GetOwningNode();
		UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
		UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].TriggerEvent == triggerEvent)
		{
			color = FLinearColor::Green;
		}
	}

	return color;
}

FSlateFontInfo SGraphPin_Input::GetTriggerEventFont(const ETriggerEvent triggerEvent) const
{
	if (PinObject && PinObject->DefaultObject)
	{
		UEdGraphNode* graphNode = PinObject->GetOwningNode();
		UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
		UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].TriggerEvent == triggerEvent)
		{
			return inputEventFontInfo_Selected;
		}
	}

	return inputEventFontInfo;
}

EVisibility SGraphPin_Input::GetTriggerEventStrongMatchVisibility(const ETriggerEvent triggerEvent) const
{
	if (PinObject && PinObject->DefaultObject)
	{
		UEdGraphNode* graphNode = PinObject->GetOwningNode();
		UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
		UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].TriggerEvent == triggerEvent)
		{
			if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch &&
				!inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch)
			{
				return EVisibility::Visible;
			}
		}
	}

	return EVisibility::Hidden;
}

EVisibility SGraphPin_Input::GetTriggerEventPreciseMatchVisibility(const ETriggerEvent triggerEvent) const
{
	if (PinObject && PinObject->DefaultObject)
	{
		UEdGraphNode* graphNode = PinObject->GetOwningNode();
		UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
		UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].TriggerEvent == triggerEvent)
		{
			if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch)
			{
				return EVisibility::Visible;
			}
		}
	}

	return EVisibility::Hidden;
}

FText SGraphPin_Input::ToolTipText(const ETriggerEvent triggerEvent) const
{
	UEdGraphNode* graphNode = PinObject->GetOwningNode();
	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
	UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

	switch (triggerEvent)
	{
	case ETriggerEvent::Started:
	{
		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch)
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Started_P", "Started (Precise)");
		}
		else if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch)
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Started_S", "Started (Strong)");
		}
		else
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Started", "Started");
		}
	}
	break;

	case ETriggerEvent::Triggered:
	{
		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch)
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Triggered_P", "Triggered (Precise)");
		}
		else if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch)
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Triggered_S", "Triggered (Strong)");
		}
		else
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Triggered", "Triggered");
		}
	}
	break;

	case ETriggerEvent::Completed:
	{
		if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequirePreciseMatch)
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Completed_P", "Completed (Precise)");
		}
		else if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].bRequireStrongMatch)
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Completed_S", "Completed (Strong)");
		}
		else
		{
			return LOCTEXT("SGraphPin_Input_TooltipText_Completed", "Completed");
		}
	}
	break;
	}

	return FText::GetEmpty();
}

FSlateColor SGraphPin_Input::GetPinTextColor() const { return PinObject && PinObject->DefaultObject.IsResolved() && PinObject->DefaultObject.operator bool() ? FLinearColor::White : FLinearColor::Red; }

FText SGraphPin_Input::GetPinFriendlyName() const { return PinObject && PinObject->DefaultObject ? FText::FromString(PinObject->DefaultObject->GetName()) : LOCTEXT("SGraphPin_Input_PinFriendlyName", "???"); }

void SGraphPin_Input::SetWaitTimeValue(const float NewValue) const
{
	UEdGraphNode* graphNode = PinObject->GetOwningNode();
	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
	UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

	if (inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].WaitTime != NewValue)
	{
		if (!bIsSliderValueChange)
		{
			const FScopedTransaction Transaction(LOCTEXT("Transaction_SGraphPin_Input::SetWaitTimeValue", "Wait Time"));

			inputState->Modify();
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].WaitTime = NewValue;
		}
		else
		{
			inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].WaitTime = NewValue;
		}
	}
}

TOptional<float> SGraphPin_Input::GetWaitTimeValue() const
{
	UEdGraphNode* graphNode = PinObject->GetOwningNode();
	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
	UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

	return inputState->InputActionInfos[Cast<UInputAction>(PinObject->DefaultObject)].WaitTime;
}

void SGraphPin_Input::OnBeginSliderMovement() const
{
	bIsSliderValueChange = 1;

	UEdGraphNode* graphNode = PinObject->GetOwningNode();
	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
	UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(inputSequence->GetState(graphNode->NodeGuid));

	const FScopedTransaction Transaction(LOCTEXT("Transaction_SGraphPin_Input::OnBeginSliderMovement", "Wait Time"));

	inputState->Modify();
	// Change will be during slider movement
}

void SGraphPin_Input::OnEndSliderMovement(const float NewValue) const
{
	bIsSliderValueChange = 0;
}

//------------------------------------------------------
// SInputSequenceGraphNode_Input
//------------------------------------------------------

class SInputSequenceGraphNode_Input : public SInputSequenceGraphNode_Dynamic
{
protected:

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;

	TSharedRef<SWidget> OnGetAddButtonMenuContent();

protected:

	TSharedPtr<SComboButton> AddButton;
};

void SInputSequenceGraphNode_Input::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	TSharedPtr<SVerticalBox> innerVerticalBox = SNew(SVerticalBox);

	MainBox->AddSlot().AutoHeight()[innerVerticalBox.ToSharedRef()];

	TArray<UEdGraphPin*> orderedPins;

	// Create Pin widgets for each of the pins.
	for (int32 PinIndex = 0; PinIndex < GraphNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* CurPin = GraphNode->Pins[PinIndex];

		if (CurPin->PinType.PinCategory == UInputSequenceGraphSchema::PC_Input)
		{
			orderedPins.Add(CurPin);
		}
	}

	orderedPins.Sort([](UEdGraphPin& a, UEdGraphPin& b)->bool { return FNameLexicalLess().operator()(a.DefaultObject->GetFName(), b.DefaultObject->GetFName()); });

	for (UEdGraphPin* CurPin : orderedPins)
	{
		if (CurPin->PinType.PinCategory == UInputSequenceGraphSchema::PC_Input)
		{
			innerVerticalBox->AddSlot().AutoHeight()[SNew(SGraphPin_Input, CurPin)];
		}
	}

	MainBox->AddSlot().AutoHeight().Padding(padding).HAlign(HAlign_Right)
		[
			SAssignNew(AddButton, SComboButton)
				.HasDownArrow(false)
				.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
				.ForegroundColor(FLinearColor::White)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.ToolTipText(LOCTEXT("SInputSequenceGraphNode_Input_Add_ToolTipText", "Click to add new pin"))
				.OnGetMenuContent(this, &SInputSequenceGraphNode_Input::OnGetAddButtonMenuContent)
				.ButtonContent()
				[
					SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
		];
}

TSharedRef<SWidget> SInputSequenceGraphNode_Input::OnGetAddButtonMenuContent()
{
	TSharedRef<SInputSequenceParameterMenu_Pin> MenuWidget = SNew(SInputSequenceParameterMenu_Pin).Node(GraphNode);

	AddButton->SetMenuContentWidgetToFocus(MenuWidget->GetSearchBox());

	return MenuWidget;
}

//------------------------------------------------------
// SInputSequenceGraphNode_Hub
//------------------------------------------------------

class SInputSequenceGraphNode_Hub : public SInputSequenceGraphNode_Dynamic
{
protected:

	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;

	FReply OnClickedAddButton();
};

void SInputSequenceGraphNode_Hub::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	MainBox->AddSlot().HAlign(HAlign_Right)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
				.ForegroundColor(FLinearColor::White)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.ToolTipText(LOCTEXT("SInputSequenceGraphNode_Hub_ToolTipText", "Click to add new pin"))
				.OnClicked_Raw(this, &SInputSequenceGraphNode_Hub::OnClickedAddButton)
				[
					SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
		];
}

FReply SInputSequenceGraphNode_Hub::OnClickedAddButton()
{
	int32 outputPinsCount = 1;
	
	for (UEdGraphPin* pin : GraphNode->Pins)
	{
		if (pin->Direction == EGPD_Output) outputPinsCount++;
	}

	AddPinToDynamicNode(GraphNode, UInputSequenceGraphSchema::PC_Exec, FName(FString::FromInt(outputPinsCount)), nullptr);

	return FReply::Handled();
}

//------------------------------------------------------
// SGraphPin_HubExec
//------------------------------------------------------

class SGraphPin_HubExec : public SGraphPinExec
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_HubExec) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:
	
	FReply OnClicked_RemovePin() const;
};

void SGraphPin_HubExec::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
	SGraphPin::FArguments InArgs = SGraphPin::FArguments();

	bUsePinColorForText = InArgs._UsePinColorForText;
	this->SetCursor(EMouseCursor::Default);

	SetVisibility(MakeAttributeSP(this, &SGraphPin_HubExec::GetPinVisiblity));

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	checkf(
		Schema, 
		TEXT("Missing schema for pin: %s with outer: %s of type %s"), 
		*(GraphPinObj->GetName()),
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetName()) : TEXT("NULL OUTER"), 
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetClass()->GetName()) : TEXT("NULL OUTER")
	);
	
	// Create the pin icon widget
	TSharedRef<SWidget> PinWidgetRef = SPinTypeSelector::ConstructPinTypeImage(
		MakeAttributeSP(this, &SGraphPin_HubExec::GetPinIcon),
		MakeAttributeSP(this, &SGraphPin_HubExec::GetPinColor),
		MakeAttributeSP(this, &SGraphPin_HubExec::GetSecondaryPinIcon),
		MakeAttributeSP(this, &SGraphPin_HubExec::GetSecondaryPinColor));
	PinImage = PinWidgetRef;

	PinWidgetRef->SetCursor(
		TAttribute<TOptional<EMouseCursor::Type> >::Create(
			TAttribute<TOptional<EMouseCursor::Type> >::FGetter::CreateRaw(this, &SGraphPin_HubExec::GetPinCursor)
		)
	);

	// Create the pin indicator widget (used for watched values)
	TSharedRef<SWidget> PinStatusIndicator =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.Visibility(this, &SGraphPin_HubExec::GetPinStatusIconVisibility)
		.ContentPadding(0)
		.OnClicked(this, &SGraphPin_HubExec::ClickedOnPinStatusIcon)
		[
			SNew(SImage).Image(this, &SGraphPin_HubExec::GetPinStatusIcon)
		];

	TSharedRef<SWidget> LabelWidget = GetLabelWidget(InArgs._PinLabelStyle);

	// Create the widget used for the pin body (status indicator, label, and value)
	LabelAndValue =
		SNew(SWrapBox)
		.PreferredSize(150.f);

	LabelAndValue->AddSlot()
		.VAlign(VAlign_Center)
		[
			PinStatusIndicator
		];

	LabelAndValue->AddSlot()
		.VAlign(VAlign_Center)
		[
			LabelWidget
		];

	TSharedPtr<SHorizontalBox> PinContent;
	// Output pin
	FullPinHorizontalRowWidget = PinContent = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			LabelAndValue.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(InArgs._SideToSideMargin, 0, 0, 0)
		[
			PinWidgetRef
		];

	// Set up a hover for pins that is tinted the color of the pin.
	
	SBorder::Construct(SBorder::FArguments().BorderImage(FAppStyle::GetBrush(NAME_NoBorder))
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SLevelOfDetailBranchNode)
						.UseLowDetailSlot(this, &SGraphPin_HubExec::UseLowDetailPinNames)
						.LowDetail()
						[
							SNullWidget::NullWidget
						]
						.HighDetail()
						[
							SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
								.ForegroundColor(GetPinColor())
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.Cursor(EMouseCursor::Hand)
								.ToolTipText(LOCTEXT("SGraphPin_HubExec_TooltipText_RemovePin", "Click to remove pin"))
								.OnClicked_Raw(this, &SGraphPin_HubExec::OnClicked_RemovePin)
								[
									SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
								]
						]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
						.BorderImage(this, &SGraphPin_HubExec::GetPinBorder)
						.BorderBackgroundColor(this, &SGraphPin_HubExec::GetHighlightColor)
						.OnMouseButtonDown(this, &SGraphPin_HubExec::OnPinNameMouseDown)
						[
							SNew(SBorder)
								.BorderImage(CachedImg_Pin_DiffOutline)
								.BorderBackgroundColor(this, &SGraphPin_HubExec::GetPinDiffColor)
								[
									SNew(SLevelOfDetailBranchNode)
										.UseLowDetailSlot(this, &SGraphPin_HubExec::UseLowDetailPinNames)
										.LowDetail()
										[
											//@TODO: Try creating a pin-colored line replacement that doesn't measure text / call delegates but still renders
											PinWidgetRef
										]
										.HighDetail()
										[
											PinContent.ToSharedRef()
										]
								]
						]
				]
		]
	);

	SetToolTip(SNew(SToolTip_Dummy));

	CachePinIcons();
}

FReply SGraphPin_HubExec::OnClicked_RemovePin() const
{
	UEdGraphPin* graphPin = GetPinObj();
	UEdGraphNode* graphNode = graphPin->GetOwningNode();

	const FScopedTransaction Transaction(LOCTEXT("Transaction_SGraphPin_HubExec::OnClicked_RemovePin", "Remove Pin"));

	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();
	
	if (graphPin->HasAnyConnections())
	{
		inputSequence->Modify();
		inputSequence->RemoveNextStateId(graphNode->NodeGuid, graphPin->LinkedTo[0]->GetOwningNode()->NodeGuid);
	}

	graphNode->Modify();
	graphPin->Modify();

	int nextGraphPinIndex = graphNode->Pins.IndexOfByKey(graphPin) + 1;

	if (graphNode->Pins.IsValidIndex(nextGraphPinIndex))
	{
		for (size_t i = nextGraphPinIndex; i < graphNode->Pins.Num(); i++)
		{
			if (graphNode->Pins[i]->Direction == EGPD_Output && graphNode->Pins[i]->PinType.PinCategory == UInputSequenceGraphSchema::PC_Exec)
			{
				graphNode->Pins[i]->Modify();
				graphNode->Pins[i]->PinName = FName(FString::FromInt(i - 1));
			}
		}
	}

	graphNode->RemovePin(graphPin);

	Cast<UInputSequenceGraphNode_Dynamic>(graphNode)->OnUpdateGraphNode.ExecuteIfBound();

	return FReply::Handled();
}

//------------------------------------------------------
// FInputSequenceGraphNodeFactory
//------------------------------------------------------

TSharedPtr<SGraphNode> FInputSequenceGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UInputSequenceGraphNode_Input* inputGraphNode = Cast<UInputSequenceGraphNode_Input>(InNode))
	{
		return SNew(SInputSequenceGraphNode_Input, inputGraphNode);
	}
	else if (UInputSequenceGraphNode_Hub* hubGraphNode = Cast<UInputSequenceGraphNode_Hub>(InNode))
	{
		return SNew(SInputSequenceGraphNode_Hub, hubGraphNode);
	}

	return nullptr;
}

//------------------------------------------------------
// FInputSequenceGraphPinFactory
//------------------------------------------------------

TSharedPtr<SGraphPin> FInputSequenceGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (InPin->GetSchema()->IsA<UInputSequenceGraphSchema>())
	{
		if (InPin->PinType.PinCategory == UInputSequenceGraphSchema::PC_Exec)
		{
			if (InPin->Direction == EEdGraphPinDirection::EGPD_Output && InPin->GetOwningNode() && InPin->GetOwningNode()->IsA<UInputSequenceGraphNode_Hub>())
			{
				return SNew(SGraphPin_HubExec, InPin);
			}
			else
			{
				TSharedPtr<SGraphPin> sGraphPin = SNew(SGraphPinExec, InPin);
				sGraphPin->SetToolTip(SNew(SToolTip_Dummy));
				return sGraphPin;
			}
		}

		return FGraphPanelPinFactory::CreatePin(InPin);
	}

	return nullptr;
}

//------------------------------------------------------
// FInputSequenceGraphPinConnectionFactory
//------------------------------------------------------

class FInputSequenceConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FInputSequenceConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
		, GraphObj(InGraphObj)
	{}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

		if (OutputPin->PinType.PinCategory == UInputSequenceGraphSchema::PC_Exec)
		{
			Params.WireThickness = 4;
		}
		else
		{
			Params.bUserFlag1 = true;
		}

		const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
		if (bDeemphasizeUnhoveredPins)
		{
			ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
		}
	}

	virtual void DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params) override
	{
		DrawConnection(
			WireLayerID,
			StartPoint,
			EndPoint,
			Params);

		// Draw the arrow
		if (ArrowImage != nullptr && Params.bUserFlag1)
		{
			FVector2D ArrowPoint = EndPoint - ArrowRadius;

			FSlateDrawElement::MakeBox(
				DrawElementsList,
				ArrowLayerID,
				FPaintGeometry(ArrowPoint, ArrowImage->ImageSize * ZoomFactor, ZoomFactor),
				ArrowImage,
				ESlateDrawEffect::None,
				Params.WireColor
			);
		}
	}

protected:
	UEdGraph* GraphObj;
	TMap<UEdGraphNode*, int32> NodeWidgetMap;
};

FConnectionDrawingPolicy* FInputSequenceGraphPinConnectionFactory::CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	if (Schema->IsA<UInputSequenceGraphSchema>())
	{
		return new FInputSequenceConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);
	}

	return nullptr;
}

//------------------------------------------------------
// UInputSequenceGraph
//------------------------------------------------------

UInputSequenceGraph::UInputSequenceGraph(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	Schema = UInputSequenceGraphSchema::StaticClass();
}

//------------------------------------------------------
// FInputSequenceGraphSchemaAction_NewComment
//------------------------------------------------------

UEdGraphNode* FInputSequenceGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	// Add menu item for creating comment boxes
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2D SpawnLocation = Location;

	CommentTemplate->SetBounds(SelectedNodesBounds);
	SpawnLocation.X = CommentTemplate->NodePosX;
	SpawnLocation.Y = CommentTemplate->NodePosY;

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation);
}

//------------------------------------------------------
// FInputSequenceGraphSchemaAction_NewNode
//------------------------------------------------------

UEdGraphNode* FInputSequenceGraphSchemaAction_NewNode::PerformAction(class UEdGraph* graph, UEdGraphPin* graphPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction(LOCTEXT("Transaction_FInputSequenceGraphSchemaAction_NewNode::PerformAction", "Add Node"));

		graph->Modify();

		if (graphPin)
		{
			graphPin->Modify();
		}

		NodeTemplate->SetFlags(RF_Transactional);

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, graph);
		graph->AddNode(NodeTemplate, true, bSelectNewNode);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();

		NodeTemplate->NodePosX = Location.X;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

		UInputSequence* inputSequence = graph->GetTypedOuter<UInputSequence>();

		inputSequence->Modify();

		if (UInputSequenceGraphNode_Reset* resetGraphNode = Cast<UInputSequenceGraphNode_Reset>(NodeTemplate))
		{
			inputSequence->AddState(NodeTemplate->NodeGuid, NewObject<UInputSequenceState_Reset>(inputSequence, NAME_None, RF_Transactional));
		}
		else if (UInputSequenceGraphNode_Input* inputGraphNode = Cast<UInputSequenceGraphNode_Input>(NodeTemplate))
		{
			inputSequence->AddState(NodeTemplate->NodeGuid, NewObject<UInputSequenceState_Input>(inputSequence, NAME_None, RF_Transactional));
		}
		else if (UInputSequenceGraphNode_Hub* hubGraphNode = Cast<UInputSequenceGraphNode_Hub>(NodeTemplate))
		{
			inputSequence->AddState(NodeTemplate->NodeGuid, NewObject<UInputSequenceState_Hub>(inputSequence, NAME_None, RF_Transactional));
		}

		NodeTemplate->AutowireNewNode(graphPin);

		graph->NotifyGraphChanged();

		ResultNode = NodeTemplate;
	}

	return ResultNode;
}

void FInputSequenceGraphSchemaAction_NewNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(NodeTemplate);
}

//------------------------------------------------------
// FInputSequenceGraphSchemaAction_AddPin
//------------------------------------------------------

UEdGraphNode* FInputSequenceGraphSchemaAction_AddPin::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	check(InputAction);

	AddPinToDynamicNode(FromPin->GetOwningNode(), UInputSequenceGraphSchema::PC_Input, FName(FGuid::NewGuid().ToString()), InputAction);

	return nullptr;
}

//------------------------------------------------------
// UInputSequenceGraphSchema
//------------------------------------------------------

template<class T>
TSharedPtr<T> AddNewActionAs(FGraphContextMenuBuilder& ContextMenuBuilder, const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 Grouping = 0)
{
	TSharedPtr<T> Action(new T(Category, MenuDesc, Tooltip, Grouping));
	ContextMenuBuilder.AddAction(Action);
	return Action;
}

const FName UInputSequenceGraphSchema::PC_Exec("UInputSequenceGraphSchema_PC_Exec");

const FName UInputSequenceGraphSchema::PC_Input("UInputSequenceGraphSchema_PC_Input");

void UInputSequenceGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	{
		// Add Input node
		TSharedPtr<FInputSequenceGraphSchemaAction_NewNode> Action = AddNewActionAs<FInputSequenceGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Input", "Add Input node..."), LOCTEXT("AddNode_Input_Tooltip", "A new Input node"));
		Action->NodeTemplate = NewObject<UInputSequenceGraphNode_Input>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	{
		// Add Hub node
		TSharedPtr<FInputSequenceGraphSchemaAction_NewNode> Action = AddNewActionAs<FInputSequenceGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Hub", "Add Hub node..."), LOCTEXT("AddNode_Hub_Tooltip", "A new Hub node"));
		Action->NodeTemplate = NewObject<UInputSequenceGraphNode_Hub>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	{
		// Add Reset node
		TSharedPtr<FInputSequenceGraphSchemaAction_NewNode> Action = AddNewActionAs<FInputSequenceGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Reset", "Add Reset node..."), LOCTEXT("AddNode_Reset_Tooltip", "A new Reset node"));
		Action->NodeTemplate = NewObject<UInputSequenceGraphNode_Reset>(ContextMenuBuilder.OwnerOfTemporaries);
	}
}

const FPinConnectionResponse UInputSequenceGraphSchema::CanCreateConnection(const UEdGraphPin* pinA, const UEdGraphPin* pinB) const
{
	if (pinA == nullptr || pinB == nullptr) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Pin(s)Null", "One or both of pins was null"));

	if (pinA->GetOwningNode() == pinB->GetOwningNode()) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsOfSameNode", "Both pins are on same node"));

	if (pinA->Direction == pinB->Direction) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsOfSameDirection", "Both pins have same direction (both input or both output)"));

	return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, TEXT(""));
}

bool UInputSequenceGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	const FPinConnectionResponse Response = CanCreateConnection(PinA, PinB);

	bool bModified = false;

	UEdGraphNode* nodeA = PinA->GetOwningNode();
	UEdGraphNode* nodeB = PinB->GetOwningNode();

	UInputSequence* inputSequence = nodeA->GetTypedOuter<UInputSequence>();

	inputSequence->Modify();

	switch (Response.Response)
	{
	case CONNECT_RESPONSE_MAKE:
		PinA->Modify();
		PinB->Modify();
		PinA->MakeLinkTo(PinB);
		inputSequence->AddNextStateId(nodeA->NodeGuid, nodeB->NodeGuid);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_A:
		PinA->Modify();
		PinB->Modify();
		PinA->BreakAllPinLinks(true);
		if (PinA->LinkedTo.Num() > 0)
		{
			inputSequence->RemoveNextStateId(nodeA->NodeGuid, PinA->LinkedTo[0]->GetOwningNode()->NodeGuid);
		}
		PinA->MakeLinkTo(PinB);
		inputSequence->AddNextStateId(nodeA->NodeGuid, nodeB->NodeGuid);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_B:
		PinA->Modify();
		PinB->Modify();
		PinB->BreakAllPinLinks(true);
		if (PinA->LinkedTo.Num() > 0)
		{
			inputSequence->RemoveNextStateId(nodeA->NodeGuid, PinA->LinkedTo[0]->GetOwningNode()->NodeGuid);
		}
		PinA->MakeLinkTo(PinB);
		inputSequence->AddNextStateId(nodeA->NodeGuid, nodeB->NodeGuid);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_AB:
		PinA->Modify();
		PinB->Modify();
		PinA->BreakAllPinLinks(true);
		PinB->BreakAllPinLinks(true);
		if (PinA->LinkedTo.Num() > 0)
		{
			inputSequence->RemoveNextStateId(nodeA->NodeGuid, PinA->LinkedTo[0]->GetOwningNode()->NodeGuid);
		}
		PinA->MakeLinkTo(PinB);
		inputSequence->AddNextStateId(nodeA->NodeGuid, nodeB->NodeGuid);
		bModified = true;
		break;

	case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
		bModified = CreateAutomaticConversionNodeAndConnections(PinA, PinB);
		break;

	case CONNECT_RESPONSE_MAKE_WITH_PROMOTION:
		bModified = CreatePromotedConnection(PinA, PinB);
		break;

	case CONNECT_RESPONSE_DISALLOW:
	default:
		break;
	}

#if WITH_EDITOR
	if (bModified)
	{
		PinA->GetOwningNode()->PinConnectionListChanged(PinA);
		PinB->GetOwningNode()->PinConnectionListChanged(PinB);
	}
#endif	//#if WITH_EDITOR

	return bModified;
}

void UInputSequenceGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	UEdGraphNode* graphNode = TargetPin.GetOwningNode();
	UInputSequence* inputSequence = graphNode->GetTypedOuter<UInputSequence>();

	if (TargetPin.HasAnyConnections())
	{
		const FScopedTransaction Transaction(LOCTEXT("Transaction_UInputSequenceGraphSchema::BreakPinLinks", "Break Pin Links"));

		inputSequence->Modify();

		for (UEdGraphPin* otherPin : TargetPin.LinkedTo)
		{
			inputSequence->RemoveNextStateId(graphNode->NodeGuid, otherPin->GetOwningNode()->NodeGuid);
		}

		Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);
	}
}

void UInputSequenceGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	UEdGraphNode* sourceGraphNode = SourcePin->GetOwningNode();
	UEdGraphNode* targetGraphNode = TargetPin->GetOwningNode();
	
	UInputSequence* inputSequence = sourceGraphNode->GetTypedOuter<UInputSequence>();

	inputSequence->Modify();
	inputSequence->RemoveNextStateId(sourceGraphNode->NodeGuid, targetGraphNode->NodeGuid);

	Super::BreakSinglePinLink(SourcePin, TargetPin);
}

void UInputSequenceGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	FGraphNodeCreator<UInputSequenceGraphNode_Entry> entryGraphNodeCreator(Graph);
	UInputSequenceGraphNode_Entry* entryGraphNode = entryGraphNodeCreator.CreateNode();
	entryGraphNode->NodePosX = -300;
	entryGraphNodeCreator.Finalize();
	SetNodeMetaData(entryGraphNode, FNodeMetadata::DefaultGraphNode);

	UInputSequence* inputSequence = Graph.GetTypedOuter<UInputSequence>();

	inputSequence->AddState(entryGraphNode->NodeGuid, NewObject<UInputSequenceState_Base>(inputSequence, NAME_None, RF_Transactional));

	inputSequence->AddEntryStateId(entryGraphNode->NodeGuid);
}

TSharedPtr<FEdGraphSchemaAction> UInputSequenceGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FInputSequenceGraphSchemaAction_NewComment));
}

//------------------------------------------------------
// UInputSequenceGraphNode_Entry
//------------------------------------------------------

void UInputSequenceGraphNode_Entry::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UInputSequenceGraphSchema::PC_Exec, NAME_None);
}

FText UInputSequenceGraphNode_Entry::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UInputSequenceGraphNode_Entry_NodeTitle", "Entry");
}

FLinearColor UInputSequenceGraphNode_Entry::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UInputSequenceGraphNode_Entry::GetTooltipText() const
{
	return LOCTEXT("UInputSequenceGraphNode_Entry_TooltipText", "This is Entry node of Input sequence...");
}

//------------------------------------------------------
// UInputSequenceGraphNode_Base
//------------------------------------------------------

void UInputSequenceGraphNode_Base::PrepareForCopying()
{
	Super::PrepareForCopying();

	PrevOwningAsset = GetTypedOuter<UInputSequence>();
}

void UInputSequenceGraphNode_Base::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != NULL)
	{
		if (UEdGraphPin* toPin = GetExecPin(FromPin->Direction == EGPD_Input ? EEdGraphPinDirection::EGPD_Output : EEdGraphPinDirection::EGPD_Input))
		{
			if (GetSchema()->TryCreateConnection(FromPin, toPin))
			{
				FromPin->GetOwningNode()->NodeConnectionListChanged();
			}
		}
	}
}

UEdGraphPin* UInputSequenceGraphNode_Base::GetExecPin(EEdGraphPinDirection direction) const
{
	for (UEdGraphPin* pin : Pins)
	{
		if (pin->Direction == direction && pin->PinType.PinCategory == UInputSequenceGraphSchema::PC_Exec)
		{
			return pin;
		}
	}

	return nullptr;
}

//------------------------------------------------------
// UInputSequenceGraphNode_Input
//------------------------------------------------------

void UInputSequenceGraphNode_Input::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UInputSequenceGraphSchema::PC_Exec, NAME_None);
	CreatePin(EGPD_Output, UInputSequenceGraphSchema::PC_Exec, NAME_None);
}

FText UInputSequenceGraphNode_Input::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UInputSequenceGraphNode_Input_NodeTitle", "Input");
}

FLinearColor UInputSequenceGraphNode_Input::GetNodeTitleColor() const { return FLinearColor::Blue; }

FText UInputSequenceGraphNode_Input::GetTooltipText() const
{
	return LOCTEXT("UInputSequenceGraphNode_Input_TooltipText", "This is Input node of Input sequence...");
}

//------------------------------------------------------
// UInputSequenceGraphNode_Hub
//------------------------------------------------------

void UInputSequenceGraphNode_Hub::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UInputSequenceGraphSchema::PC_Exec, NAME_None);
	CreatePin(EGPD_Output, UInputSequenceGraphSchema::PC_Exec, "1");
}

FText UInputSequenceGraphNode_Hub::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UInputSequenceGraphNode_Hub_NodeTitle", "Hub");
}

FLinearColor UInputSequenceGraphNode_Hub::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UInputSequenceGraphNode_Hub::GetTooltipText() const
{
	return LOCTEXT("UInputSequenceGraphNode_Hub_TooltipText", "This is Hub node of Input sequence...");
}

//------------------------------------------------------
// UInputSequenceGraphNode_Reset
//------------------------------------------------------

void UInputSequenceGraphNode_Reset::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UInputSequenceGraphSchema::PC_Exec, NAME_None);
}

FText UInputSequenceGraphNode_Reset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UInputSequenceGraphNode_Reset_NodeTitle", "Reset");
}

FLinearColor UInputSequenceGraphNode_Reset::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UInputSequenceGraphNode_Reset::GetTooltipText() const
{
	return LOCTEXT("UInputSequenceGraphNode_Reset_TooltipText", "This is Reset node of Input sequence...");
}

//------------------------------------------------------
// FInputSequenceEditor
//------------------------------------------------------

class FInputSequenceEditor : public FEditorUndoClient, public FAssetEditorToolkit
{
public:

	static const FName AppIdentifier;
	static const FName DetailsTabId;
	static const FName GraphTabId;

	virtual ~FInputSequenceEditor();

	void InitInputSequenceEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UInputSequence* inputSequence);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

	virtual FName GetToolkitFName() const override { return FName("InputSequenceEditor"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("FInputSequenceEditor", "BaseToolkitName", "Input Sequence Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return "InputSequenceEditor"; }

protected:

	TSharedRef<SDockTab> SpawnTab_DetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphTab(const FSpawnTabArgs& Args);

	void OnSelectionChanged(const TSet<UObject*>& selectedNodes);

	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

	FGraphPanelSelectionSet GetSelectedNodes() const;

	void SelectAllNodes();

	bool CanSelectAllNodes() const { return true; }

	void DeleteSelectedNodes();

	bool CanDeleteNodes() const;

	void CopySelectedNodes();

	bool CanCopyNodes() const;

	void DeleteSelectedDuplicatableNodes();

	void CutSelectedNodes() { CopySelectedNodes(); DeleteSelectedDuplicatableNodes(); }

	bool CanCutNodes() const { return CanCopyNodes() && CanDeleteNodes(); }

	void PasteNodes();

	bool CanPasteNodes() const;

	void DuplicateSelectedNodes() { CopySelectedNodes(); PasteNodes(); }

	bool CanDuplicateNodes() const { return CanCopyNodes(); }

	void OnCreateComment();

	bool CanCreateComment() const;

protected:

	UInputSequence* InputSequence;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	TWeakPtr<SGraphEditor> GraphEditorPtr;

	TSharedPtr<IDetailsView> DetailsView;
};

const FName FInputSequenceEditor::AppIdentifier(TEXT("FInputSequenceEditor_AppIdentifier"));
const FName FInputSequenceEditor::DetailsTabId(TEXT("FInputSequenceEditor_DetailsTab_Id"));
const FName FInputSequenceEditor::GraphTabId(TEXT("FInputSequenceEditor_GraphTab_Id"));

FInputSequenceEditor::~FInputSequenceEditor()
{
	GEditor->UnregisterForUndo(this);
}

void FInputSequenceEditor::InitInputSequenceEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UInputSequence* inputSequence)
{
	GEditor->RegisterForUndo(this);

	check(inputSequence != NULL);

	InputSequence = inputSequence;

	InputSequence->SetFlags(RF_Transactional);

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("FInputSequenceEditor_StandaloneDefaultLayout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(GraphTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, StandaloneDefaultLayout, true, true, InputSequence);
}

void FInputSequenceEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Input Sequence Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FInputSequenceEditor::SpawnTab_DetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab_DisplayName", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FInputSequenceEditor::SpawnTab_GraphTab))
		.SetDisplayName(LOCTEXT("GraphTab_DisplayName", "Graph"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));
}

void FInputSequenceEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FInputSequenceEditor::SpawnTab_DetailsTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs = FDetailsViewArgs();
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(InputSequence);

	return SNew(SDockTab).Label(LOCTEXT("DetailsTab_Label", "Details"))[DetailsView.ToSharedRef()];
}

TSharedRef<SDockTab> FInputSequenceEditor::SpawnTab_GraphTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == GraphTabId);

	check(InputSequence != NULL);

	if (InputSequence->EdGraph == NULL)
	{
		InputSequence->EdGraph = NewObject<UInputSequenceGraph>(InputSequence, NAME_None, RF_Transactional);
		InputSequence->EdGraph->GetSchema()->CreateDefaultNodesForGraph(*InputSequence->EdGraph);
	}

	check(InputSequence->EdGraph != NULL);

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("GraphTab_AppearanceInfo_CornerText", "Input Sequence");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FInputSequenceEditor::OnSelectionChanged);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FInputSequenceEditor::OnNodeTitleCommitted);

	if (!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShareable(new FUICommandList);

		GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::SelectAllNodes),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanSelectAllNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::DeleteSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanDeleteNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::CopySelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanCopyNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::CutSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanCutNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::PasteNodes),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanPasteNodes)
		);

		GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::DuplicateSelectedNodes),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanDuplicateNodes)
		);

		GraphEditorCommands->MapAction(
			FGraphEditorCommands::Get().CreateComment,
			FExecuteAction::CreateRaw(this, &FInputSequenceEditor::OnCreateComment),
			FCanExecuteAction::CreateRaw(this, &FInputSequenceEditor::CanCreateComment)
		);
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("GraphTab_Label", "Graph"))
		.TabColorScale(GetTabColorScale())
		[
			SAssignNew(GraphEditorPtr, SGraphEditor)
				.AdditionalCommands(GraphEditorCommands)
				.Appearance(AppearanceInfo)
				.GraphEvents(InEvents)
				.TitleBar(SNew(STextBlock).Text(LOCTEXT("GraphTab_Title", "Input Sequence")).TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText")))
				.GraphToEdit(InputSequence->EdGraph)
		];
}

void FInputSequenceEditor::OnSelectionChanged(const TSet<UObject*>& selectedNodes)
{
	if (selectedNodes.Num() == 1)
	{
		if (UInputSequenceGraphNode_Base* graphNode = Cast<UInputSequenceGraphNode_Base>(*selectedNodes.begin()))
		{
			return DetailsView->SetObject(InputSequence->GetState(graphNode->NodeGuid));
		}

		if (UEdGraphNode_Comment* commentNode = Cast<UEdGraphNode_Comment>(*selectedNodes.begin()))
		{
			return DetailsView->SetObject(commentNode);
		}
	}

	return DetailsView->SetObject(InputSequence);
}

void FInputSequenceEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* graphNode)
{
	if (graphNode)
	{
		const FScopedTransaction Transaction(LOCTEXT("Transaction_FInputSequenceEditor::OnNodeTitleCommitted", "Rename Node"));
		
		graphNode->Modify();		
		graphNode->OnRenameNode(NewText.ToString());
	}
}

void FInputSequenceEditor::PostUndo(bool bSuccess)
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			graphEditor->NotifyGraphChanged();
		}
	}
	FSlateApplication::Get().DismissAllMenus();
}

FGraphPanelSelectionSet FInputSequenceEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;

	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			CurrentSelection = graphEditor->GetSelectedNodes();
		}
	}

	return CurrentSelection;
}

void FInputSequenceEditor::SelectAllNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			graphEditor->SelectAllNodes();
		}
	}
}

void FInputSequenceEditor::DeleteSelectedNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			TSet<UEdGraphNode*> graphNodes;

			const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* graphNode = Cast<UEdGraphNode>(*SelectedIter);

				if (graphNode->CanUserDeleteNode())
				{
					graphNodes.Add(graphNode);
				}
			}

			const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
			
			InputSequence->Modify();

			for (UEdGraphNode* graphNode : graphNodes)
			{
				InputSequence->RemoveState(graphNode->NodeGuid);
			}

			graphEditor->GetCurrentGraph()->Modify();

			graphEditor->ClearSelectionSet();

			for (UEdGraphNode* graphNode : graphNodes)
			{
				graphNode->Modify();
				graphNode->DestroyNode();
			}
		}
	}
}

bool FInputSequenceEditor::CanDeleteNodes() const
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* graphNode = Cast<UEdGraphNode>(*SelectedIter);
		if (graphNode && graphNode->CanUserDeleteNode()) return true;
	}

	return false;
}

void FInputSequenceEditor::CopySelectedNodes()
{
	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* graphNode = Cast<UEdGraphNode>(*SelectedIter);
		graphNode->PrepareForCopying();
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FInputSequenceEditor::CanCopyNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* graphNode = Cast<UEdGraphNode>(*SelectedIter);
		if (graphNode && graphNode->CanDuplicateNode()) return true;
	}

	return false;
}

void FInputSequenceEditor::DeleteSelectedDuplicatableNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			const FGraphPanelSelectionSet OldSelectedNodes = GetSelectedNodes();

			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* graphNode = Cast<UEdGraphNode>(*SelectedIter);
				if (graphNode && graphNode->CanDuplicateNode())
				{
					graphEditor->SetNodeSelection(graphNode, true);
				}
			}

			DeleteSelectedNodes();

			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* graphNode = Cast<UEdGraphNode>(*SelectedIter);
				graphEditor->SetNodeSelection(graphNode, true);
			}
		}
	}
}

void FInputSequenceEditor::PasteNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			FVector2D Location = graphEditor->GetPasteLocation();

			UEdGraph* graph = graphEditor->GetCurrentGraph();

			// Undo/Redo support
			const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
			
			graph->Modify();

			// Clear the selection set (newly pasted stuff will be selected)
			graphEditor->ClearSelectionSet();

			// Grab the text to paste from the clipboard.
			FString TextToImport;
			FPlatformApplicationMisc::ClipboardPaste(TextToImport);

			// Import the nodes
			TSet<UEdGraphNode*> pastedGraphNodes;
			FEdGraphUtilities::ImportNodesFromText(graph, TextToImport, /*out*/ pastedGraphNodes);

			if (pastedGraphNodes.Num() > 0)
			{
				//Average position of nodes so we can move them while still maintaining relative distances to each other
				FVector2D AvgNodePosition(0.0f, 0.0f);

				for (TSet<UEdGraphNode*>::TIterator It(pastedGraphNodes); It; ++It)
				{
					UEdGraphNode* graphNode = *It;
					AvgNodePosition.X += graphNode->NodePosX;
					AvgNodePosition.Y += graphNode->NodePosY;
				}

				float InvNumNodes = 1.0f / float(pastedGraphNodes.Num());
				AvgNodePosition.X *= InvNumNodes;
				AvgNodePosition.Y *= InvNumNodes;

				UInputSequence* prevOwningAsset = Cast<UInputSequenceGraphNode_Base>(*pastedGraphNodes.begin())->PrevOwningAsset;

				InputSequence->Modify();

				for (TSet<UEdGraphNode*>::TIterator It(pastedGraphNodes); It; ++It)
				{
					UEdGraphNode* graphNode = *It;

					// Select the newly pasted stuff
					graphEditor->SetNodeSelection(graphNode, true);

					graphNode->NodePosX = (graphNode->NodePosX - AvgNodePosition.X) + Location.X;
					graphNode->NodePosY = (graphNode->NodePosY - AvgNodePosition.Y) + Location.Y;

					graphNode->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

					const FGuid prevNodeGuid = graphNode->NodeGuid;

					// Give new node a different Guid from the old one
					graphNode->CreateNewGuid();

					InputSequence->AddState(graphNode->NodeGuid, DuplicateObject(prevOwningAsset->GetState(prevNodeGuid), InputSequence));

					if (UInputSequenceGraphNode_Input* inputGraphNode = Cast<UInputSequenceGraphNode_Input>(graphNode))
					{
						inputGraphNode->PrevOwningAsset = nullptr;
					}
				}

				graph->NotifyGraphChanged();
			}
		}
	}
}

bool FInputSequenceEditor::CanPasteNodes() const
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			FString ClipboardContent;
			FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

			return FEdGraphUtilities::CanImportNodesFromText(graphEditor->GetCurrentGraph(), ClipboardContent);
		}
	}

	return false;
}

void FInputSequenceEditor::OnCreateComment()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			TSharedPtr<FEdGraphSchemaAction> Action = graphEditor->GetCurrentGraph()->GetSchema()->GetCreateCommentAction();
			TSharedPtr<FInputSequenceGraphSchemaAction_NewComment> newCommentAction = StaticCastSharedPtr<FInputSequenceGraphSchemaAction_NewComment>(Action);

			if (newCommentAction.IsValid())
			{
				graphEditor->GetBoundsForSelectedNodes(newCommentAction->SelectedNodesBounds, 50);
				newCommentAction->PerformAction(graphEditor->GetCurrentGraph(), nullptr, FVector2D());
			}
		}
	}
}

bool FInputSequenceEditor::CanCreateComment() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	return SelectedNodes.Num() > 0;
}

//------------------------------------------------------
// FAssetTypeActions_InputSequence
//------------------------------------------------------

FText FAssetTypeActions_InputSequence::GetName() const { return LOCTEXT("FAssetTypeActions_InputSequence_Name", "Input Sequence"); }

UClass* FAssetTypeActions_InputSequence::GetSupportedClass() const { return UInputSequence::StaticClass(); }

void FAssetTypeActions_InputSequence::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (TArray<UObject*>::TConstIterator ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UInputSequence* inputSequence = Cast<UInputSequence>(*ObjIt))
		{
			TSharedRef<FInputSequenceEditor> NewEditor(new FInputSequenceEditor());
			NewEditor->InitInputSequenceEditor(Mode, EditWithinLevelEditor, inputSequence);
		}
	}
}

uint32 FAssetTypeActions_InputSequence::GetCategories() { return EAssetTypeCategories::Misc; }

//------------------------------------------------------
// FAssetTypeActions_RequestKey
//------------------------------------------------------

FText FAssetTypeActions_RequestKey::GetName() const { return LOCTEXT("FAssetTypeActions_RequestKey_Name", "Request Key (for Input Sequence)"); }

UClass* FAssetTypeActions_RequestKey::GetSupportedClass() const { return URequestKey::StaticClass(); }

uint32 FAssetTypeActions_RequestKey::GetCategories() { return EAssetTypeCategories::Misc; }

//------------------------------------------------------
// UFactory_InputSequence
//------------------------------------------------------

UFactory_InputSequence::UFactory_InputSequence(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UInputSequence::StaticClass();
}

UObject* UFactory_InputSequence::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return NewObject<UInputSequence>(InParent, InClass, InName, Flags);
}

FText UFactory_InputSequence::GetDisplayName() const { return LOCTEXT("UFactory_InputSequence_DisplayName", "Input Sequence"); }

uint32 UFactory_InputSequence::GetMenuCategories() const { return EAssetTypeCategories::Misc; }

//------------------------------------------------------
// UFactory_RequestKey
//------------------------------------------------------

UFactory_RequestKey::UFactory_RequestKey(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = URequestKey::StaticClass();
}

UObject* UFactory_RequestKey::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	return NewObject<URequestKey>(InParent, InClass, InName, Flags);
}

FText UFactory_RequestKey::GetDisplayName() const { return LOCTEXT("UFactory_RequestKey_DisplayName", "Request Key (for Input Sequence)"); }

uint32 UFactory_RequestKey::GetMenuCategories() const { return EAssetTypeCategories::Misc; }

//------------------------------------------------------
// FInputSequenceCoreEditor
//------------------------------------------------------

const FName AssetToolsModuleName("AssetTools");

void FInputSequenceCoreEditor::StartupModule()
{
	RegisteredAssetTypeActions.Add(MakeShared<FAssetTypeActions_InputSequence>());
	RegisteredAssetTypeActions.Add(MakeShared<FAssetTypeActions_RequestKey>());

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsModuleName).Get();
	for (TSharedPtr<FAssetTypeActions_Base>& registeredAssetTypeAction : RegisteredAssetTypeActions)
	{
		if (registeredAssetTypeAction.IsValid()) AssetTools.RegisterAssetTypeActions(registeredAssetTypeAction.ToSharedRef());
	}

	InputSequenceGraphNodeFactory = MakeShareable(new FInputSequenceGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(InputSequenceGraphNodeFactory);

	InputSequenceGraphPinFactory = MakeShareable(new FInputSequenceGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(InputSequenceGraphPinFactory);

	InputSequenceGraphPinConnectionFactory = MakeShareable(new FInputSequenceGraphPinConnectionFactory());
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(InputSequenceGraphPinConnectionFactory);
}

void FInputSequenceCoreEditor::ShutdownModule()
{
	FEdGraphUtilities::UnregisterVisualPinConnectionFactory(InputSequenceGraphPinConnectionFactory);
	InputSequenceGraphPinConnectionFactory.Reset();

	FEdGraphUtilities::UnregisterVisualPinFactory(InputSequenceGraphPinFactory);
	InputSequenceGraphPinFactory.Reset();

	FEdGraphUtilities::UnregisterVisualNodeFactory(InputSequenceGraphNodeFactory);
	InputSequenceGraphNodeFactory.Reset();

	if (FModuleManager::Get().IsModuleLoaded(AssetToolsModuleName))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsModuleName).Get();
		for (TSharedPtr<FAssetTypeActions_Base>& registeredAssetTypeAction : RegisteredAssetTypeActions)
		{
			if (registeredAssetTypeAction.IsValid()) AssetTools.UnregisterAssetTypeActions(registeredAssetTypeAction.ToSharedRef());
		}
	}

	RegisteredAssetTypeActions.Empty();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInputSequenceCoreEditor, InputSequenceCoreEditor)