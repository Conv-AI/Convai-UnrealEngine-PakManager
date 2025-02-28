// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(ConvaiPakManagerLog, Log, All);

// Use the macro from the Build.cs definition
#if CONVAI_PAK_MANAGER_LOG
#define CPM_LOG(Verbosity, Format, ...) UE_LOG(ConvaiPakManagerLog, Verbosity, Format, ##__VA_ARGS__)
#else
#define CPM_LOG(Verbosity, Format, ...)
#endif
