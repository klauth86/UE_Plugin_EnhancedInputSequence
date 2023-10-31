// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "EnhancedInputSequenceEditor.h"

#include "Factory_InputSequence.h"
#include "AssetTypeActions_InputSequence.h"
#include "AssetTypeCategories.h"
#include "InputSequence.h"

#include "InputAction.h"
#include "InputTriggers.h"

#include "ISGraph.h"
#include "ISGraphSchema.h"
#include "ISGraphNodes.h"
#include "EdGraphNode_Comment.h"

#include "ConnectionDrawingPolicy.h"
#include "SLevelOfDetailBranchNode.h"
#include "SPinTypeSelector.h"
#include "SGraphActionMenu.h"
#include "KismetPins/SGraphPinExec.h"

#include "EdGraphUtilities.h"
#include "GraphEditorActions.h"
#include "Settings/EditorStyleSettings.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Commands/GenericCommands.h"

#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FEnhancedInputSequenceEditorModule"

void AddPin(UEdGraphNode* node, FName category, FName pinName, const UEdGraphNode::FCreatePinParams& params, TObjectPtr<UInputAction> inputActionObj)
{
	UEdGraphPin* graphPin = node->CreatePin(EGPD_Output, category, pinName, params);

	if (inputActionObj)
	{
		if (UISGraphNode_Input* inputGraphNode = Cast<UISGraphNode_Input>(node))
		{
			inputGraphNode->InputActions.FindOrAdd(pinName, inputActionObj);
		}
	}

	node->Modify();

	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(node))
	{
		dynamicGraphNode->OnUpdateGraphNode.ExecuteIfBound();
	}
}

const FName NAME_NoBorder("NoBorder");

//------------------------------------------------------
// SISGraphNode_Dynamic
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
// SISGraphNode_Dynamic
//------------------------------------------------------

class SISGraphNode_Dynamic : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SISGraphNode_Dynamic) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UEdGraphNode* InNode);

	virtual ~SISGraphNode_Dynamic();
};

void SISGraphNode_Dynamic::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	SetCursor(EMouseCursor::CardinalCross);

	GraphNode = InNode;

	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(InNode))
	{
		dynamicGraphNode->OnUpdateGraphNode.BindLambda([&]() { UpdateGraphNode(); });
	}

	UpdateGraphNode();
}

SISGraphNode_Dynamic::~SISGraphNode_Dynamic()
{
	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(GraphNode))
	{
		dynamicGraphNode->OnUpdateGraphNode.Unbind();
	}
}

//------------------------------------------------------
// SISParameterMenu
//------------------------------------------------------

class SISParameterMenu : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetSectionTitle, int32);

	SLATE_BEGIN_ARGS(SISParameterMenu) : _AutoExpandMenu(false) {}

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
									.OnCollectStaticSections(this, &SISParameterMenu::OnCollectStaticSections)
									.OnGetSectionTitle(this, &SISParameterMenu::OnGetSectionTitle)
									.OnCollectAllActions(this, &SISParameterMenu::CollectAllActions)
									.OnActionSelected(this, &SISParameterMenu::OnActionSelected)
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
// SISParameterMenu_Pin
//------------------------------------------------------

class SISParameterMenu_Pin : public SISParameterMenu
{
public:
	SLATE_BEGIN_ARGS(SISParameterMenu_Pin)
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

		SISParameterMenu::FArguments SuperArgs;
		SuperArgs._AutoExpandMenu = InArgs._AutoExpandMenu;
		SISParameterMenu::Construct(SuperArgs);
	}

protected:

	virtual void OnCollectStaticSections(TArray<int32>& StaticSectionIDs) override { StaticSectionIDs.Add(1); }

	virtual FText OnGetSectionTitle(int32 InSectionID) override { return LOCTEXT("SISParameterMenu_Pin_SectionTitle", "Input Actions"); }

	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		FARFilter filter;
		filter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());
		filter.bRecursiveClasses = true;
		filter.bRecursivePaths = true;

		TArray<FAssetData> assetDatas;
		AssetRegistryModule.Get().GetAssets(filter, assetDatas);

		TSet<UInputAction*> inputActions;

		for (const FAssetData& assetData : assetDatas)
		{
			if (UInputAction* inputAction = Cast<UInputAction>(assetData.GetAsset()))
			{
				inputActions.FindOrAdd(inputAction);
			}
		}

		int32 mappingIndex = 0;
		TSet<int32> alreadyAdded;

		TArray<TSharedPtr<FEdGraphSchemaAction>> schemaActions;

		// Enhanced Input
		for (UInputAction* inputAction : inputActions)
		{
			const FName inputActionName = inputAction->GetFName();

			if (Node && Node->FindPin(inputActionName))
			{
				alreadyAdded.Add(mappingIndex);
			}
			else
			{
				const FText tooltip = FText::Format(LOCTEXT("SISParameterMenu_Pin_Tooltip", "Add {0} for {1}"), FText::FromString("Action pin"), FText::FromName(inputActionName));

				TSharedPtr<FISGraphSchemaAction_AddPin> schemaAction(
					new FISGraphSchemaAction_AddPin(
						FText::GetEmpty()
						, FText::FromName(inputActionName)
						, tooltip
						, 0
						, 1
					)
				);

				schemaAction->InputAction = inputAction;
				schemaAction->InputIndex = mappingIndex;
				schemaAction->CorrectedInputIndex = 0;

				schemaActions.Add(schemaAction);
			}

			mappingIndex++;
		}

		for (TSharedPtr<FEdGraphSchemaAction> schemaAction : schemaActions)
		{
			TSharedPtr<FISGraphSchemaAction_AddPin> addPinAction = StaticCastSharedPtr<FISGraphSchemaAction_AddPin>(schemaAction);

			for (int32 alreadyAddedIndex : alreadyAdded)
			{
				if (alreadyAddedIndex < addPinAction->InputIndex) addPinAction->CorrectedInputIndex++;
			}

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
// SGraphPin_Add
//------------------------------------------------------

class SGraphPin_Add : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_Add) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	TSharedRef<SWidget> OnGetAddButtonMenuContent();

	TSharedPtr<SComboButton> AddButton;
};

void SGraphPin_Add::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
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
	TSharedRef<SWidget> PinWidgetRef = SAssignNew(AddButton, SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.ForegroundColor(GetPinColor())
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Cursor(EMouseCursor::Hand)
		.ToolTipText(LOCTEXT("SGraphPin_Add_ToolTipText", "Click to add new pin"))
		.OnGetMenuContent(this, &SGraphPin_Add::OnGetAddButtonMenuContent)
		.ButtonContent()
		[
			SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
		];

	// Set up a hover for pins that is tinted the color of the pin.

	SBorder::Construct(SBorder::FArguments().BorderImage(FAppStyle::GetBrush(NAME_NoBorder))[PinWidgetRef]);
}

TSharedRef<SWidget> SGraphPin_Add::OnGetAddButtonMenuContent()
{
	TSharedRef<SISParameterMenu_Pin> MenuWidget = SNew(SISParameterMenu_Pin).Node(GetPinObj()->GetOwningNode());

	AddButton->SetMenuContentWidgetToFocus(MenuWidget->GetSearchBox());

	return MenuWidget;
}

//------------------------------------------------------
// SGraphPin_InputAction
//------------------------------------------------------

class SGraphPin_InputAction : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_InputAction) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

	virtual FSlateColor GetPinTextColor() const override { return FLinearColor::White; }

protected:

	virtual TSharedRef<SWidget> GetLabelWidget(const FName& InLabelStyle) override;

	FReply OnClicked_RemovePin() const;

	FReply OnClicked_Started() const;

	FReply OnClicked_Triggered() const;

	FReply OnClicked_Completed() const;

	void SetTriggerEvent(const ETriggerEvent triggerEvent) const;

	FSlateColor GetTriggerEventForegroundColor_Started() const { return GetTriggerEventForegroundColor(ButtonStartedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Started))); }

	FSlateColor GetTriggerEventForegroundColor_Triggered() const { return GetTriggerEventForegroundColor(ButtonTriggeredPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Triggered))); }

	FSlateColor GetTriggerEventForegroundColor_Completed() const { return GetTriggerEventForegroundColor(ButtonCompletedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Completed))); }

	FSlateColor GetTriggerEventForegroundColor(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const;

	TOptional<FSlateRenderTransform> GetTriggerEventRenderTransform_Started() const { return GetTriggerEventRenderTransform(ButtonStartedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Started))); }

	TOptional<FSlateRenderTransform> GetTriggerEventRenderTransform_Triggered() const { return GetTriggerEventRenderTransform(ButtonTriggeredPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Triggered))); }

	TOptional<FSlateRenderTransform> GetTriggerEventRenderTransform_Completed() const { return GetTriggerEventRenderTransform(ButtonCompletedPtr, FString::FromInt(static_cast<int32>(ETriggerEvent::Completed))); }

	TOptional<FSlateRenderTransform> GetTriggerEventRenderTransform(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const;

	TSharedPtr<SButton> ButtonStartedPtr;
	TSharedPtr<SButton> ButtonTriggeredPtr;
	TSharedPtr<SButton> ButtonCompletedPtr;
};

void SGraphPin_InputAction::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
	SGraphPin::FArguments InArgs = SGraphPin::FArguments();

	bUsePinColorForText = InArgs._UsePinColorForText;
	this->SetCursor(EMouseCursor::Default);

	SetVisibility(MakeAttributeSP(this, &SGraphPin_InputAction::GetPinVisiblity));

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

	// Create the pin indicator widget (used for watched values)
	TSharedRef<SWidget> PinStatusIndicator =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.Visibility(this, &SGraphPin_InputAction::GetPinStatusIconVisibility)
		.ContentPadding(0)
		.OnClicked(this, &SGraphPin_InputAction::ClickedOnPinStatusIcon)
		[
			SNew(SImage).Image(this, &SGraphPin_InputAction::GetPinStatusIcon)
		];

	TSharedRef<SWidget> LabelWidget = GetLabelWidget(InArgs._PinLabelStyle);

	// Create the widget used for the pin body (status indicator, label, and value)
	LabelAndValue =
		SNew(SWrapBox)
		.PreferredSize(150.f);

	LabelAndValue->AddSlot().VAlign(VAlign_Center)
		[
			PinStatusIndicator
		];

	LabelAndValue->AddSlot().VAlign(VAlign_Center)
		[
			LabelWidget
		];

	// Set up a hover for pins that is tinted the color of the pin.

	SBorder::Construct(SBorder::FArguments().BorderImage(FAppStyle::GetBrush(NAME_NoBorder))
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBorder)
						.BorderImage(this, &SGraphPin_InputAction::GetPinBorder)
						.BorderBackgroundColor(this, &SGraphPin_InputAction::GetHighlightColor)
						[
							SNew(SBorder)
								.BorderImage(CachedImg_Pin_DiffOutline)
								.BorderBackgroundColor(this, &SGraphPin_InputAction::GetPinDiffColor)
								[
									SNew(SLevelOfDetailBranchNode)
										.UseLowDetailSlot(this, &SGraphPin_InputAction::UseLowDetailPinNames)
										.LowDetail()
										[
											SNullWidget::NullWidget
										]
										.HighDetail()
										[
											LabelAndValue.ToSharedRef()
										]
								]
						]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SLevelOfDetailBranchNode)
						.UseLowDetailSlot(this, &SGraphPin_InputAction::UseLowDetailPinNames)
						.LowDetail()
						[
							SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
								.ForegroundColor(GetPinColor())
								.Cursor(EMouseCursor::Hand)
								.ToolTipText(LOCTEXT("SGraphPin_InputAction_TooltipText_RemovePin", "Click to remove pin"))
								.OnClicked_Raw(this, &SGraphPin_InputAction::OnClicked_RemovePin)
								[
									SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
								]
						]
						.HighDetail()
						[
							SNew(SWrapBox)

								+ SWrapBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(2)
								[
									SAssignNew(ButtonStartedPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
										.Cursor(EMouseCursor::Hand)
										.ToolTipText(LOCTEXT("SGraphPin_InputAction_TooltipText_Started", "Started"))
										.OnClicked_Raw(this, &SGraphPin_InputAction::OnClicked_Started)
										[
											SNew(STextBlock).Text(FText::FromString("S")).Font(FAppStyle::Get().GetFontStyle("Font.Large.Bold"))
												.RenderTransform_Raw(this, &SGraphPin_InputAction::GetTriggerEventRenderTransform_Started).RenderTransformPivot(FVector2D(0.5f, 0.5f))
												.ColorAndOpacity_Raw(this, &SGraphPin_InputAction::GetTriggerEventForegroundColor_Started)
										]
								]
								+ SWrapBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(2)
								[
									SAssignNew(ButtonTriggeredPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
										.Cursor(EMouseCursor::Hand)
										.ToolTipText(LOCTEXT("SGraphPin_InputAction_TooltipText_Trigger", "Triggered"))
										.OnClicked_Raw(this, &SGraphPin_InputAction::OnClicked_Triggered)
										[
											SNew(STextBlock).Text(FText::FromString("T")).Font(FAppStyle::Get().GetFontStyle("Font.Large.Bold"))
												.RenderTransform_Raw(this, &SGraphPin_InputAction::GetTriggerEventRenderTransform_Triggered).RenderTransformPivot(FVector2D(0.5f, 0.5f))
												.ColorAndOpacity_Raw(this, &SGraphPin_InputAction::GetTriggerEventForegroundColor_Triggered)
										]
								]
								+ SWrapBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(2)
								[
									SAssignNew(ButtonCompletedPtr, SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
										.Cursor(EMouseCursor::Hand)
										.ToolTipText(LOCTEXT("SGraphPin_InputAction_TooltipText_Complete", "Completed"))
										.OnClicked_Raw(this, &SGraphPin_InputAction::OnClicked_Completed)
										[
											SNew(STextBlock).Text(FText::FromString("C")).Font(FAppStyle::Get().GetFontStyle("Font.Large.Bold"))
												.RenderTransform_Raw(this, &SGraphPin_InputAction::GetTriggerEventRenderTransform_Completed).RenderTransformPivot(FVector2D(0.5f, 0.5f))
												.ColorAndOpacity_Raw(this, &SGraphPin_InputAction::GetTriggerEventForegroundColor_Completed)
										]
								]

								+ SWrapBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(2)
								[
									SNew(SButton).ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
										.ForegroundColor(GetPinColor())
										.Cursor(EMouseCursor::Hand)
										.ToolTipText(LOCTEXT("SGraphPin_InputAction_TooltipText_RemovePin", "Click to remove pin"))
										.OnClicked_Raw(this, &SGraphPin_InputAction::OnClicked_RemovePin)
										[
											SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
										]
								]
						]
				]
		]
	);

	SetToolTip(SNew(SToolTip_Dummy));
}

TSharedRef<SWidget> SGraphPin_InputAction::GetLabelWidget(const FName& InLabelStyle)
{
	TSharedRef<STextBlock> labelWidget = SNew(STextBlock)
		.Text(this, &SGraphPin_InputAction::GetPinLabel)
		.TextStyle(FAppStyle::Get(), InLabelStyle)
		.Visibility(this, &SGraphPin_InputAction::GetPinLabelVisibility)
		.ColorAndOpacity(this, &SGraphPin_InputAction::GetPinTextColor);

	labelWidget->SetFont(FAppStyle::Get().GetFontStyle("SmallFont"));

	return labelWidget;
}

FReply SGraphPin_InputAction::OnClicked_RemovePin() const
{
	if (UEdGraphPin* FromPin = GetPinObj())
	{
		UEdGraphNode* FromNode = FromPin->GetOwningNode();

		UEdGraph* ParentGraph = FromNode->GetGraph();

		if (FromPin->HasAnyConnections())
		{
			const FScopedTransaction Transaction(LOCTEXT("K2_DeleteNode", "Delete Node"));

			ParentGraph->Modify();

			UEdGraphNode* linkedGraphNode = FromPin->LinkedTo[0]->GetOwningNode();

			linkedGraphNode->Modify();
			linkedGraphNode->DestroyNode();
		}

		{
			const FScopedTransaction Transaction(LOCTEXT("K2_DeletePin", "Delete Pin"));

			FromNode->RemovePin(FromPin);

			FromNode->Modify();

			if (UISGraphNode_Input* inputGraphNode = Cast<UISGraphNode_Input>(FromNode))
			{
				inputGraphNode->InputActions.Remove(FromPin->PinName);
			}

			if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(FromNode))
			{
				dynamicGraphNode->OnUpdateGraphNode.ExecuteIfBound();
			}
		}
	}

	return FReply::Handled();
}

FReply SGraphPin_InputAction::OnClicked_Started() const
{
	SetTriggerEvent(ETriggerEvent::Started);
	return FReply::Handled();
}

FReply SGraphPin_InputAction::OnClicked_Triggered() const
{
	SetTriggerEvent(ETriggerEvent::Triggered);
	return FReply::Handled();
}

FReply SGraphPin_InputAction::OnClicked_Completed() const
{
	SetTriggerEvent(ETriggerEvent::Completed);
	return FReply::Handled();
}

void SGraphPin_InputAction::SetTriggerEvent(const ETriggerEvent triggerEvent) const
{
	if (UEdGraphPin* PinObject = GetPinObj())
	{
		const ETriggerEvent currentTriggerEvent = static_cast<ETriggerEvent>(FCString::Atoi(*(PinObject->DefaultValue.IsEmpty() ? "0" : PinObject->DefaultValue)));

		if (currentTriggerEvent != triggerEvent)
		{
			if (UISGraphNode_Input* inputGraphNode = Cast<UISGraphNode_Input>(PinObject->GetOwningNode()))
			{
				const FScopedTransaction Transaction(LOCTEXT("SGraphPin_InputAction_SetTriggerEvent", "Set Trigger Event"));

				PinObject->Modify();
				PinObject->DefaultValue = FString::FromInt(static_cast<int32>(triggerEvent));
			}
		}
	}
}

FSlateColor SGraphPin_InputAction::GetTriggerEventForegroundColor(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const
{
	return GetPinObj()->DefaultValue == triggerEventString ? FLinearColor::Green : GetPinColor();
}

TOptional<FSlateRenderTransform> SGraphPin_InputAction::GetTriggerEventRenderTransform(const TSharedPtr<SButton>& buttonPtr, const FString& triggerEventString) const
{
	static FSlateRenderTransform identity(FScale2f(1.f, 1.f));
	static FSlateRenderTransform decreased(FScale2f(.5f, .5f));
	return buttonPtr->IsHovered() || (GetPinObj()->DefaultValue == triggerEventString) ? identity : decreased;
}

//------------------------------------------------------
// SGraphPin_HubAdd
//------------------------------------------------------

class SGraphPin_HubAdd : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPin_HubAdd) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	FReply OnClicked_AddPin();
};

void SGraphPin_HubAdd::Construct(const FArguments& Args, UEdGraphPin* InPin)
{
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
	TSharedRef<SWidget> PinWidgetRef = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.ForegroundColor(GetPinColor())
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Cursor(EMouseCursor::Hand)
		.ToolTipText(LOCTEXT("SGraphPin_HubAdd_ToolTipText", "Click to add new pin"))
		.OnClicked_Raw(this, &SGraphPin_HubAdd::OnClicked_AddPin)
		[
			SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
		];

	// Set up a hover for pins that is tinted the color of the pin.
	
	SBorder::Construct(SBorder::FArguments().BorderImage(FAppStyle::GetBrush(NAME_NoBorder))[PinWidgetRef]);
}

FReply SGraphPin_HubAdd::OnClicked_AddPin()
{
	if (UEdGraphPin* FromPin = GetPinObj())
	{
		const FScopedTransaction Transaction(LOCTEXT("K2_AddPin", "Add Pin"));

		int32 outputPinsCount = 0;
		for (UEdGraphPin* pin : FromPin->GetOwningNode()->Pins)
		{
			if (pin->Direction == EGPD_Output) outputPinsCount++;
		}

		UEdGraphNode::FCreatePinParams params;
		params.Index = outputPinsCount;

		AddPin(FromPin->GetOwningNode(), UISGraphSchema::PC_Exec, FName(FString::FromInt(outputPinsCount)), params, nullptr);
	}

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
	if (UEdGraphPin* FromPin = GetPinObj())
	{
		if (UEdGraphNode* FromNode = FromPin->GetOwningNode())
		{
			const FScopedTransaction Transaction(LOCTEXT("K2_DeletePin", "Delete Pin"));

			int nextAfterRemovedIndex = FromNode->Pins.IndexOfByKey(FromPin) + 1;

			if (FromNode->Pins.IsValidIndex(nextAfterRemovedIndex))
			{
				for (size_t i = nextAfterRemovedIndex; i < FromNode->Pins.Num(); i++)
				{
					UEdGraphPin* pin = FromNode->Pins[i];

					if (pin->Direction == EGPD_Output && pin->PinType.PinCategory == UISGraphSchema::PC_Exec)
					{
						pin->PinName = FName(FString::FromInt(i - 1));
					}
				}
			}

			FromNode->RemovePin(FromPin);

			FromNode->Modify();

			if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(FromNode)) dynamicGraphNode->OnUpdateGraphNode.ExecuteIfBound();
		}
	}

	return FReply::Handled();
}

//------------------------------------------------------
// FISGraphNodeFactory
//------------------------------------------------------

TSharedPtr<SGraphNode> FISGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (UISGraphNode_Dynamic* dynamicGraphNode = Cast<UISGraphNode_Dynamic>(InNode))
	{
		return SNew(SISGraphNode_Dynamic, dynamicGraphNode);
	}

	return nullptr;
}

//------------------------------------------------------
// FISGraphPinFactory
//------------------------------------------------------

TSharedPtr<SGraphPin> FISGraphPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (InPin->GetSchema()->IsA<UISGraphSchema>())
	{
		if (InPin->PinType.PinCategory == UISGraphSchema::PC_Exec)
		{
			if (InPin->Direction == EEdGraphPinDirection::EGPD_Output && InPin->GetOwningNode() && InPin->GetOwningNode()->IsA<UISGraphNode_Hub>())
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
		if (InPin->PinType.PinCategory == UISGraphSchema::PC_Add)
		{
			if (InPin->GetOwningNode() && InPin->GetOwningNode()->IsA<UISGraphNode_Hub>())
			{
				return SNew(SGraphPin_HubAdd, InPin);
			}
			else
			{
				return SNew(SGraphPin_Add, InPin);
			}
		}
		else if (InPin->PinType.PinCategory == UISGraphSchema::PC_InputAction)
		{
			return SNew(SGraphPin_InputAction, InPin);
		}

		return FGraphPanelPinFactory::CreatePin(InPin);
	}

	return nullptr;
}

//------------------------------------------------------
// FISGraphPinConnectionFactory
//------------------------------------------------------

class FISConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FISConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
		, GraphObj(InGraphObj)
	{}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

		if (OutputPin->PinType.PinCategory == UISGraphSchema::PC_Exec)
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

FConnectionDrawingPolicy* FISGraphPinConnectionFactory::CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const
{
	if (Schema->IsA<UISGraphSchema>())
	{
		return new FISConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj);;
	}

	return nullptr;
}

//------------------------------------------------------
// FISGraphSchemaAction_NewComment
//------------------------------------------------------

UISGraph::UISGraph(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	Schema = UISGraphSchema::StaticClass();
}

//------------------------------------------------------
// FISGraphSchemaAction_NewComment
//------------------------------------------------------

UEdGraphNode* FISGraphSchemaAction_NewComment::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
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
// FISGraphSchemaAction_NewNode
//------------------------------------------------------

UEdGraphNode* FISGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	// If there is a template, we actually use it
	if (NodeTemplate != NULL)
	{
		const FScopedTransaction Transaction(LOCTEXT("K2_AddNode", "Add Node"));
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(NULL, ParentGraph);
		ParentGraph->AddNode(NodeTemplate, true, bSelectNewNode);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();
		NodeTemplate->AutowireNewNode(FromPin);

		NodeTemplate->NodePosX = Location.X;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

		ResultNode = NodeTemplate;

		ResultNode->SetFlags(RF_Transactional);
	}

	return ResultNode;
}

void FISGraphSchemaAction_NewNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(NodeTemplate);
}

//------------------------------------------------------
// FISGraphSchemaAction_AddPin
//------------------------------------------------------

UEdGraphNode* FISGraphSchemaAction_AddPin::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode* ResultNode = NULL;

	if (InputAction)
	{
		const int32 execPinCount = 2;

		const FScopedTransaction Transaction(LOCTEXT("K2_AddPin", "Add Pin"));

		UEdGraphNode::FCreatePinParams params;
		params.Index = CorrectedInputIndex + execPinCount;

		AddPin(FromPin->GetOwningNode(), UISGraphSchema::PC_InputAction, InputAction->GetFName(), params, InputAction);
	}

	return ResultNode;
}

//------------------------------------------------------
// UISGraphSchema
//------------------------------------------------------

template<class T>
TSharedPtr<T> AddNewActionAs(FGraphContextMenuBuilder& ContextMenuBuilder, const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 Grouping = 0)
{
	TSharedPtr<T> Action(new T(Category, MenuDesc, Tooltip, Grouping));
	ContextMenuBuilder.AddAction(Action);
	return Action;
}

const FName UISGraphSchema::PC_Exec("UISGraphSchema_PC_Exec");

const FName UISGraphSchema::PC_Add("UISGraphSchema_PC_Add");

const FName UISGraphSchema::PC_InputAction("UISGraphSchema_PC_InputAction");

void UISGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	{
		// Add Input node
		TSharedPtr<FISGraphSchemaAction_NewNode> Action = AddNewActionAs<FISGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Input", "Add Input node..."), LOCTEXT("AddNode_Input_Tooltip", "A new Input node"));
		Action->NodeTemplate = NewObject<UISGraphNode_Input>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	{
		// Add Hub node
		TSharedPtr<FISGraphSchemaAction_NewNode> Action = AddNewActionAs<FISGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Hub", "Add Hub node..."), LOCTEXT("AddNode_Hub_Tooltip", "A new Hub node"));
		Action->NodeTemplate = NewObject<UISGraphNode_Hub>(ContextMenuBuilder.OwnerOfTemporaries);
	}

	{
		// Add Reset node
		TSharedPtr<FISGraphSchemaAction_NewNode> Action = AddNewActionAs<FISGraphSchemaAction_NewNode>(ContextMenuBuilder, FText::GetEmpty(), LOCTEXT("AddNode_Reset", "Add Reset node..."), LOCTEXT("AddNode_Reset_Tooltip", "A new Reset node"));
		Action->NodeTemplate = NewObject<UISGraphNode_Reset>(ContextMenuBuilder.OwnerOfTemporaries);
	}
}

const FPinConnectionResponse UISGraphSchema::CanCreateConnection(const UEdGraphPin* pinA, const UEdGraphPin* pinB) const
{
	if (pinA == nullptr || pinB == nullptr) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("Pin(s)Null", "One or both of pins was null"));

	if (pinA->GetOwningNode() == pinB->GetOwningNode()) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsOfSameNode", "Both pins are on same node"));

	if (pinA->Direction == pinB->Direction) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsOfSameDirection", "Both pins have same direction (both input or both output)"));

	if (pinA->PinType.PinCategory != pinB->PinType.PinCategory) return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinsCategoryMismatched", "Pins Categories are mismatched (Flow pin should be connected to Flow pin, Input pin - to Input pin)"));

	return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, TEXT(""));
}

void UISGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	FGraphNodeCreator<UISGraphNode_Entry> entryGraphNodeCreator(Graph);
	UISGraphNode_Entry* entryGraphNode = entryGraphNodeCreator.CreateNode();
	entryGraphNode->NodePosX = -300;
	entryGraphNodeCreator.Finalize();
	SetNodeMetaData(entryGraphNode, FNodeMetadata::DefaultGraphNode);
}

TSharedPtr<FEdGraphSchemaAction> UISGraphSchema::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FISGraphSchemaAction_NewComment));
}

//------------------------------------------------------
// UISGraphNode_Entry
//------------------------------------------------------

void UISGraphNode_Entry::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UISGraphSchema::PC_Exec, NAME_None);
}

FText UISGraphNode_Entry::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Entry_NodeTitle", "Entry");
}

FLinearColor UISGraphNode_Entry::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UISGraphNode_Entry::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Entry_TooltipText", "This is Entry node of Input sequence...");
}

//------------------------------------------------------
// UISGraphNode_Input
//------------------------------------------------------

void UISGraphNode_Input::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UISGraphSchema::PC_Exec, NAME_None);
	CreatePin(EGPD_Output, UISGraphSchema::PC_Exec, NAME_None);

	CreatePin(EGPD_Output, UISGraphSchema::PC_Add, NAME_None);
}

FText UISGraphNode_Input::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Input_NodeTitle", "Input");
}

FLinearColor UISGraphNode_Input::GetNodeTitleColor() const { return FLinearColor::Blue; }

FText UISGraphNode_Input::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Input_TooltipText", "This is Input node of Input sequence...");
}

//------------------------------------------------------
// UISGraphNode_Hub
//------------------------------------------------------

void UISGraphNode_Hub::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UISGraphSchema::PC_Exec, NAME_None);

	CreatePin(EGPD_Output, UISGraphSchema::PC_Exec, "1");
	CreatePin(EGPD_Output, UISGraphSchema::PC_Add, NAME_None);

}

FText UISGraphNode_Hub::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Hub_NodeTitle", "Hub");
}

FLinearColor UISGraphNode_Hub::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UISGraphNode_Hub::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Hub_TooltipText", "This is Hub node of Input sequence...");
}

//------------------------------------------------------
// UISGraphNode_Reset
//------------------------------------------------------

void UISGraphNode_Reset::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UISGraphSchema::PC_Exec, NAME_None);
}

FText UISGraphNode_Reset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("UISGraphNode_Reset_NodeTitle", "Reset");
}

FLinearColor UISGraphNode_Reset::GetNodeTitleColor() const { return FLinearColor::Green; }

FText UISGraphNode_Reset::GetTooltipText() const
{
	return LOCTEXT("UISGraphNode_Reset_TooltipText", "This is Reset node of Input sequence...");
}

//------------------------------------------------------
// FInputSequenceEditor
//------------------------------------------------------

class FISEditor : public FEditorUndoClient, public FAssetEditorToolkit
{
public:

	static const FName AppIdentifier;
	static const FName DetailsTabId;
	static const FName GraphTabId;

	void InitISEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UInputSequence* inputSequence);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

	virtual FName GetToolkitFName() const override { return FName("ISEditor"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("FISEditor", "BaseToolkitName", "Input Sequence Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return "ISEditor"; }

protected:

	TSharedRef<SDockTab> SpawnTab_DetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphTab(const FSpawnTabArgs& Args);

	void CreateCommandList();

	void OnSelectionChanged(const TSet<UObject*>& selectedNodes);

	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
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

	void DuplicateNodes() { CopySelectedNodes(); PasteNodes(); }

	bool CanDuplicateNodes() const { return CanCopyNodes(); }

	void OnCreateComment();

	bool CanCreateComment() const;

protected:

	UInputSequence* InputSequence;

	TSharedPtr<FUICommandList> GraphEditorCommands;

	TWeakPtr<SGraphEditor> GraphEditorPtr;

	TSharedPtr<IDetailsView> DetailsView;
};

const FName FISEditor::AppIdentifier(TEXT("FISEditor_AppIdentifier"));
const FName FISEditor::DetailsTabId(TEXT("FISEditor_DetailsTab_Id"));
const FName FISEditor::GraphTabId(TEXT("FISEditor_GraphTab_Id"));

void FISEditor::InitISEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UInputSequence* inputSequence)
{
	check(inputSequence != NULL);

	InputSequence = inputSequence;

	InputSequence->SetFlags(RF_Transactional);

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("FISEditor_StandaloneDefaultLayout")
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

void FISEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuCategory", "Input Sequence Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FISEditor::SpawnTab_DetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab_DisplayName", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FISEditor::SpawnTab_GraphTab))
		.SetDisplayName(LOCTEXT("GraphTab_DisplayName", "Graph"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));
}

void FISEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FISEditor::SpawnTab_DetailsTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs = FDetailsViewArgs();
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(InputSequence);

	return SNew(SDockTab).Label(LOCTEXT("DetailsTab_Label", "Details"))[DetailsView.ToSharedRef()];
}

TSharedRef<SDockTab> FISEditor::SpawnTab_GraphTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == GraphTabId);

	check(InputSequence != NULL);

	if (InputSequence->EdGraph == NULL)
	{
		UISGraph* isGraph = NewObject<UISGraph>(InputSequence, NAME_None, RF_Transactional);
		isGraph->SetInputSequence(InputSequence);

		InputSequence->EdGraph = isGraph;
		InputSequence->EdGraph->GetSchema()->CreateDefaultNodesForGraph(*InputSequence->EdGraph);
	}

	check(InputSequence->EdGraph != NULL);

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("GraphTab_AppearanceInfo_CornerText", "Input Sequence");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FISEditor::OnSelectionChanged);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FISEditor::OnNodeTitleCommitted);

	CreateCommandList();

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

void FISEditor::CreateCommandList()
{
	if (GraphEditorCommands.IsValid()) return;

	GraphEditorCommands = MakeShareable(new FUICommandList);

	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateRaw(this, &FISEditor::SelectAllNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanSelectAllNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateRaw(this, &FISEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanDeleteNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateRaw(this, &FISEditor::CopySelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanCopyNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateRaw(this, &FISEditor::CutSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanCutNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateRaw(this, &FISEditor::PasteNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanPasteNodes)
	);

	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateRaw(this, &FISEditor::DuplicateNodes),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanDuplicateNodes)
	);

	GraphEditorCommands->MapAction(
		FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateRaw(this, &FISEditor::OnCreateComment),
		FCanExecuteAction::CreateRaw(this, &FISEditor::CanCreateComment)
	);
}

void FISEditor::OnSelectionChanged(const TSet<UObject*>& selectedNodes)
{
	if (selectedNodes.Num() == 1)
	{
		if (UISGraphNode_Input* inputNode = Cast<UISGraphNode_Input>(*selectedNodes.begin()))
		{
			return DetailsView->SetObject(inputNode);
		}

		if (UEdGraphNode_Comment* commentNode = Cast<UEdGraphNode_Comment>(*selectedNodes.begin()))
		{
			return DetailsView->SetObject(commentNode);
		}
	}

	return DetailsView->SetObject(InputSequence);;
}

void FISEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction(LOCTEXT("K2_RenameNode", "Rename Node"));
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

void FISEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
		{
			if (graphEditor.IsValid())
			{
				graphEditor->ClearSelectionSet();
				graphEditor->NotifyGraphChanged();
			}
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

void FISEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		// Clear selection, to avoid holding refs to nodes that go away
		if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
		{
			if (graphEditor.IsValid())
			{
				graphEditor->ClearSelectionSet();
				graphEditor->NotifyGraphChanged();
			}
		}
		FSlateApplication::Get().DismissAllMenus();
	}
}

FGraphPanelSelectionSet FISEditor::GetSelectedNodes() const
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

void FISEditor::SelectAllNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			graphEditor->SelectAllNodes();
		}
	}
}

void FISEditor::DeleteSelectedNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());

			graphEditor->GetCurrentGraph()->Modify();

			const FGraphPanelSelectionSet SelectedNodes = graphEditor->GetSelectedNodes();
			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
				{
					if (Node->CanUserDeleteNode())
					{
						Node->Modify();
						Node->DestroyNode();
					}
				}
			}
		}
	}
}

bool FISEditor::CanDeleteNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanUserDeleteNode()) return true;
	}

	return false;
}

void FISEditor::CopySelectedNodes()
{
	////// TODO
	//////TSet<UEdGraphNode*> pressGraphNodes;
	//////TSet<UEdGraphNode*> releaseGraphNodes;

	//////FGraphPanelSelectionSet InitialSelectedNodes = GetSelectedNodes();

	//////for (FGraphPanelSelectionSet::TIterator SelectedIter(InitialSelectedNodes); SelectedIter; ++SelectedIter)
	//////{
	//////	UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);

	//////	if (Cast<UISGraphNode_Press>(Node)) pressGraphNodes.FindOrAdd(Node);
	//////	if (Cast<UISGraphNode_Release>(Node)) releaseGraphNodes.FindOrAdd(Node);
	//////}

	//////TSet<UEdGraphNode*> graphNodesToSelect;

	//////for (UEdGraphNode* pressGraphNode : pressGraphNodes)
	//////{
	//////	for (UEdGraphPin* pin : pressGraphNode->Pins)
	//////	{
	//////		if (pin->PinType.PinCategory == UISGraphSchema::PC_Action &&
	//////			pin->LinkedTo.Num() > 0)
	//////		{
	//////			UEdGraphNode* linkedGraphNode = pin->LinkedTo[0]->GetOwningNode();

	//////			if (!releaseGraphNodes.Contains(linkedGraphNode) && !graphNodesToSelect.Contains(linkedGraphNode))
	//////			{
	//////				graphNodesToSelect.Add(linkedGraphNode);
	//////			}
	//////		}
	//////	}
	//////}

	//////for (UEdGraphNode* releaseGraphNode : releaseGraphNodes)
	//////{
	//////	for (UEdGraphPin* pin : releaseGraphNode->Pins)
	//////	{
	//////		if (pin->PinType.PinCategory == UISGraphSchema::PC_Action &&
	//////			pin->LinkedTo.Num() > 0)
	//////		{
	//////			UEdGraphNode* linkedGraphNode = pin->LinkedTo[0]->GetOwningNode();

	//////			if (!pressGraphNodes.Contains(linkedGraphNode) && !graphNodesToSelect.Contains(linkedGraphNode))
	//////			{
	//////				graphNodesToSelect.Add(linkedGraphNode);
	//////			}
	//////		}
	//////	}
	//////}

	//////if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	//////{
	//////	if (graphEditor.IsValid())
	//////	{

	//////		for (UEdGraphNode* graphNodeToSelect : graphNodesToSelect)
	//////		{
	//////			graphEditor->SetNodeSelection(graphNodeToSelect, true);
	//////		}
	//////	}
	//////}

	//////FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	//////for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	//////{
	//////	UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);

	//////	if (Node == nullptr)
	//////	{
	//////		SelectedIter.RemoveCurrent();
	//////		continue;
	//////	}

	//////	Node->PrepareForCopying();
	//////}

	//////FString ExportedText;
	//////FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
	//////FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FISEditor::CanCopyNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if (Node && Node->CanDuplicateNode()) return true;
	}

	return false;
}

void FISEditor::DeleteSelectedDuplicatableNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			const FGraphPanelSelectionSet OldSelectedNodes = graphEditor->GetSelectedNodes();
			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
				if (Node && Node->CanDuplicateNode())
				{
					graphEditor->SetNodeSelection(Node, true);
				}
			}

			DeleteSelectedNodes();

			graphEditor->ClearSelectionSet();

			for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
			{
				if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
				{
					graphEditor->SetNodeSelection(Node, true);
				}
			}
		}
	}
}

void FISEditor::PasteNodes()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			FVector2D Location = graphEditor->GetPasteLocation();

			UEdGraph* EdGraph = graphEditor->GetCurrentGraph();

			// Undo/Redo support
			const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());

			EdGraph->Modify();

			// Clear the selection set (newly pasted stuff will be selected)
			graphEditor->ClearSelectionSet();

			// Grab the text to paste from the clipboard.
			FString TextToImport;
			FPlatformApplicationMisc::ClipboardPaste(TextToImport);

			// Import the nodes
			TSet<UEdGraphNode*> PastedNodes;
			FEdGraphUtilities::ImportNodesFromText(EdGraph, TextToImport, /*out*/ PastedNodes);

			//Average position of nodes so we can move them while still maintaining relative distances to each other
			FVector2D AvgNodePosition(0.0f, 0.0f);

			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* Node = *It;
				AvgNodePosition.X += Node->NodePosX;
				AvgNodePosition.Y += Node->NodePosY;
			}

			if (PastedNodes.Num() > 0)
			{
				float InvNumNodes = 1.0f / float(PastedNodes.Num());
				AvgNodePosition.X *= InvNumNodes;
				AvgNodePosition.Y *= InvNumNodes;
			}

			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* Node = *It;

				// Select the newly pasted stuff
				graphEditor->SetNodeSelection(Node, true);

				Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X;
				Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y;

				Node->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);

				// Give new node a different Guid from the old one
				Node->CreateNewGuid();
			}

			EdGraph->NotifyGraphChanged();

			InputSequence->PostEditChange();
			InputSequence->MarkPackageDirty();
		}
	}
}

bool FISEditor::CanPasteNodes() const
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

void FISEditor::OnCreateComment()
{
	if (TSharedPtr<SGraphEditor> graphEditor = GraphEditorPtr.Pin())
	{
		if (graphEditor.IsValid())
		{
			TSharedPtr<FEdGraphSchemaAction> Action = graphEditor->GetCurrentGraph()->GetSchema()->GetCreateCommentAction();
			TSharedPtr<FISGraphSchemaAction_NewComment> newCommentAction = StaticCastSharedPtr<FISGraphSchemaAction_NewComment>(Action);

			if (newCommentAction.IsValid())
			{
				graphEditor->GetBoundsForSelectedNodes(newCommentAction->SelectedNodesBounds, 50);
				newCommentAction->PerformAction(graphEditor->GetCurrentGraph(), nullptr, FVector2D());
			}
		}
	}
}

bool FISEditor::CanCreateComment() const
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
			TSharedRef<FISEditor> NewEditor(new FISEditor());
			NewEditor->InitISEditor(Mode, EditWithinLevelEditor, inputSequence);
		}
	}
}

uint32 FAssetTypeActions_InputSequence::GetCategories() { return EAssetTypeCategories::Misc; }

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
// FEnhancedInputSequenceEditorModule
//------------------------------------------------------

const FName AssetToolsModuleName("AssetTools");

void FEnhancedInputSequenceEditorModule::StartupModule()
{
	RegisteredAssetTypeActions.Add(MakeShared<FAssetTypeActions_InputSequence>());

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsModuleName).Get();
	for (TSharedPtr<FAssetTypeActions_Base>& registeredAssetTypeAction : RegisteredAssetTypeActions)
	{
		if (registeredAssetTypeAction.IsValid()) AssetTools.RegisterAssetTypeActions(registeredAssetTypeAction.ToSharedRef());
	}

	ISGraphNodeFactory = MakeShareable(new FISGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(ISGraphNodeFactory);

	ISGraphPinFactory = MakeShareable(new FISGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(ISGraphPinFactory);

	ISGraphPinConnectionFactory = MakeShareable(new FISGraphPinConnectionFactory());
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(ISGraphPinConnectionFactory);
}

void FEnhancedInputSequenceEditorModule::ShutdownModule()
{
	FEdGraphUtilities::UnregisterVisualPinConnectionFactory(ISGraphPinConnectionFactory);
	ISGraphPinConnectionFactory.Reset();

	FEdGraphUtilities::UnregisterVisualPinFactory(ISGraphPinFactory);
	ISGraphPinFactory.Reset();

	FEdGraphUtilities::UnregisterVisualNodeFactory(ISGraphNodeFactory);
	ISGraphNodeFactory.Reset();

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
	
IMPLEMENT_MODULE(FEnhancedInputSequenceEditorModule, EnhancedInputSequenceEditor)