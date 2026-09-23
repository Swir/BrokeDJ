// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Swir
#pragma once

#include <cstddef>

namespace broke::library {

struct PresenceUpdateAccounting final {
    std::size_t changed = 0;
    std::size_t unresolved = 0;
    bool discardObservedMissing = false;
};

// Translate the guarded SQLite UPDATE outcome into the user-visible refresh
// summary. A row-count other than one means the snapshotted record changed
// before reconciliation could commit. Fail closed: surface that race as
// unresolved and do not count a stale "missing" observation as settled.
[[nodiscard]] constexpr PresenceUpdateAccounting accountPresenceConditionalUpdate(
    bool observedMissing, int changedRows) noexcept {
    if (changedRows == 1)
        return PresenceUpdateAccounting{1, 0, false};

    return PresenceUpdateAccounting{0, 1, observedMissing};
}

static_assert(accountPresenceConditionalUpdate(false, 1).changed == 1);
static_assert(accountPresenceConditionalUpdate(true, 1).unresolved == 0);
static_assert(accountPresenceConditionalUpdate(false, 0).unresolved == 1);
static_assert(accountPresenceConditionalUpdate(true, 0).discardObservedMissing);
static_assert(accountPresenceConditionalUpdate(true, 2).unresolved == 1);

} // namespace broke::library
