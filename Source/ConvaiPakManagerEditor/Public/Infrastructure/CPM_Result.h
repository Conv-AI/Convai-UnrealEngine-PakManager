// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Utility/CPM_Log.h"

/**
 * Result type for explicit error handling without exceptions.
 * Provides monadic operations for chaining and error propagation.
 */
template <typename T>
class TCPM_Result
{
public:
	TCPM_Result() : bSuccess(false) {}

	/** Check if the result is successful */
	bool IsSuccess() const { return bSuccess; }
	
	/** Check if the result is a failure */
	bool IsFailure() const { return !bSuccess; }

	/** Get the success value (asserts if failed) */
	const T& GetValue() const
	{
		checkf(bSuccess, TEXT("Attempted to get value from failed result: %s"), *ErrorMessage);
		return Value.GetValue();
	}

	T& GetValue()
	{
		checkf(bSuccess, TEXT("Attempted to get value from failed result: %s"), *ErrorMessage);
		return Value.GetValue();
	}

	/** Get the error message (asserts if successful) */
	const FString& GetError() const
	{
		checkf(!bSuccess, TEXT("Attempted to get error from successful result"));
		return ErrorMessage;
	}

	/** Create a successful result */
	static TCPM_Result<T> Success(const T& InValue)
	{
		TCPM_Result<T> Result;
		Result.Value = InValue;
		Result.bSuccess = true;
		return Result;
	}

	static TCPM_Result<T> Success(T&& InValue)
	{
		TCPM_Result<T> Result;
		Result.Value = MoveTemp(InValue);
		Result.bSuccess = true;
		return Result;
	}

	/** Create a failed result */
	static TCPM_Result<T> Failure(const FString& InError)
	{
		TCPM_Result<T> Result;
		Result.ErrorMessage = InError;
		Result.bSuccess = false;
		return Result;
	}

	/** Map - Transform the success value */
	template <typename TFunc>
	auto Map(TFunc Func) const -> TCPM_Result<decltype(Func(DeclVal<T>()))>
	{
		using TResult = decltype(Func(DeclVal<T>()));

		if (IsFailure())
		{
			return TCPM_Result<TResult>::Failure(ErrorMessage);
		}

		return TCPM_Result<TResult>::Success(Func(GetValue()));
	}

	/** Bind - Chain operations that return Result types */
	template <typename TFunc>
	auto Bind(TFunc Func) const -> decltype(Func(DeclVal<T>()))
	{
		if (IsFailure())
		{
			using TResultType = decltype(Func(DeclVal<T>()));
			return TResultType::Failure(ErrorMessage);
		}

		return Func(GetValue());
	}

	/** Tap - Execute a side effect on success without modifying the result */
	template <typename TFunc>
	TCPM_Result<T> Tap(TFunc Func) const
	{
		if (IsSuccess())
		{
			Func(GetValue());
		}
		return *this;
	}

	/** TapError - Execute a side effect on error */
	template <typename TFunc>
	TCPM_Result<T> TapError(TFunc Func) const
	{
		if (IsFailure())
		{
			Func(ErrorMessage);
		}
		return *this;
	}

	/** LogOnFailure - Log error if failed */
	TCPM_Result<T> LogOnFailure(const TCHAR* Context) const
	{
		if (IsFailure())
		{
			CPM_LOG(Error, TEXT("%s: %s"), Context, *ErrorMessage);
		}
		return *this;
	}

	/** GetValueOr - Get value or default if failed */
	T GetValueOr(const T& DefaultValue) const
	{
		return IsSuccess() ? GetValue() : DefaultValue;
	}

private:
	TOptional<T> Value;
	FString ErrorMessage;
	bool bSuccess;
};

/**
 * Void specialization of Result type
 */
template <>
class TCPM_Result<void>
{
public:
	TCPM_Result() : bSuccess(false) {}

	bool IsSuccess() const { return bSuccess; }
	bool IsFailure() const { return !bSuccess; }

	void GetValue() const
	{
		checkf(bSuccess, TEXT("Attempted to get value from failed result: %s"), *ErrorMessage);
	}

	const FString& GetError() const
	{
		checkf(!bSuccess, TEXT("Attempted to get error from successful result"));
		return ErrorMessage;
	}

	static TCPM_Result<void> Success()
	{
		TCPM_Result<void> Result;
		Result.bSuccess = true;
		return Result;
	}

	static TCPM_Result<void> Failure(const FString& InError)
	{
		TCPM_Result<void> Result;
		Result.ErrorMessage = InError;
		Result.bSuccess = false;
		return Result;
	}

	/** Bind - Chain operations */
	template <typename TFunc>
	auto Bind(TFunc Func) const -> decltype(Func())
	{
		if (IsFailure())
		{
			using TResultType = decltype(Func());
			return TResultType::Failure(ErrorMessage);
		}

		return Func();
	}

	/** Tap - Execute a side effect on success */
	template <typename TFunc>
	TCPM_Result<void> Tap(TFunc Func) const
	{
		if (IsSuccess())
		{
			Func();
		}
		return *this;
	}

	/** LogOnFailure - Log error if failed */
	TCPM_Result<void> LogOnFailure(const TCHAR* Context) const
	{
		if (IsFailure())
		{
			CPM_LOG(Error, TEXT("%s: %s"), Context, *ErrorMessage);
		}
		return *this;
	}

private:
	FString ErrorMessage;
	bool bSuccess;
};
