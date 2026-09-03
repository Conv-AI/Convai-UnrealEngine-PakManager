// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Thumbnail/CPM_Thumbnail.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_TESTS

using namespace ConvaiPakManager::Thumbnail;

namespace
{
	/** A throwaway directory to write fixture images into and import them back out of. */
	struct FScratchThumbnailDir
	{
		FString Path;

		explicit FScratchThumbnailDir(const TCHAR* TestName)
		{
			Path = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("CPM_Tests"), TestName);
			IFileManager::Get().DeleteDirectory(*Path, false, true);
			IFileManager::Get().MakeDirectory(*Path, true);
		}

		~FScratchThumbnailDir()
		{
			IFileManager::Get().DeleteDirectory(*Path, false, true);
		}

		FString File(const TCHAR* FileName) const
		{
			return FPaths::Combine(Path, FileName);
		}
	};

	TArray<FColor> Filled(const int32 Count, const FColor Colour)
	{
		TArray<FColor> Pixels;
		Pixels.Init(Colour, Count);
		return Pixels;
	}
}

/**
 * A thumbnail is the whole of what a player sees before they take an Asset, and a capture taken of
 * an unloaded scene or an unrenderable blueprint comes back black or fully transparent. Publishing
 * one is not recoverable from the tool, so a blank image has to fail the check that gates it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMThumbnailRefusesAnEmptyImage,
	"ConvaiPakManager.Thumbnail.RefusesAnEmptyImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMThumbnailRefusesAnEmptyImage::RunTest(const FString&)
{
	constexpr int32 Count = 64 * 64;

	TestFalse(TEXT("an all-black opaque image has no content"), HasContent(Filled(Count, FColor(0, 0, 0, 255))));
	TestFalse(TEXT("a fully transparent image has no content"), HasContent(Filled(Count, FColor(255, 255, 255, 0))));
	TestFalse(TEXT("an empty view has no content"), HasContent(TArrayView<const FColor>()));

	// 1% of 4096 is 40.96, so 41 lit pixels is the first count that clears the default ratio.
	TArray<FColor> JustEnough = Filled(Count, FColor(0, 0, 0, 255));
	for (int32 Index = 0; Index < 41; ++Index)
	{
		JustEnough[Index] = FColor(128, 128, 128, 255);
	}
	TestTrue(TEXT("1% of the pixels lit is content"), HasContent(JustEnough));

	TArray<FColor> HalfAsMany = Filled(Count, FColor(0, 0, 0, 255));
	for (int32 Index = 0; Index < 20; ++Index)
	{
		HalfAsMany[Index] = FColor(128, 128, 128, 255);
	}
	TestFalse(TEXT("half a percent lit is not content"), HasContent(HalfAsMany));

	return true;
}

/**
 * A creator who already has artwork should not have to fake a viewport that shows it. The import
 * has to say which of the three ways it refused - unreadable, not an image, blank - because they
 * have three different fixes, and it must not leave a half-written destination behind on refusal.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMThumbnailImportsAnAuthoredImage,
	"ConvaiPakManager.Thumbnail.ImportsAnAuthoredImage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCPMThumbnailImportsAnAuthoredImage::RunTest(const FString&)
{
	const FScratchThumbnailDir Scratch(TEXT("ImportsAnAuthoredImage"));

	// The missing-file case is the point of the case, and the loader warns about it on the way past.
	AddExpectedErrorPlain(TEXT("gone.png"), EAutomationExpectedErrorFlags::Contains, 1);

	const FColor Solid(12, 200, 90, 255);
	const FString Source = Scratch.File(TEXT("src.png"));
	const FString Destination = Scratch.File(TEXT("Thumbnail_10.png"));
	TestTrue(TEXT("the fixture is written"), WritePng(Source, 32, 16, Filled(32 * 16, Solid)));

	FString Error;
	TestTrue(TEXT("an authored image is adopted"), ImportImageFile(Source, Destination, Error));
	TestTrue(TEXT("and says nothing went wrong"), Error.IsEmpty());

	int32 Width = 0;
	int32 Height = 0;
	TArray<FColor> Pixels;
	TestTrue(TEXT("the destination decodes"), DecodeImageFile(Destination, Width, Height, Pixels));
	TestEqual(TEXT("at the source width"), Width, 32);
	TestEqual(TEXT("at the source height"), Height, 16);
	TestEqual(TEXT("with the source colour intact"), Pixels[0], Solid);
	TestTrue(TEXT("and passes the content check"), FileHasContent(Destination));

	const FString BlankSource = Scratch.File(TEXT("src2.png"));
	const FString BlankDestination = Scratch.File(TEXT("Thumbnail_11.png"));
	TestTrue(TEXT("the blank fixture is written"), WritePng(BlankSource, 32, 16, Filled(32 * 16, FColor(0, 0, 0, 255))));
	TestFalse(TEXT("a blank image is not adopted"), ImportImageFile(BlankSource, BlankDestination, Error));
	TestTrue(TEXT("and says it is blank"), Error.Contains(TEXT("blank")));
	TestFalse(TEXT("leaving no destination behind"), IFileManager::Get().FileExists(*BlankDestination));
	TestFalse(TEXT("and the blank file fails the content check"), FileHasContent(BlankSource));

	const FString NotAnImage = Scratch.File(TEXT("notes.txt"));
	FFileHelper::SaveStringToFile(TEXT("this is not a picture"), *NotAnImage);
	TestFalse(TEXT("a text file is not adopted"), ImportImageFile(NotAnImage, Destination, Error));
	TestTrue(TEXT("and says it is not an image"), Error.Contains(TEXT("not an image")));

	TestFalse(TEXT("a missing file is not adopted"),
		ImportImageFile(Scratch.File(TEXT("gone.png")), Destination, Error));
	TestTrue(TEXT("and says it could not be read"), Error.Contains(TEXT("could not be read")));

	return true;
}

#endif
