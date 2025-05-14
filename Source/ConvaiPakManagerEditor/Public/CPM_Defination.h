
#pragma once

#include "CoreMinimal.h"
#include "CPM_Defination.generated.h"

USTRUCT(BlueprintType)
struct FCPM_PackageParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	ECPM_Platform Platform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString Configuration;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString OutputDirectory;

	bool IsValid() const
	{		
		return !GetPlatform().IsEmpty()
			&& !Configuration.IsEmpty()
			&& !OutputDirectory.IsEmpty();
	}

	FString GetPlatform() const
	{
		switch (Platform)
		{
		case ECPM_Platform::Windows:
			return FString(TEXT("Win64"));
		case ECPM_Platform::Linux:
			return FString(TEXT("Linux"));
		default:
			return FString();
		}
	}
};
