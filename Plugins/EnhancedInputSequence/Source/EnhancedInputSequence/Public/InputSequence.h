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

USTRUCT(BlueprintType)
struct ENHANCEDINPUTSEQUENCE_API FResetSource
{
	GENERATED_USTRUCT_BODY()

public:

	FResetSource() :StateGuid(FGuid()), bResetAsset(0), SourceObject(nullptr), SourceContext("") {}

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

USTRUCT(BlueprintType)
struct ENHANCEDINPUTSEQUENCE_API FEventCall
{
	GENERATED_USTRUCT_BODY()

public:

	FEventCall() :Event(nullptr), StateGuid(FGuid()), SourceObject(nullptr), SourceContext("") {}

	/* Input Sequence Event for this Event Call */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event Call")
	TObjectPtr<UInputSequenceEvent> Event;

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

UCLASS(Abstract, EditInlineNew, BlueprintType, Blueprintable)
class ENHANCEDINPUTSEQUENCE_API UInputSequenceEvent : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic, Category = "Input Sequence Event")
	void Execute(UObject* callingObject, const FString& callingContext, const FGuid& stateGuid, UObject* stateObject, const FString& stateContext, const TArray<FResetSource>& resetSources);

	virtual void Execute_Implementation(UObject* callingObject, const FString& callingContext, const FGuid& stateGuid, UObject* stateObject, const FString& stateContext, const TArray<FResetSource>& resetSources) {}
};

USTRUCT()
struct ENHANCEDINPUTSEQUENCE_API FInputSequenceState
{
	GENERATED_USTRUCT_BODY()

public:

	FInputSequenceState() { Reset(); }

	bool operator==(const FInputSequenceState& state) const { return state.StateGuid == StateGuid; }

	bool IsEmpty() const { return ActionStateData.IsEmpty(); }

	void Reset() {}

	UPROPERTY()
	FGuid StateGuid;

	UPROPERTY()
	TMap<TObjectPtr<const UInputAction>, ETriggerState> ActionStateData;
	UPROPERTY()
	TMap<TObjectPtr<const UInputAction>, float> ActionPassData;

	UPROPERTY()
	TArray<TObjectPtr<UInputSequenceEvent>> EnterEvents;
	UPROPERTY()
	TArray<TObjectPtr<UInputSequenceEvent>> PassEvents;
	UPROPERTY()
	TArray<TObjectPtr<UInputSequenceEvent>> ResetEvents;

	UPROPERTY()
	TObjectPtr<UObject> StateObject;
	UPROPERTY()
	FString StateContext;

	UPROPERTY()
	TSet<FGuid> NextStates;

	UPROPERTY()
	int32 DepthIndex;
	UPROPERTY()
	FGuid ParentState;

	UPROPERTY()
	uint8 bOverrideRequirePreciseMatch : 1;
	UPROPERTY()
	uint8 bRequirePreciseMatch : 1;

	UPROPERTY()
	uint8 bOverrideResetAfterTime : 1;
	UPROPERTY()
	uint8 bIsResetAfterTime : 1;

	UPROPERTY()
	uint8 bIsResetState : 1;
};

int32 GetTypeHash(const FInputSequenceState& state) { return GetTypeHash(state.StateGuid); }

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
	void OnInput(const float deltaTime, const bool bGamePaused, const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, TArray<FEventCall>& outEventCalls, TArray<FResetSource>& outResetSources);

	UFUNCTION(BlueprintCallable, Category = "Input Sequence")
	void RequestReset(UObject* sourceObject, const FString& sourceContext);

protected:

	void MakeTransition(const FGuid& stateGuid, const TSet<FGuid>& nextStates, TArray<FEventCall>& outEventCalls);

	void RequestReset(const FGuid& stateGuid, const bool resetAsset);

	void EnterState(FInputSequenceState* state, TArray<FEventCall>& outEventCalls);

	void PassState(FInputSequenceState* state, TArray<FEventCall>& outEventCalls);

	EConsumeInputResponse OnInput(const TMap<TObjectPtr<UInputAction>, ETriggerState>& actionStateData, FInputSequenceState* state);

	EConsumeInputResponse OnTick(const float deltaTime, FInputSequenceState* state);

	void ProcessResetSources(TArray<FEventCall>& outEventCalls, TArray<FResetSource>& outResetSources);

	void ProcessResetSources_Internal(const TSet<FGuid>& statesToReset, TArray<FEventCall>& outEventCalls);

	bool IsEntryState(const FGuid& stateGuid) { return EntryStates.Contains(stateGuid); }

protected:

	mutable FCriticalSection resetSourcesCS;

	TSet<FGuid> EntryStates;

	TSet<FGuid> ActiveStates;

	UPROPERTY()
	TSet<FInputSequenceState> States;

	UPROPERTY()
	TArray<FResetSource> ResetSources;

	/* Time interval, after which asset will be reset to initial state if no any successful steps will be made during that period */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (DisplayPriority = 2, UIMin = 0.01, Min = 0.01, UIMax = 10, Max = 10, EditCondition = bIsResetAfterTime, EditConditionHides))
	float ResetAfterTime;

	/* If true, any mismatched input will reset asset to initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (DisplayPriority = 0))
	uint8 bRequirePreciseMatch : 1;

	/* If true, asset will be reset if no any successful steps are made during some time interval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (DisplayPriority = 1))
	uint8 bIsResetAfterTime : 1;

	/* If true, active states will continue to try stepping further even if Game is paused (Input Sequence is stepping by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (DisplayPriority = 10))
	uint8 bStepFromStatesWhenGamePaused : 1;

	/* If true, active states will continue to tick even if Game is paused (Input Sequence is ticking by OnInput method call) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input Sequence", meta = (DisplayPriority = 11))
	uint8 bTickStatesWhenGamePaused : 1;
};