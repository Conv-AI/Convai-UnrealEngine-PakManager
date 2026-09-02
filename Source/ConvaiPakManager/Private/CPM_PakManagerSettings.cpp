// Copyright 2025 Convai Inc. All Rights Reserved.

#include "CPM_PakManagerSettings.h"

#if WITH_EDITOR

void UCPM_PakManagerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bool bFilledSomething = false;

	// Only where the creator has asked for the platform: an untouched configuration on a platform
	// nobody is packaging is not a gap, and filling it would put a build setting in front of them
	// that has no bearing on anything.
	for (FCPM_PlatformPolicy* Platform : { &PolicyOverride.Windows, &PolicyOverride.Linux })
	{
		if (Platform->bShouldPackage && Platform->Configuration.IsEmpty())
		{
			Platform->Configuration = FCPM_PublishPolicy::Defaults().Windows.Configuration;
			bFilledSomething = true;
		}
	}

	if (bFilledSomething)
	{
		// Written back here because this is a change nobody made in the panel, and an unsaved one
		// would come back empty on the next restart - the very problem it exists to fix.
		TryUpdateDefaultConfigFile();
	}
}

#endif
