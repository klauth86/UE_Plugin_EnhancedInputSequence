// Fill out your copyright notice in the Description page of Project Settings.

#include "EnhancedPlayerInput_IS.h"
#include "InputSequence.h"

void UEnhancedPlayerInput_IS::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);

	TMap<FSoftObjectPath, ETriggerEvent> enhancedActionInputStack;

	for (const FEnhancedActionKeyMapping& enhancedActionMapping : GetEnhancedActionMappings())
	{
		const FSoftObjectPath inputActionPath = FSoftObjectPath(enhancedActionMapping.Action);
		const FInputActionInstance* inputActionInstance = FindActionInstanceData(enhancedActionMapping.Action);
		enhancedActionInputStack.Add(inputActionPath, inputActionInstance ? inputActionInstance->GetTriggerEvent() : ETriggerEvent::None);
	}

	TArray<FISEventCall> eventCalls;
	TArray<FISResetSource> resetSources;

	for (UInputSequence* inputSequence : InputSequences)
	{
		inputSequence->OnInput(DeltaTime, bGamePaused, enhancedActionInputStack, eventCalls, resetSources);
	}

	for (const FISEventCall& eventCall : eventCalls)
	{
		eventCall.Event->Execute(eventCall.StateGuid, eventCall.StateObject, eventCall.StateContext, resetSources);
	}
}