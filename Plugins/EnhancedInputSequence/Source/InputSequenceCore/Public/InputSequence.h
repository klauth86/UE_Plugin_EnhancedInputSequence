// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "InputTriggers.h"
#include "InputSequence.generated.h"

class UEdGraph;
class UInputAction;

enum class EConsumeInputResponse :uint8
{
	NONE,
	RESET,
	PASSED
};

//------------------------------------------------------
// URequestKey
//------------------------------------------------------

UCLASS(BlueprintType)
class INPUTSEQUENCECORE_API URequestKey :public UObject
{
	GENERATED_BODY()
};

//------------------------------------------------------
// FResetRequest
//------------------------------------------------------

USTRUCT(BlueprintType)
struct INPUTSEQUENCECORE_API FResetRequest
{
	GENERATED_USTRUCT_BODY()

public:

	FResetRequest() :State(nullptr), RequestKey(nullptr), bResetAll(0) {}

	/* Requested by State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	TObjectPtr<UInputSequenceState_Base> State;

	/* Requested with Request Key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	TObjectPtr<URequestKey> RequestKey;

	/* If true, request will reset all active states */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	uint8 bResetAll : 1;
};

//------------------------------------------------------
// FEventRequest
//------------------------------------------------------

USTRUCT(BlueprintType)
struct INPUTSEQUENCECORE_API FEventRequest
{
	GENERATED_USTRUCT_BODY()

public:

	FEventRequest() :State(nullptr), RequestKey(nullptr), Event(nullptr) {}

	/* Requested by State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UInputSequenceState_Base> State;

	/* Requested with Request Key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<URequestKey> RequestKey;

	/* Requested Input Sequence Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UInputSequenceEvent> Event;
};

//------------------------------------------------------
// UInputSequenceEvent
//------------------------------------------------------

UCLASS(Abstract, EditInlineNew, BlueprintType, Blueprintable)
class INPUTSEQUENCECORE_API UInputSequenceEvent : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic, BlueprintCallable, Category = "Input Sequence Event")
	void Execute(UInputSequenceState_Base* state, URequestKey* requestKey, const TArray<FResetRequest>& resetRequests);

	virtual void Execute_Implementation(UInputSequenceState_Base* state, URequestKey* requestKey, const TArray<FResetRequest>& resetRequests) {}

	virtual UWorld* GetWorld() const override;
};

//------------------------------------------------------
// FInputActionInfo
//------------------------------------------------------

USTRUCT()
struct INPUTSEQUENCECORE_API FInputActionInfo
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
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	TObjectPtr<UInputSequenceState_Base> RootState;

	UPROPERTY()
	TSet<TObjectPtr<UInputSequenceState_Base>> NextStates;

	virtual void OnEnter(TArray<FEventRequest>& outEventCalls, const float resetTime) {}
	virtual void OnPass(TArray<FEventRequest>& outEventCalls) {}
	virtual void OnReset(TArray<FEventRequest>& outEventCalls) {}
};

//------------------------------------------------------
// UInputSequenceState_Hub
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Hub : public UInputSequenceState_Base
{
	GENERATED_UCLASS_BODY()
};

//------------------------------------------------------
// UInputSequenceState_Reset
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Reset : public UInputSequenceState_Base
{
	GENERATED_UCLASS_BODY()

public:

	/* Request Key for this state */
	UPROPERTY(EditAnywhere, Category = "Context:")
	TObjectPtr<URequestKey> RequestKey;
};

//------------------------------------------------------
// UInputSequenceState_Input
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequenceState_Input : public UInputSequenceState_Base
{
	GENERATED_UCLASS_BODY()

public:

	virtual void OnEnter(TArray<FEventRequest>& outEventCalls, const float resetTime) override;
	virtual void OnPass(TArray<FEventRequest>& outEventCalls) override;
	virtual void OnReset(TArray<FEventRequest>& outEventCalls) override;

	UPROPERTY()
	TMap<FSoftObjectPath, FInputActionInfo> InputActionInfos;

	/* Enter Events for this state */
	UPROPERTY(EditAnywhere, Category = "Events:", meta=(EditInline))
	TArray<TObjectPtr<UInputSequenceEvent>> EnterEvents;
	/* Pass Events for this state */
	UPROPERTY(EditAnywhere, Category = "Events:", meta = (EditInline))
	TArray<TObjectPtr<UInputSequenceEvent>> PassEvents;
	/* Reset Events for this state */
	UPROPERTY(EditAnywhere, Category = "Events:", meta = (EditInline))
	TArray<TObjectPtr<UInputSequenceEvent>> ResetEvents;

	/* Request Key for this state */
	UPROPERTY(EditAnywhere, Category = "Context:")
	TObjectPtr<URequestKey> RequestKey;

	/* If true, this state will be reset if there will be any unexpected input */
	UPROPERTY(EditAnywhere, Category = "Input:")
	uint8 bRequirePreciseMatch : 1;

	/* If true, this state will be reset if no any successful steps will be made within some time interval */
	UPROPERTY(EditAnywhere, Category = "Overrides", meta = (InlineEditConditionToggle))
	uint8 bOverrideResetTime : 1;

	/* Time interval, after which this state will be reset if no any successful steps will be made within this interval */
	UPROPERTY(EditAnywhere, Category = "Overrides", meta = (UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10, EditCondition = bOverrideResetTime))
	float ResetTime;

	float ResetTimeLeft;
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

	TSet<TObjectPtr<UInputSequenceState_Base>>& GetEntryStates() { return EntryStates; }

	UPROPERTY()
	TMap<FGuid, TObjectPtr<UInputSequenceState_Base>> NodeToStateMapping;

#endif

	/**
	* Feed input to Input Sequence and receives Event Requests and Reset Requests as result
	*
	* @param deltaTime				Delta Time
	* @param bGamePaused			Game Paused flag
	* @param actionStateData		Collection of Input Actions Trigger Events
	* @param outEventRequests		Collection of Event Requests, that will be filled
	* @param outResetRequests		Collection of Reset Requests, that will be filled
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void OnInput(const float deltaTime, const bool bGamePaused, const TMap<UInputAction*, ETriggerEvent>& actionStateData, TArray<FEventRequest>& outEventRequests, TArray<FResetRequest>& outResetRequests);

	/**
	* Requests reset for Input Sequence with Request Key
	*
	* @param requestKey				Request key
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void RequestReset(URequestKey* requestKey) { RequestReset(nullptr, requestKey, true); }

	/**
	* Sets World Context for Input Sequence
	*
	* @param worldContextObject		World Context to set
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void SetWorldContext(UObject* worldContextObject) { WorldPtr = worldContextObject ? worldContextObject->GetWorld() : nullptr; }

	virtual UWorld* GetWorld() const override { return WorldPtr.Get(); }

protected:

	void MakeTransition(UInputSequenceState_Base* fromState, const TSet<TObjectPtr<UInputSequenceState_Base>>& toStates, TArray<FEventRequest>& outEventCalls);

	void RequestReset(const TObjectPtr<UInputSequenceState_Base> state, const TObjectPtr<URequestKey> requestKey, const bool resetAsset);

	void EnterState(UInputSequenceState_Base* state, TArray<FEventRequest>& outEventCalls);

	void PassState(UInputSequenceState_Base* state, TArray<FEventRequest>& outEventCalls);

	EConsumeInputResponse OnInput(const TMap<UInputAction*, ETriggerEvent>& actionStateData, UInputSequenceState_Input* state);

	EConsumeInputResponse OnTick(const float deltaTime, UInputSequenceState_Input* state);

	void ProcessResetSources(TArray<FEventRequest>& outEventCalls, TArray<FResetRequest>& outResetSources);

	void CacheRootStates();

protected:

	UPROPERTY()
	TSet<TObjectPtr<UInputSequenceState_Base>> EntryStates;

	/* Time interval, after which any active state will be reset if no any successful steps will be made within this interval (can be override in state). Zero value means reset will not trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10, EditCondition = bHasResetTime))
	float ResetTime;

	/* If true, active states will continue to try stepping further even if Game is paused (Input Sequence is stepping by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bStepWhenGamePaused : 1;

	/* If true, active states will continue to tick even if Game is paused (Input Sequence is ticking by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bTickWhenGamePaused : 1;

	uint8 bHasCachedRootStates : 1;

	mutable FCriticalSection resetSourcesCS;

	TSet<TObjectPtr<UInputSequenceState_Base>> ActiveStates;

	TArray<FResetRequest> ResetSources;

	TWeakObjectPtr<UWorld> WorldPtr;
};