// Fill out your copyright notice in the Description page of Project Settings.

#include "InputSequence.h"
#include "PlayerController_IS.h"
#include "EnhancedPlayerInput_IS.h"

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
	TriggerEvent = ETriggerEvent::Completed;
	bIsPassed = 0;
	bRequireStrongMatch = 0;
	bRequirePreciseMatch = 0;
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
	RootStates.Empty();
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

void UInputSequenceState_Input::OnEnter(TArray<FEventRequest>& outEventCalls, const float resetTime)
{
	for (const TObjectPtr<UInputSequenceEvent>& enterEvent : EnterEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].Event = enterEvent;
	}

	InputActionPassCount = 0;

	for (TPair<FSoftObjectPath, FInputActionInfo>& inputActionInfoEntry : InputActionInfos)
	{
		inputActionInfoEntry.Value.Reset();
	}

	ResetTimeLeft = ResetTime > 0 ? ResetTime : resetTime;
}

void UInputSequenceState_Input::OnPass(TArray<FEventRequest>& outEventCalls)
{
	for (const TObjectPtr<UInputSequenceEvent>& passEvent : PassEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].Event = passEvent;
	}
}

void UInputSequenceState_Input::OnReset(TArray<FEventRequest>& outEventCalls)
{
	for (const TObjectPtr<UInputSequenceEvent>& resetEvent : ResetEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].Event = resetEvent;
	}
}

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

UInputSequence::UInputSequence(const FObjectInitializer& ObjectInitializer) :Super(ObjectInitializer)
{
	ResetTime = 0.5;
	bHasCachedRootStates = 0;
}

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<UInputAction*, ETriggerEvent>& actionStateData, TArray<FEventRequest>& outEventCalls, TArray<FResetRequest>& outResetSources)
{
	if (!bHasCachedRootStates)
	{
		CacheRootStates();
		bHasCachedRootStates = 1;
	}

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
		if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(prevActiveState.Get()))
		{
			if (!bGamePaused || bStepWhenGamePaused)
			{
				switch (OnInput(actionStateData, inputState))
				{
				case EConsumeInputResponse::RESET: RequestReset(inputState, inputState->RequestKey, false); break;
				case EConsumeInputResponse::PASSED: MakeTransition(inputState, inputState->NextStates.IsEmpty() ? inputState->RootStates : inputState->NextStates, outEventCalls); break;
				case EConsumeInputResponse::NONE: break;
				default: check(0); break;
				}
			}

			if (!bGamePaused || bTickWhenGamePaused)
			{
				switch (OnTick(deltaTime, inputState))
				{
				case EConsumeInputResponse::RESET: RequestReset(inputState, inputState->RequestKey, false); break;
				case EConsumeInputResponse::PASSED: MakeTransition(inputState, inputState->NextStates.IsEmpty() ? inputState->RootStates : inputState->NextStates, outEventCalls); break;
				case EConsumeInputResponse::NONE: break;
				default: check(0); break;
				}
			}
		}
	}

	ProcessResetSources(outEventCalls, outResetSources);
}

void UInputSequence::RequestReset(const TObjectPtr<UInputSequenceState_Base> state, const TObjectPtr<URequestKey> requestKey, const bool resetAll)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].State = state;
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAll = resetAll;
}

void UInputSequence::MakeTransition(UInputSequenceState_Base* fromState, const TSet<TObjectPtr<UInputSequenceState_Base>>& toStates, TArray<FEventRequest>& outEventCalls)
{
	if (fromState)
	{
		PassState(fromState, outEventCalls);
	}

	for (const TObjectPtr<UInputSequenceState_Base>& toState : toStates)
	{
		if (UInputSequenceState_Base* state = toState.Get())
		{
			EnterState(state, outEventCalls);
		}
	}
}

void UInputSequence::EnterState(UInputSequenceState_Base* state, TArray<FEventRequest>& outEventCalls)
{
	if (!ActiveStates.Contains(state))
	{
		state->OnEnter(outEventCalls, ResetTime);
		ActiveStates.Add(state);

		if (UInputSequenceState_Reset* resetState = Cast<UInputSequenceState_Reset>(state))
		{
			RequestReset(resetState, resetState->RequestKey, true);
		}
		else if (UInputSequenceState_Hub* hubState = Cast<UInputSequenceState_Hub>(state))
		{
			MakeTransition(hubState, state->NextStates.IsEmpty() ? state->RootStates : state->NextStates, outEventCalls);
		}
		else if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(state))
		{
			if (inputState->InputActionInfos.IsEmpty())
			{
				MakeTransition(inputState, state->NextStates.IsEmpty() ? state->RootStates : state->NextStates, outEventCalls); // Jump through empty states
			}
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
	TSet<UInputAction*> inputActions;

	// Actual actions to check

	for (const TPair<FSoftObjectPath, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
	{
		if (UInputAction* inputAction = Cast<UInputAction>(inputActionInfoEntry.Key.TryLoad()))
		{
			inputActions.FindOrAdd(inputAction);
		}
	}

	// Check state Precise Match

	if (state->bRequirePreciseMatch)
	{
		for (UInputAction* inputAction : inputActions)
		{
			if (!actionStateDatas.Contains(inputAction) || actionStateDatas[inputAction] == ETriggerEvent::None)
			{
				return EConsumeInputResponse::RESET;
			}
		}

		for (const TPair<UInputAction*, ETriggerEvent>& actionStateData : actionStateDatas)
		{
			if (actionStateData.Value == ETriggerEvent::None) continue;

			if (!inputActions.Contains(actionStateData.Key))
			{
				return EConsumeInputResponse::RESET;
			}
		}
	}

	for (UInputAction* inputAction : inputActions)
	{
		FSoftObjectPath inputActionPath(inputAction);

		if (state->InputActionInfos.Contains(inputActionPath))
		{
			// Check actions Wait Time

			if (state->InputActionInfos[inputActionPath].WaitTimeLeft > 0)
			{
				if (!actionStateDatas.Contains(inputAction) || actionStateDatas[inputAction] != state->InputActionInfos[inputActionPath].TriggerEvent)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			// Check actions Precise Match

			if (state->InputActionInfos[inputActionPath].bRequirePreciseMatch)
			{
				if (!actionStateDatas.Contains(inputAction) || actionStateDatas[inputAction] != state->InputActionInfos[inputActionPath].TriggerEvent)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			// Check actions Strong Match

			if (state->InputActionInfos[inputActionPath].bRequireStrongMatch)
			{
				if (actionStateDatas.Contains(inputAction) && actionStateDatas[inputAction] != state->InputActionInfos[inputActionPath].TriggerEvent &&
					actionStateDatas[inputAction] != ETriggerEvent::None)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			if (!state->InputActionInfos[inputActionPath].IsPassed())
			{
				if (actionStateDatas.Contains(inputAction) && actionStateDatas[inputAction] == state->InputActionInfos[inputActionPath].TriggerEvent)
				{
					state->InputActionInfos[inputActionPath].SetIsPassed();
					state->InputActionPassCount++;

					if (state->InputActionInfos.Num() == state->InputActionPassCount)
					{
						return EConsumeInputResponse::PASSED;
					}
				}
			}
		}
	}

	return EConsumeInputResponse::NONE;
}

EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, UInputSequenceState_Input* state)
{
	// Tick Input Action Infos

	for (TPair<FSoftObjectPath, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
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

		outResetSources.SetNum(ResetSources.Num());
		memcpy(outResetSources.GetData(), ResetSources.GetData(), ResetSources.Num() * ResetSources.GetTypeSize());
		ResetSources.Empty();
	}

	for (const FResetRequest& resetSource : outResetSources)
	{
		resetAll |= resetSource.bResetAll;

		if (resetAll) break;

		if (!resetSource.bResetAll)
		{
			statesToReset.FindOrAdd(resetSource.State);
		}
	}

	if (resetAll)
	{
		TSet<TObjectPtr<UInputSequenceState_Base>> activeStates = ActiveStates;

		for (const TObjectPtr<UInputSequenceState_Base>& stateToReset : activeStates)
		{
			ActiveStates.Remove(stateToReset);
			stateToReset->OnReset(outEventCalls);
		}

		ActiveStates.Empty();
	}
	else
	{
		for (const TObjectPtr<UInputSequenceState_Base>& stateToReset : statesToReset)
		{
			check(ActiveStates.Contains(stateToReset));

			// If stateToReset is Root State itself then it should stay active

			ActiveStates.Remove(stateToReset);
			stateToReset->OnReset(outEventCalls);

			MakeTransition(nullptr, stateToReset->RootStates, outEventCalls);
		}
	}
}

void UInputSequence::CacheRootStates()
{
	struct FInputSequenceStateLayer
	{
		TSet<TObjectPtr<UInputSequenceState_Base>> States;
	}
	stateLayer;

	for (const TObjectPtr<UInputSequenceState_Base>& state : EntryStates)
	{
		stateLayer.States.Add(state);
	}

	while (stateLayer.States.Num() > 0)
	{
		FInputSequenceStateLayer tmpLayer;

		for (const TObjectPtr<UInputSequenceState_Base>& state : stateLayer.States)
		{
			for (TObjectPtr<UInputSequenceState_Base>& nextState : state->NextStates)
			{
				if (state->IsA<UInputSequenceState_Input>() && state->RootStates.IsEmpty())
				{
					state->RootStates.Add(state);
					nextState->RootStates.Add(state);
				}
				else
				{
					nextState->RootStates = state->RootStates;
				}

				tmpLayer.States.Add(nextState);
			}
		}

		stateLayer.States = tmpLayer.States;
	}
}

//------------------------------------------------------
// PlayerController_IS
//------------------------------------------------------

void APlayerController_IS::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	OnPostProcessInput(DeltaTime, bGamePaused);

	for (TPair<UInputAction*, ETriggerEvent>& InputActionEvent : InputActionEvents)
	{
		InputActionEvent.Value = ETriggerEvent::None;
	}
}

//------------------------------------------------------
// UEnhancedPlayerInput_IS
//------------------------------------------------------

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