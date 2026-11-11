# Temporary audit notes

These notes describe gaps observed on `fix/integration-contract-gaps`. This file is temporary and is not part of a committed change.

## Gaps

1. **Multiple accounts under one profile:** The [project overview](project_overview.md) describes several account types under one profile. The current model and login flow identify a single account by account ID; there is no shared profile or account switching. **ACTION:** Implement it.
2. **Monthly balance chart:** The overview describes monthly balance fluctuations. `ReportGenerator.cpp` plots a point for each matching transaction. The [architecture document](cas_project_architecture_roles.md) describes transaction-based chart points, so the documents disagree on the intended feature.
**ACTION:** Remove that description from the overview.
3. **Currency quote wording:** The overview calls conversion “real-time.” The [shared contract](cas_shared_development_contract.md) and implementation use fixed demo rates.
**ACTION:** Ignore it. That's beyond our scope.
4. **Visible PIN entry:** [Member 1's note](member1_auth.md) states that PIN input is visible in the terminal. This limits the security experience described in the overview.
**ACTION:** Make it not visible.
5. **Stale documentation and submitted test code:** The architecture document still calls source files planned even though they are present. Tracked C++ files exist under `tests/`, despite the earlier decision to avoid submitted test code.
**ACTION:** Settle this properly.

## Checks still open

The complete application has not been exercised for overflow, failed writes, or account and transaction IDs longer than six digits.
