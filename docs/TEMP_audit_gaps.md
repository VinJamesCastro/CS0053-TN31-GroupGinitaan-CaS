# Temporary audit notes

These notes describe gaps observed on `fix/integration-contract-gaps`. This file is temporary and is not part of a committed change.

## Gaps

1. **Multiple accounts under one profile:** The [project overview](project_overview.md) describes several account types under one profile. The current model and login flow identify a single account by account ID; there is no shared profile or account switching.
2. **Monthly balance chart:** The overview describes monthly balance fluctuations. `ReportGenerator.cpp` plots a point for each matching transaction. The [architecture document](cas_project_architecture_roles.md) describes transaction-based chart points, so the documents disagree on the intended feature.
3. **Currency quote wording:** The overview calls conversion “real-time.” The [shared contract](cas_shared_development_contract.md) and implementation use fixed demo rates.
4. **Visible PIN entry:** [Member 1's note](member1_auth.md) states that PIN input is visible in the terminal. This limits the security experience described in the overview.
5. **Stale documentation and submitted test code:** The architecture document still calls source files planned even though they are present. Tracked C++ files exist under `tests/`, despite the earlier decision to avoid submitted test code.

## Checks completed

- The complete application built on Windows with MSYS2 UCRT64 G++, C++17, `Authentication.cpp`, and `-lbcrypt`.
- A temporary end-to-end run covered first-run admin setup, registration, restart, deposit, withdrawal, transfer, history, chart, statement, receipt, status change, and rejection of a transfer to a frozen account. Saved balances and report amounts agreed.
- A malformed existing snapshot stopped startup and was not overwritten.

## Checks still open

The complete application has not been exercised for overflow, failed writes, or account and transaction IDs longer than six digits.
