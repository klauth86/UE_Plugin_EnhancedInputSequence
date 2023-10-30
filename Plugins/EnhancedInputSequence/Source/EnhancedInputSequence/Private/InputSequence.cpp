// Fill out your copyright notice in the Description page of Project Settings.

#include "InputSequence.h"

//------------------------------------------------------
// FISState
//------------------------------------------------------

void FISState::Reset()
{

}

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources)
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
			if (state->bIsResetState)
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
					else if (consumeInputResponse == EConsumeInputResponse::PASS)
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
						else if (tickResponse == EConsumeInputResponse::PASS)
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

void UInputSequence::RequestReset(UObject* sourceObject, const FString& sourceContext)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].bResetAsset = true;
	ResetSources[emplacedIndex].SourceObject = sourceObject;
	ResetSources[emplacedIndex].SourceContext = sourceContext;
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

			if (toState->IsEmpty()) MakeTransition(toStateGuid, GetTransitions(toStateGuid), outEventCalls); // Jump through empty states
		}
	}
}

void UInputSequence::PassState(FISState* state, TArray<FISEventCall>& outEventCalls)
{
	check(!state->bIsResetState);

	if (ActiveStates.Contains(state->StateGuid))
	{
		ActiveStates.Remove(state->StateGuid);

		for (const TObjectPtr<UISEvent>& passEvent : state->PassEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].Event = passEvent;
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].SourceObject = state->StateObject;
			outEventCalls[emplacedIndex].SourceContext = state->StateContext;
		}
	}
}

void UInputSequence::EnterState(FISState* state, TArray<FISEventCall>& outEventCalls)
{
	check(!state->bIsResetState);

	if (!ActiveStates.Contains(state->StateGuid))
	{
		for (const TObjectPtr<UISEvent>& enterEvent : state->EnterEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].Event = enterEvent;
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].SourceObject = state->StateObject;
			outEventCalls[emplacedIndex].SourceContext = state->StateContext;
		}

		state->Reset();

		ActiveStates.Add(state->StateGuid);
	}
}

EConsumeInputResponse UInputSequence::OnInput(const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, FISState* state)
{
	return EConsumeInputResponse::NONE;
}

EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, FISState* state)
{
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
					outEventCalls[emplacedIndex].SourceObject = state->StateObject;
					outEventCalls[emplacedIndex].SourceContext = state->StateContext;
				}

				MakeTransition(FGuid(), { state->ParentState }, outEventCalls);
			}
		}
	}
}