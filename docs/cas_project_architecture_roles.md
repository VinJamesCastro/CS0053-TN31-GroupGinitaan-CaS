# Cents and Sensibility (CaS): architecture and member roles

CaS is a C++17 terminal banking application. This document assigns implementation ownership, explains how the modules cooperate, and gives each member a plan with at least three commits. The [shared development contract](cas_shared_development_contract.md) records the agreed type names, function interfaces, data format, and rules that let the four branches proceed in parallel.

## Architecture and parallel workflow

```text
CLI and session (Member 1)
  |-- banking operations (Member 2)
  |-- input validation and persistence (Member 3)
  `-- charts and exports (Member 4)
              ^
       shared BankData snapshot
```

Member 3 loads `BankData` at startup. Member 2 applies a requested financial change to a **candidate copy** of that snapshot. Member 1 asks Member 3 to save the candidate and adopts it as the active state only if saving succeeds. Member 4 reads the active snapshot to produce charts and reports. This call order keeps menu navigation, business rules, file I/O, and output formatting in their assigned modules.

The project has no HTTP server or web endpoints. The Markdown contract records the C++ types, fields, and function signatures agreed by the developers. Each member may branch from it and begin their assigned `.cpp` work immediately; the plan requires no header handoff. Complete application integration and PR review still depend on the relevant work being finished. The [parallel-work boundaries](cas_shared_development_contract.md#parallel-work-boundaries) list those later steps. Changes to a shared signature or persisted field need a reviewed contract update communicated to the affected owners.

| Author | Feature branch | Implementation ownership | PR reviewer |
| --- | --- | --- | --- |
| Member 1 | `feature/cli-menu-auth` | `main.cpp`, `MenuController.*`, authentication and session routing | Member 4 |
| Member 2 | `feature/transaction-engine` | `TransactionEngine.*`, account and financial rules | Member 1 |
| Member 3 | `feature/file-storage-validation` | `FileStorage.*`, `InputValidator.*`, persistence and history queries | Member 2 |
| Member 4 | `feature/ascii-charts-reports` | `ReportGenerator.*`, charts and exports | Member 3 |

Each member shall split their entire assigned work into **at least three meaningful commits** in one feature PR. The three milestones below for each member cover a usable module foundation, feature implementation, and cleanup; additional commits are allowed for distinct work or review fixes. The PR description should name the contract functions implemented and any known limitations. A reviewer checks behavior against the shared contract and requests changes in the author's branch. Shared integration fixes should be coordinated with the module owner so that branches do not introduce competing definitions.

## Member 1: menu, authentication, and integration

Build the entry point and state-driven guest, client, and admin menus. Guest actions are login, registration, About, and exit. Clients can view their own summary, deposit, withdraw, transfer, view an FX quote and filtered history, see a chart, export a statement or receipt, and log out. Admins can list accounts, change account status, read the audit log, and log out. An invalid choice returns to the current menu; logout clears the account ID and role.

Member 1 owns authentication, role checks, and call order. It uses Member 3's validators to collect input; Member 2's operations to change a candidate snapshot; Member 3's save function to persist it; and Member 4's read-only functions for visual output. A client action must use the authenticated account ID rather than trusting an arbitrary ID entered at the prompt. The registration flow must return the newly assigned account ID to the user after a successful save.

1. `feat(menu): add entry point and state-driven menu skeleton` — implement guest/client/admin loops and navigation against the agreed function signatures.
2. `feat(auth): add login, registration, and role-gated actions` — wire validators, authentication, transactions, status changes, reports, and persistence, including failure paths.
3. `refactor(menu): finish integration wiring and cleanup` — complete navigation and failure paths, then format touched code.

**Review focus:** No stuck menu loop, unauthorized account access, or successful message after a failed save. A rejected action leaves the active snapshot unchanged.

## Member 2: accounts, transactions, and FX

Implement account creation and summaries, deposits, withdrawals, peer-to-peer transfers, account status changes, and indicative currency conversion. This module receives `BankData` and performs no console or file I/O. Money uses integer minor units; conversion to display text belongs at the menu/report boundary. Generate unique account and transaction IDs from the loaded state. Apply the account's minimum balance and status rules to every outgoing operation. A transfer checks both accounts before changing either and records one ledger entry containing both balances.

Keep account type parameters, interest rates, minimum balances, and demo FX rates as named constants or model fields. A summary reports the stored annual rate; it does not silently post interest. The FX function returns a quote and never changes an account balance.

1. `feat(accounts): add creation, summaries, and product rules` — implement account IDs, opening deposits, type parameters, and read-only summaries.
2. `feat(transactions): add balance operations, status changes, and FX` — enforce invariants, append ledger entries, and return informative results.
3. `refactor(engine): finish edge cases and cleanup` — complete failure and monetary boundary handling, then format the module.

**Review focus:** No partial transfer, duplicate ID, balance drift, or mutation after a rejected operation.

## Member 3: persistence, validation, and queries

Implement the authoritative snapshot loader/saver, safe prompt helpers, filtered transaction history, and audit-log append/reading. Loading must distinguish a missing file (new installation) from a malformed existing file. Saving writes a complete temporary snapshot, checks it, then replaces the authoritative file. Parsing must reject invalid enum values, duplicate IDs, and truncated records instead of inventing default values. The PIN digest is an opaque string to storage; Member 1 owns credential verification.

Validators clear failed stream state, reject values outside the requested range, and keep a PIN as a string so leading zeroes survive. The history query filters the in-memory ledger by authenticated account, optional inclusive date range, type, and amount. Audit-log reading tolerates a missing or empty log.

1. `feat(storage): add snapshot parser and serializer` — implement the agreed records, startup load, temporary-file save, and useful errors.
2. `feat(input): add prompts, history filtering, and audit records` — implement range-safe numeric/PIN input, read-only ledger queries, and audit append/reading.
3. `refactor(storage): finish error handling and cleanup` — complete malformed-input and failed-write handling, then format the module.

**Review focus:** A failed write preserves the prior readable snapshot, and bad input cannot leave `std::cin` stuck in a failed state.

## Member 4: charts and reports

Implement read-only ASCII balance charts, account statements, and transaction receipts. Derive chart points from the account's chronological ledger entries, label values and dates, and cap bar width even for large balances. Statements show the account and period, opening/closing balances, and matching transactions. Receipts look up a transaction reference and display its amount and relevant account IDs. Return clear errors for missing accounts, missing transactions, or unwritable output paths.

Defects in another member's module should be reported to that owner and fixed in the appropriate PR or reviewed follow-up.

1. `feat(charts): add bounded ASCII balance chart` — implement chronological points and safe scaling for empty or large histories.
2. `feat(reports): add statement and receipt exports` — implement text formatting, lookup, output paths, and write errors.
3. `refactor(reports): finish edge cases and cleanup` — complete empty-history, lookup, and output-failure handling, then format the module.

**Review focus:** Exports never mutate the snapshot; reported values match the ledger; chart output remains bounded.

## Integration and acceptance

After review and merge, build with C++17 and complete this workflow: register, restart, log in, deposit, withdraw, transfer, filter history, draw a chart, export a statement and receipt, change status as admin, and confirm rejected actions leave the saved snapshot intact. The cyclic review chain is Member 1 reviews Member 2, Member 2 reviews Member 3, Member 3 reviews Member 4, and Member 4 reviews Member 1.

Source filenames above describe planned ownership, not existing implementation on `main`.
