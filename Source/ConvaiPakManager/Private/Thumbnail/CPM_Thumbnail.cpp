// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Thumbnail/CPM_Thumbnail.h"
#include "ConvaiAvatarThumbnail.h"

namespace ConvaiPakManager::Thumbnail
{
FIntPoint WrittenShape(const ECPM_AssetType AssetType)
{
	return AssetType == ECPM_AssetType::Avatar
		? FIntPoint(WrittenWidth, WrittenHeight) : FIntPoint(WrittenHeight, WrittenWidth);
}

bool HasContent(const TArrayView<const FColor> Pixels, const float MinRatio)
{
	return ConvaiAvatarPreparation::Thumbnail::HasContent(Pixels, MinRatio);
}

bool DecodeImageFile(const FString& Path, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels)
{
	return ConvaiAvatarPreparation::Thumbnail::DecodeImageFile(Path, OutWidth, OutHeight, OutPixels);
}

bool WritePng(const FString& Path, const int32 Width, const int32 Height, const TArrayView<const FColor> Pixels)
{
	return ConvaiAvatarPreparation::Thumbnail::WritePng(Path, Width, Height, Pixels);
}

bool FileHasContent(const FString& Path)
{
	return ConvaiAvatarPreparation::Thumbnail::FileHasContent(Path);
}

bool ImportImageFile(const FString& SourcePath, const FString& DestinationPath, FString& OutError)
{
	return ConvaiAvatarPreparation::Thumbnail::ImportImageFile(SourcePath, DestinationPath, OutError);
}

bool ReadTextureSource(UTexture2D* Texture, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels, FString& OutError)
{
	return ConvaiAvatarPreparation::Thumbnail::ReadTextureSource(Texture, OutWidth, OutHeight, OutPixels, OutError);
}

FIntRect CentreCrop(const FIntPoint Rendered, const FIntPoint Shape)
{
	return ConvaiAvatarPreparation::Thumbnail::CentreCrop(Rendered, Shape);
}

bool RenderBlueprintThumbnail(UBlueprint* Blueprint, int32& InOutWidth, int32& InOutHeight, TArray<FColor>& OutPixels, FString& OutError)
{
	return ConvaiAvatarPreparation::Thumbnail::RenderBlueprintThumbnail(Blueprint, InOutWidth, InOutHeight, OutPixels, OutError);
}
}
