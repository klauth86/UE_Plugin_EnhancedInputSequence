// Fill out your copyright notice in the Description page of Project Settings.

#include "EnhancedPlayerInput_IS.h"
#include "InputSequence.h"

void UEnhancedPlayerInput_IS::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);

	//////TMap<TObjectPtr<const UInputAction>, ETriggerState> actionStateData;

	//////for (TPair<TObjectPtr<const UInputAction>, FInputActionInstance>& ActionPair : ActionInstanceData)
	//////{
	//////	FInputActionInstance& ActionData = ActionPair.Value;

	//////	if (ActionData.TriggerEvent != ETriggerEvent::None)
	//////	{
	//////		actionStateData.Add(ActionPair.Key, ActionData.TriggerEvent);
	//////	}
	//////}

	//////for (const TObjectPtr<UInputSequence>& inputSequence : InputSequences)
	//////{
	//////	inputSequence->OnInput(DeltaTime, bGamePaused, actionStateData);
	//////}
}