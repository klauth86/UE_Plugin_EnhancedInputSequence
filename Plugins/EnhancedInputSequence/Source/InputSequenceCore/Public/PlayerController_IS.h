// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "GameFramework/PlayerController.h"
#include "PlayerController_IS.generated.h"

UCLASS()
class INPUTSEQUENCECORE_API APlayerController_IS : public APlayerController
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintImplementableEvent, Category = "Player Controller IS")
	void OnPreProcessInput(const float DeltaTime, const bool bGamePaused);

	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Player Controller IS")
	void OnPostProcessInput(const float DeltaTime, const bool bGamePaused);

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

	UFUNCTION(BlueprintCallable, Category = "Player Controller IS")
	void RegisterInputActionEvent(UInputAction* inputAction, ETriggerEvent triggerEvent);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Player Controller IS")
	TMap<UInputAction*, ETriggerEvent> InputActionEvents;
};