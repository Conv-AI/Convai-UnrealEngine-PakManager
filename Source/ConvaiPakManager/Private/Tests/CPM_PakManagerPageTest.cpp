// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UI/Pages/SBasePage.h"
#include "UI/SCPM_PakManagerPage.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#if WITH_AUTOMATION_TESTS

/**
 * The shell casts every page it is handed to SBasePage* and calls a virtual on the result:
 *
 *     SBasePage* BasePage = (SBasePage*)&WidgetRef.Get();
 *     if (BasePage->IsA(SBasePage::StaticClass()))
 *
 * An unchecked cast, so a page deriving from anything else reads a wrong vtable and takes the editor
 * down - and the IsA guard cannot catch it, being the very virtual call that needs the vtable.
 *
 * This reproduces that cast exactly. It fails as a crash rather than an assertion if the base class
 * is ever changed, which is louder than the silence that preceded it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCPMPakManagerPageSurvivesTheShellsCast,
	"ConvaiPakManager.UI.PageSurvivesTheShellsCast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCPMPakManagerPageSurvivesTheShellsCast::RunTest(const FString&)
{
	const TSharedRef<SWidget> Page = SNew(SCPM_PakManagerPage);

	SBasePage* AsBasePage = static_cast<SBasePage*>(&Page.Get());
	TestNotNull(TEXT("the page is an SBasePage"), AsBasePage);
	TestTrue(TEXT("and answers the shell's type check"), AsBasePage->IsA(SBasePage::StaticClass()));

	// The shell calls this every time it shows the page. A null view model is expected and handled;
	// what must not happen is a crash.
	TestFalse(TEXT("supplies no view model, which the shell tolerates"), AsBasePage->GetViewModel().IsValid());
	AsBasePage->OnPageActivated();

	return true;
}

#endif  // WITH_AUTOMATION_TESTS
