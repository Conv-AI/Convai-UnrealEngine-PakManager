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
#include "CanvasTypes.h"
#include "AssetCompilingManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "ContentStreaming.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineModule.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "LegacyScreenPercentageDriver.h"
#include "RendererInterface.h"
#include "SceneView.h"
#include "ThumbnailHelpers.h"
#include "UnrealClient.h"

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

	/**
	 * Long lens, not the engine thumbnail's 30 degrees. The fit puts the camera wherever the subject
	 * fills the frame, so narrowing this does not change the framing - it changes how much the head
	 * diverges from the feet, and a catalogue portrait of a person should barely diverge at all.
	 */
	constexpr float AvatarFieldOfView = 18.0f;

	/**
	 * How much of the frame the avatar stands in, vertically. 1.0 would touch both edges; the gap is
	 * headroom above the hair, and the feet fall out of the bottom of the portrait crop, which is
	 * what a full-length figure looks like on a card rather than a specimen in a box.
	 */
	constexpr float AvatarHeightFill = 0.92f;

	/**
	 * Where the camera looks, up the figure from the middle of its bounds, as a fraction of half its
	 * height. Aiming at the centre puts the waist in the middle of the frame; a portrait wants the
	 * chest there, which leaves headroom above and drops the feet past the bottom edge.
	 */
	constexpr float AvatarAimUp = 0.28f;

	bool ShowsGeometry(const USceneComponent* Component)
	{
		const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive || !Primitive->IsVisible() || Primitive->bHiddenInGame)
		{
			return false;
		}

		const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Component);
		if (StaticMesh)
		{
			return StaticMesh->GetStaticMesh() != nullptr;
		}

		const USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component);
		return SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset();
	}

	/**
	 * A preview scene holding the avatar and its lights, and nothing else.
	 *
	 * The engine's thumbnail scene adds a sky sphere scaled to 2000 and a floor plane scaled to
	 * 10000, both of which fill every pixel the subject does not - and a capture with a transparent
	 * background is exactly a capture with nothing behind the subject. The sky cubemap stays: it
	 * lights the avatar and draws none of it.
	 */
	class FCPM_AvatarThumbnailScene : public FThumbnailPreviewScene
	{
	public:
		FCPM_AvatarThumbnailScene()
			: FThumbnailPreviewScene(FConstructionValues()
				.SetCreateSkySphere(false)
				.SetCreateFloorPlane(false))
		{
		}

		virtual ~FCPM_AvatarThumbnailScene()
		{
			if (PreviewActor.IsValid())
			{
				PreviewActor->Destroy();
			}
		}

		/** Spawns the blueprint and measures it. False when it draws nothing worth photographing. */
		bool SetBlueprint(UBlueprint* Blueprint)
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.bNoFail = true;
			SpawnInfo.ObjectFlags = RF_Transient;
			PreviewActor = GetWorld()->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnInfo);

			if (!PreviewActor.IsValid() || !PreviewActor->GetRootComponent())
			{
				return false;
			}

			TArray<USceneComponent*> Visible;
			PreviewActor->GetRootComponent()->GetChildrenComponents(true, Visible);
			Visible.Add(PreviewActor->GetRootComponent());

			FBoxSphereBounds::Builder BoundsBuilder;
			bool bAnyGeometry = false;
			for (const USceneComponent* Component : Visible)
			{
				if (ShowsGeometry(Component))
				{
					BoundsBuilder += Component->Bounds;
					bAnyGeometry = true;
				}
			}
			if (!bAnyGeometry)
			{
				return false;
			}

			Bounds = BoundsBuilder;
			PreviewActor->SetActorLocation(-Bounds.Origin);

			// Re-measured after the move: the fit below is computed against these, and the ones taken
			// above are in whatever place the blueprint's own transform put the actor.
			Bounds.Origin = FVector::ZeroVector;
			return true;
		}

		/** One render. The pixels come back sRGB BGRA with real coverage in alpha, one per pixel of Side. */
		bool RenderTo(UTextureRenderTarget2D* Target, int32 Side, TArray<FColor>& OutPixels)
		{
			FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
			if (!Resource)
			{
				return false;
			}

			// Everything the avatar's materials need, resident, before anything is drawn. Without this
			// the render is whatever happened to be streamed in - which for a MetaHuman opened seconds
			// ago is the grey checker its textures fall back to.
			FlushAsyncLoading();
			FAssetCompilingManager::Get().FinishAllCompilation();
			UTexture::ForceUpdateTextureStreaming();
			IStreamingManager::Get().StreamAllResources();

			FCanvas Canvas(Resource, nullptr, FGameTime(), GetScene()->GetFeatureLevel());

			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Resource, GetScene(), FEngineShowFlags(ESFIM_Game))
				.SetTime(FGameTime()));
			ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
			ViewFamily.EngineShowFlags.MotionBlur = 0;

			// Nothing draws them and both would tint the empty background, which the matte reads as
			// coverage.
			ViewFamily.EngineShowFlags.SetAtmosphere(false);
			ViewFamily.EngineShowFlags.SetFog(false);

			FSceneView* View = CreateView(&ViewFamily, 0, 0, Side, Side);
			if (!View)
			{
				return false;
			}

			ViewFamily.EngineShowFlags.ScreenPercentage = false;
			ViewFamily.SetScreenPercentageInterface(
				new FLegacyScreenPercentageDriver(ViewFamily, /*GlobalResolutionFraction=*/1.0f));

			GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
			FlushRenderingCommands();

			FReadSurfaceDataFlags ReadFlags;
			if (!Resource->ReadPixels(OutPixels, ReadFlags))
			{
				return false;
			}

			// Scene colour's alpha is how much of the BACKGROUND still shows through - zero where the
			// avatar is solid, full where there is nothing. That is the opposite of what an image
			// means by alpha, and PNG is about to be told this is an image.
			for (FColor& Pixel : OutPixels)
			{
				Pixel.A = 255 - Pixel.A;
			}
			return true;
		}

	protected:
		virtual float GetFOV() const override { return AvatarFieldOfView; }

		// The engine clamps the orbit to 48 units minimum, which is for a scene where the creator can
		// drag the camera. This one is computed from the subject's own size every time.
		virtual bool ShouldClampOrbitZoom() const override { return false; }

		virtual void GetViewMatrixParameters(
			const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override
		{
			const float HalfFOVRadians = FMath::DegreesToRadians(InFOVDegrees) * 0.5f;

			// Fitted on height, not on the bounding sphere. A standing figure is far taller than it is
			// wide, so its sphere is mostly the empty air either side of it, and fitting that leaves the
			// avatar small in the middle of the frame.
			const double HalfHeight = FMath::Max(Bounds.BoxExtent.Z, 1.0) / AvatarHeightFill;

			OutOrigin = FVector(0.0, 0.0, -Bounds.BoxExtent.Z * AvatarAimUp);
			OutOrbitPitch = 0.0f;
			OutOrbitYaw = FrontOnOrbitYaw;
			OutOrbitZoom = static_cast<float>(HalfHeight / FMath::Tan(HalfFOVRadians));
		}

	private:
		TWeakObjectPtr<AActor> PreviewActor;
		FBoxSphereBounds Bounds;
	};
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

FIntPoint WrittenShape(const ECPM_AssetType AssetType)
{
	return AssetType == ECPM_AssetType::Avatar
		? FIntPoint(WrittenWidth, WrittenHeight)
		: FIntPoint(WrittenHeight, WrittenWidth);
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

	if (!IsValid(Blueprint->GeneratedClass) || Blueprint->bBeingCompiled)
	{
		OutError = FString::Printf(TEXT("%s has not compiled, so there is nothing to photograph."), *Blueprint->GetName());
		return false;
	}

	if (InOutWidth <= 0 || InOutHeight <= 0)
	{
		OutError = TEXT("A thumbnail cannot be rendered at that size.");
		return false;
	}

	// Rendered square and cropped rather than rendered at the asked-for shape: CreateView builds the
	// projection at a fixed 1:1 aspect, so a 1:2 target would stretch the avatar to twice its height.
	const int32 Side = FMath::Max(InOutWidth, InOutHeight);

	FCPM_AvatarThumbnailScene Scene;
	if (!Scene.SetBlueprint(Blueprint))
	{
		OutError = FString::Printf(TEXT("%s has no visible mesh to photograph."), *Blueprint->GetName());
		return false;
	}

	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	Target->AddToRoot();
	ON_SCOPE_EXIT{ Target->RemoveFromRoot(); };
	Target->RenderTargetFormat = RTF_RGBA8_SRGB;

	// Alpha zero, so a pixel the avatar does not cover ends up transparent rather than black.
	Target->ClearColor = FLinearColor::Transparent;
	Target->InitAutoFormat(Side, Side);
	Target->UpdateResourceImmediate(true);

	// Off by default, and without it the renderer is free to write whatever it likes into scene
	// colour's alpha - which in practice is opaque everywhere, background included. Restored
	// immediately: it is a global that governs every render in the editor, not just this one.
	IConsoleVariable* PropagateAlpha =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
	const bool bPropagatedAlpha = PropagateAlpha && PropagateAlpha->GetBool();
	if (PropagateAlpha)
	{
		PropagateAlpha->Set(true, ECVF_SetByCode);
	}
	ON_SCOPE_EXIT
	{
		if (PropagateAlpha)
		{
			PropagateAlpha->Set(bPropagatedAlpha, ECVF_SetByCode);
		}
	};

	TArray<FColor> Rendered;
	if (!Scene.RenderTo(Target, Side, Rendered) || Rendered.Num() != Side * Side)
	{
		OutError = FString::Printf(TEXT("The editor could not render a preview of %s."), *Blueprint->GetName());
		return false;
	}

	const FIntRect Crop = CentreCrop(FIntPoint(Side, Side), FIntPoint(InOutWidth, InOutHeight));

	OutPixels.SetNumUninitialized(Crop.Width() * Crop.Height());
	for (int32 Row = 0; Row < Crop.Height(); ++Row)
	{
		FMemory::Memcpy(
			OutPixels.GetData() + Row * Crop.Width(),
			Rendered.GetData() + (Crop.Min.Y + Row) * Side + Crop.Min.X,
			Crop.Width() * sizeof(FColor));
	}

	InOutWidth = Crop.Width();
	InOutHeight = Crop.Height();
	return true;
}
}
