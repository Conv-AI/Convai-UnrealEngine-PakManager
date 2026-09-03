// Copyright 2025 Convai Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;

namespace ConvaiPakManager::Avatar
{
/**
 * Whether an Avatar's Entry Point blueprint is a MetaHuman.
 *
 * Decided by scanning, never by reading `is_metahuman` out of the modding metadata: that field
 * survives migration byte-for-byte and nothing has ever written it truthfully, so trusting it would
 * wire anim blueprints onto a non-MetaHuman and skip them on a real one.
 *
 * The rule is the legacy uploader's, unchanged - the first SCS node whose variable name contains
 * "body", whose template is a skeletal mesh, and whose skeleton's path contains "metahuman".
 */
CONVAIPAKMANAGER_API bool IsMetaHuman(const UBlueprint* Blueprint);

/**
 * Brings an Avatar's Entry Point blueprint up to what a published Avatar needs, in place.
 *
 * A creator can pick a bare Actor blueprint and get back a working Convai avatar: the BP chatbot
 * component, the face sync component, and - for a MetaHuman - Convai's body and face anim
 * blueprints on the meshes that have none of their own. A creator's own anim blueprint is never
 * replaced, and the one class this does replace (`Face_AnimBP_C`) is what the MetaHuman importer
 * leaves behind rather than anything a creator authored.
 *
 * Refuses, changing nothing, a blueprint carrying the raw C++ UConvaiChatbotComponent. Legacy
 * accepted it - its check was `ClassIsChildOf` - and shipped Avatars missing the BP wrapper's
 * action dispatch and movement helpers.
 *
 * Compiles once at the end when something changed, and never saves: the caller owns the package,
 * and tests run this against blueprints in the transient package that cannot be saved at all.
 *
 * @param OutChanges  One line per edit, empty when the blueprint already satisfied every rule.
 * @return false with OutError set when the blueprint must be fixed by hand.
 */
CONVAIPAKMANAGER_API bool PrepareAvatarBlueprint(UBlueprint* Blueprint, FString& OutError, TArray<FString>& OutChanges);
}
