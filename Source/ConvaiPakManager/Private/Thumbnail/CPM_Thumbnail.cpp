// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Thumbnail/CPM_Thumbnail.h"

#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

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

	/**
	 * Found by rendering BP_Hana and looking: -180 gives a clean side profile and -90 puts the face
	 * at the camera. Not the engine's -157.5 default, which by that same evidence sits 22.5 degrees
	 * off a profile - and which is only a default anyway: what a blueprint carries is wherever the
	 * creator last dragged its Content Browser thumbnail to.
	 */
	constexpr float FrontOnOrbitYaw = -90.0f;
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

bool ReadTextureSource(UTexture2D* Texture, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels, FString& OutError)
{
	if (!Texture)
	{
		OutError = TEXT("There is no texture to read.");
		return false;
	}

	if (!Texture->Source.IsValid())
	{
		OutError = FString::Printf(
			TEXT("%s has no source data - it is a render target, or was imported with its source stripped."),
			*Texture->GetName());
		return false;
	}

	FImage Image;
	if (!Texture->Source.GetMipImage(Image, 0))
	{
		OutError = FString::Printf(TEXT("%s's source data could not be read."), *Texture->GetName());
		return false;
	}

	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const TArrayView64<FColor> Colours = Image.AsBGRA8();

	OutWidth = Image.SizeX;
	OutHeight = Image.SizeY;
	OutPixels.SetNumUninitialized(Colours.Num());
	FMemory::Memcpy(OutPixels.GetData(), Colours.GetData(), Colours.Num() * sizeof(FColor));
	OutError.Empty();
	return true;
}

FIntRect CentreCrop(const FIntPoint Rendered, const FIntPoint Shape)
{
	// Both axes, because which one holds the surplus depends on which way Shape is longer, and Shape
	// is the pair most likely to change.
	const int32 Width = FMath::Clamp(Rendered.Y * Shape.X / Shape.Y, 1, Rendered.X);
	const int32 Height = FMath::Clamp(Rendered.X * Shape.Y / Shape.X, 1, Rendered.Y);
	const FIntPoint Origin((Rendered.X - Width) / 2, (Rendered.Y - Height) / 2);
	return FIntRect(Origin, Origin + FIntPoint(Width, Height));
}

bool RenderBlueprintThumbnail(UBlueprint* Blueprint, int32& InOutWidth, int32& InOutHeight, TArray<FColor>& OutPixels, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("No blueprint to render.");
		return false;
	}

	if (InOutWidth <= 0 || InOutHeight <= 0)
	{
		OutError = TEXT("A thumbnail cannot be rendered at that size.");
		return false;
	}

	// Swapped in rather than written through: the renderer reads the angle off the blueprint, and
	// it also clamps OrbitZoom in place, so pointing the camera at the avatar's face by editing the
	// creator's asset would both discard their framing and dirty their package.
	USceneThumbnailInfo* FrontOn = NewObject<USceneThumbnailInfo>(GetTransientPackage());
	FrontOn->OrbitPitch = 0.0f;
	FrontOn->OrbitYaw = FrontOnOrbitYaw;
	FrontOn->OrbitZoom = 0.0f;

	UThumbnailInfo* CreatorsFraming = Blueprint->ThumbnailInfo;
	Blueprint->ThumbnailInfo = FrontOn;
	ON_SCOPE_EXIT{ Blueprint->ThumbnailInfo = CreatorsFraming; };

	// Rendered square and cropped, rather than rendered at the asked-for shape: the thumbnail
	// projection is fixed at a 1:1 aspect (FThumbnailPreviewScene::CreateView builds it as
	// FReversedZPerspectiveMatrix(HalfFOV, 1, 1, Near)), so a 1:2 target would stretch the avatar to
	// twice its height. The bounds fit centres the subject in the square, so a portrait crop takes
	// the empty sides; a landscape one would take head and feet, which is the cost of asking a
	// square fit for a wide card.
	const int32 Side = FMath::Max(InOutWidth, InOutHeight);

	FObjectThumbnail Rendered;
	ThumbnailTools::RenderThumbnail(
		Blueprint, Side, Side, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &Rendered);

	const TArray<uint8>& Image = Rendered.GetUncompressedImageData();
	const int32 RenderedWidth = Rendered.GetImageWidth();
	const int32 RenderedHeight = Rendered.GetImageHeight();
	if (Rendered.IsEmpty() || Image.Num() != RenderedWidth * RenderedHeight * 4)
	{
		OutError = FString::Printf(TEXT("The editor could not render a preview of %s."), *Blueprint->GetName());
		return false;
	}

	const FIntRect Crop = CentreCrop(FIntPoint(RenderedWidth, RenderedHeight), FIntPoint(InOutWidth, InOutHeight));

	OutPixels.SetNumUninitialized(Crop.Width() * Crop.Height());
	for (int32 Row = 0; Row < Crop.Height(); ++Row)
	{
		FMemory::Memcpy(
			OutPixels.GetData() + Row * Crop.Width(),
			Image.GetData() + ((Crop.Min.Y + Row) * RenderedWidth + Crop.Min.X) * 4,
			Crop.Width() * 4);
	}

	InOutWidth = Crop.Width();
	InOutHeight = Crop.Height();
	return true;
}
}
