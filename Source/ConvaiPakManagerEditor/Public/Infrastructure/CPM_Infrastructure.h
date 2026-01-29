// Copyright 2022 Convai Inc. All Rights Reserved.

/**
 * CPM_Infrastructure.h
 * 
 * Convenience header that includes all infrastructure components.
 * Include this single header to get access to:
 * - TCPM_Result<T> - Result type for error handling
 * - FCPM_CancellationToken - Cooperative cancellation
 * - FCPM_AsyncOperation<T> - Async operation wrapper
 */

#pragma once

#include "Infrastructure/CPM_Result.h"
#include "Infrastructure/CPM_CancellationToken.h"
#include "Infrastructure/CPM_AsyncOperation.h"
