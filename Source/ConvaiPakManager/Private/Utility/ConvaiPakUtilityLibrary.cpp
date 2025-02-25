// Fill out your copyright notice in the Description page of Project Settings.

#include "Utility/ConvaiPakUtilityLibrary.h"
#include "DesktopPlatformModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "IPlatformFilePak.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY(LogConvaiPakManager);

FString UConvaiPakUtilityLibrary::OpenFileDialog(const TArray<FString>& Extensions)
{
	FString FilePath;
	FString FileTypes = TEXT("Supported Files (");
	
	// Validate input
	if (Extensions.Num() == 0)
	{
		UE_LOG(LogConvaiPakManager, Warning, TEXT("Extensions array is empty"));
		FileTypes = TEXT("All Files (*.*)|*.*");
	}
	else
	{
		// Construct file filter string
		for (int32 Index = 0; Index < Extensions.Num(); ++Index)
		{
			if (Index > 0)
			{
				FileTypes += TEXT(";");
			}
			FileTypes += TEXT("*.") + Extensions[Index];
		}
	
		FileTypes += TEXT(")|");
		for (int32 Index = 0; Index < Extensions.Num(); ++Index)
		{
			if (Index > 0)
			{
				FileTypes += TEXT(";");
			}
			FileTypes += TEXT("*.") + Extensions[Index];
		}
	}	
	
	// Get the Desktop Platform module
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		// File Dialog Options
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		TArray<FString> OutFiles;
		const bool bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle,
			TEXT("Choose a file"),             // Dialog title
			FPaths::ProjectDir(),					// Default path
			TEXT(""),								// Default file
			FileTypes,								// File types
			EFileDialogFlags::None,
			OutFiles
		);

		if (bOpened && OutFiles.Num() > 0)
		{
			FilePath = OutFiles[0]; 
		}
	}

	return FilePath;
}

FString UConvaiPakUtilityLibrary::GetProjectName()
{
	return FApp::GetProjectName();
}

bool UConvaiPakUtilityLibrary::ValidatePakFile(const FString& PakFilePath)
{
	if (!FPaths::FileExists(PakFilePath))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Pak file does not exist: %s"), *PakFilePath);
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FPakPlatformFile* PakPlatformFile = new FPakPlatformFile();

	if (!PakPlatformFile->Initialize(&PlatformFile, TEXT("")))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to initialize PakPlatformFile."));
		delete PakPlatformFile;
		return false;
	}

	if (!PakPlatformFile->Mount(*PakFilePath, 0, *FPaths::ProjectContentDir()))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to mount Pak file: %s"), *PakFilePath);
		delete PakPlatformFile;
		return false;
	}

	PakPlatformFile->Unmount(*PakFilePath);
	
	delete PakPlatformFile;
	return true;
}

FString UConvaiPakUtilityLibrary::ExtractAssetID()
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("ConvaiEssentials"), TEXT("PakMetaData")) + TEXT(".txt");
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to read PakMetaData.txt"));
		return FString();
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FString AssetID;
		if (JsonObject->TryGetStringField(TEXT("asset_id"), AssetID))
		{
			return AssetID;
		}
		else
		{
			UE_LOG(LogConvaiPakManager, Error, TEXT("Asset ID field not found in JSON"));
		}
	}
	else
	{
		UE_LOG(LogConvaiPakManager, Error, TEXT("Failed to parse JSON from PakMetaData.txt"));
	}

	return FString();
}

bool UConvaiPakUtilityLibrary::Texture2DToPixels(UTexture2D* Texture2D, int32& Width, int32& Height,
	TArray<FColor>& Pixels)
{
	if (!Texture2D) return false;

#if WITH_EDITORONLY_DATA
	if (Texture2D->MipGenSettings != TMGS_NoMipmaps)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Updated Texture2D MipGenSettings to NoMipmaps."));
		Texture2D->MipGenSettings = TMGS_NoMipmaps;
		Texture2D->UpdateResource();
	}
#endif

	bool bChangedTextureSettings = false;
	bool PreviousSRGB = Texture2D->SRGB;
	TextureCompressionSettings PreviousCompressionSettings = Texture2D->CompressionSettings;

	if (PreviousSRGB || PreviousCompressionSettings != TC_VectorDisplacementmap)
	{
		bChangedTextureSettings = true;
		Texture2D->SRGB = false;
		Texture2D->CompressionSettings = TC_VectorDisplacementmap;
		Texture2D->UpdateResource();
	}

	FTexture2DMipMap& Mip0 = Texture2D->GetPlatformData()->Mips[0];
	Width = Mip0.SizeX;
	Height = Mip0.SizeY;
	int32 TotalPixels = Width * Height;

	Pixels.Reserve(TotalPixels);

	void* Mip0Pixels = Mip0.BulkData.Lock(LOCK_READ_ONLY);
	if (!Mip0Pixels)
	{
		UE_LOG(LogBlueprintUserMessages, Error, TEXT("Mip0.BulkData is NULL."));
		Mip0.BulkData.Unlock();
		return false;
	}

	EPixelFormat PixelFormat = Texture2D->GetPixelFormat();

	bool bSuccess = true;
	switch (PixelFormat)
	{
	case PF_B8G8R8A8:
	{
		FColor* PixelData = static_cast<FColor*>(Mip0Pixels);
		Pixels.Append(PixelData, TotalPixels);
	}
	break;

	case PF_R8G8B8A8:
	{
		FColor* PixelData = static_cast<FColor*>(Mip0Pixels);
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			Pixels.Add(FColor(PixelData[i].B, PixelData[i].G, PixelData[i].R, PixelData[i].A));
		}
	}
	break;

	case PF_A8R8G8B8:
	{
		FColor* PixelData = static_cast<FColor*>(Mip0Pixels);
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			Pixels.Add(FColor(PixelData[i].A, PixelData[i].B, PixelData[i].G, PixelData[i].R));
		}
	}
	break;

	default:
		UE_LOG(LogBlueprintUserMessages, Error, TEXT("Unsupported PixelFormat: %d"), PixelFormat);
		bSuccess = false;
		break;
	}

	Mip0.BulkData.Unlock();

	if (bChangedTextureSettings)
	{
		Texture2D->SRGB = PreviousSRGB;
		Texture2D->CompressionSettings = PreviousCompressionSettings;
		Texture2D->UpdateResource();
	}

	return bSuccess;
}

bool UConvaiPakUtilityLibrary::Texture2DToBytes(UTexture2D* Texture2D, const EImageFormat ImageFormat,
                                                TArray<uint8>& ByteArray, const int32 CompressionQuality)
{
	if (!Texture2D)
	{
		UE_LOG(LogTemp, Error, TEXT("Texture2DToBytes: Invalid texture"));
		return false;
	}

	ByteArray.Empty();

	int32 Mip0Width = 0;
	int32 Mip0Height = 0;
	TArray<FColor> Pixels;

	if (!Texture2DToPixels(Texture2D, Mip0Width, Mip0Height, Pixels))
	{
		return false;
	}

	if (Mip0Width > 0 && Mip0Height > 0)
	{
		return PixelsToBytes(Mip0Width, Mip0Height, Pixels, ImageFormat, ByteArray, CompressionQuality);
	}

	return false;
}

bool UConvaiPakUtilityLibrary::PixelsToBytes(const int32 Width, const int32 Height, const TArray<FColor>& Pixels,
	const EImageFormat ImageFormat, TArray<uint8>& ByteArray, const int32 CompressionQuality)
{
	if (Width <= 0 || Height <= 0 || CompressionQuality < 0 || CompressionQuality > 100) return false;

	const int32 TotalPixels = Width * Height;
	if (Pixels.Num() != TotalPixels)
	{
		UE_LOG(LogBlueprintUserMessages, Error, TEXT("Pixel count mismatch."));
		return false;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (!ImageWrapper.IsValid()) return false;

	if (ImageFormat == EImageFormat::GrayscaleJPEG)
	{
		TArray<uint8> MutablePixels;
		MutablePixels.Reserve(TotalPixels);

		for (const FColor& Pixel : Pixels)
		{
			MutablePixels.Add(static_cast<uint8>(FMath::RoundToDouble((0.2125 * Pixel.R) + (0.7154 * Pixel.G) + (0.0721 * Pixel.B))));
		}

		if (!ImageWrapper->SetRaw(MutablePixels.GetData(), MutablePixels.Num(), Width, Height, ERGBFormat::Gray, 8)) return false;
	}
	else
	{
		TArray<FColor> MutablePixels = Pixels;
		for (FColor& Pixel : MutablePixels)
		{
			Swap(Pixel.R, Pixel.B);
		}

		if (!ImageWrapper->SetRaw(MutablePixels.GetData(), MutablePixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::RGBA, 8)) return false;
	}

	ByteArray = ImageWrapper->GetCompressed(CompressionQuality);
	return true;
}
