// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "EnhancedPlayerInput.h"
#include "EnhancedPlayerInput_IS.generated.h"

class UInputSequence;

UCLASS()
class ENHANCEDINPUTSEQUENCE_API UEnhancedPlayerInput_IS : public UEnhancedPlayerInput
{
	GENERATED_BODY()

public:

	virtual void ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused) override;

protected:

	UPROPERTY(EditDefaultsOnly, Category = "IS Enhanced Player Input")
	TSet<TObjectPtr<UInputSequence>> InputSequences;
};