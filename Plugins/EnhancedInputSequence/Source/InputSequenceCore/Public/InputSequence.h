// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "InputTriggers.h"
#include "InputSequence.generated.h"

class UEdGraph;
class UInputAction;

namespace InputSequenceCore
{
	enum class EConsumeInputResponse :uint8
	{
		NONE,
		RESET,
		PASSED
	};
};

//------------------------------------------------------
// UInputSequenceRequestKey
//------------------------------------------------------

UCLASS(BlueprintType)
class INPUTSEQUENCECORE_API UInputSequenceRequestKey :public UObject
{
	GENERATED_BODY()
};

//------------------------------------------------------
// FInputSequenceResetRequest
//------------------------------------------------------

USTRUCT(BlueprintType)
struct INPUTSEQUENCECORE_API FInputSequenceResetRequest
{
	GENERATED_USTRUCT_BODY()

public:

	FInputSequenceResetRequest() :StateId(FGuid()), State(nullptr), RequestKey(nullptr), bResetAll(0) {}

	FGuid StateId;

	/* Requested by State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	TObjectPtr<UInputSequenceState_Base> State;

	/* Requested with Request Key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	TObjectPtr<UInputSequenceRequestKey> RequestKey;

	/* Requested with Payload Object */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UObject> PayloadObject;

	/* If true, all active states will be reset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	uint8 bResetAll : 1;
};

//------------------------------------------------------
// FInputSequenceEventRequest
//------------------------------------------------------

USTRUCT(BlueprintType)
struct INPUTSEQUENCECORE_API FInputSequenceEventRequest
{
	GENERATED_USTRUCT_BODY()

public:

	FInputSequenceEventRequest() :State(nullptr), RequestKey(nullptr), Event(nullptr) {}

	/* Requested by State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UInputSequenceState_Base> State;

	/* Requested with Request Key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UInputSequenceRequestKey> RequestKey;

	/* Requested with Payload Object */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UObject> PayloadObject;

	/* Requested Input Sequence Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UInputSequenceEvent_Base> Event;
};

//------------------------------------------------------
// UInputSequenceEvent_Base
//------------------------------------------------------

UCLASS(Abstract, EditInlineNew, BlueprintType, Blueprintable)
class INPUTSEQUENCECORE_API UInputSequenceEvent_Base : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic, BlueprintCallable, Category = "Input Sequence Event")
	void Execute(UInputSequenceState_Base* state, UInputSequenceRequestKey* requestKey, UObject* payloadObject, const TArray<FInputSequenceResetRequest>& resetRequests);

	virtual void Execute_Implementation(UInputSequenceState_Base* state, UInputSequenceRequestKey* requestKey, UObject* payloadObject, const TArray<FInputSequenceResetRequest>& resetRequests) {}

	virtual UWorld* GetWorld() const override;
};

//------------------------------------------------------
// FInputActionInfo
//------------------------------------------------------

USTRUCT()
struct FInputActionInfo
{
	GENERATED_USTRUCT_BODY()

public:

	FInputActionInfo();

	void Reset();

	bool IsPassed() const { return bIsPassed; }

	void SetIsPassed() { bIsPassed = 1; }

	UPROPERTY()
	ETriggerEvent TriggerEvent;

	UPROPERTY()
	uint8 bIsPassed : 1;

	UPROPERTY()
	uint8 bRequireStrongMatch : 1;

	UPROPERTY()
	uint8 bRequirePreciseMatch : 1;

	UPROPERTY()
	float WaitTime;

	float WaitTimeLeft;
};

//------------------------------------------------------
// UInputSequenceState_Base
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Base : public UObject
{
	GENERATED_BODY()

public:

	virtual void OnEnter(TArray<FInputSequenceEventRequest>& outEventRequests, const float resetTime) {}
	virtual void OnPass(TArray<FInputSequenceEventRequest>& outEventRequests) {}
	virtual void OnReset(TArray<FInputSequenceEventRequest>& outEventRequests) {}
};

//------------------------------------------------------
// UInputSequenceState_Hub
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Hub : public UInputSequenceState_Base
{
	GENERATED_BODY()
};

//------------------------------------------------------
// UInputSequenceState_Reset
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Reset : public UInputSequenceState_Base
{
	GENERATED_UCLASS_BODY()

public:

	/* Request Key (used to identify context in Event and Reset Requests) */
	UPROPERTY(EditAnywhere, Category = "Context:")
	TObjectPtr<UInputSequenceRequestKey> RequestKey;

	/* Payload object (used to customize Event and Reset Requests) */
	UPROPERTY(EditAnywhere, Category = "Context:")
	TObjectPtr<UObject> PayloadObject;
};

//------------------------------------------------------
// UInputSequenceState_Input
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Input : public UInputSequenceState_Base
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA

	void AddInputActionInfo(UInputAction* inputAction) { InputActionInfos.Add(inputAction, FInputActionInfo()); }

#endif

	virtual void OnEnter(TArray<FInputSequenceEventRequest>& outEventRequests, const float resetTime) override;
	virtual void OnPass(TArray<FInputSequenceEventRequest>& outEventRequests) override;
	virtual void OnReset(TArray<FInputSequenceEventRequest>& outEventRequests) override;

	uint32 InputActionPassCount;

	UPROPERTY()
	TMap<TObjectPtr<UInputAction>, FInputActionInfo> InputActionInfos;

	/* Collection of Input Sequence Events to execute when state is entered */
	UPROPERTY(EditAnywhere, Category = "Events:", meta=(EditInline))
	TArray<TObjectPtr<UInputSequenceEvent_Base>> EnterEvents;
	/* Collection of Input Sequence Events to execute when state is passed */
	UPROPERTY(EditAnywhere, Category = "Events:", meta = (EditInline))
	TArray<TObjectPtr<UInputSequenceEvent_Base>> PassEvents;
	/* Collection of Input Sequence Events to execute when state is reset */
	UPROPERTY(EditAnywhere, Category = "Events:", meta = (EditInline))
	TArray<TObjectPtr<UInputSequenceEvent_Base>> ResetEvents;

	/* Request Key (used to identify context in Event and Reset Requests) */
	UPROPERTY(EditAnywhere, Category = "Context:")
	TObjectPtr<UInputSequenceRequestKey> RequestKey;

	/* Payload object (used to customize Event and Reset Requests) */
	UPROPERTY(EditAnywhere, Category = "Context:")
	TObjectPtr<UObject> PayloadObject;

	/* If true, state will be reset by any Input Action that is not listed */
	UPROPERTY(EditAnywhere, Category = "Input:")
	uint8 bRequireStrongMatch : 1;

	UPROPERTY(EditAnywhere, Category = "Overrides", meta = (InlineEditConditionToggle))
	uint8 bOverrideResetTime : 1;

	/* Time interval after which this state will be reset (overrides global value) */
	UPROPERTY(EditAnywhere, Category = "Overrides", meta = (UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10, EditCondition = bOverrideResetTime))
	float ResetTime;

	float ResetTimeLeft;
};

//------------------------------------------------------
// FInputSequenceStateCollection
//------------------------------------------------------

USTRUCT()
struct FInputSequenceStateCollection
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY()
	TSet<FGuid> StateIds;
};

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

UCLASS(BlueprintType)
class INPUTSEQUENCECORE_API UInputSequence : public UObject
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	UEdGraph* EdGraph;

	TObjectPtr<UInputSequenceState_Base> GetState(const FGuid& stateId) const { return States.Contains(stateId) ? States[stateId] : nullptr; }

	void AddState(const FGuid& stateId, const TObjectPtr<UInputSequenceState_Base> state);

	void RemoveState(const FGuid& stateId);

	void AddEntryStateId(const FGuid& stateId) { EntryStateIds.FindOrAdd(stateId); }

	const TMap<FGuid, FInputSequenceStateCollection>& GetNextStateIds() const { return NextStateIds; }

	void AddNextStateId(const FGuid& stateId, const FGuid& nextStateId) { NextStateIds.FindOrAdd(stateId).StateIds.FindOrAdd(nextStateId); }
	
	void RemoveNextStateId(const FGuid& stateId, const FGuid& nextStateId) { NextStateIds.FindOrAdd(stateId).StateIds.Remove(nextStateId); }

	float GetResetTime() const { return ResetTime; }

	bool IsStateActive(const FGuid& stateId) const { return ActiveStateIds.Contains(stateId); }

#endif

	/* Feed input to Input Sequence and receives the collections of Event Requests and Reset Requests as result
	 * @param deltaTime - the delta time of this tick, in seconds
	 * @param bGamePaused - whether the game is currently paused
	 * @param inputActionEvents - the collection of all Trigger Events happened during this tick
	 * @param outEventRequests - the collection of Event Requests, that will be filled
	 * @param outResetRequests - the collection of Reset Requests, that will be filled
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void OnInput(const float deltaTime, const bool bGamePaused, const TMap<UInputAction*, ETriggerEvent>& inputActionEvents, TArray<FInputSequenceEventRequest>& outEventRequests, TArray<FInputSequenceResetRequest>& outResetRequests);

	/* Requests external reset
	 * @param requestKey - Request key to associate with this Reset Request
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void RequestReset(UInputSequenceRequestKey* requestKey, UObject* payloadObject)
	{
		bHasCachedRootStates = 0;
		RequestReset(FGuid(), requestKey, payloadObject, true);
	}

	/* Sets World
	 * @param worldContextObject - object to get World context from
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void SetWorldContext(UObject* worldContextObject);

	virtual UWorld* GetWorld() const override { return WorldPtr.Get(); }

protected:

	void MakeTransition(const FGuid& fromStateId, const FInputSequenceStateCollection& nextStateCollection, TArray<FInputSequenceEventRequest>& outEventRequests);

	void RequestReset(const FGuid& stateId, const TObjectPtr<UInputSequenceRequestKey> requestKey, const TObjectPtr<UObject> payloadObject, const bool resetAll);

	void EnterState(const FGuid& stateId, TArray<FInputSequenceEventRequest>& outEventRequests);

	void PassState(const FGuid& stateId, TArray<FInputSequenceEventRequest>& outEventRequests);

	InputSequenceCore::EConsumeInputResponse OnInput(const TMap<UInputAction*, ETriggerEvent>& inputActionEvents, UInputSequenceState_Input* state);

	InputSequenceCore::EConsumeInputResponse OnTick(const float deltaTime, UInputSequenceState_Input* state);

	void ProcessResetRequests(TArray<FInputSequenceEventRequest>& outEventRequests, TArray<FInputSequenceResetRequest>& outResetRequests);

	void CacheRootStates();

protected:

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UInputSequenceState_Base>> States;

	UPROPERTY()
	TSet<FGuid> EntryStateIds;

	UPROPERTY()
	TMap<FGuid, FInputSequenceStateCollection> RootStateIds;

	UPROPERTY()
	TMap<FGuid, FInputSequenceStateCollection> NextStateIds;

	/* Time interval after which any state will be reset (global, can be overriden on any state) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10))
	float ResetTime;

	/* If true, active states will continue to step even if Game is paused (see OnInput method) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bStepWhenGamePaused : 1;

	/* If true, active states will continue to tick even if Game is paused (see OnInput method) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bTickWhenGamePaused : 1;

	uint8 bHasCachedRootStates : 1;

	mutable FCriticalSection resetRequestsCS;

	TSet<FGuid> ActiveStateIds;

	TArray<FInputSequenceResetRequest> ResetRequests;

	TWeakObjectPtr<UWorld> WorldPtr;
};