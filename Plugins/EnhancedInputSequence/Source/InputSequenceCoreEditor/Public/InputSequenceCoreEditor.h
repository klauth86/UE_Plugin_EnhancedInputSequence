// Copyright 2023 Pentangle Studio under EULA https://www.unrealengine.com/en-US/eula/unreal

#pragma once

#include "Modules/ModuleManager.h"

class FAssetTypeActions_Base;
struct FISGraphNodeFactory;
struct FISGraphPinFactory;
struct FISGraphPinConnectionFactory;

class FInputSequenceCoreEditor : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
protected:
	TArray<TSharedPtr<FAssetTypeActions_Base>> RegisteredAssetTypeActions;
	TSharedPtr<FISGraphNodeFactory> ISGraphNodeFactory;
	TSharedPtr<FISGraphPinFactory> ISGraphPinFactory;
	TSharedPtr<FISGraphPinConnectionFactory> ISGraphPinConnectionFactory;
};