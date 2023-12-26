// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputTriggers.h"
#include "InputSequence.generated.h"

class UEdGraph;
class UInputAction;

enum class EConsumeInputResponse : uint8
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
// FISResetSource
//------------------------------------------------------

USTRUCT(BlueprintType)
struct ENHANCEDINPUTSEQUENCE_API FISResetSource
{
	GENERATED_USTRUCT_BODY()

public:

	FISResetSource() :StateGuid(FGuid()), bResetAsset(0), RequestObject(nullptr), RequestContext("") {}

	/* Request State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	FGuid StateGuid;

	/* Request should reset whole asset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	uint8 bResetAsset : 1;

	/* Request State Source Object or external Source Object */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	TObjectPtr<UObject> RequestObject;

	/* Request State Source Context or external Source Context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	FString RequestContext;
};

//------------------------------------------------------
// FISEventCall
//------------------------------------------------------

USTRUCT(BlueprintType)
struct ENHANCEDINPUTSEQUENCE_API FISEventCall
{
	GENERATED_USTRUCT_BODY()

public:

	FISEventCall() :Event(nullptr), StateGuid(FGuid()), StateObject(nullptr), StateContext("") {}

	/* Input Sequence Event for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	TObjectPtr<UISEvent> Event;

	/* Owning State for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	FGuid StateGuid;

	/* Owning State Source Object for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	TObjectPtr<UObject> StateObject;

	/* Owning State Source Context for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	FString StateContext;
};

//------------------------------------------------------
// UISEvent
//------------------------------------------------------

UCLASS(Abstract, EditInlineNew, BlueprintType, Blueprintable)
class ENHANCEDINPUTSEQUENCE_API UISEvent : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic, Category = "Input Sequence Event")
	void Execute(const FGuid& stateGuid, UObject* stateObject, const FString& stateContext, const TArray<FISResetSource>& resetSources);

	virtual void Execute_Implementation(const FGuid& stateGuid, UObject* stateObject, const FString& stateContext, const TArray<FISResetSource>& resetSources) {}

	virtual UWorld* GetWorld() const override;
};

//------------------------------------------------------
// FISState
//------------------------------------------------------

USTRUCT()
struct ENHANCEDINPUTSEQUENCE_API FISStateCollection
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY()
	TSet<FGuid> StateGuids;
};

//------------------------------------------------------
// FISStateInputCheck
//------------------------------------------------------

USTRUCT()
struct ENHANCEDINPUTSEQUENCE_API FISInputActionInfo
{
	GENERATED_USTRUCT_BODY()

public:

	FISInputActionInfo()
	{
		TriggerEvent = ETriggerEvent::None;
		InputActionInfoFlags = EFlags::NONE;
		WaitTime = 0;
		WaitTimeLeft = 0;
	}

	UPROPERTY()
	ETriggerEvent TriggerEvent;

	UPROPERTY()
	uint8 InputActionInfoFlags;

	UPROPERTY()
	float WaitTime;

	float WaitTimeLeft;
};

//------------------------------------------------------
// FISState
//------------------------------------------------------

USTRUCT()
struct ENHANCEDINPUTSEQUENCE_API FISState
{
	GENERATED_USTRUCT_BODY()

public:

	FISState(uint32 typeHash = INDEX_NONE) { Reset(); }

	bool operator==(const FISState& state) const { return state.StateGuid == StateGuid; }

	void Reset();

	UPROPERTY()
	FGuid StateGuid;

	UPROPERTY()
	TMap<FSoftObjectPath, FISInputActionInfo> InputActionInfos;

	UPROPERTY()
	TArray<TObjectPtr<UISEvent>> EnterEvents;
	UPROPERTY()
	TArray<TObjectPtr<UISEvent>> PassEvents;
	UPROPERTY()
	TArray<TObjectPtr<UISEvent>> ResetEvents;

	UPROPERTY()
	TObjectPtr<UObject> StateObject;
	UPROPERTY()
	FString StateContext;

	UPROPERTY()
	uint8 StateFlags;
	UPROPERTY()
	float ResetTime;

	float ResetTimeLeft;

	UPROPERTY()
	int32 DepthIndex;
	UPROPERTY()
	FGuid ParentState;
};

int32 GetTypeHash(const FISState& state) { return GetTypeHash(state.StateGuid); }

//------------------------------------------------------
// UInputSequence
//------------------------------------------------------

UCLASS()
class ENHANCEDINPUTSEQUENCE_API UInputSequence : public UObject
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	UEdGraph* EdGraph;

	TSet<FGuid>& GetEntryStates() { return EntryStates; }

	TSet<FISState>& GetStates() { return States; }

	TMap<FGuid, FISStateCollection>& GetTransitions() { return Transitions; }

	bool GetRequirePreciseMatchDefaultValue() const { return bRequirePreciseMatchDefaultValue; }

#endif

	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void OnInput(const float deltaTime, const bool bGamePaused, const TMap<FSoftObjectPath, ETriggerEvent>& actionStateData, TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources);

	/**
	* Sets World Context for Flow
	*
	* @param worldContextObject		World Context to set
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void RequestReset(UObject* requestObject, const FString& requestContext);

	/**
	* Sets World Context for Flow
	*
	* @param worldContextObject		World Context to set
	*/
	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void SetWorldContext(UObject* worldContextObject) { WorldPtr = worldContextObject ? worldContextObject->GetWorld() : nullptr; }

	virtual UWorld* GetWorld() const override { return WorldPtr.Get(); }

protected:

	void MakeTransition(const FGuid& stateGuid, const TSet<FGuid>& nextStates, TArray<FISEventCall>& outEventCalls);

	const TSet<FGuid>& GetTransitions(const FGuid& stateGuid) const
	{
		static const TSet<FGuid> emptySet;
		return Transitions.Contains(stateGuid) ? Transitions[stateGuid].StateGuids : emptySet;
	}

	void RequestReset(const FGuid& stateGuid, const bool resetAsset);

	void PassState(FISState* state, TArray<FISEventCall>& outEventCalls);

	void EnterState(FISState* state, TArray<FISEventCall>& outEventCalls);

	EConsumeInputResponse OnInput(const TMap<FSoftObjectPath, ETriggerEvent>& actionStateData, FISState* state);

	EConsumeInputResponse OnTick(const float deltaTime, FISState* state);

	void ProcessResetSources(TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources);

	void ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FISEventCall>& outEventCalls);

protected:

	mutable FCriticalSection resetSourcesCS;

	TSet<FGuid> ActiveStates;

	UPROPERTY()
	TSet<FGuid> EntryStates;

	UPROPERTY()
	TSet<FISState> States;

	UPROPERTY()
	TMap<FGuid, FISStateCollection> Transitions;

	UPROPERTY(Transient)
	TArray<FISResetSource> ResetSources;

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