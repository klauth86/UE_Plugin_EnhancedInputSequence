// Fill out your copyright notice in the Description page of Project Settings.

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

namespace EFlags
{
	const uint8 NONE = 0;
}

namespace EFlags_State
{
	const uint8 OVERRIDE_HAS_RESET_TIME = 1;
	const uint8 HAS_RESET_TIME = 2;

	const uint8 IS_ENTRY_STATE = 64;
	const uint8 IS_RESET_STATE = 128;
};

namespace EFlags_InputActionInfo
{
	const uint8 REQUIRE_PRECISE_MATCH = 1;

	const uint8 PASSED = 128;
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

	FResetRequest() :StateGuid(FGuid()), RequestKey(nullptr), bResetAsset(0) {}

	/* Requested by State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	FGuid StateGuid;

	/* Requested with Request Key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	TObjectPtr<URequestKey> RequestKey;

	/* If true, request will reset all active states */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Request")
	uint8 bResetAsset : 1;
};

//------------------------------------------------------
// FEventRequest
//------------------------------------------------------

USTRUCT(BlueprintType)
struct INPUTSEQUENCECORE_API FEventRequest
{
	GENERATED_USTRUCT_BODY()

public:

	FEventRequest() :StateGuid(FGuid()), RequestKey(nullptr), InputSequenceEvent(nullptr) {}

	/* Requested by State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	FGuid StateGuid;

	/* Requested with Request Key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<URequestKey> RequestKey;

	/* Requested Input Sequence Event */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Request")
	TObjectPtr<UInputSequenceEvent> InputSequenceEvent;
};

//------------------------------------------------------
// UInputSequenceEvent
//------------------------------------------------------

UCLASS(Abstract, EditInlineNew, BlueprintType, Blueprintable)
class INPUTSEQUENCECORE_API UInputSequenceEvent : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic, Category = "Input Sequence Event")
	void Execute(const FGuid& stateGuid, URequestKey* requestKey, const TArray<FResetRequest>& resetRequests);

	virtual void Execute_Implementation(const FGuid& stateGuid, URequestKey* requestKey, const TArray<FResetRequest>& resetRequests) {}

	virtual UWorld* GetWorld() const override;
};

//------------------------------------------------------
// FInputSequenceState
//------------------------------------------------------

USTRUCT()
struct INPUTSEQUENCECORE_API FInputSequenceStateCollection
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY()
	TSet<FGuid> NextStateGuids;
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

	bool IsPassed() const { return (Flags & EFlags_InputActionInfo::PASSED) != EFlags::NONE; }

	UPROPERTY()
	ETriggerEvent TriggerEvent;

	UPROPERTY()
	uint8 Flags;

	UPROPERTY()
	float WaitTime;

	float WaitTimeLeft;
};

//------------------------------------------------------
// FInputSequenceState
//------------------------------------------------------

USTRUCT()
struct INPUTSEQUENCECORE_API FInputSequenceState
{
	GENERATED_USTRUCT_BODY()

public:

	FInputSequenceState(uint32 typeHash = INDEX_NONE) { Reset(); }

	bool operator==(const FInputSequenceState& state) const { return state.StateGuid == StateGuid; }

	void Reset();

	UPROPERTY()
	FGuid StateGuid;

	UPROPERTY()
	TMap<FSoftObjectPath, FInputActionInfo> InputActionInfos;

	UPROPERTY()
	TArray<TObjectPtr<UInputSequenceEvent>> EnterEvents;
	UPROPERTY()
	TArray<TObjectPtr<UInputSequenceEvent>> PassEvents;
	UPROPERTY()
	TArray<TObjectPtr<UInputSequenceEvent>> ResetEvents;

	UPROPERTY()
	TObjectPtr<URequestKey> RequestKey;

	UPROPERTY()
	uint8 Flags;
	UPROPERTY()
	float ResetTime;

	float ResetTimeLeft;

	UPROPERTY()
	FGuid RootStateGuid;
};

int32 GetTypeHash(const FInputSequenceState& state) { return GetTypeHash(state.StateGuid); }

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

UCLASS()
class INPUTSEQUENCECORE_API UInputSequence : public UObject
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	UEdGraph* EdGraph;

	TSet<FGuid>& GetEntryStates() { return EntryStates; }

	TSet<FInputSequenceState>& GetStates() { return States; }

	TMap<FGuid, FInputSequenceStateCollection>& GetTransitions() { return Transitions; }

	bool GetRequirePreciseMatchDefaultValue() const { return bRequirePreciseMatchDefaultValue; }

#endif

	/**
	* Feed input to Input Sequence and receives Event Requests and Reset Requests as result
	*
	* @param deltaTime				Request key
	* @param bGamePaused			Request key
	* @param actionStateData		Collection of Input Actions Trigger Events
	* @param outEventRequests		Collection of Event Requests, that will be filled
	* @param outResetRequests		Collection of Reset Requests, that will be filled
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void OnInput(const float deltaTime, const bool bGamePaused, const TMap<FSoftObjectPath, ETriggerEvent>& actionStateData, TArray<FEventRequest>& outEventRequests, TArray<FResetRequest>& outResetRequests);

	/**
	* Requests reset for Input Sequence
	*
	* @param requestKey				Request key
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void RequestReset(URequestKey* requestKey);

	/**
	* Sets World Context for Input Sequence
	*
	* @param worldContextObject		World Context to set
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void SetWorldContext(UObject* worldContextObject) { WorldPtr = worldContextObject ? worldContextObject->GetWorld() : nullptr; }

	virtual UWorld* GetWorld() const override { return WorldPtr.Get(); }

protected:

	void MakeTransition(const FGuid& stateGuid, const TSet<FGuid>& nextStates, TArray<FEventRequest>& outEventCalls);

	const TSet<FGuid>& GetTransitions(const FGuid& stateGuid) const
	{
		static const TSet<FGuid> emptySet;
		return Transitions.Contains(stateGuid) ? Transitions[stateGuid].NextStateGuids : emptySet;
	}

	void RequestReset(const FGuid& stateGuid, URequestKey* requestKey, const bool resetAsset);

	void EnterState(FInputSequenceState* state, TArray<FEventRequest>& outEventCalls);

	void PassState(FInputSequenceState* state, TArray<FEventRequest>& outEventCalls);

	EConsumeInputResponse OnInput(const TMap<FSoftObjectPath, ETriggerEvent>& actionStateData, FInputSequenceState* state);

	EConsumeInputResponse OnTick(const float deltaTime, FInputSequenceState* state);

	void ProcessResetSources(TArray<FEventRequest>& outEventCalls, TArray<FResetRequest>& outResetSources);

	void ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FEventRequest>& outEventCalls);

protected:

	mutable FCriticalSection resetSourcesCS;

	TSet<FGuid> ActiveStates;

	UPROPERTY()
	TSet<FGuid> EntryStates;

	UPROPERTY()
	TSet<FInputSequenceState> States;

	UPROPERTY()
	TMap<FGuid, FInputSequenceStateCollection> Transitions;

	UPROPERTY(Transient)
	TArray<FResetRequest> ResetSources;

	/* If true, any mismatched input will reset asset to initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bRequirePreciseMatchDefaultValue : 1;

	/* If true, asset will be reset if no any successful steps are made during some time interval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (InlineEditConditionToggle))
	uint8 bHasResetTime : 1;

	/* Time interval, after which asset will be reset to initial state if no any successful steps will be made during that period */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10, EditCondition = bHasResetTime))
	float ResetTime;

	/* If true, active states will continue to try stepping further even if Game is paused (Input Sequence is stepping by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bStepWhenGamePaused : 1;

	/* If true, active states will continue to tick even if Game is paused (Input Sequence is ticking by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bTickWhenGamePaused : 1;

	TWeakObjectPtr<UWorld> WorldPtr;
};