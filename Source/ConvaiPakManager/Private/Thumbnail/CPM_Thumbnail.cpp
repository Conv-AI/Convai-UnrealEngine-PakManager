// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Thumbnail/CPM_Thumbnail.h"

#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"

namespace
{
	/**
	 * BGRA8 is asked for rather than RGBA8 because FColor is laid out B,G,R,A on every platform this
	 * tool builds for, so the decoded bytes are already an FColor array and no per-pixel swizzle runs.
	 */
	bool DecodeBytes(const TArray<uint8>& Bytes, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		const EImageFormat Format = ImageWrapperModule.DetectImageFormat(Bytes.GetData(), Bytes.Num());
		if (Format == EImageFormat::Invalid)
		{
			return false;
		}

		const TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Format);
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num()))
		{
			return false;
		}

		TArray<uint8> Raw;
		if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
		{
			return false;
		}

		OutWidth = Wrapper->GetWidth();
		OutHeight = Wrapper->GetHeight();
		if (OutWidth <= 0 || OutHeight <= 0 || Raw.Num() != OutWidth * OutHeight * 4)
		{
			return false;
		}

		OutPixels.SetNumUninitialized(OutWidth * OutHeight);
		FMemory::Memcpy(OutPixels.GetData(), Raw.GetData(), Raw.Num());
		return true;
	}
}

namespace ConvaiPakManager::Thumbnail
{
bool HasContent(const TArrayView<const FColor> Pixels, const float MinRatio)
{
	if (Pixels.IsEmpty())
	{
		return false;
	}

	int32 Counted = 0;
	for (const FColor& Pixel : Pixels)
	{
		if (Pixel.A > 0 && (Pixel.R > 5 || Pixel.G > 5 || Pixel.B > 5))
		{
			++Counted;
		}
	}

	return static_cast<float>(Counted) / static_cast<float>(Pixels.Num()) >= MinRatio;
}

bool DecodeImageFile(const FString& Path, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() == 0)
	{
		return false;
	}
	return DecodeBytes(Bytes, OutWidth, OutHeight, OutPixels);
}

bool WritePng(const FString& Path, const int32 Width, const int32 Height, const TArrayView<const FColor> Pixels)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return false;
	}

	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
	if (Png.Num() == 0)
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveArrayToFile(Png, *Path);
}

bool FileHasContent(const FString& Path)
{
	int32 Width = 0;
	int32 Height = 0;
	TArray<FColor> Pixels;
	return DecodeImageFile(Path, Width, Height, Pixels) && HasContent(Pixels);
}

bool ImportImageFile(const FString& SourcePath, const FString& DestinationPath, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *SourcePath) || Bytes.Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s could not be read."), *SourcePath);
		return false;
	}

	int32 Width = 0;
	int32 Height = 0;
	TArray<FColor> Pixels;
	if (!DecodeBytes(Bytes, Width, Height, Pixels))
	{
		OutError = FString::Printf(TEXT("%s is not an image this tool can read."), *SourcePath);
		return false;
	}

	if (!HasContent(Pixels))
	{
		OutError = FString::Printf(TEXT("%s is blank."), *SourcePath);
		return false;
	}

	if (!WritePng(DestinationPath, Width, Height, Pixels))
	{
		OutError = FString::Printf(TEXT("Could not write the thumbnail to %s."), *DestinationPath);
		return false;
	}

	OutError.Empty();
	return true;
}

bool RenderBlueprintThumbnail(UBlueprint* Blueprint, int32& InOutWidth, int32& InOutHeight, TArray<FColor>& OutPixels, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("No blueprint to render.");
		return false;
	}

	FObjectThumbnail Rendered;
	ThumbnailTools::RenderThumbnail(
		Blueprint, InOutWidth, InOutHeight, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &Rendered);

	const TArray<uint8>& Image = Rendered.GetUncompressedImageData();
	const int32 Width = Rendered.GetImageWidth();
	const int32 Height = Rendered.GetImageHeight();
	if (Rendered.IsEmpty() || Image.Num() != Width * Height * 4)
	{
		OutError = FString::Printf(TEXT("The editor could not render a preview of %s."), *Blueprint->GetName());
		return false;
	}

	InOutWidth = Width;
	InOutHeight = Height;
	OutPixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(OutPixels.GetData(), Image.GetData(), Image.Num());
	return true;
}
}
