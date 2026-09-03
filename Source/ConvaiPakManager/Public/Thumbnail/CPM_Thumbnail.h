// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;

/**
 * The thumbnail is the only thing a player sees before they take an Asset, so a black or fully
 * transparent capture publishes a blank card that nothing downstream can fix - and a capture taken
 * before the scene finished loading, or of a blueprint with nothing renderable in it, is exactly
 * that. Every path that produces a thumbnail is gated here on the legacy uploader's rule: a pixel
 * counts when it is at all opaque and not almost black, and an image passes when 1% of it counts.
 *
 * The check runs on decoded pixels rather than on a UTexture2D, because the tool's thumbnails are
 * transient textures built from file bytes and a transient texture has no `Source` - the old
 * `CPM_IsThumbnailValid` read `Texture->Source` and was therefore false for every thumbnail this
 * tool ever made, which is why nothing called it.
 */
namespace ConvaiPakManager::Thumbnail
{
/** The legacy rule: at least MinRatio of the pixels are opaque and not near-black. Empty is false. */
CONVAIPAKMANAGER_API bool HasContent(TArrayView<const FColor> Pixels, float MinRatio = 0.01f);

/** Decodes any format IImageWrapper detects. False when the file is unreadable or not an image. */
CONVAIPAKMANAGER_API bool DecodeImageFile(const FString& Path, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels);

/** Writes a PNG, creating the directory tree. False when the pixel count disagrees with WxH. */
CONVAIPAKMANAGER_API bool WritePng(const FString& Path, int32 Width, int32 Height, TArrayView<const FColor> Pixels);

/** HasContent for a file on disk. False when it cannot be read or decoded. */
CONVAIPAKMANAGER_API bool FileHasContent(const FString& Path);

/**
 * Adopts a creator's own image as the thumbnail, re-encoded as PNG at the destination.
 *
 * Refuses - writing nothing - a file that cannot be read, is not an image, or is blank, and says
 * which in OutError so the creator does not have to guess which of the three it was.
 */
CONVAIPAKMANAGER_API bool ImportImageFile(const FString& SourcePath, const FString& DestinationPath, FString& OutError);

/**
 * Renders the asset thumbnail of a blueprint - what the Content Browser shows for it.
 *
 * InOutWidth/InOutHeight carry the requested size in and the rendered size out: the engine's
 * renderer is free to hand back something smaller, and the caller needs the real dimensions to
 * write the pixels anywhere.
 *
 * Has no automated coverage - it needs an RHI, and the suite runs with -NullRHI.
 */
CONVAIPAKMANAGER_API bool RenderBlueprintThumbnail(UBlueprint* Blueprint, int32& InOutWidth, int32& InOutHeight, TArray<FColor>& OutPixels, FString& OutError);
}
