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
// UInputSequenceState_Input
//------------------------------------------------------

UInputSequenceState_Input::UInputSequenceState_Input(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	RootState = nullptr;
	NextStates.Empty();

	InputActionInfos.Empty();

	EnterEvents.Empty();
	PassEvents.Empty();
	ResetEvents.Empty();

	RequestKey = nullptr;

	bHasResetTime = 0;
	bRequirePreciseMatch = 0;
	
	bIsResetState = 0;

	ResetTime = 0;
	ResetTimeLeft = 0;
}

void UInputSequenceState_Input::Reset()
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
		for (const TObjectPtr<UInputSequenceState_Input>& entryState : EntryStates)
		{
			MakeTransition(nullptr, entryState->NextStates, outEventCalls);
		}
	}

	const TSet<TObjectPtr<UInputSequenceState_Input>> prevActiveStates = ActiveStates;

	for (const TObjectPtr<UInputSequenceState_Input>& prevActiveState : prevActiveStates)
	{
		if (UInputSequenceState_Input* state = prevActiveState.Get())
		{
			if (state->bIsResetState)
			{
				RequestReset(state, state->RequestKey, true);
			}
			else if (state->InputActionInfos.IsEmpty())
			{
				MakeTransition(state, state->NextStates, outEventCalls);
			}
			else
			{
				if (!bGamePaused || bStepWhenGamePaused)
				{
					switch (OnInput(actionStateData, state))
					{
					case EConsumeInputResponse::RESET: RequestReset(state, state->RequestKey, false); break;
					case EConsumeInputResponse::PASSED: MakeTransition(state, state->NextStates, outEventCalls); break;
					default: check(0); break;
					}
				}

				if (!bGamePaused || bTickWhenGamePaused)
				{
					switch (OnTick(deltaTime, state))
					{
					case EConsumeInputResponse::RESET: RequestReset(state, state->RequestKey, false); break;
					case EConsumeInputResponse::PASSED: MakeTransition(state, state->NextStates, outEventCalls); break;
					default: check(0); break;
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
	ResetSources[emplacedIndex].State = nullptr;
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAll = true;
}

void UInputSequence::RequestReset(const TObjectPtr<UInputSequenceState_Input> state, const TObjectPtr<URequestKey> requestKey, const bool resetAll)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].State = state;
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAll = resetAll;

	ActiveStates.Remove(state);
}

void UInputSequence::MakeTransition(const TObjectPtr<UInputSequenceState_Input> fromState, const TSet<TObjectPtr<UInputSequenceState_Input>>& toStates, TArray<FEventRequest>& outEventCalls)
{
	check(toStates.Num() > 0);

	if (UInputSequenceState_Input* state = fromState.Get())
	{
		PassState(fromState, outEventCalls);
	}

	for (const TObjectPtr<UInputSequenceState_Input>& toState : toStates)
	{
		if (UInputSequenceState_Input* state = toState.Get())
		{
			EnterState(toState, outEventCalls);

			if (toState->InputActionInfos.IsEmpty()) MakeTransition(toState, toState->NextStates, outEventCalls); // Jump through empty states
		}
	}
}

void UInputSequence::PassState(UInputSequenceState_Input* state, TArray<FEventRequest>& outEventCalls)
{
	check(!state->bIsResetState);

	if (ActiveStates.Contains(state))
	{
		ActiveStates.Remove(state);

		for (const TObjectPtr<UInputSequenceEvent>& passEvent : state->PassEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].State = state;
			outEventCalls[emplacedIndex].RequestKey = state->RequestKey;
			outEventCalls[emplacedIndex].InputSequenceEvent = passEvent;
		}
	}
}

void UInputSequence::EnterState(UInputSequenceState_Input* state, TArray<FEventRequest>& outEventCalls)
{
	check(!state->bIsResetState);

	if (!ActiveStates.Contains(state))
	{
		for (const TObjectPtr<UInputSequenceEvent>& enterEvent : state->EnterEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].State = state;
			outEventCalls[emplacedIndex].RequestKey = state->RequestKey;
			outEventCalls[emplacedIndex].InputSequenceEvent = enterEvent;
		}

		state->Reset();

		ActiveStates.Add(state);
	}
}

EConsumeInputResponse UInputSequence::OnInput(const TMap<UInputAction*, ETriggerEvent>& actionStateDatas, UInputSequenceState_Input* state)
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

EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, UInputSequenceState_Input* state)
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
	TSet<TObjectPtr<UInputSequenceState_Input>> requestingStates;

	{
		FScopeLock Lock(&resetSourcesCS);

		outResetSources = ResetSources;

		for (const FResetRequest& resetSource : ResetSources)
		{
			resetAll |= resetSource.bResetAll;
			
			if (!resetSource.bResetAll) // Request by some state
			{
				requestingStates.FindOrAdd(resetSource.State);
			}
		}
	}

	if (resetAll)
	{
		const TSet<TObjectPtr<UInputSequenceState_Input>> prevActiveStates = ActiveStates;
		ProcessResetSources_Internal(prevActiveStates, outEventCalls);
	}
	else
	{
		ProcessResetSources_Internal(requestingStates, outEventCalls);
	}
}

void UInputSequence::ProcessResetSources_Internal(const TSet<TObjectPtr<UInputSequenceState_Input>>& statesToReset, TArray<FEventRequest>& outEventCalls)
{
	for (const TObjectPtr<UInputSequenceState_Input>& stateToReset : statesToReset)
	{
		if (ActiveStates.Contains(stateToReset))
		{
			if (UInputSequenceState_Input* state = stateToReset.Get())
			{
				// States are reset to NONE state and after that we make transition from NONE state

				ActiveStates.Remove(state);

				for (const TObjectPtr<UInputSequenceEvent>& resetEvent : state->ResetEvents)
				{
					int32 emplacedIndex = outEventCalls.Emplace();
					outEventCalls[emplacedIndex].State = state;
					outEventCalls[emplacedIndex].RequestKey = state->RequestKey;
					outEventCalls[emplacedIndex].InputSequenceEvent = resetEvent;
				}

				MakeTransition(nullptr, { state->RootState }, outEventCalls);
			}
		}
	}
}