// Copyright 2023 Pentangle Studio under EULA https ://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "InputSequenceGraphNodes.generated.h"

class URequestKey;
class UInputAction;
class UInputSequence;
class UInputSequenceEvent;
enum class ETriggerEvent :uint8;

//------------------------------------------------------
// UInputSequenceGraphNode_Entry
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphNode_Entry : public UEdGraphNode
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
// UInputSequenceGraphNode_Base
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphNode_Base : public UEdGraphNode
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
// UInputSequenceGraphNode_Dynamic
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphNode_Dynamic : public UInputSequenceGraphNode_Base
{
	GENERATED_BODY()

public:

	DECLARE_DELEGATE(FInvalidateWidgetEvent);

	FInvalidateWidgetEvent OnUpdateGraphNode;
};

//------------------------------------------------------
// UInputSequenceGraphNode_Input
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphNode_Input : public UInputSequenceGraphNode_Dynamic
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UInputSequenceGraphNode_Hub
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphNode_Hub : public UInputSequenceGraphNode_Dynamic
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UInputSequenceGraphNode_Reset
//------------------------------------------------------

UCLASS()
class UInputSequenceGraphNode_Reset : public UInputSequenceGraphNode_Base
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};