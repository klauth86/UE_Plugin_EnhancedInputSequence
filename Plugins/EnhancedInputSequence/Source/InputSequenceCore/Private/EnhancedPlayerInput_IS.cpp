// Fill out your copyright notice in the Description page of Project Settings.

#include "EnhancedPlayerInput_IS.h"
#include "InputSequence.h"

void UEnhancedPlayerInput_IS::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);

	TMap<UInputAction*, ETriggerEvent> enhancedActionInputStack;

	for (const FEnhancedActionKeyMapping& enhancedActionMapping : GetEnhancedActionMappings())
	{
		const FInputActionInstance* inputActionInstance = FindActionInstanceData(enhancedActionMapping.Action);
		enhancedActionInputStack.Add(const_cast<UInputAction*>(enhancedActionMapping.Action.Get()), inputActionInstance ? inputActionInstance->GetTriggerEvent() : ETriggerEvent::None);
	}

	TArray<FEventRequest> eventRequests;
	
	TArray<FResetRequest> resetRequests;

	for (UInputSequence* inputSequence : InputSequences)
	{
		inputSequence->OnInput(DeltaTime, bGamePaused, enhancedActionInputStack, eventRequests, resetRequests);
	}

	for (const FEventRequest& eventRequest : eventRequests)
	{
		eventRequest.Event->Execute(eventRequest.State, eventRequest.RequestKey, resetRequests);
	}
}