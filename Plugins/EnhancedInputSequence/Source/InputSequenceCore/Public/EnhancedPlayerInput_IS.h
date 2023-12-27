// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "EnhancedPlayerInput.h"
#include "EnhancedPlayerInput_IS.generated.h"

class UInputSequence;

UCLASS()
class INPUTSEQUENCECORE_API UEnhancedPlayerInput_IS : public UEnhancedPlayerInput
{
	GENERATED_BODY()

public:

	virtual void ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused) override;

protected:

	UPROPERTY(EditDefaultsOnly, Category = "IS Enhanced Player Input")
	TSet<TObjectPtr<UInputSequence>> InputSequences;
};