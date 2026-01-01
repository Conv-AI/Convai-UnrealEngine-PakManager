// Copyright 2022 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CPM_WidgetTypes.generated.h"

/**
 * Represents a single key-value pair entry.
 * Used by the KeyValuePair widget for dynamic data entry.
 */
USTRUCT(BlueprintType)
struct CONVAIPAKMANAGER_API FCPM_KeyValuePair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Widgets")
	FString Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convai|Widgets")
	FString Value;

	FCPM_KeyValuePair()
		: Key(TEXT(""))
		, Value(TEXT(""))
	{
	}

	FCPM_KeyValuePair(const FString& InKey, const FString& InValue)
		: Key(InKey)
		, Value(InValue)
	{
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

