// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include "LibraryDatabase.h"

namespace broke::library::qualification {

struct M4FixtureQualification final {
    bool workflowStable = false;
    bool qualified = false;
};

[[nodiscard]] inline bool sameWorkflowSummary(
    const ContentHashWorkflowSummary& first,
    const ContentHashWorkflowSummary& confirmation) noexcept {
    return first.trackCount == confirmation.trackCount
        && first.missingTrackCount == confirmation.missingTrackCount
        && first.historyCount == confirmation.historyCount
        && first.tagAssociationCount == confirmation.tagAssociationCount
        && first.playlistMembershipCount == confirmation.playlistMembershipCount;
}

// The connected M4 verifier intentionally uses two bounded presence passes around
// two aggregate snapshots. The initial pass may repair a stale persisted missing
// flag, but the confirmation pass must be fully settled and change-free. This
// narrows the filesystem/database race window without claiming atomicity with the
// external filesystem: a mutation after the confirmation pass is still outside
// the verifier's control and must be handled by a later witness retry.
[[nodiscard]] inline M4FixtureQualification evaluateM4FixtureQualification(
    const FilePresenceRefreshResult& initialRefresh,
    const ContentHashWorkflowSummary& initialWorkflow,
    const FilePresenceRefreshResult& confirmationRefresh,
    const ContentHashWorkflowSummary& confirmationWorkflow) noexcept {
    const bool workflowStable = sameWorkflowSummary(initialWorkflow, confirmationWorkflow);
    const bool qualified = initialRefresh.complete
        && initialRefresh.unresolved == 0
        && initialRefresh.missing == 0
        && confirmationRefresh.complete
        && confirmationRefresh.changed == 0
        && confirmationRefresh.unresolved == 0
        && confirmationRefresh.missing == 0
        && workflowStable
        && confirmationWorkflow.trackCount >= 2
        && confirmationWorkflow.missingTrackCount == 0
        && confirmationWorkflow.historyCount >= 1
        && confirmationWorkflow.tagAssociationCount >= 1
        && confirmationWorkflow.playlistMembershipCount >= 1;
    return M4FixtureQualification{workflowStable, qualified};
}

} // namespace broke::library::qualification
