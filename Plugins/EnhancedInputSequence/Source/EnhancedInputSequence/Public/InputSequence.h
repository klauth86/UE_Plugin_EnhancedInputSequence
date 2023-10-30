// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UObject/NoExportTypes.h"
#include "InputSequence.generated.h"

class UEdGraph;
class UInputAction;

enum class ETriggerState : uint8;

enum class EConsumeInputResponse : uint8
{
	NONE,
	RESET,
	PASS
};

//------------------------------------------------------
// FISResetSource
//------------------------------------------------------

USTRUCT(BlueprintType)
struct ENHANCEDINPUTSEQUENCE_API FISResetSource
{
	GENERATED_USTRUCT_BODY()

public:

	FISResetSource() :StateGuid(FGuid()), bResetAsset(0), SourceObject(nullptr), SourceContext("") {}

	/* Request State */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	FGuid StateGuid;

	/* Request should reset whole asset */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	uint8 bResetAsset : 1;

	/* Request State Source Object or external Source Object */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	TObjectPtr<UObject> SourceObject;

	/* Request State Source Context or external Source Context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reset Source")
	FString SourceContext;
};

//------------------------------------------------------
// FISEventCall
//------------------------------------------------------

USTRUCT(BlueprintType)
struct ENHANCEDINPUTSEQUENCE_API FISEventCall
{
	GENERATED_USTRUCT_BODY()

public:

	FISEventCall() :Event(nullptr), StateGuid(FGuid()), SourceObject(nullptr), SourceContext("") {}

	/* Input Sequence Event for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	TObjectPtr<UISEvent> Event;

	/* Owning State for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	FGuid StateGuid;

	/* Owning State Source Object for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	TObjectPtr<UObject> SourceObject;

	/* Owning State Source Context for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	FString SourceContext;
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
	void Execute(UObject* callingObject, const FString& callingContext, const FGuid& stateGuid, UObject* stateObject, const FString& stateContext, const TArray<FISResetSource>& resetSources);

	virtual void Execute_Implementation(UObject* callingObject, const FString& callingContext, const FGuid& stateGuid, UObject* stateObject, const FString& stateContext, const TArray<FISResetSource>& resetSources) {}
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
// FISState
//------------------------------------------------------

USTRUCT()
struct ENHANCEDINPUTSEQUENCE_API FISState
{
	GENERATED_USTRUCT_BODY()

public:

	FISState() { Reset(); }

	bool operator==(const FISState& state) const { return state.StateGuid == StateGuid; }

	bool IsEmpty() const { return ActionStateData.IsEmpty(); }

	void Reset();

	UPROPERTY()
	FGuid StateGuid;

	UPROPERTY()
	TMap<TObjectPtr<const UInputAction>, ETriggerState> ActionStateData;
	UPROPERTY()
	TMap<TObjectPtr<const UInputAction>, float> ActionPassData;

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
	int32 DepthIndex;
	UPROPERTY()
	FGuid ParentState;

	UPROPERTY()
	uint8 bOverrideRequirePreciseMatch : 1;
	UPROPERTY()
	uint8 bRequirePreciseMatch : 1;

	UPROPERTY()
	uint8 bOverrideHasResetTime : 1;
	UPROPERTY()
	uint8 bHasResetTime : 1;

	UPROPERTY()
	float ResetTime;

	UPROPERTY()
	uint8 bIsResetState : 1;
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

#endif

	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void OnInput(const float deltaTime, const bool bGamePaused, const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources);

	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void RequestReset(UObject* sourceObject, const FString& sourceContext);

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

	EConsumeInputResponse OnInput(const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, FISState* state);

	EConsumeInputResponse OnTick(const float deltaTime, FISState* state);

	void ProcessResetSources(TArray<FISEventCall>& outEventCalls, TArray<FISResetSource>& outResetSources);

	void ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FISEventCall>& outEventCalls);

protected:

	mutable FCriticalSection resetSourcesCS;

	TSet<FGuid> EntryStates;

	TSet<FGuid> ActiveStates;

	UPROPERTY()
	TSet<FISState> States;

	UPROPERTY()
	TMap<FGuid, FISStateCollection> Transitions;

	UPROPERTY(Transient)
	TArray<FISResetSource> ResetSources;

	/* If true, any mismatched input will reset asset to initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bRequirePreciseMatch : 1;

	/* If true, asset will be reset if no any successful steps are made during some time interval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (InlineEditConditionToggle))
	uint8 bHasResetTime : 1;

	/* Time interval, after which asset will be reset to initial state if no any successful steps will be made during that period */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10, EditCondition = bIsResetAfterTime))
	float ResetTime;

	/* If true, active states will continue to try stepping further even if Game is paused (Input Sequence is stepping by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bStepWhenGamePaused : 1;

	/* If true, active states will continue to tick even if Game is paused (Input Sequence is ticking by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence")
	uint8 bTickWhenGamePaused : 1;
};