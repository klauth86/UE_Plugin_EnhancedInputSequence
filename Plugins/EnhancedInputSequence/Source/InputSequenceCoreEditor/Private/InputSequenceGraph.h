// Copyright 2023 Pentangle Studio under EULA https ://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraph.h"
#include "InputSequenceGraph.generated.h"

class UInputSequence;

//------------------------------------------------------
// UInputSequenceGraph
//------------------------------------------------------

UCLASS()
class UInputSequenceGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:

	void SetInputSequence(UInputSequence* inputSequence) { InputSequence = inputSequence; }

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

protected:

	UPROPERTY()
	UInputSequence* InputSequence;
};