// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "GameFramework/PlayerController.h"
#include "PlayerController_EIS.generated.h"

UCLASS()
class INPUTSEQUENCECORE_API APlayerController_EIS : public APlayerController
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent, Category = "Player Controller EIS")
	void OnPreProcessInput(const float DeltaTime, const bool bGamePaused);

	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Player Controller EIS")
	void OnPostProcessInput(const float DeltaTime, const bool bGamePaused);

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION(BlueprintCallable, Category = "Player Controller EIS")
	void RegisterInputActionEvent(UInputAction* inputAction, ETriggerEvent triggerEvent);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Player Controller EIS")
	TMap<UInputAction*, ETriggerEvent> InputActionEvents;
};