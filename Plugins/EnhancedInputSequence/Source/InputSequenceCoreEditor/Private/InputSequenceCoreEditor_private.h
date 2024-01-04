// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "EdGraphUtilities.h"
#include "InputSequenceCoreEditor_private.generated.h"

class UInputSequenceRequestKey;
class UInputAction;
class UInputSequence;
class UInputSequenceEvent;
enum class ETriggerEvent :uint8;

//------------------------------------------------------
// UFactory_InputSequence
//------------------------------------------------------

UCLASS()
class UFactory_InputSequence : public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

//------------------------------------------------------
// UFactory_InputSequenceRequestKey
//------------------------------------------------------

UCLASS()
class UFactory_InputSequenceRequestKey : public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphNode_Entry
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphNode_Entry : public UEdGraphNode
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;

	virtual bool CanDuplicateNode() const { return false; }

	virtual bool CanUserDeleteNode() const { return false; }
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphNode_Base
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphNode_Base : public UEdGraphNode
{
	GENERATED_BODY()

public:

	virtual void PrepareForCopying() override;

	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;

	UEdGraphPin* GetExecPin(EEdGraphPinDirection direction) const;

	UPROPERTY()
	TObjectPtr<UInputSequence> PrevOwningAsset;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphNode_Dynamic
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphNode_Dynamic : public UEnhancedInputSequenceGraphNode_Base
{
	GENERATED_BODY()

public:

	DECLARE_DELEGATE(FInvalidateWidgetEvent);

	FInvalidateWidgetEvent OnUpdateGraphNode;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphNode_Input
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphNode_Input : public UEnhancedInputSequenceGraphNode_Dynamic
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphNode_Hub
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphNode_Hub : public UEnhancedInputSequenceGraphNode_Dynamic
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphNode_Reset
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphNode_Reset : public UEnhancedInputSequenceGraphNode_Base
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraph
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()
};

//------------------------------------------------------
// FEnhancedInputSequenceGraphNodeFactory
//------------------------------------------------------

struct FEnhancedInputSequenceGraphNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override;
};

//------------------------------------------------------
// FEnhancedInputSequenceGraphPinFactory
//------------------------------------------------------

struct FEnhancedInputSequenceGraphPinFactory : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* Pin) const override;
};

//------------------------------------------------------
// FEnhancedInputSequenceGraphPinConnectionFactory
//------------------------------------------------------

struct FEnhancedInputSequenceGraphPinConnectionFactory : public FGraphPanelPinConnectionFactory
{
public:
	virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const override;
};

//------------------------------------------------------
// FEnhancedInputSequenceGraphSchemaAction_NewComment
//------------------------------------------------------

USTRUCT()
struct FEnhancedInputSequenceGraphSchemaAction_NewComment : public FEdGraphSchemaAction
{
	GENERATED_BODY();

	FEnhancedInputSequenceGraphSchemaAction_NewComment() : FEdGraphSchemaAction() {}

	FEnhancedInputSequenceGraphSchemaAction_NewComment(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	FSlateRect SelectedNodesBounds;
};

//------------------------------------------------------
// FEnhancedInputSequenceGraphSchemaAction_NewNode
//------------------------------------------------------

USTRUCT()
struct FEnhancedInputSequenceGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	UEdGraphNode* NodeTemplate;

	FEnhancedInputSequenceGraphSchemaAction_NewNode() : FEdGraphSchemaAction(), NodeTemplate(nullptr) {}

	FEnhancedInputSequenceGraphSchemaAction_NewNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping), NodeTemplate(nullptr)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

//------------------------------------------------------
// FEnhancedInputSequenceGraphSchemaAction_AddPin
//------------------------------------------------------

USTRUCT()
struct FEnhancedInputSequenceGraphSchemaAction_AddPin : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	TObjectPtr<UInputAction> InputAction;

	FEnhancedInputSequenceGraphSchemaAction_AddPin() : FEdGraphSchemaAction(), InputAction(nullptr) {}

	FEnhancedInputSequenceGraphSchemaAction_AddPin(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, int32 InSectionID)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, FText::GetEmpty(), InSectionID), InputAction(nullptr)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
};

//------------------------------------------------------
// UEnhancedInputSequenceGraphSchema
//------------------------------------------------------

UCLASS()
class UEnhancedInputSequenceGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:

	static const FName PC_Exec;

	static const FName PC_Input;

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* pinA, const UEdGraphPin* pinB) const override;

	virtual bool TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;

	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;

	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;

	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override { return FLinearColor::White; }

	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;

	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;
};