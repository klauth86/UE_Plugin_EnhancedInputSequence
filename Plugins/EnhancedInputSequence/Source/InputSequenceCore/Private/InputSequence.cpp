// Fill out your copyright notice in the Description page of Project Settings.

#include "InputSequence.h"

//------------------------------------------------------
// UInputSequenceEvent
//------------------------------------------------------

UWorld* UInputSequenceEvent::GetWorld() const
{
	if (UInputSequence* inputSequence = GetTypedOuter<UInputSequence>())
	{
		return inputSequence->GetWorld();
	}

	return nullptr;
}

//------------------------------------------------------
// FInputActionInfo
//------------------------------------------------------

FInputActionInfo::FInputActionInfo()
{
	TriggerEvent = ETriggerEvent::None;
	bIsPassed = 0;
	WaitTime = 0;
	WaitTimeLeft = 0;
}

void FInputActionInfo::Reset()
{
	bIsPassed = 0;
	WaitTimeLeft = WaitTime;
}

//------------------------------------------------------
// FInputSequenceState
//------------------------------------------------------

void FInputSequenceState::Reset()
{
	for (TPair<UInputAction*, FInputActionInfo>& inputActionInfoEntry : InputActionInfos)
	{
		inputActionInfoEntry.Value.Reset();
	}
}

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<UInputAction*, ETriggerEvent>& actionStateData, TArray<FEventRequest>& outEventCalls, TArray<FResetRequest>& outResetSources)
{
	if (ActiveStates.IsEmpty())
	{
		ActiveStates = EntryStates;
	}

	const TSet<FGuid> prevActiveStates = ActiveStates;

	for (const FGuid& stateGuid : prevActiveStates)
	{
		FInputSequenceState key;
		if (FInputSequenceState* state = States.FindByHash(GetTypeHash(stateGuid), key))
		{
			if (state->bIsResetState)
			{
				RequestReset(stateGuid, state->RequestKey, true);
			}
			else
			{
				if (!bGamePaused || bStepWhenGamePaused)
				{
					const EConsumeInputResponse consumeInputResponse = OnInput(actionStateData, state);

					if (consumeInputResponse == EConsumeInputResponse::RESET)
					{
						RequestReset(stateGuid, state->RequestKey, false);
					}
					else if (consumeInputResponse == EConsumeInputResponse::PASSED)
					{
						MakeTransition(stateGuid, GetTransitions(stateGuid), outEventCalls);
					}
				}

				if (!bGamePaused || bTickWhenGamePaused)
				{
					if (ActiveStates.Contains(stateGuid)) // Tick on states that already were active before this frame
					{
						const EConsumeInputResponse tickResponse = OnTick(deltaTime, state);

						if (tickResponse == EConsumeInputResponse::RESET)
						{
							RequestReset(stateGuid, state->RequestKey, false);
						}
						else if (tickResponse == EConsumeInputResponse::PASSED)
						{
							MakeTransition(stateGuid, GetTransitions(stateGuid), outEventCalls);
						}
					}
				}
			}
		}
	}

	ProcessResetSources(outEventCalls, outResetSources);
}

void UInputSequence::RequestReset(URequestKey* requestKey)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].StateGuid.Invalidate();
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAsset = true;
}

void UInputSequence::RequestReset(const FGuid& stateGuid, URequestKey* requestKey, const bool resetAsset)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].StateGuid = stateGuid;
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAsset = resetAsset;

	ActiveStates.Remove(stateGuid);
}

void UInputSequence::MakeTransition(const FGuid& fromStateGuid, const TSet<FGuid>& toStateGuids, TArray<FEventRequest>& outEventCalls)
{
	check(toStateGuids.Num() > 0);

	FInputSequenceState key;
	if (FInputSequenceState* fromState = States.FindByHash(GetTypeHash(fromStateGuid), key))
	{
		PassState(fromState, outEventCalls);
	}

	for (const FGuid& toStateGuid : toStateGuids)
	{
		if (FInputSequenceState* toState = States.FindByHash(GetTypeHash(toStateGuid), key))
		{
			EnterState(toState, outEventCalls);

			if (toState->InputActionInfos.IsEmpty()) MakeTransition(toStateGuid, GetTransitions(toStateGuid), outEventCalls); // Jump through empty states
		}
	}
}

void UInputSequence::PassState(FInputSequenceState* state, TArray<FEventRequest>& outEventCalls)
{
	check(!state->bIsResetState);

	if (ActiveStates.Contains(state->StateGuid))
	{
		ActiveStates.Remove(state->StateGuid);

		for (const TObjectPtr<UInputSequenceEvent>& passEvent : state->PassEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].RequestKey = state->RequestKey;
			outEventCalls[emplacedIndex].InputSequenceEvent = passEvent;
		}
	}
}

void UInputSequence::EnterState(FInputSequenceState* state, TArray<FEventRequest>& outEventCalls)
{
	check(!state->bIsResetState);

	if (!ActiveStates.Contains(state->StateGuid))
	{
		for (const TObjectPtr<UInputSequenceEvent>& enterEvent : state->EnterEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].RequestKey = state->RequestKey;
			outEventCalls[emplacedIndex].InputSequenceEvent = enterEvent;
		}

		state->Reset();

		ActiveStates.Add(state->StateGuid);
	}
}

EConsumeInputResponse UInputSequence::OnInput(const TMap<UInputAction*, ETriggerEvent>& actionStateDatas, FInputSequenceState* state)
{
	// Check Wait Time

	for (const TPair<UInputAction*, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
	{
		if (inputActionInfoEntry.Value.WaitTimeLeft > 0)
		{
			if (!actionStateDatas.Contains(inputActionInfoEntry.Key))
			{
				return EConsumeInputResponse::RESET;
			}
			else if (*actionStateDatas.Find(inputActionInfoEntry.Key) != inputActionInfoEntry.Value.TriggerEvent)
			{
				return EConsumeInputResponse::RESET;
			}
		}
	}

	for (const TPair<UInputAction*, ETriggerEvent>& actionStateData : actionStateDatas)
	{		
		const ETriggerEvent triggerEvent = actionStateData.Value;

		if (state->InputActionInfos.Contains(actionStateData.Key))
		{
			FInputActionInfo* inputActionInfo = state->InputActionInfos.Find(actionStateData.Key);

			 // Precise Match

			if (inputActionInfo->RequirePreciseMatch())
			{
				if (inputActionInfo->TriggerEvent != triggerEvent)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			if (inputActionInfo->TriggerEvent == triggerEvent && inputActionInfo->WaitTimeLeft == 0)
			{
				inputActionInfo->SetIsPassed();

				for (const TPair<UInputAction*, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
				{
					if (!inputActionInfoEntry.Value.IsPassed())
					{
						return EConsumeInputResponse::NONE;
					}
				}

				return EConsumeInputResponse::PASSED;
			}
		}
	}

	return EConsumeInputResponse::NONE;
}

EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, FInputSequenceState* state)
{
	// Tick Input Action Infos

	for (TPair<UInputAction*, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
	{
		if (inputActionInfoEntry.Value.WaitTimeLeft > 0)
		{
			inputActionInfoEntry.Value.WaitTimeLeft = FMath::Max(inputActionInfoEntry.Value.WaitTimeLeft - deltaTime, 0);
		}
	}

	// Check Reset Time

	if (state->ResetTimeLeft > 0)
	{
		state->ResetTimeLeft = FMath::Max(state->ResetTimeLeft - deltaTime, 0);

		if (state->ResetTimeLeft == 0)
		{
			return EConsumeInputResponse::RESET;
		}
	}

	return EConsumeInputResponse::NONE;
}

void UInputSequence::ProcessResetSources(TArray<FEventRequest>& outEventCalls, TArray<FResetRequest>& outResetSources)
{
	bool resetAll = false;
	TSet<FGuid> requestStates;

	{
		FScopeLock Lock(&resetSourcesCS);

		outResetSources = ResetSources;

		for (const FResetRequest& resetSource : ResetSources)
		{
			resetAll |= resetSource.bResetAsset;
			
			if (!resetSource.bResetAsset) // Request by some state
			{
				requestStates.FindOrAdd(resetSource.StateGuid);
			}
		}
	}

	if (resetAll)
	{
		const TSet<FGuid> prevActiveStates = ActiveStates;
		ProcessResetSources_Internal(prevActiveStates, outEventCalls);
	}
	else
	{
		ProcessResetSources_Internal(requestStates, outEventCalls);
	}
}

void UInputSequence::ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FEventRequest>& outEventCalls)
{
	for (const FGuid& stateGuid : statesToReset)
	{
		if (ActiveStates.Contains(stateGuid))
		{
			FInputSequenceState key;
			if (FInputSequenceState* state = States.FindByHash(GetTypeHash(stateGuid), key))
			{
				// States are reset to NONE state and after that we make transition from NONE state

				ActiveStates.Remove(state->StateGuid);

				for (const TObjectPtr<UInputSequenceEvent>& resetEvent : state->ResetEvents)
				{
					int32 emplacedIndex = outEventCalls.Emplace();
					outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
					outEventCalls[emplacedIndex].RequestKey = state->RequestKey;
					outEventCalls[emplacedIndex].InputSequenceEvent = resetEvent;
				}

				MakeTransition(FGuid(), { state->RootStateGuid }, outEventCalls);
			}
		}
	}
}