// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Base interface for all Pak Manager services.
 * Provides lifecycle management and type identification.
 */
class CONVAIPAKMANAGEREDITOR_API ICPM_Service : public TSharedFromThis<ICPM_Service>
{
public:
	virtual ~ICPM_Service() = default;

	/** Initialize the service */
	virtual void Initialize() {}

	/** Shutdown the service */
	virtual void Shutdown() {}

	/** Get service type name for debugging */
	virtual FName GetServiceName() const = 0;
};
