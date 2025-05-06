// Fill out your copyright notice in the Description page of Project Settings.

#include "Utility/CPM_UtilityLibrary.h"
#include "DesktopPlatformModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "IPlatformFilePak.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Serialization/JsonSerializer.h"
#include "Utility/CPM_Utils.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Utility/CPM_Log.h"


FString UCPM_UtilityLibrary::OpenFileDialog(const TArray<FString>& Extensions)
{
	FString FilePath;
	FString FileTypes = TEXT("Supported Files (");
	
	// Validate input
	if (Extensions.Num() == 0)
	{
		CPM_LogMessage(TEXT("Extensions array is empty"), ECPM_LogLevel::Warning);
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

FString UCPM_UtilityLibrary::GetProjectName()
{
	return FApp::GetProjectName();
}

bool UCPM_UtilityLibrary::ValidatePakFile(const FString& PakFilePath)
{
	if (!FPaths::FileExists(PakFilePath))
	{
		CPM_LogMessage(FString::Printf(TEXT("Pak file does not exist: %s"), *PakFilePath), ECPM_LogLevel::Error);
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FPakPlatformFile* PakPlatformFile = new FPakPlatformFile();

	if (!PakPlatformFile->Initialize(&PlatformFile, TEXT("")))
	{
		CPM_LogMessage(TEXT("Failed to initialize PakPlatformFile."), ECPM_LogLevel::Error);
		delete PakPlatformFile;
		return false;
	}

	if (!PakPlatformFile->Mount(*PakFilePath, 0, *FPaths::ProjectContentDir()))
	{
		CPM_LogMessage(FString::Printf(TEXT("Failed to mount Pak file: %s"), *PakFilePath), ECPM_LogLevel::Error);
		delete PakPlatformFile;
		return false;
	}

	PakPlatformFile->Unmount(*PakFilePath);
	
	delete PakPlatformFile;
	return true;
}

void UCPM_UtilityLibrary::GetAssetID(FString& AssetID)
{
	FCPM_CreatedAssets OutData;
	if(LoadConvaiCreateAssetData(OutData))
	{
		AssetID = OutData.Assets.Num() > 0 ? OutData.Assets[0].Asset.AssetId : TEXT(""); 
	}
}

ECPM_AssetType UCPM_UtilityLibrary::GetAssetType()
{
	static const TMap<FString, ECPM_AssetType> StringToAssetTypeMap = {
		{TEXT("Avatar"), ECPM_AssetType::Avatar},
		{TEXT("Scene"),  ECPM_AssetType::Scene},
	};

	FCPM_ModdingMetadata OutData;
	GetModdingMetadata(OutData);	
	const ECPM_AssetType* FoundType = StringToAssetTypeMap.Find(OutData.AssetType);
	return FoundType ? *FoundType : ECPM_AssetType::Max;
}

bool UCPM_UtilityLibrary::SaveConvaiCreateAssetData(const FString& ResponseString)
{
	FString FilePath = GetCreateAssetDataFilePath();
	
	if (FFileHelper::SaveStringToFile(ResponseString, *FilePath))
	{
		return true;
	}
	
	CPM_LogMessage(FString::Printf(TEXT("Failed to save asset data to %s"), *FilePath), ECPM_LogLevel::Error);
	return false;
}

bool UCPM_UtilityLibrary::LoadConvaiCreateAssetData(FCPM_CreatedAssets& OutData)
{
	const FString FilePath = GetCreateAssetDataFilePath();
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		//CPM_LogMessage(TEXT("Failed to read PakMetaData.txt"), ECPM_LogLevel::Error);
		return false;
	}

	return GetCreatedAssetsFromJSON(FileContent, OutData);
}

bool UCPM_UtilityLibrary::SaveConvaiAssetMetadata(const FString& ResponseString)
{
	FString FilePath = GetPakMetadataFilePath();
	
	if (FFileHelper::SaveStringToFile(ResponseString, *FilePath))
	{
		return true;
	}
	
	CPM_LogMessage(FString::Printf(TEXT("Failed to save asset data to %s"), *FilePath), ECPM_LogLevel::Error);
	return false;
}

void UCPM_UtilityLibrary::GetAssetMetaDataString(FString& MetaData)
{
	FFileHelper::LoadFileToString(MetaData, *GetPakMetadataFilePath());
}

FString UCPM_UtilityLibrary::GetPakMetadataFilePath()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("ConvaiEssentials"), TEXT("PakMetaData")) + TEXT(".json");
}

FString UCPM_UtilityLibrary::GetPackageDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("PackagedApp"));
}

FString UCPM_UtilityLibrary::GetPakFilePathFromChunkID(const FString& ChunkID)
{
	return FPaths::Combine(GetPackageDirectory(), TEXT("Windows"), GetProjectName(), TEXT("Content"), TEXT("Paks"), FString::Printf(TEXT("pakchunk%s-Windows"), *ChunkID)) + TEXT(".pak");
}

void UCPM_UtilityLibrary::GetModdingMetadata(FCPM_ModdingMetadata& OutData)
{
	const FString FilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("ConvaiEssentials"), TEXT("ModdingMetaData")) + TEXT(".txt");
	FString FileContent;

	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		CPM_LogMessage(TEXT("Failed to read ModdingMetaData.txt"), ECPM_LogLevel::Error);
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	JsonObject->TryGetStringField(TEXT("project_name"), OutData.ProjectName);
	JsonObject->TryGetStringField(TEXT("plugin_name"), OutData.PluginName);
	JsonObject->TryGetStringField(TEXT("asset_type"), OutData.AssetType);
}

bool UCPM_UtilityLibrary::GetCreatedAssetsFromJSON(const FString& JsonString, FCPM_CreatedAssets& OutCreatedAssets)
{
	TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return false;
    }

    JsonObject->TryGetStringField(TEXT("transactionID"), OutCreatedAssets.TransactionID);

    const TArray<TSharedPtr<FJsonValue>>* AssetsArray;
    if (JsonObject->TryGetArrayField(TEXT("assets"), AssetsArray))
    {
        for (const TSharedPtr<FJsonValue>& AssetValue : *AssetsArray)
        {
            const TSharedPtr<FJsonObject>* AssetEntryObject;
            if (!AssetValue->TryGetObject(AssetEntryObject))
                continue;

            FCPM_Asset ParsedAsset;

            // Asset details
            const TSharedPtr<FJsonObject>* AssetDetailsObj;
            if ((*AssetEntryObject)->TryGetObjectField(TEXT("asset"), AssetDetailsObj))
            {
                (*AssetDetailsObj)->TryGetStringField(TEXT("asset_id"), ParsedAsset.Asset.AssetId);
                (*AssetDetailsObj)->TryGetStringField(TEXT("gcp_file_name"), ParsedAsset.Asset.GCPFileName);
                (*AssetDetailsObj)->TryGetStringField(TEXT("file_name"), ParsedAsset.Asset.FileName);
                (*AssetDetailsObj)->TryGetStringField(TEXT("uploaded_on"), ParsedAsset.Asset.UploadedOn);
                (*AssetDetailsObj)->TryGetStringField(TEXT("thumbnail_gcp_path"), ParsedAsset.Asset.ThumbnailGCPPath);

                // Tags
                const TArray<TSharedPtr<FJsonValue>>* TagsArray;
                if ((*AssetDetailsObj)->TryGetArrayField(TEXT("tags"), TagsArray))
                {
                    for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
                    {
                        ParsedAsset.Asset.Tags.Add(TagValue->AsString());
                    }
                }

                // Versions
                const TArray<TSharedPtr<FJsonValue>>* VersionsArray;
                if ((*AssetDetailsObj)->TryGetArrayField(TEXT("versions"), VersionsArray))
                {
                    for (const TSharedPtr<FJsonValue>& VersionValue : *VersionsArray)
                    {
                        ParsedAsset.Asset.Versions.Add(VersionValue->AsString());
                    }
                }

                // Asset Metadata
                const TSharedPtr<FJsonObject>* MetadataObj;
                if ((*AssetDetailsObj)->TryGetObjectField(TEXT("metadata"), MetadataObj))
                {
                    (*MetadataObj)->TryGetStringField(TEXT("version"), ParsedAsset.Asset.Metadata.Version);
                    (*MetadataObj)->TryGetStringField(TEXT("scene_id"), ParsedAsset.Asset.Metadata.SceneId);
                    (*MetadataObj)->TryGetStringField(TEXT("entity_id"), ParsedAsset.Asset.Metadata.EntityId);
                    (*MetadataObj)->TryGetStringField(TEXT("root_path"), ParsedAsset.Asset.Metadata.RootPath);
                    (*MetadataObj)->TryGetStringField(TEXT("asset_type"), ParsedAsset.Asset.Metadata.AssetType);
                    (*MetadataObj)->TryGetStringField(TEXT("level_name"), ParsedAsset.Asset.Metadata.LevelName);
                    (*MetadataObj)->TryGetStringField(TEXT("content_path"), ParsedAsset.Asset.Metadata.ContentPath);
                    (*MetadataObj)->TryGetStringField(TEXT("project_name"), ParsedAsset.Asset.Metadata.ProjectName);
                    (*MetadataObj)->TryGetStringField(TEXT("blueprint_class"), ParsedAsset.Asset.Metadata.BlueprintClass);
                    (*MetadataObj)->TryGetStringField(TEXT("blueprint_class_path"), ParsedAsset.Asset.Metadata.BlueprintClassPath);
                	(*MetadataObj)->TryGetStringField(TEXT("asset_name"), ParsedAsset.Asset.Metadata.AssetName);
                	(*MetadataObj)->TryGetStringField(TEXT("asset_description"), ParsedAsset.Asset.Metadata.AssetDescription);
                	
                    // Entity Data
                    const TSharedPtr<FJsonObject>* EntityDataObj;
                    if ((*MetadataObj)->TryGetObjectField(TEXT("entity_data"), EntityDataObj))
                    {
                        (*EntityDataObj)->TryGetStringField(TEXT("scene_name"), ParsedAsset.Asset.Metadata.EntityData.SceneName);
                        (*EntityDataObj)->TryGetStringField(TEXT("scene_description"), ParsedAsset.Asset.Metadata.EntityData.SceneDescription);
                    }

                	TSharedRef<TJsonWriter<>> MetadataWriter = TJsonWriterFactory<>::Create(&ParsedAsset.Asset.MetadataString);
                	FJsonSerializer::Serialize(MetadataObj->ToSharedRef(), MetadataWriter);
                }
            }

            // Scene Details
            const TSharedPtr<FJsonObject>* SceneObj;
            if ((*AssetEntryObject)->TryGetObjectField(TEXT("scene"), SceneObj))
            {
                (*SceneObj)->TryGetStringField(TEXT("scene_id"), ParsedAsset.Scene.SceneId);
                (*SceneObj)->TryGetStringField(TEXT("build_id"), ParsedAsset.Scene.BuildId);
                (*SceneObj)->TryGetStringField(TEXT("owner_id"), ParsedAsset.Scene.OwnerId);
                (*SceneObj)->TryGetStringField(TEXT("scene_name"), ParsedAsset.Scene.SceneName);
                (*SceneObj)->TryGetStringField(TEXT("scene_description"), ParsedAsset.Scene.SceneDescription);
                (*SceneObj)->TryGetStringField(TEXT("scene_thumbnail"), ParsedAsset.Scene.SceneThumbnail);
                (*SceneObj)->TryGetStringField(TEXT("visibility"), ParsedAsset.Scene.Visibility);
                (*SceneObj)->TryGetStringField(TEXT("created_on"), ParsedAsset.Scene.CreatedOn);
            }

            // Upload URLs
            const TSharedPtr<FJsonObject>* UploadUrlsObj;
            if ((*AssetEntryObject)->TryGetObjectField(TEXT("upload_urls"), UploadUrlsObj))
            {
                for (const auto& Pair : (*UploadUrlsObj)->Values)
                {
                    FString UrlKey = Pair.Key;
                    FString UrlValue;
                    if (Pair.Value->TryGetString(UrlValue))
                    {
                        ParsedAsset.UploadUrls.UploadURLsMap.Add(UrlKey, UrlValue);
                    }
                }
            }

            OutCreatedAssets.Assets.Add(ParsedAsset);
        }
    }
	else
	{
		return false;
	}
	
	return true;
}

FString UCPM_UtilityLibrary::GetCreateAssetDataFilePath()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("ConvaiEssentials"), TEXT("CreateAssetData")) + TEXT(".json");
}

bool UCPM_UtilityLibrary::ShouldCreateAsset()
{
	FString AssetID;
	GetAssetID(AssetID);
	return AssetID.IsEmpty();
}

void UCPM_UtilityLibrary::CPM_LogMessage(const FString& Message, const ECPM_LogLevel Verbosity)
{
	switch (Verbosity)
	{
	case ECPM_LogLevel::Error:
		CPM_LOG(Error, TEXT("%s"), *Message);
		break;

	case ECPM_LogLevel::Warning:
		CPM_LOG(Warning, TEXT("%s"), *Message);
		break;

	case ECPM_LogLevel::Log:
	default:
		CPM_LOG(Log, TEXT("%s"), *Message);
		break;
	}
}

FString UCPM_UtilityLibrary::GetPythonScriptDirectory()
{
	return FPaths::Combine(FPaths::ProjectDir(),TEXT("Plugins"), TEXT("ConvaiPakManager"), TEXT("Scripts/"));
}

UClass* UCPM_UtilityLibrary::CPM_LoadClassByPath(const FString& ClassPath)
{
	FString ObjectPath = ClassPath;
	if (ObjectPath.EndsWith(TEXT("_C")))
		ObjectPath.RemoveFromEnd(TEXT("_C"));	
	
	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	if (!FPackageName::DoesPackageExist(PackageName))
	{
		return nullptr;
	}
	
	return StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
}

UObject* UCPM_UtilityLibrary::CPM_LoadAssetByPath(const FString& AssetPath)
{
	return StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
}

FAssetData UCPM_UtilityLibrary::CPM_LoadAssetDataByPath(const FString& AssetPath)
{
	const FString ObjectPath = AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath);
	const IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		UE_LOG(LogTemp, Error, TEXT("AssetRegistry is not available."));
		return FAssetData();
	}

	const FSoftObjectPath SoftPath(ObjectPath);
	return AssetRegistry->GetAssetByObjectPath(SoftPath);
}

bool UCPM_UtilityLibrary::CPM_DeleteFileByPath(const FString& FilePath)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath) ?
		   FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FilePath) : false;
}

bool UCPM_UtilityLibrary::CPM_DeleteDirectory(const FString& DirectoryPath)
{
	return  FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*DirectoryPath) ?
		    FPlatformFileManager::Get().GetPlatformFile().DeleteDirectoryRecursively(*DirectoryPath) : false;
}

bool UCPM_UtilityLibrary::CPM_IsThumbnailValid(UTexture2D* Texture, float MinValidRatio, int32 SampleStep)
{
#if WITH_EDITOR
	// Must have a valid source (imported or explicitly set)
	if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetNumMips() == 0)
	{
		return false;
	}
	
	// Lock the SOURCE mip (returns const void*)
	const void* RawData = Texture->Source.LockMip(0);
	if (!RawData)
	{
		return false;
	}
	
	const int32 Width  = Texture->Source.GetSizeX();
	const int32 Height = Texture->Source.GetSizeY();
	const int32 Total  = Width * Height;
	const FColor* Colors = static_cast<const FColor*>(RawData);

	const int32 Step    = FMath::Max(1, SampleStep);
	const int32 Sampled = (Total + Step - 1) / Step;
	int32 ValidCount    = 0;

	for (int32 Index = 0; Index < Total; Index += Step)
	{
		const FColor& C = Colors[Index];
		if (C.A > 0 && (C.R > 5 || C.G > 5 || C.B > 5))
		{
			++ValidCount;
		}
	}

	Texture->Source.UnlockMip(0);

	return static_cast<float>(ValidCount) / static_cast<float>(Sampled) >= MinValidRatio;
#else
	return false;
#endif
}

UTexture2D* UCPM_UtilityLibrary::CPM_LoadTexture2DFromDisk(const FString& FilePath, bool bGenerateMips)
{
	 // 1) Check file exists and load bytes
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Image file not found: %s"), *FilePath);
        return nullptr;
    }

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath) || FileData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to read or empty file: %s"), *FilePath);
        return nullptr;
    }

    // 2) Detect image format by content
    IImageWrapperModule& ImgWrapperMod = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    EImageFormat DetectedFormat = ImgWrapperMod.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (DetectedFormat == EImageFormat::Invalid)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unknown or unsupported image format: %s"), *FilePath);
        return nullptr;
    }

    // 3) Create wrapper and feed it
    TSharedPtr<IImageWrapper> ImgWrapper = ImgWrapperMod.CreateImageWrapper(DetectedFormat);
    if (!ImgWrapper.IsValid() ||
        !ImgWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTemp, Warning, TEXT("Unsupported or corrupt image data: %s"), *FilePath);
        return nullptr;
    }

    // 4) Decompress into raw BGRA8
    TArray<uint8> RawRGBA;
    if (!ImgWrapper->GetRaw(ERGBFormat::BGRA, 8, RawRGBA))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to decompress image: %s"), *FilePath);
        return nullptr;
    }

    // 5) Create and fill the transient texture
    int32 Width  = ImgWrapper->GetWidth();
    int32 Height = ImgWrapper->GetHeight();

    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!Texture) return nullptr;

    Texture->MipGenSettings = bGenerateMips ? TMGS_FromTextureGroup : TMGS_NoMipmaps;
    Texture->SRGB           = true;

    void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, RawRGBA.GetData(), RawRGBA.Num());
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

    Texture->UpdateResource();
    return Texture;
}

bool UCPM_UtilityLibrary::Texture2DToPixels(UTexture2D* Texture2D, int32& Width, int32& Height,
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

bool UCPM_UtilityLibrary::Texture2DToBytes(UTexture2D* Texture2D, const EImageFormat ImageFormat,
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

bool UCPM_UtilityLibrary::PixelsToBytes(const int32 Width, const int32 Height, const TArray<FColor>& Pixels,
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

bool UCPM_UtilityLibrary::ExtractAssetListFromResponseString(const FString& ResponseString, FCPM_AssetResponse& AssetResponse)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        JsonObject->TryGetStringField(TEXT("transactionID"), AssetResponse.transactionID);

        const TArray<TSharedPtr<FJsonValue>>* AssetsArray;
        const TArray<TSharedPtr<FJsonValue>>* AnimationsArray; 
        const TSharedPtr<FJsonObject>* AssetObjectForAnim; 
        if (JsonObject->TryGetArrayField(TEXT("assets"), AssetsArray))
        {
            for (const TSharedPtr<FJsonValue>& Value : *AssetsArray)
            {
                TSharedPtr<FJsonObject> AssetObject = Value->AsObject();
                if (AssetObject.IsValid())
                {
                    FCPM_AssetData AssetData;

                    AssetObject->TryGetStringField(TEXT("asset_id"), AssetData.asset_id);
                    AssetObject->TryGetStringField(TEXT("gcp_file_name"), AssetData.gcp_file_name);
                    AssetObject->TryGetStringField(TEXT("file_name"), AssetData.file_name);

                    // Parse tags array
                    const TArray<TSharedPtr<FJsonValue>>* TagsArray;
                    if (AssetObject->TryGetArrayField(TEXT("tags"), TagsArray))
                    {
                        for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
                        {
                            AssetData.tags.Add(TagValue->AsString());
                        }
                    }

                    const TSharedPtr<FJsonObject>* MetadataObjectPtr;
                    if (AssetObject->TryGetObjectField(TEXT("metadata"), MetadataObjectPtr))
                    {
                        FString MetadataString;
                        TSharedRef<TJsonWriter<>> MetadataWriter = TJsonWriterFactory<>::Create(&MetadataString);
                        FJsonSerializer::Serialize(MetadataObjectPtr->ToSharedRef(), MetadataWriter);
                        AssetData.metadata = MetadataString;
                    }

                    AssetObject->TryGetStringField(TEXT("uploaded_on"), AssetData.uploaded_on);
                    AssetObject->TryGetStringField(TEXT("signed_url"), AssetData.signed_url);

                    AssetResponse.assets.Add(AssetData);
                }
            }
        }

        if (JsonObject->TryGetArrayField(TEXT("animations"), AnimationsArray))
        {
            for (const TSharedPtr<FJsonValue>& Value : *AnimationsArray)
            {
                TSharedPtr<FJsonObject> AnimationObject = Value->AsObject();
                if (AnimationObject.IsValid())
                {
                    FCPM_AssetData AnimationData;
                    AnimationObject->TryGetStringField(TEXT("animation_id"), AnimationData.asset_id);
                    AnimationObject->TryGetStringField(TEXT("animation_name"), AnimationData.file_name);
                    AnimationObject->TryGetStringField(TEXT("fbx_gcp_file"), AnimationData.signed_url);
                    AnimationObject->TryGetStringField(TEXT("created_at"), AnimationData.uploaded_on);

                    AssetResponse.assets.Add(AnimationData);
                }
            }
        }

        else if (JsonObject->TryGetObjectField(TEXT("animation"), AssetObjectForAnim)) {
            if (AssetObjectForAnim != nullptr && AssetObjectForAnim->IsValid())
            {
                TSharedPtr<FJsonObject> AnimationObject = *AssetObjectForAnim;
                FCPM_AssetData AnimationData;

                AnimationObject->TryGetStringField(TEXT("animation_id"), AnimationData.asset_id);
                AnimationObject->TryGetStringField(TEXT("animation_name"), AnimationData.file_name);
                AnimationObject->TryGetStringField(TEXT("fbx_gcp_file"), AnimationData.signed_url);
                AnimationObject->TryGetStringField(TEXT("created_at"), AnimationData.uploaded_on);


                AssetResponse.assets.Add(AnimationData);
            }
        }
        return true;
    }
    return false;
}
