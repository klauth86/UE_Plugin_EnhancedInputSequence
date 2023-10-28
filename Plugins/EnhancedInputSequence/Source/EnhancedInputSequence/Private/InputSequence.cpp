// Fill out your copyright notice in the Description page of Project Settings.

#include "InputSequence.h"

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, TArray<FEventCall>& outEventCalls, TArray<FResetSource>& outResetSources)
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
				RequestReset(stateGuid, true);
			}
			else
			{
				if (!bGamePaused || bStepFromStatesWhenGamePaused)
				{
					EConsumeInputResponse consumeInputResponse = OnInput(actionStateData, state);

					if (consumeInputResponse == EConsumeInputResponse::RESET)
					{
						RequestReset(stateGuid, false);
					}
					else if (consumeInputResponse == EConsumeInputResponse::PASS)
					{
						MakeTransition(stateGuid, state->NextStates, outEventCalls);
					}
				}

				if (!bGamePaused || bTickStatesWhenGamePaused)
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
							MakeTransition(stateGuid, state->NextStates, outEventCalls);
						}
					}
				}
			}
		}
	}

	ProcessResetSources(outEventCalls, outResetSources);
}

void UInputSequence::MakeTransition(const FGuid& stateGuid, const TSet<FGuid>& nextStates, TArray<FEventCall>& outEventCalls)
{
	check(nextStates.Num() > 0);

	FInputSequenceState key;
	if (FInputSequenceState* state = States.FindByHash(GetTypeHash(stateGuid), key))
	{
		PassState(state, outEventCalls);
	}

	for (const FGuid& nextStateGuid : nextStates)
	{
		if (FInputSequenceState* state = States.FindByHash(GetTypeHash(nextStateGuid), key))
		{
			EnterState(state, outEventCalls);
			if (state->IsEmpty()) MakeTransition(state->StateGuid, state->NextStates, outEventCalls); // Jump through empty states
		}
	}
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

void UInputSequence::EnterState(FInputSequenceState* state, TArray<FEventCall>& outEventCalls)
{
	check(!state->bIsResetState);

	if (!ActiveStates.Contains(state->StateGuid))
	{
		for (const TObjectPtr<UInputSequenceEvent>& enterEvent : state->EnterEvents)
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

void UInputSequence::PassState(FInputSequenceState* state, TArray<FEventCall>& outEventCalls)
{
	check(!state->bIsResetState);

	if (ActiveStates.Contains(state->StateGuid))
	{
		ActiveStates.Remove(state->StateGuid);

		for (const TObjectPtr<UInputSequenceEvent>& passEvent : state->PassEvents)
		{
			int32 emplacedIndex = outEventCalls.Emplace();
			outEventCalls[emplacedIndex].Event = passEvent;
			outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
			outEventCalls[emplacedIndex].SourceObject = state->StateObject;
			outEventCalls[emplacedIndex].SourceContext = state->StateContext;
		}
	}
}

EConsumeInputResponse UInputSequence::OnInput(const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, FInputSequenceState* state)
{
	return EConsumeInputResponse::NONE;
}

EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, FInputSequenceState* state)
{
	return EConsumeInputResponse::NONE;
}

void UInputSequence::ProcessResetSources(TArray<FEventCall>& outEventCalls, TArray<FResetSource>& outResetSources)
{
	bool resetAll = false;
	TSet<FGuid> requestStates;

	{
		FScopeLock Lock(&resetSourcesCS);

		outResetSources = ResetSources;

		for (const FResetSource& resetSource : ResetSources)
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

void UInputSequence::ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FEventCall>& outEventCalls)
{
	for (const FGuid& stateGuid : statesToReset)
	{
		if (ActiveStates.Contains(stateGuid))
		{
			FInputSequenceState key;
			if (FInputSequenceState* state = States.FindByHash(GetTypeHash(stateGuid), key))
			{
				// States are reset to NONE state and after that we make tyransition from NONE state

				ActiveStates.Remove(state->StateGuid);

				for (const TObjectPtr<UInputSequenceEvent>& resetEvent : state->ResetEvents)
				{
					int32 emplacedIndex = outEventCalls.Emplace();
					outEventCalls[emplacedIndex].Event = resetEvent;
					outEventCalls[emplacedIndex].StateGuid = state->StateGuid;
					outEventCalls[emplacedIndex].SourceObject = state->StateObject;
					outEventCalls[emplacedIndex].SourceContext = state->StateContext;
				}

				MakeTransition(FGuid(), state->NextStates, outEventCalls);
			}
		}
	}
}