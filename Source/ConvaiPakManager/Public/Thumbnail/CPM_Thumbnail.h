// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UTexture2D;

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
/**
 * The shape of every thumbnail this tool captures or renders, shared by the render and the viewport
 * capture so the two paths cannot drift. A picked texture or an imported file keeps its own shape.
 * 1:2 - a tall portrait card - is what was asked for; no confirmation of that intent arrived before
 * this was built, so it is built as written. See issue 04.
 */
constexpr int32 WrittenWidth = 512;
constexpr int32 WrittenHeight = 1024;

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
 * Reads a texture's authored source pixels as BGRA8, leaving the texture as it found it.
 *
 * The source and not the platform data: cooked mips are compressed, and the one helper that decodes
 * them - UCPM_UtilityLibrary::Texture2DToPixels - rewrites SRGB and CompressionSettings on the
 * creator's own asset to do it. Refuses, saying "no source", a render target or an import whose
 * source data was stripped, rather than handing back a black image.
 */
CONVAIPAKMANAGER_API bool ReadTextureSource(UTexture2D* Texture, int32& OutWidth, int32& OutHeight, TArray<FColor>& OutPixels, FString& OutError);

/**
 * The largest centred Shape-shaped rectangle that fits inside a Rendered-sized image. Both sides of
 * Shape must be positive.
 *
 * Split out of RenderBlueprintThumbnail because that call needs an RHI and this arithmetic - the
 * part that has to keep holding if the WrittenWidth/WrittenHeight pair is flipped - does not.
 */
CONVAIPAKMANAGER_API FIntRect CentreCrop(FIntPoint Rendered, FIntPoint Shape);

/**
 * Renders the asset thumbnail of a blueprint - what the Content Browser shows for it - from the front.
 *
 * InOutWidth/InOutHeight carry the requested size in and the rendered size out: the engine's
 * renderer is free to hand back something smaller, and the caller needs the real dimensions to
 * write the pixels anywhere. Only the ratio of the two is honoured, not the exact pair.
 *
 * Has no automated coverage - it needs an RHI, and the suite runs with -NullRHI.
 */
CONVAIPAKMANAGER_API bool RenderBlueprintThumbnail(UBlueprint* Blueprint, int32& InOutWidth, int32& InOutHeight, TArray<FColor>& OutPixels, FString& OutError);
}
