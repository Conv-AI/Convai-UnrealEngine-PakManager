// Copyright 2025 Convai Inc. All Rights Reserved.

#include "Avatar/CPM_AvatarBlueprint.h"
#include "ConvaiAvatarBlueprint.h"

namespace ConvaiPakManager::Avatar
{
bool IsMetaHuman(const UBlueprint* Blueprint)
{
	return ConvaiAvatarPreparation::Avatar::IsMetaHuman(Blueprint);
}

bool PrepareAvatarBlueprint(UBlueprint* Blueprint, FString& OutError, TArray<FString>& OutChanges)
{
	return ConvaiAvatarPreparation::Avatar::PrepareAvatarBlueprint(Blueprint, OutError, OutChanges);
}
}
