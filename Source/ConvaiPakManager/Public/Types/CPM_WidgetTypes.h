// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CPM_WidgetTypes.generated.h"

/**
 * Represents a single key-value pair entry with optional control settings.
 * Used by the KeyValuePair widget for dynamic data entry.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_KeyValuePair
{
	GENERATED_BODY()

	/** The key/label for this entry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePair")
	FString Key;

	/** The value for this entry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePair")
	FString Value;

	/** If true, the key field cannot be edited by the user */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePair|Control")
	bool bKeyReadOnly;

	/** If true, shows a dropdown instead of text input for the value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePair|Control")
	bool bUseDropdownForValue;

	/** If true, the remove button is hidden for this pair */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePair|Control")
	bool bCannotRemove;

	/** Options to show in dropdown when bUseDropdownForValue is true */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|PakManager|KeyValuePair|Control", meta = (EditCondition = "bUseDropdownForValue"))
	TArray<FString> ValueOptions;

	FCPM_KeyValuePair()
		: Key(TEXT(""))
		, Value(TEXT(""))
		, bKeyReadOnly(false)
		, bUseDropdownForValue(false)
		, bCannotRemove(false)
	{
	}

	FCPM_KeyValuePair(const FString& InKey, const FString& InValue)
		: Key(InKey)
		, Value(InValue)
		, bKeyReadOnly(false)
		, bUseDropdownForValue(false)
		, bCannotRemove(false)
	{
	}

	/** Create a locked key pair (key is read-only) */
	static FCPM_KeyValuePair MakeLockedKey(const FString& InKey, const FString& InValue = FString())
	{
		FCPM_KeyValuePair Pair(InKey, InValue);
		Pair.bKeyReadOnly = true;
		return Pair;
	}

	/** Create a dropdown pair with predefined options */
	static FCPM_KeyValuePair MakeDropdown(const FString& InKey, const TArray<FString>& InOptions, const FString& InDefaultValue = FString())
	{
		FCPM_KeyValuePair Pair(InKey, InDefaultValue.IsEmpty() && InOptions.Num() > 0 ? InOptions[0] : InDefaultValue);
		Pair.bKeyReadOnly = true;
		Pair.bUseDropdownForValue = true;
		Pair.ValueOptions = InOptions;
		return Pair;
	}

	/** Create a fixed pair that cannot be removed */
	static FCPM_KeyValuePair MakeFixed(const FString& InKey, const FString& InValue = FString())
	{
		FCPM_KeyValuePair Pair(InKey, InValue);
		Pair.bKeyReadOnly = true;
		Pair.bCannotRemove = true;
		return Pair;
	}

	bool IsEmpty() const
	{
		return Key.IsEmpty() && Value.IsEmpty();
	}

	bool IsValid() const
	{
		return !Key.IsEmpty();
	}

	bool operator==(const FCPM_KeyValuePair& Other) const
	{
		return Key == Other.Key && Value == Other.Value;
	}

	bool operator!=(const FCPM_KeyValuePair& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * Enum for button style variants
 */
UENUM(BlueprintType)
enum class ECPM_ButtonStyle : uint8
{
	Primary		UMETA(DisplayName = "Primary"),
	Secondary	UMETA(DisplayName = "Secondary"),
	Danger		UMETA(DisplayName = "Danger"),
	Ghost		UMETA(DisplayName = "Ghost")
};

/**
 * Enum for text/label style variants
 */
UENUM(BlueprintType)
enum class ECPM_TextStyle : uint8
{
	Header		UMETA(DisplayName = "Header"),
	Body		UMETA(DisplayName = "Body"),
	Caption		UMETA(DisplayName = "Caption"),
	Button		UMETA(DisplayName = "Button")
};

