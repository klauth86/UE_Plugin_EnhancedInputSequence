// Fill out your copyright notice in the Description page of Project Settings.

#include "InputSequence.h"

//------------------------------------------------------
// UISEvent
//------------------------------------------------------

UWorld* UISEvent::GetWorld() const
{
	if (UInputSequence* inputSequence = GetTypedOuter<UInputSequence>())
	{
		return inputSequence->GetWorld();
	}

	return nullptr;
}

//------------------------------------------------------
// FISState
//------------------------------------------------------

void FISState::Reset()
{
	for (TPair<FSoftObjectPath, FISInputActionInfo>& InputActionInfo : InputActionInfos)
	{
		InputActionInfo.Value.InputActionInfoFlags &= ~EFlags_InputActionInfo::PASSED;
		InputActionInfo.Value.WaitTimeLeft = InputActionInfo.Value.WaitTime;
	}
}

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<FSoftObjectPath, ETriggerEvent>& actionStateData, TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources)
{
	if (ActiveStates.IsEmpty())
	{
		ActiveStates = EntryStates;
	}

	const TSet<FGuid> prevActiveStates = ActiveStates;

	for (const FGuid& stateGuid : prevActiveStates)
	{
		FISState key;
		if (FISState* state = States.FindByHash(GetTypeHash(stateGuid), key))
		{
			if ((state->StateFlags & EFlags_State::IS_RESET_STATE) != EFlags::NONE)
			{
				RequestReset(stateGuid, true);
			}
			else
			{
				if (!bGamePaused || bStepWhenGamePaused)
				{
					EConsumeInputResponse consumeInputResponse = OnInput(actionStateData, state);

					if (consumeInputResponse == EConsumeInputResponse::RESET)
					{
						RequestReset(stateGuid, false);
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
						EConsumeInputResponse tickResponse = OnTick(deltaTime, state);

						if (tickResponse == EConsumeInputResponse::RESET)
						{
							RequestReset(stateGuid, false);
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

void UInputSequence::RequestReset(UObject* requestObject, const FString& requestContext)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].bResetAsset = true;
	ResetSources[emplacedIndex].RequestObject = requestObject;
	ResetSources[emplacedIndex].RequestContext = requestContext;
}

void UInputSequence::RequestReset(const FGuid& stateGuid, const bool resetAsset)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].StateGuid = stateGuid;
	ResetSources[emplacedIndex].bResetAsset = resetAsset;

	ActiveStates.Remove(stateGuid);
}

void UInputSequence::MakeTransition(const FGuid& fromStateGuid, const TSet<FGuid>& toStateGuids, TArray<FISEventCall>& outEventCalls)
{
	check(toStateGuids.Num() > 0);

	FISState key;
	if (FISState* fromState = States.FindByHash(GetTypeHash(fromStateGuid), key))
	{
		PassState(fromState, outEventCalls);
	}

	for (const FGuid& toStateGuid : toStateGuids)
	{
		if (FISState* toState = States.FindByHash(GetTypeHash(toStateGuid), key))
		{
			EnterState(toState, outEventCalls);

			if (toState->InputActionInfos.IsEmpty()) MakeTransition(toStateGuid, GetTransitions(toStateGuid), outEventCalls); // Jump through empty states
		}
	}
}

void UInputSequence::PassState(FISState* state, TArray<FISEventCall>& outEventCalls)
{
	check((state->StateFlags & EFlags_State::IS_RESET_STATE) == EFlags::NONE);

	if (ActiveStates.Contains(state->StateGuid))
	{
		ActiveStates.Remove(state->StateGuid);

		for (const TObjectPtr<UISEvent>& passEvent : state->PassEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].Event = passEvent;
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].StateObject = state->StateObject;
			outEventCalls[emplacedIndex].StateContext = state->StateContext;
		}
	}
}

void UInputSequence::EnterState(FISState* state, TArray<FISEventCall>& outEventCalls)
{
	check((state->StateFlags & EFlags_State::IS_RESET_STATE) == EFlags::NONE);

	if (!ActiveStates.Contains(state->StateGuid))
	{
		for (const TObjectPtr<UISEvent>& enterEvent : state->EnterEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].Event = enterEvent;
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].StateObject = state->StateObject;
			outEventCalls[emplacedIndex].StateContext = state->StateContext;
		}

		state->Reset();

		ActiveStates.Add(state->StateGuid);
	}
}

EConsumeInputResponse UInputSequence::OnInput(const TMap<FSoftObjectPath, ETriggerEvent>& actionStateDatas, FISState* state)
{

	// Check Wait Time

	for (const TPair<FSoftObjectPath, FISInputActionInfo>& inputActionInfo : state->InputActionInfos)
	{
		const FSoftObjectPath inputActionPath = inputActionInfo.Key;

		if (inputActionInfo.Value.WaitTimeLeft > 0)
		{
			if (!actionStateDatas.Contains(inputActionPath))
			{
				return EConsumeInputResponse::RESET;
			}
			else if (*actionStateDatas.Find(inputActionPath) != inputActionInfo.Value.TriggerEvent)
			{
				return EConsumeInputResponse::RESET;
			}
		}
	}

	for (const TPair<FSoftObjectPath, ETriggerEvent>& actionStateData : actionStateDatas)
	{
		const FSoftObjectPath inputActionPath = actionStateData.Key;
		
		const ETriggerEvent triggerEvent = actionStateData.Value;

		if (state->InputActionInfos.Contains(inputActionPath))
		{
			FISInputActionInfo* InputActionInfo = state->InputActionInfos.Find(inputActionPath);

			// Precise Match

			if ((InputActionInfo->InputActionInfoFlags & EFlags_InputActionInfo::REQUIRE_PRECISE_MATCH) != EFlags::NONE)
			{
				if (InputActionInfo->TriggerEvent != triggerEvent)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			if (InputActionInfo->TriggerEvent == triggerEvent && InputActionInfo->WaitTimeLeft == 0)
			{
				InputActionInfo->InputActionInfoFlags |= EFlags_InputActionInfo::PASSED;

				for (const TPair<FSoftObjectPath, FISInputActionInfo>& otherInputActionInfo : state->InputActionInfos)
				{
					if ((otherInputActionInfo.Value.InputActionInfoFlags & EFlags_InputActionInfo::PASSED) == EFlags::NONE)
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

EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, FISState* state)
{
	// Tick Input Action Infos

	for (TPair<FSoftObjectPath, FISInputActionInfo>& inputActionInfo : state->InputActionInfos)
	{
		if (inputActionInfo.Value.WaitTimeLeft > 0)
		{
			inputActionInfo.Value.WaitTimeLeft = FMath::Max(inputActionInfo.Value.WaitTimeLeft - deltaTime, 0);
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

void UInputSequence::ProcessResetSources(TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources)
{
	bool resetAll = false;
	TSet<FGuid> requestStates;

	{
		FScopeLock Lock(&resetSourcesCS);

		outResetSources = ResetSources;

		for (const FISResetSource& resetSource : ResetSources)
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

void UInputSequence::ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FISEventCall>& outEventCalls)
{
	for (const FGuid& stateGuid : statesToReset)
	{
		if (ActiveStates.Contains(stateGuid))
		{
			FISState key;
			if (FISState* state = States.FindByHash(GetTypeHash(stateGuid), key))
			{
				// States are reset to NONE state and after that we make tyransition from NONE state

				ActiveStates.Remove(state->StateGuid);

				for (const TObjectPtr<UISEvent>& resetEvent : state->ResetEvents)
				{
					int32 emplacedIndex = outEventCalls.Emplace();
					outEventCalls[emplacedIndex].Event = resetEvent;
					outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
					outEventCalls[emplacedIndex].StateObject = state->StateObject;
					outEventCalls[emplacedIndex].StateContext = state->StateContext;
				}

				MakeTransition(FGuid(), { state->ParentState }, outEventCalls);
			}
		}
	}
}