// Copyright 2023 Pentangle Studio under EULA https ://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "EdGraphUtilities.h"
#include "InputSequenceGraphSchema.generated.h"

class UInputAction;

//------------------------------------------------------
// UInputSequenceGraph
//------------------------------------------------------

UCLASS()
class UInputSequenceGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()
};

//------------------------------------------------------
// FInputSequenceGraphNodeFactory
//------------------------------------------------------

struct FInputSequenceGraphNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override;
};

//------------------------------------------------------
// FInputSequenceGraphPinFactory
//------------------------------------------------------

struct FInputSequenceGraphPinFactory : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* Pin) const override;
};

//------------------------------------------------------
// FInputSequenceGraphPinConnectionFactory
//------------------------------------------------------

struct FInputSequenceGraphPinConnectionFactory : public FGraphPanelPinConnectionFactory
{
public:
	virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const override;
};

//------------------------------------------------------
// FInputSequenceGraphSchemaAction_NewComment
//------------------------------------------------------

USTRUCT()
struct FInputSequenceGraphSchemaAction_NewComment : public FEdGraphSchemaAction
{
	GENERATED_BODY();

	FInputSequenceGraphSchemaAction_NewComment() : FEdGraphSchemaAction() {}

	FInputSequenceGraphSchemaAction_NewComment(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	FSlateRect SelectedNodesBounds;
};

//------------------------------------------------------
// FInputSequenceGraphSchemaAction_NewNode
//------------------------------------------------------

USTRUCT()
struct FInputSequenceGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	UEdGraphNode* NodeTemplate;

	FInputSequenceGraphSchemaAction_NewNode() : FEdGraphSchemaAction(), NodeTemplate(nullptr) {}

	FInputSequenceGraphSchemaAction_NewNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping), NodeTemplate(nullptr)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

//------------------------------------------------------
// FInputSequenceGraphSchemaAction_AddPin
//------------------------------------------------------

USTRUCT()
struct FInputSequenceGraphSchemaAction_AddPin : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	TObjectPtr<UInputAction> InputAction;

	FInputSequenceGraphSchemaAction_AddPin() : FEdGraphSchemaAction(), InputAction(nullptr) {}

	FInputSequenceGraphSchemaAction_AddPin(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, int32 InSectionID)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, FText::GetEmpty(), InSectionID), InputAction(nullptr)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
};

//------------------------------------------------------
// UInputSequenceGraphSchema
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:

	static const FName PC_Exec;

	static const FName PC_Input;

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* pinA, const UEdGraphPin* pinB) const override;

	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override { return FLinearColor::White; }

	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;

	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;
};