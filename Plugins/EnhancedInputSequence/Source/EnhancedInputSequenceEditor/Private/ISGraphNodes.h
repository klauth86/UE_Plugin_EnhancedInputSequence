// Copyright 2023 Pentangle Studio under EULA https ://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "ISGraphNodes.generated.h"

class UInputAction;
enum class ETriggerEvent :uint8;

//------------------------------------------------------
// UISGraphNode_Entry
//------------------------------------------------------

UCLASS()
class UISGraphNode_Entry : public UEdGraphNode
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
// UISGraphNode_Input
//------------------------------------------------------

UCLASS()
class UISGraphNode_Dynamic : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

public:

	DECLARE_DELEGATE(FInvalidateWidgetEvent);

	FInvalidateWidgetEvent OnUpdateGraphNode;

	UPROPERTY()
	float ResetTime;

	UPROPERTY()
	uint8 bOverrideResetTime : 1;
};

//------------------------------------------------------
// UISGraphNode_Input
//------------------------------------------------------

UCLASS()
class UISGraphNode_Input : public UISGraphNode_Dynamic
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UISGraphNode_Hub
//------------------------------------------------------

UCLASS()
class UISGraphNode_Hub : public UISGraphNode_Dynamic
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};

//------------------------------------------------------
// UISGraphNode_Reset
//------------------------------------------------------

UCLASS()
class UISGraphNode_Reset : public UEdGraphNode
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual FLinearColor GetNodeTitleColor() const override;

	virtual FText GetTooltipText() const override;
};