
#pragma once

#include "CoreMinimal.h"
#include "CPM_Defination.generated.h"

USTRUCT(BlueprintType)
struct FCPM_PackageParam
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString Platform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString Configuration;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Convai|PakManager")
	FString OutputDirectory;

	bool IsValid() const
	{		
		return !Platform.IsEmpty()
			&& !Configuration.IsEmpty()
			&& !OutputDirectory.IsEmpty();
	}
};
