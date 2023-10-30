// Copyright 2023 Pentangle Studio under EULA https ://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EdGraph/EdGraph.h"
#include "ISGraph.generated.h"

class UInputSequence;

//------------------------------------------------------
// UISGraph
//------------------------------------------------------

UCLASS()
class UISGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:

	void SetInputSequence(UInputSequence* inputSequence) { InputSequence = inputSequence; }

protected:

	UPROPERTY()
	UInputSequence* InputSequence;
};