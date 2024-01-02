// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "InputSequence.h"
#include "PlayerController_IS.h"
#include "EnhancedPlayerInput_IS.h"

//------------------------------------------------------
// UInputSequenceEvent
//------------------------------------------------------

UWorld* UInputSequenceEvent::GetWorld() const
{
	UInputSequence* inputSequence = GetTypedOuter<UInputSequence>();
	return inputSequence ? inputSequence->GetWorld() : nullptr;
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
	InputActionPassCount = 0;

	if (InputActionInfos.Num() > 0)
	{
		for (TPair<TObjectPtr<UInputAction>, FInputActionInfo>& inputActionInfoEntry : InputActionInfos)
		{
			inputActionInfoEntry.Value.Reset();
		}
	}

	ResetTimeLeft = bOverrideResetTime ? ResetTime : resetTime;

	for (const TObjectPtr<UInputSequenceEvent>& enterEvent : EnterEvents)
	{
		int32 emplacedIndex = outEventCalls.Emplace();
		outEventCalls[emplacedIndex].State = this;
		outEventCalls[emplacedIndex].RequestKey = RequestKey;
		outEventCalls[emplacedIndex].Event = enterEvent;
	}
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

#if WITH_EDITORONLY_DATA

void UInputSequence::AddState(const FGuid& stateId, const TObjectPtr<UInputSequenceState_Base> state)
{
	if (!States.Contains(stateId))
	{
		States.FindOrAdd(stateId, state);
		RootStateIds.Add(stateId);
		NextStateIds.Add(stateId);
	}
}

void UInputSequence::RemoveState(const FGuid& stateId)
{
	if (States.Contains(stateId))
	{
		NextStateIds.Remove(stateId);
		RootStateIds.Remove(stateId);
		States.Remove(stateId);

		for (TPair<FGuid, TObjectPtr<UInputSequenceState_Base>>& stateEntry : States)
		{
			NextStateIds[stateEntry.Key].StateIds.Remove(stateId);
			RootStateIds[stateEntry.Key].StateIds.Remove(stateId);
		}
	}
}

#endif

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<UInputAction*, ETriggerEvent>& inputActionEvents, TArray<FEventRequest>& outEventCalls, TArray<FResetRequest>& outResetSources)
{
	if (!bHasCachedRootStates)
	{
		CacheRootStates();
		bHasCachedRootStates = 1;
	}

	if (ActiveStateIds.IsEmpty())
	{
		for (const FGuid& stateId : EntryStateIds)
		{
			MakeTransition(FGuid(), NextStateIds[stateId], outEventCalls);
		}
	}

	const TSet<FGuid> prevActiveStateIds = ActiveStateIds;

	for (const FGuid& stateId : prevActiveStateIds)
	{
		if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(States[stateId]))
		{
			if (!bGamePaused || bStepWhenGamePaused)
			{
				switch (OnInput(inputActionEvents, inputState))
				{
				case EConsumeInputResponse::RESET: RequestReset(stateId, inputState->RequestKey, false); break;
				case EConsumeInputResponse::PASSED: MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventCalls); break;
				case EConsumeInputResponse::NONE:
				{
					if (!bGamePaused || bTickWhenGamePaused)
					{
						switch (OnTick(deltaTime, inputState))
						{
						case EConsumeInputResponse::RESET: RequestReset(stateId, inputState->RequestKey, false); break;
						case EConsumeInputResponse::PASSED: MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventCalls); break;
						case EConsumeInputResponse::NONE: break;
						default: check(0); break;
						}
					}

					break;
				}
				default: check(0); break;
				}
			}
		}
	}

	ProcessResetSources(outEventCalls, outResetSources);
}

void UInputSequence::MakeTransition(const FGuid& stateId, const FInputSequenceStateCollection& nextStateCollection, TArray<FEventRequest>& outEventCalls)
{
	if (stateId.IsValid())
	{
		PassState(stateId, outEventCalls);
	}

	for (const FGuid& nextStateId : nextStateCollection.StateIds)
	{
		EnterState(nextStateId, outEventCalls);
	}
}

void UInputSequence::RequestReset(const FGuid& stateId, const TObjectPtr<URequestKey> requestKey, const bool resetAll)
{
	FScopeLock Lock(&resetSourcesCS);

	int32 emplacedIndex = ResetSources.Emplace();
	ResetSources[emplacedIndex].StateId = stateId;
	ResetSources[emplacedIndex].State = stateId.IsValid() ? States[stateId] : nullptr;
	ResetSources[emplacedIndex].RequestKey = requestKey;
	ResetSources[emplacedIndex].bResetAll = resetAll;
}


void UInputSequence::EnterState(const FGuid& stateId, TArray<FEventRequest>& outEventCalls)
{
	if (!ActiveStateIds.Contains(stateId))
	{
		States[stateId]->OnEnter(outEventCalls, ResetTime);
		ActiveStateIds.Add(stateId);

		if (UInputSequenceState_Reset* resetState = Cast<UInputSequenceState_Reset>(States[stateId]))
		{
			RequestReset(stateId, resetState->RequestKey, true);
		}
		else if (UInputSequenceState_Hub* hubState = Cast<UInputSequenceState_Hub>(States[stateId]))
		{
			MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventCalls);
		}
		else if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(States[stateId]))
		{
			if (inputState->InputActionInfos.IsEmpty())
			{
				MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventCalls); // Jump through empty states
			}
		}
	}
}

void UInputSequence::PassState(const FGuid& stateId, TArray<FEventRequest>& outEventCalls)
{
	if (ActiveStateIds.Contains(stateId))
	{
		ActiveStateIds.Remove(stateId);
		States[stateId]->OnPass(outEventCalls);
	}
}

EConsumeInputResponse UInputSequence::OnInput(const TMap<UInputAction*, ETriggerEvent>& inputActionEvents, UInputSequenceState_Input* state)
{
	TSet<UInputAction*> inputActions;

	// Actual actions to check

	for (const TPair<TObjectPtr<UInputAction>, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
	{
		if (UInputAction* inputAction = inputActionInfoEntry.Key.Get())
		{
			inputActions.FindOrAdd(inputAction);
		}
	}

	// Check state Precise Match

	if (state->bRequirePreciseMatch)
	{
		for (UInputAction* inputAction : inputActions)
		{
			if (!inputActionEvents.Contains(inputAction) || inputActionEvents[inputAction] == ETriggerEvent::None)
			{
				return EConsumeInputResponse::RESET;
			}
		}

		for (const TPair<UInputAction*, ETriggerEvent>& inputActionEvent : inputActionEvents)
		{
			if (inputActionEvent.Value == ETriggerEvent::None) continue;

			if (!inputActions.Contains(inputActionEvent.Key))
			{
				return EConsumeInputResponse::RESET;
			}
		}
	}

	for (UInputAction* inputAction : inputActions)
	{
		if (state->InputActionInfos.Contains(inputAction))
		{
			// Check actions Wait Time

			if (state->InputActionInfos[inputAction].WaitTimeLeft > 0)
			{
				if (!inputActionEvents.Contains(inputAction) || inputActionEvents[inputAction] != state->InputActionInfos[inputAction].TriggerEvent)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			// Check actions Precise Match

			if (state->InputActionInfos[inputAction].bRequirePreciseMatch)
			{
				if (!inputActionEvents.Contains(inputAction) || inputActionEvents[inputAction] != state->InputActionInfos[inputAction].TriggerEvent)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			// Check actions Strong Match

			if (state->InputActionInfos[inputAction].bRequireStrongMatch)
			{
				if (inputActionEvents.Contains(inputAction) && inputActionEvents[inputAction] != state->InputActionInfos[inputAction].TriggerEvent &&
					inputActionEvents[inputAction] != ETriggerEvent::None)
				{
					return EConsumeInputResponse::RESET;
				}
			}

			if (!state->InputActionInfos[inputAction].IsPassed())
			{
				if (inputActionEvents.Contains(inputAction) && inputActionEvents[inputAction] == state->InputActionInfos[inputAction].TriggerEvent)
				{
					if (state->InputActionInfos[inputAction].WaitTimeLeft == 0)
					{
						state->InputActionInfos[inputAction].SetIsPassed();

						state->InputActionPassCount++;

						if (state->InputActionInfos.Num() == state->InputActionPassCount)
						{
							return EConsumeInputResponse::PASSED;
						}
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

	for (TPair<TObjectPtr<UInputAction>, FInputActionInfo>& inputActionInfoEntry : state->InputActionInfos)
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

	TSet<FGuid> stateIdsToReset;

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
			stateIdsToReset.FindOrAdd(resetSource.StateId);
		}
	}

	if (resetAll)
	{
		const TSet<FGuid> prevActiveStateIds = ActiveStateIds;

		for (const FGuid& stateId : prevActiveStateIds)
		{
			ActiveStateIds.Remove(stateId);
			States[stateId]->OnReset(outEventCalls);
		}

		ActiveStateIds.Empty();
	}
	else
	{
		for (const FGuid& stateId : stateIdsToReset)
		{
			check(ActiveStateIds.Contains(stateId));

			ActiveStateIds.Remove(stateId);
			States[stateId]->OnReset(outEventCalls);

			MakeTransition(FGuid(), RootStateIds[stateId], outEventCalls);
		}
	}
}

void UInputSequence::CacheRootStates()
{
	struct FInputSequenceStateLayer
	{
		TSet<FGuid> States;
	}
	stateLayer;

	for (const FGuid& stateId : EntryStateIds)
	{
		stateLayer.States.Add(stateId);
	}

	while (stateLayer.States.Num() > 0)
	{
		FInputSequenceStateLayer tmpLayer;

		for (const FGuid& stateId : stateLayer.States)
		{
			const bool isRootState = States[stateId]->IsA<UInputSequenceState_Input>() && RootStateIds[stateId].StateIds.IsEmpty();

			if (isRootState)
			{
				RootStateIds[stateId].StateIds.Add(stateId);
			}

			for (const FGuid& nextStateId : NextStateIds[stateId].StateIds)
			{
				if (isRootState)
				{
					RootStateIds[nextStateId].StateIds.Add(stateId);
				}
				else
				{
					RootStateIds[nextStateId].StateIds = RootStateIds[stateId].StateIds;
				}

				tmpLayer.States.Add(nextStateId);
			}
		}

		stateLayer.States = tmpLayer.States;
	}
}

//------------------------------------------------------
// PlayerController_IS
//------------------------------------------------------

void APlayerController_IS::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	OnPreProcessInput(DeltaTime, bGamePaused);

	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void APlayerController_IS::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	
	OnPostProcessInput(DeltaTime, bGamePaused);

	for (TPair<UInputAction*, ETriggerEvent>& InputActionEvent : InputActionEvents)
	{
		InputActionEvent.Value = ETriggerEvent::None;
	}
}

void APlayerController_IS::RegisterInputActionEvent(UInputAction* inputAction, ETriggerEvent triggerEvent)
{
	if (!InputActionEvents.Contains(inputAction) || InputActionEvents[inputAction] == ETriggerEvent::None)
	{
		InputActionEvents.FindOrAdd(inputAction) = triggerEvent;
	}
}
//------------------------------------------------------
// UEnhancedPlayerInput_IS
//------------------------------------------------------

void UEnhancedPlayerInput_IS::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);

	TMap<UInputAction*, ETriggerEvent> inputActionEvents;

	for (const FEnhancedActionKeyMapping& enhancedActionMapping : GetEnhancedActionMappings())
	{
		const FInputActionInstance* inputActionInstance = FindActionInstanceData(enhancedActionMapping.Action);
		inputActionEvents.Add(const_cast<UInputAction*>(enhancedActionMapping.Action.Get()), inputActionInstance ? inputActionInstance->GetTriggerEvent() : ETriggerEvent::None);
	}

	TArray<FEventRequest> eventRequests;
	TArray<FResetRequest> resetRequests;

	for (UInputSequence* inputSequence : InputSequences)
	{
		inputSequence->OnInput(DeltaTime, bGamePaused, inputActionEvents, eventRequests, resetRequests);
	}

	for (const FEventRequest& eventRequest : eventRequests)
	{
		eventRequest.Event->Execute(eventRequest.State, eventRequest.RequestKey, resetRequests);
	}
}