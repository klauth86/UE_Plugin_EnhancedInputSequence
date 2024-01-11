// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#include "InputSequenceCore.h"
#include "InputSequence.h"
#include "PlayerController_EIS.h"
#include "EnhancedPlayerInput_EIS.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"

#define LOCTEXT_NAMESPACE "FInputSequenceCore"

void FInputSequenceCore::StartupModule()
{
}

void FInputSequenceCore::ShutdownModule()
{
}

//------------------------------------------------------
// UInputSequenceEvent_Base
//------------------------------------------------------

UWorld* UInputSequenceEvent_Base::GetWorld() const
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
	bRequireStrongMatch = 0;

	ResetTime = 0;
	ResetTimeLeft = 0;
}

void UInputSequenceState_Input::OnEnter(TArray<FInputSequenceEventRequest>& outEventRequests, const float resetTime)
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

	for (const TObjectPtr<UInputSequenceEvent_Base>& enterEvent : EnterEvents)
	{
		int32 emplacedIndex = outEventRequests.Emplace();
		outEventRequests[emplacedIndex].State = this;
		outEventRequests[emplacedIndex].RequestKey = RequestKey;
		outEventRequests[emplacedIndex].PayloadObject = PayloadObject;
		outEventRequests[emplacedIndex].Event = enterEvent;
	}
}

void UInputSequenceState_Input::OnPass(TArray<FInputSequenceEventRequest>& outEventRequests)
{
	for (const TObjectPtr<UInputSequenceEvent_Base>& passEvent : PassEvents)
	{
		int32 emplacedIndex = outEventRequests.Emplace();
		outEventRequests[emplacedIndex].State = this;
		outEventRequests[emplacedIndex].RequestKey = RequestKey;
		outEventRequests[emplacedIndex].PayloadObject = PayloadObject;
		outEventRequests[emplacedIndex].Event = passEvent;
	}
}

void UInputSequenceState_Input::OnReset(TArray<FInputSequenceEventRequest>& outEventRequests)
{
	for (const TObjectPtr<UInputSequenceEvent_Base>& resetEvent : ResetEvents)
	{
		int32 emplacedIndex = outEventRequests.Emplace();
		outEventRequests[emplacedIndex].State = this;
		outEventRequests[emplacedIndex].RequestKey = RequestKey;
		outEventRequests[emplacedIndex].PayloadObject = PayloadObject;
		outEventRequests[emplacedIndex].Event = resetEvent;
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

void UInputSequence::OnInput(const float deltaTime, const bool bGamePaused, const TMap<UInputAction*, ETriggerEvent>& inputActionEvents, TArray<FInputSequenceEventRequest>& outEventRequests, TArray<FInputSequenceResetRequest>& outResetRequests)
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
			MakeTransition(FGuid(), NextStateIds[stateId], outEventRequests);
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
				case InputSequenceCore::EConsumeInputResponse::RESET: RequestReset(stateId, inputState->RequestKey, inputState->PayloadObject, false); break;
				case InputSequenceCore::EConsumeInputResponse::PASSED: MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventRequests); break;
				case InputSequenceCore::EConsumeInputResponse::NONE:
				{
					if (!bGamePaused || bTickWhenGamePaused)
					{
						switch (OnTick(deltaTime, inputState))
						{
						case InputSequenceCore::EConsumeInputResponse::RESET: RequestReset(stateId, inputState->RequestKey, inputState->PayloadObject, false); break;
						case InputSequenceCore::EConsumeInputResponse::PASSED: MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventRequests); break;
						case InputSequenceCore::EConsumeInputResponse::NONE: break;
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

	ProcessResetRequests(outEventRequests, outResetRequests);
}

void UInputSequence::SetWorldContext(UObject* worldContextObject)
{
	WorldPtr = worldContextObject ? worldContextObject->GetWorld() : nullptr;
}

void UInputSequence::MakeTransition(const FGuid& fromStateId, const FInputSequenceStateCollection& nextStateCollection, TArray<FInputSequenceEventRequest>& outEventRequests)
{
	if (fromStateId.IsValid())
	{
		PassState(fromStateId, outEventRequests);
	}

	for (const FGuid& nextStateId : nextStateCollection.StateIds)
	{
		EnterState(nextStateId, outEventRequests);
	}
}

void UInputSequence::RequestReset(const FGuid& stateId, const TObjectPtr<UInputSequenceRequestKey> requestKey, const TObjectPtr<UObject> payloadObject, const bool resetAll)
{
	FScopeLock Lock(&resetRequestsCS);

	int32 emplacedIndex = ResetRequests.Emplace();
	ResetRequests[emplacedIndex].StateId = stateId;
	ResetRequests[emplacedIndex].State = stateId.IsValid() ? States[stateId] : nullptr;
	ResetRequests[emplacedIndex].RequestKey = requestKey;
	ResetRequests[emplacedIndex].PayloadObject = payloadObject;
	ResetRequests[emplacedIndex].bResetAll = resetAll;
}


void UInputSequence::EnterState(const FGuid& stateId, TArray<FInputSequenceEventRequest>& outEventRequests)
{
	if (!ActiveStateIds.Contains(stateId))
	{
		States[stateId]->OnEnter(outEventRequests, ResetTime);
		ActiveStateIds.Add(stateId);

		if (UInputSequenceState_Reset* resetState = Cast<UInputSequenceState_Reset>(States[stateId]))
		{
			RequestReset(stateId, resetState->RequestKey, resetState->PayloadObject, true);
		}
		else if (UInputSequenceState_Hub* hubState = Cast<UInputSequenceState_Hub>(States[stateId]))
		{
			MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventRequests);
		}
		else if (UInputSequenceState_Input* inputState = Cast<UInputSequenceState_Input>(States[stateId]))
		{
			if (inputState->InputActionInfos.IsEmpty())
			{
				MakeTransition(stateId, NextStateIds[stateId].StateIds.IsEmpty() ? RootStateIds[stateId] : NextStateIds[stateId], outEventRequests); // Jump through empty states
			}
		}
	}
}

void UInputSequence::PassState(const FGuid& stateId, TArray<FInputSequenceEventRequest>& outEventRequests)
{
	if (ActiveStateIds.Contains(stateId))
	{
		ActiveStateIds.Remove(stateId);
		States[stateId]->OnPass(outEventRequests);
	}
}

InputSequenceCore::EConsumeInputResponse UInputSequence::OnInput(const TMap<UInputAction*, ETriggerEvent>& inputActionEvents, UInputSequenceState_Input* state)
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

	if (state->bRequireStrongMatch)
	{
		for (const TPair<UInputAction*, ETriggerEvent>& inputActionEvent : inputActionEvents)
		{
			if (inputActionEvent.Value == ETriggerEvent::None) continue;

			if (!inputActions.Contains(inputActionEvent.Key))
			{
				return InputSequenceCore::EConsumeInputResponse::RESET;
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
					return InputSequenceCore::EConsumeInputResponse::RESET;
				}
			}

			// Check actions Precise Match

			if (state->InputActionInfos[inputAction].bRequirePreciseMatch)
			{
				if (!inputActionEvents.Contains(inputAction) || inputActionEvents[inputAction] != state->InputActionInfos[inputAction].TriggerEvent)
				{
					return InputSequenceCore::EConsumeInputResponse::RESET;
				}
			}

			// Check actions Strong Match

			if (state->InputActionInfos[inputAction].bRequireStrongMatch)
			{
				if (inputActionEvents.Contains(inputAction) && inputActionEvents[inputAction] != state->InputActionInfos[inputAction].TriggerEvent &&
					inputActionEvents[inputAction] != ETriggerEvent::None)
				{
					return InputSequenceCore::EConsumeInputResponse::RESET;
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
							return InputSequenceCore::EConsumeInputResponse::PASSED;
						}
					}
				}
			}
		}
	}

	return InputSequenceCore::EConsumeInputResponse::NONE;
}

InputSequenceCore::EConsumeInputResponse UInputSequence::OnTick(const float deltaTime, UInputSequenceState_Input* state)
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
			return InputSequenceCore::EConsumeInputResponse::RESET;
		}
	}

	return InputSequenceCore::EConsumeInputResponse::NONE;
}

void UInputSequence::ProcessResetRequests(TArray<FInputSequenceEventRequest>& outEventRequests, TArray<FInputSequenceResetRequest>& outResetRequests)
{
	bool resetAll = false;

	TSet<FGuid> stateIdsToReset;

	{
		FScopeLock Lock(&resetRequestsCS);

		outResetRequests.SetNum(ResetRequests.Num());
		memcpy(outResetRequests.GetData(), ResetRequests.GetData(), ResetRequests.Num() * ResetRequests.GetTypeSize());
		ResetRequests.Empty();
	}

	for (const FInputSequenceResetRequest& resetRequest : outResetRequests)
	{
		resetAll |= resetRequest.bResetAll;

		if (resetAll) break;

		if (!resetRequest.bResetAll)
		{
			stateIdsToReset.FindOrAdd(resetRequest.StateId);
		}
	}

	if (resetAll)
	{
		const TSet<FGuid> prevActiveStateIds = ActiveStateIds;

		for (const FGuid& stateId : prevActiveStateIds)
		{
			ActiveStateIds.Remove(stateId);
			States[stateId]->OnReset(outEventRequests);
		}

		ActiveStateIds.Empty();
	}
	else
	{
		for (const FGuid& stateId : stateIdsToReset)
		{
			check(ActiveStateIds.Contains(stateId));

			ActiveStateIds.Remove(stateId);
			States[stateId]->OnReset(outEventRequests);

			MakeTransition(FGuid(), RootStateIds[stateId], outEventRequests);
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
// PlayerController_EIS
//------------------------------------------------------

void APlayerController_EIS::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	OnPreProcessInput(DeltaTime, bGamePaused);

	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void APlayerController_EIS::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);

	OnPostProcessInput(DeltaTime, bGamePaused);

	for (TPair<UInputAction*, ETriggerEvent>& InputActionEvent : InputActionEvents)
	{
		InputActionEvent.Value = ETriggerEvent::None;
	}
}

void APlayerController_EIS::RegisterInputActionEvent(UInputAction* inputAction, ETriggerEvent triggerEvent)
{
	if (!InputActionEvents.Contains(inputAction) || InputActionEvents[inputAction] == ETriggerEvent::None)
	{
		InputActionEvents.FindOrAdd(inputAction) = triggerEvent;
	}
}
//------------------------------------------------------
// UEnhancedPlayerInput_EIS
//------------------------------------------------------

void UEnhancedPlayerInput_EIS::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);

	TMap<UInputAction*, ETriggerEvent> inputActionEvents;

	for (const FEnhancedActionKeyMapping& enhancedActionMapping : GetEnhancedActionMappings())
	{
		const FInputActionInstance* inputActionInstance = FindActionInstanceData(enhancedActionMapping.Action);
		inputActionEvents.Add(const_cast<UInputAction*>(enhancedActionMapping.Action.Get()), inputActionInstance ? inputActionInstance->GetTriggerEvent() : ETriggerEvent::None);
	}

	TArray<FInputSequenceEventRequest> eventRequests;
	TArray<FInputSequenceResetRequest> resetRequests;

	for (UInputSequence* inputSequence : InputSequences)
	{
		inputSequence->OnInput(DeltaTime, bGamePaused, inputActionEvents, eventRequests, resetRequests);
	}

	for (const FInputSequenceEventRequest& eventRequest : eventRequests)
	{
		eventRequest.Event->Execute(eventRequest.State, eventRequest.RequestKey, eventRequest.PayloadObject, resetRequests);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FInputSequenceCore, InputSequenceCore)