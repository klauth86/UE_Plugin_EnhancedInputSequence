// Copyright 2023 Pentangle Studio under EULA https ://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "EdGraphUtilities.h"
#include "ISGraphSchema.generated.h"

class UInputAction;

//------------------------------------------------------
// FISGraphNodeFactory
//------------------------------------------------------

struct FISGraphNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* InNode) const override;
};

//------------------------------------------------------
// FISGraphPinFactory
//------------------------------------------------------

struct FISGraphPinFactory : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* Pin) const override;
};

//------------------------------------------------------
// FISGraphPinConnectionFactory
//------------------------------------------------------

struct FISGraphPinConnectionFactory : public FGraphPanelPinConnectionFactory
{
public:
	virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const class FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj) const override;
};

//------------------------------------------------------
// FISGraphSchemaAction_NewComment
//------------------------------------------------------

USTRUCT()
struct FISGraphSchemaAction_NewComment : public FEdGraphSchemaAction
{
	GENERATED_BODY();

	FISGraphSchemaAction_NewComment() : FEdGraphSchemaAction() {}

	FISGraphSchemaAction_NewComment(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	FSlateRect SelectedNodesBounds;
};

//------------------------------------------------------
// FISGraphSchemaAction_NewNode
//------------------------------------------------------

USTRUCT()
struct FISGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	UEdGraphNode* NodeTemplate;

	FISGraphSchemaAction_NewNode() : FEdGraphSchemaAction(), NodeTemplate(nullptr) {}

	FISGraphSchemaAction_NewNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping), NodeTemplate(nullptr)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

//------------------------------------------------------
// FISGraphSchemaAction_AddPin
//------------------------------------------------------

USTRUCT()
struct FISGraphSchemaAction_AddPin : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	TObjectPtr<UInputAction> InputAction;

	int32 InputIndex;
	int32 CorrectedInputIndex;

	FISGraphSchemaAction_AddPin() : FEdGraphSchemaAction(), InputAction(nullptr), InputIndex(INDEX_NONE), CorrectedInputIndex(INDEX_NONE) {}

	FISGraphSchemaAction_AddPin(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, int32 InSectionID)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, FText::GetEmpty(), InSectionID), InputAction(nullptr), InputIndex(INDEX_NONE), CorrectedInputIndex(INDEX_NONE)
	{}

	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
};

//------------------------------------------------------
// UISGraphSchema
//------------------------------------------------------

UCLASS()
class UISGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:

	static const FName PC_Exec;

	static const FName PC_Add;

	static const FName PC_InputAction;

	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* pinA, const UEdGraphPin* pinB) const override;

	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override { return FLinearColor::White; }

	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;

	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;
};