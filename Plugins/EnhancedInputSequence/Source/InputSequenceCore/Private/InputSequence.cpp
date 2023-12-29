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
// UInputSequenceState_Base
//------------------------------------------------------

UInputSequenceState_Base::UInputSequenceState_Base(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	RootState = nullptr;
	NextStates.Empty();
}

//------------------------------------------------------
// UInputSequenceState_Hub
//------------------------------------------------------

UInputSequenceState_Hub::UInputSequenceState_Hub(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
}

//------------------------------------------------------
// UInputSequenceState_Reset
//------------------------------------------------------

UInputSequenceState_Reset::UInputSequenceState_Reset(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	RequestKey = nullptr;
}

//------------------------------------------------------
// UInputSequenceState_Input
//------------------------------------------------------

UInputSequenceState_Input::UInputSequenceState_Input(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	InputActionInfos.Empty();

	EnterEvents.Empty();
	PassEvents.Empty();
	ResetEvents.Empty();

	RequestKey = nullptr;

	bOverrideResetTime = 0;
	bRequirePreciseMatch = 0;

	ResetTime = 0;
	ResetTimeLeft = 0;
}

void UInputSequenceState_Input::OnEnter(TArray<FEventRequest>& outEventCalls)
{
	for (const TObjectPtr<UInputSequenceEvent>& enterEvent : EnterEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].InputSequenceEvent = enterEvent;
	}

	Reset();
}

void UInputSequenceState_Input::OnPass(TArray<FEventRequest>& outEventCalls)
{
	for (const TObjectPtr<UInputSequenceEvent>& passEvent : PassEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].InputSequenceEvent = passEvent;
	}
}

void UInputSequenceState_Input::OnReset(TArray<FEventRequest>& outEventCalls)
{
	for (const TObjectPtr<UInputSequenceEvent>& resetEvent : ResetEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].InputSequenceEvent = resetEvent;
	}
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
		for (const TObjectPtr<UInputSequenceState_Base>& entryState : EntryStates)
		{
			MakeTransition(nullptr, entryState->NextStates, outEventCalls);
		}
	}

	const TSet<TObjectPtr<UInputSequenceState_Base>> prevActiveStates = ActiveStates;

	for (const TObjectPtr<UInputSequenceState_Base>& prevActiveState : prevActiveStates)
	{
		if (UInputSequenceState_Base* state = prevActiveState.Get())
		{
			if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(state))
			{
				if (!bGamePaused || bStepWhenGamePaused)
				{
					switch (OnInput(actionStateData, inputState))
					{
					case EConsumeInputResponse::RESET: RequestReset(inputState, inputState->RequestKey, false); break;
					case EConsumeInputResponse::PASSED: MakeTransition(inputState, inputState->NextStates, outEventCalls); break;
					default: check(0); break;
					}
				}

				if (!bGamePaused || bTickWhenGamePaused)
				{
					switch (OnTick(deltaTime, inputState))
					{
					case EConsumeInputResponse::RESET: RequestReset(inputState, inputState->RequestKey, false); break;
					case EConsumeInputResponse::PASSED: MakeTransition(inputState, inputState->NextStates, outEventCalls); break;
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

void UInputSequence::RequestReset(const TObjectPtr<UInputSequenceState_Base> state, const TObjectPtr<URequestKey> requestKey, const bool resetAll)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].State = state;
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAll = resetAll;

	ActiveStates.Remove(state);
}

void UInputSequence::MakeTransition(const TObjectPtr<UInputSequenceState_Base> fromState, const TSet<TObjectPtr<UInputSequenceState_Base>>& toStates, TArray<FEventRequest>& outEventCalls)
{
	check(toStates.Num() > 0);

	if (UInputSequenceState_Base* state = fromState.Get())
	{
		PassState(fromState, outEventCalls);
	}

	for (const TObjectPtr<UInputSequenceState_Base>& toState : toStates)
	{
		if (UInputSequenceState_Base* state = toState.Get())
		{
			EnterState(toState, outEventCalls);
		}
	}
}

void UInputSequence::EnterState(UInputSequenceState_Base* state, TArray<FEventRequest>& outEventCalls)
{
	if (!ActiveStates.Contains(state))
	{
		state->OnEnter(outEventCalls);
		ActiveStates.Add(state);

		if (UInputSequenceState_Reset* resetState = Cast<UInputSequenceState_Reset>(state))
		{
			RequestReset(resetState, resetState->RequestKey, true);
		}
		else if (UInputSequenceState_Hub* hubState = Cast<UInputSequenceState_Hub>(state))
		{
			MakeTransition(hubState, hubState->NextStates, outEventCalls);
		}
		else if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(state))
		{
			if (inputState->InputActionInfos.IsEmpty()) MakeTransition(inputState, inputState->NextStates, outEventCalls); // Jump through empty states
		}
	}
}

void UInputSequence::PassState(UInputSequenceState_Base* state, TArray<FEventRequest>& outEventCalls)
{
	if (ActiveStates.Contains(state))
	{
		ActiveStates.Remove(state);
		state->OnPass(outEventCalls);
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
	TSet<TObjectPtr<UInputSequenceState_Base>> statesToReset;

	{
		FScopeLock Lock(&resetSourcesCS);

		outResetSources = ResetSources;

		for (const FResetRequest& resetSource : ResetSources)
		{
			resetAll |= resetSource.bResetAll;
			
			if (resetAll) break;

			if (!resetSource.bResetAll)
			{
				statesToReset.FindOrAdd(resetSource.State);
			}
		}
	}

	if (resetAll)
	{
		for (const TObjectPtr<UInputSequenceState_Base>& stateToReset : ActiveStates)
		{
			if (UInputSequenceState_Base* state = stateToReset.Get())
			{
				ActiveStates.Remove(state);
				state->OnReset(outEventCalls);
			}
		}

		ActiveStates.Empty();
	}
	else
	{
		for (const TObjectPtr<UInputSequenceState_Base>& stateToReset : statesToReset)
		{
			if (ActiveStates.Contains(stateToReset))
			{
				if (UInputSequenceState_Base* state = stateToReset.Get())
				{
					ActiveStates.Remove(state);
					state->OnReset(outEventCalls);

					MakeTransition(nullptr, { state->RootState }, outEventCalls);
				}
			}
		}
	}
}