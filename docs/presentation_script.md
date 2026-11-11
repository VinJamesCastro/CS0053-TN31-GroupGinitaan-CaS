# CaS codebase presentation script (about 19–20 minutes)

**Audience:** instructor and classmates

**Code baseline:** `4c4b90c` (11 November 2026)

**Style:** explain what each module does and how data moves between modules; use the linked lines as places to point in VS Code, not as lines to read aloud.

**Ownership:** Member 1 created the repository and built startup, menus, and authentication; Member 2 built the transaction engine; Member 3 built validation and file storage; Member 4 is the leader, built charts and reports, and completed the final connection of the pushed modules. The commit tables are presenter cues: explain each change in plain language without reading hashes aloud.

## Before the timed presentation

- Open the repository in VS Code and the GitHub pull request pages linked below. Keep the terminal, Explorer, source files, and GitHub ready to switch between. Each member opens their own PR while discussing their module.
- Run `.\run.ps1` once beforehand to confirm the C++17 build. It compiles all seven `.cpp` files and links Windows `bcrypt` ([`run.ps1:12`](../run.ps1#L12), [`run.ps1:25`](../run.ps1#L25)). The script also starts the program. For a faster restart during the talk, run `.\cas_app.exe` from the same directory.
- Have an admin account and one demo client ready. Record their IDs, PINs, account type, and starting balance in private presenter notes. The screen sharer will use the client account for the opening tour and the admin account for a quick view of its menu. If the installation is new, set up the admin and client before the timed presentation. Do not display a PIN or delete an existing `data/` folder to reset the demo.
- Keep VS Code's Explorer open so the audience can see `data/bank_state.txt` appear or change and `reports/` receive exported files. Both directories are ignored by Git ([`.gitignore:5`](../.gitignore#L5)).
- If the live tour runs long, skip the optional withdrawal and statement preview. Keep the invalid amount, receipt, restart, and four PR explanations.

| Time | Speaker | Focus |
| --- | --- | --- |
| 0:00–0:45 | Member 1 | Hook |
| 0:45–5:45 | Screen sharer (Member 1) | Tour the running program, including invalid input, an export, and persistence after restart |
| 5:45–8:45 | Member 1 | Menu and authentication code, then their PR and commits |
| 8:45–11:45 | Member 2 | Transaction engine code, then their PR and commits |
| 11:45–15:00 | Member 3 | Validation, storage, debugging case, then their PR and commits |
| 15:00–19:15 | Member 4 | Chart/report code, their PR and commits, final integration, and merge order |
| 19:15–19:45 | Member 4 | Leader's outro |

## 0:00–0:45 — Member 1: hook

**On screen:** run the program from the repository root; leave the welcome screen visible.

> "A banking program can print *Deposit completed* in an instant. But what if it closes before the new balance is safely saved? That is the question behind our project. Cents and Sensibility, or CaS, is a terminal banking simulation that connects account rules, input checks, persistent records, and reports. Today we will make a transaction, restart the program, and show that the balance and transaction are still there."

**Handoff:** "Let us see the complete program first. Then each member will explain the code and GitHub work behind their part."

## 0:45–5:45 — Screen sharer: guided tour of the program

**On screen:** terminal first; use Explorer only when showing saved and exported files. Keep the source code closed until the walkthrough ends. Run `.\cas_app.exe` from the repository root after the rehearsal build. Press Enter after each result screen when the program asks to continue.

1. **Welcome and login (about 45 seconds).** Advance past the welcome card. Show the guest menu, choose **1, Login**, and enter the prepared client's `ACC-...` ID and PIN. Point out that account IDs have an example on screen, ordinary input is visible, and PIN digits are hidden. Pause briefly at the 1–11 client menu.
2. **Account and invalid input (about 75 seconds).** Choose **1, Account summary** and state the starting balance. Choose **2, Deposit funds**. Enter `12.345`; the validator rejects the third decimal place and asks again. Enter `25.00`, note the new `TXN-...` reference, and show the updated balance with **1, Account summary**. If time permits, try an oversized withdrawal to show a separate business-rule rejection.
3. **History, chart, and export (about 90 seconds).** Choose **6, Transaction history**. Leave start and end dates blank, select type `0` for all, then enter `0` for both minimum and maximum filter questions. Show the new ledger row. Choose **7, Balance chart**; identify the date, bar, printed balance, and reference. Choose **8, Export**, then **2, Receipt**. Copy the deposited transaction's ID from the on-screen table and open `reports/Receipt_<transaction ID>.txt` in Explorer. Mention that **8 → 1** exports a full statement; a pre-generated statement may be opened if time allows.
4. **Persistence and admin view (about 90 seconds).** Show `data/bank_state.txt` in Explorer without editing it. Log out with client option **9**, exit with guest option **4**, and run `.\cas_app.exe` again from the *same directory*. Log in as the same client, choose **1, Account summary**, and compare the balance with the starting balance: it increased by PHP 25.00. Log out, log in with the prepared `ADM-...` ID, show the admin menu, then choose **1** to list accounts and **3** to view the audit log. Status changes are available under **2**; no status change is needed for this tour.

> "We have now seen the user-facing path: a login, a rejected entry, a saved transaction, a report, and the same balance after restarting. The rest of our presentation follows that path through the four modules. Each member will also show the feature branch, pull request, and commits for the code they own."

**Behind this tour:** [`MenuController.cpp:739`](../MenuController.cpp#L739) (`runMenu`), [`InputValidator.cpp:219`](../InputValidator.cpp#L219) (`getValidMoney`), [`MenuController.cpp:258`](../MenuController.cpp#L258) (`commitChange`), [`FileStorage.cpp:443`](../FileStorage.cpp#L443) (`saveSnapshot`), and [`MenuController.cpp:541`](../MenuController.cpp#L541) (`exportReport`). The screen sharer shows behavior here; the owners explain these functions in their sections below.

## 5:45–8:45 — Member 1: startup, authentication, menus, and PR

**On screen:** `BankModel.h`, `main.cpp`, `Authentication.cpp`, `MenuController.cpp`, then Member 1's GitHub PRs.

> "The shared `BankData` object holds accounts and their transaction ledger. The menu calls the transaction engine for a change, storage saves it, and reports read the resulting state. All modules use the same C++ types and function contract. The engine fills in the transaction's amount and resulting balances; storage serializes those fields; the chart and statement read them later."

**Point to:** [`BankModel.h:12`](../BankModel.h#L12) (`Money`), [`BankModel.h:20`](../BankModel.h#L20) (`Account`), [`BankModel.h:33`](../BankModel.h#L33) (`Transaction`), [`BankModel.h:52`](../BankModel.h#L52) (`BankData`), and [`TeamInterfaces.h:11`](../TeamInterfaces.h#L11) (module function declarations).

> "`main()` is the entry point. It shows the welcome card and looks for `data/bank_state.txt`. If a valid snapshot exists, it loads it. If there is no snapshot, `bootstrapAdmin()` asks for a new administrator name and PIN and saves the new account. A malformed existing snapshot causes an error instead of silently starting over."

**Point to:** [`main.cpp:16`](../main.cpp#L16) (`bootstrapAdmin`), [`main.cpp:60`](../main.cpp#L60) (`main`), and [`FileStorage.cpp:320`](../FileStorage.cpp#L320) (`loadSnapshot`).

> "The menu is driven by a small session state: guest, client, or admin. `runMenu()` routes to the matching menu. A client action uses the account ID from the authenticated session, while the admin menu exposes account status and audit actions. `commitChange()` makes a candidate copy, applies the engine operation, saves it, and only then adopts it as active state and reports success."

> "The client menu offers banking, history, charts, reports, and profile switching. A linked account keeps the holder and PIN digest, and switching checks profile ownership. The separate admin menu provides account listing, status changes, and audit history."

**Point to:** [`MenuController.cpp:194`](../MenuController.cpp#L194) (`SessionRole` and `Session`), [`MenuController.cpp:258`](../MenuController.cpp#L258) (`commitChange`), [`MenuController.cpp:653`](../MenuController.cpp#L653) (`runClientMenu`), [`MenuController.cpp:677`](../MenuController.cpp#L677) (`runAdminMenu`), and [`MenuController.cpp:739`](../MenuController.cpp#L739) (`runMenu`).

**Also show if asked:** [`MenuController.cpp:349`](../MenuController.cpp#L349) (`openLinkedAccount`), [`MenuController.cpp:377`](../MenuController.cpp#L377) (`switchProfileAccount`), and [`MenuController.cpp:95`](../MenuController.cpp#L95) (`frame`).

> "For login, we store a salted PBKDF2 PIN digest, not the PIN itself. `login()` checks the entered account ID and PIN and returns a generic error when they do not match. The PIN prompt hides its digits; ordinary fields remain visible."

**Point to:** [`Authentication.cpp:111`](../Authentication.cpp#L111) (`digestPin`), [`Authentication.cpp:122`](../Authentication.cpp#L122) (`login`), and [`InputValidator.cpp:240`](../InputValidator.cpp#L240) (`getValidPin`).

> "I also created the repository, so our first commit established the shared place for all four branches. My feature PR focused on the app's entry point and user flow; the banking rules, storage, and reports stayed in their owners' modules."

**Member 1's GitHub walkthrough:** Open [`feature/cli-menu-auth` PR #2](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/2), show these commits in order, then its merge [`fb149b1`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/fb149b1).

| Commit | What changed in the PR | Where to point in the current code |
| --- | --- | --- |
| [`918f38f`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/918f38f) | Introduced a working guest flow with startup and choices for login, registration, About, and exit. | [`main.cpp:60`](../main.cpp#L60) `main`; [`MenuController.cpp:234`](../MenuController.cpp#L234) `showGuestMenu` |
| [`aaf7a57`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/aaf7a57) | Added guest, client, and admin session states and the menus routed by role. | [`MenuController.cpp:194`](../MenuController.cpp#L194) `Session`; [`MenuController.cpp:739`](../MenuController.cpp#L739) `runMenu` |
| [`dcab1e6`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/dcab1e6) | Separated the code that displays menu choices from the code that handles the selected action. | [`MenuController.cpp:234`](../MenuController.cpp#L234) `showGuestMenu`; [`MenuController.cpp:739`](../MenuController.cpp#L739) `runMenu` |
| [`0e5deea`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/0e5deea) | Wired account registration, PIN hashing/login, and role-specific actions to the other modules. | [`Authentication.cpp:111`](../Authentication.cpp#L111) `digestPin`; [`Authentication.cpp:122`](../Authentication.cpp#L122) `login`; [`MenuController.cpp:322`](../MenuController.cpp#L322) `registerAccount` |

Also show the repository's [initial commit `ee28cf6`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/ee28cf6) when stating that Member 1 created it. Then open Member 1's [integration PR #7](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/7): its [`30e9a53`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/30e9a53) commit removed duplicate bank-type definitions and aligned storage, input, menu, and report code on `BankModel.h`. The PR merged as [`7f97342`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/7f97342). Member 4's later final integration is explained in their section.

> "My first commit made the guest menu usable. The next introduced client and admin sessions; a refactor separated menu display from navigation; then I wired PIN login and role actions. PR 7 fixed a shared-model mismatch after the feature merges. Member 4 later led the final connection and UI follow-up."

**Handoff:** "A menu choice is only a request. Member 2's engine decides whether the financial change is allowed."

## 8:45–11:45 — Member 2: transaction rules and PR

**On screen:** `TransactionEngine.cpp`, then Member 2's GitHub PR.

> "The engine works on `BankData` and does not read from the terminal or write files. `Money` is a signed integer count of centavos, so account balances do not depend on floating point rounding. `ProductRules` defines each product's opening amount, minimum balance, and displayed annual rate: checking starts at PHP 500, savings at PHP 100, and time deposit at PHP 1,000. The engine reports the interest rate; it does not post interest automatically."

**Point to:** [`BankModel.h:12`](../BankModel.h#L12) (`Money`), [`TransactionEngine.cpp:20`](../TransactionEngine.cpp#L20) (`ProductRules`), and [`TransactionEngine.cpp:391`](../TransactionEngine.cpp#L391) (`getAccountSummary`).

> "`createAccount()` checks the product and opening deposit, generates the next account ID, and creates an opening transaction. `deposit()` adds a ledger entry only for a positive amount and an active client account. `withdraw()` checks that enough money remains above the minimum. `transfer()` checks both accounts and the sender's remaining balance before recording one transaction with both resulting balances. A time-deposit account cannot be used for withdrawals or transfers while there is no maturity rule."

> "A ledger entry is more than a message on the screen. It records the transaction ID, time, type, amount, affected account IDs, and balances after the operation. That is why later screens can explain *how* an account reached its current balance. The functions return an `OperationResult` to the menu, including a success flag and reference. If a rule fails, the operation returns an error and the menu never saves that candidate as the new state."

**Point to:** [`TransactionEngine.cpp:118`](../TransactionEngine.cpp#L118) (`makeNextId`), [`TransactionEngine.cpp:300`](../TransactionEngine.cpp#L300) (`createAccount`), [`TransactionEngine.cpp:417`](../TransactionEngine.cpp#L417) (`deposit`), [`TransactionEngine.cpp:448`](../TransactionEngine.cpp#L448) (`withdraw`), and [`TransactionEngine.cpp:484`](../TransactionEngine.cpp#L484) (`transfer`).

> "The same engine handles status changes and an FX *quote*. An admin can freeze, reactivate, or close a client account under the allowed transitions. Currency conversion uses fixed demonstration rates for PHP, USD, EUR, and JPY; it returns an estimate and never changes a bank balance."

> "For our simulation, the fixed rates are USD 1 = PHP 56, EUR 1 = PHP 60, and JPY 1 = PHP 0.40. JPY has whole-yen minor units, while PHP, USD, and EUR have hundredths. Frozen accounts cannot be used for new money movements, and a closed account cannot be reopened through the status function."

**Point to:** [`TransactionEngine.cpp:542`](../TransactionEngine.cpp#L542) (`updateAccountStatus`) and [`TransactionEngine.cpp:591`](../TransactionEngine.cpp#L591) (`convertCurrency`).

**Member 2's GitHub walkthrough:** Open [`feature/transaction-engine` PR #4](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/4) and explain its changes before merge [`0b01c40`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/0b01c40).

| Commit | What changed in the PR | Where to point in the current code |
| --- | --- | --- |
| [`8f1d05f`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/8f1d05f) | Established product minimums and rates; created accounts and opening ledger entries; generated account IDs and read-only summaries. | [`TransactionEngine.cpp:20`](../TransactionEngine.cpp#L20) `ProductRules`; [`TransactionEngine.cpp:118`](../TransactionEngine.cpp#L118) `makeNextId`; [`TransactionEngine.cpp:300`](../TransactionEngine.cpp#L300) `createAccount` |
| [`c2851f1`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/c2851f1) | Added deposit, withdrawal, transfer, account status changes, and fixed-rate FX with range and balance checks. | [`TransactionEngine.cpp:417`](../TransactionEngine.cpp#L417) `deposit`; [`TransactionEngine.cpp:448`](../TransactionEngine.cpp#L448) `withdraw`; [`TransactionEngine.cpp:484`](../TransactionEngine.cpp#L484) `transfer`; [`TransactionEngine.cpp:591`](../TransactionEngine.cpp#L591) `convertCurrency` |
| [`326035f`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/326035f) | Closed an edge case by blocking transfers *into* time-deposit accounts as well as transfers out. | [`TransactionEngine.cpp:484`](../TransactionEngine.cpp#L484) `transfer` |
| [`507c422`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/507c422) | Brought newer `main` changes into the branch before the PR merge; this was a branch synchronization commit. | Compare the PR commit graph rather than presenting it as another banking feature. |

> "My first commit created product rules, IDs, opening records, and summaries. The second added deposits, withdrawals, transfers, status changes, and FX. The final fix blocked transfers into time deposits, then I synced with `main` for the PR merge. The opening demo's deposit exercised this engine."

**Handoff:** "The engine can calculate a correct result, but we still need to reject bad entries and make that result survive a restart."

## 11:45–15:00 — Member 3: validation, persistence, debugging, and PR

**On screen:** `InputValidator.cpp`, `FileStorage.cpp`, then Member 3's GitHub PR. Refer back to the invalid amount and restart shown in the opening tour.

> "The validator collects complete lines rather than letting a failed number conversion trap the menu. `getValidInt()` repeats until a whole number is in range. `getValidMoney()` accepts a PHP amount with up to two decimal places and converts it to centavos without floating point. `getValidPin()` requires exactly four or six digits, keeping leading zeroes because a PIN is text, not a number."

> "There are two kinds of rejection to notice. A malformed amount such as three decimal places is caught before the engine runs, so the prompt simply repeats. A well-formed but disallowed withdrawal reaches the engine, which rejects it because of the account rules. Both leave the saved balance alone, but the messages explain different problems to the user."

**Point to:** [`InputValidator.cpp:103`](../InputValidator.cpp#L103) (`parseInteger`), [`InputValidator.cpp:121`](../InputValidator.cpp#L121) (`parseMoney`), [`InputValidator.cpp:200`](../InputValidator.cpp#L200) (`getValidInt`), [`InputValidator.cpp:219`](../InputValidator.cpp#L219) (`getValidMoney`), and [`InputValidator.cpp:240`](../InputValidator.cpp#L240) (`getValidPin`).

> "Persistence is separate from the engine. `saveSnapshot()` acquires a save lock, writes a temporary snapshot, flushes it, loads that temporary file again to verify it, and then replaces the authoritative file. `loadSnapshot()` checks the header, record types, IDs, counts, and profile consistency. It distinguishes a missing file from a broken existing file. `filterTransactions()` supplies the account-scoped history query; the audit functions maintain the admin status-change log."

> "The snapshot has an explicit `CAS_SNAPSHOT` version header, account and transaction records, and an `END` record with counts. The loader rejects duplicate IDs, incomplete records, and unsupported versions. Versions 1 and 2 can be read; version 2 includes persistent profile links. A successful save verifies the temporary file by loading it before replacing the prior snapshot."

> "The saved snapshot is the source of truth after a restart. A bad existing file stops startup instead of creating an empty bank. History filters narrow the ledger by account, date, type, and amount; the separate audit log records admin status changes."

**Point to:** [`FileStorage.cpp:320`](../FileStorage.cpp#L320) (`loadSnapshot`), [`FileStorage.cpp:443`](../FileStorage.cpp#L443) (`saveSnapshot`), [`FileStorage.cpp:547`](../FileStorage.cpp#L547) (`filterTransactions`), [`FileStorage.cpp:642`](../FileStorage.cpp#L642) (`readAuditLogs`), and [`FileStorage.cpp:703`](../FileStorage.cpp#L703) (`appendAuditLog`).

### Debugging case to explain (about 45 seconds)

> "A reported terminal bug made ordinary fields invisible after PIN entry. For the debugger walkthrough, put one breakpoint in `HiddenInput` and another in `prepareVisibleInput()`. Inspect `requested`, `originalMode_`, `mode`, and `visibleMode`, especially the `ENABLE_ECHO_INPUT` bit. The diagnosis is that a normal prompt could inherit echo-off mode. The fix explicitly enables line input and echo for every non-PIN prompt, while PIN entry briefly disables echo. The observed result after the fix was visible names, choices, and amounts, with PIN digits still hidden."

**Point to:** [`InputValidator.cpp:20`](../InputValidator.cpp#L20) (`HiddenInput`), [`InputValidator.cpp:188`](../InputValidator.cpp#L188) (`prepareVisibleInput`), and [`MenuController.cpp:186`](../MenuController.cpp#L186) (`promptFor`). The fix appears in commit [`07e60f4`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/07e60f4).

**Member 3's GitHub walkthrough:** Open [`feature/file-storage-validation` PR #3](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/3) and explain its changes before merge [`eea02cf`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/eea02cf).

| Commit | What changed in the PR | Where to point in the current code |
| --- | --- | --- |
| [`c86c63c`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/c86c63c) | Implemented snapshot writing and strict loading, including record parsing, IDs, counts, temporary-file replacement, and errors. | [`FileStorage.cpp:320`](../FileStorage.cpp#L320) `loadSnapshot`; [`FileStorage.cpp:443`](../FileStorage.cpp#L443) `saveSnapshot` |
| [`ab2a139`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/ab2a139) | Added range-checked number/money/PIN prompts, account-scoped history filters, and audit log append/read. | [`InputValidator.cpp:200`](../InputValidator.cpp#L200) `getValidInt`; [`InputValidator.cpp:219`](../InputValidator.cpp#L219) `getValidMoney`; [`FileStorage.cpp:547`](../FileStorage.cpp#L547) `filterTransactions`; [`FileStorage.cpp:703`](../FileStorage.cpp#L703) `appendAuditLog` |
| [`9add588`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/9add588) | Tightened UTC timestamp checks and addressed parser/validator analysis warnings. | [`FileStorage.cpp:65`](../FileStorage.cpp#L65) `isValidUtcTimestamp`; [`FileStorage.cpp:300`](../FileStorage.cpp#L300) `parseTransaction` |
| [`44ca3f0`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/44ca3f0) | Brought newer `main` changes into the branch before PR merge. | Show as branch synchronization, not a new storage feature. |

The branch originally had a separate `BankModels.h`; integration PR #7 later moved the whole app to the current shared [`BankModel.h:52`](../BankModel.h#L52). This is a historical integration change, not a missing file in the present repository.

> "My first commit established the snapshot loader and saver. The next added typed prompts, history filters, and audit records. The final change tightened timestamp validation, then I synced with `main` for the PR merge. The later echo fix belongs to this input module's integration history; the demo showed both visible prompts and saved-state behavior."

**Handoff:** "Now that the ledger is validated and saved, Member 4 can present it without changing the account."

## 15:00–19:15 — Member 4: charts, reports, PR, and merge recap

**On screen:** `ReportGenerator.cpp`, then Member 4's GitHub PR. Refer back to the chart and receipt shown in the opening tour.

> "This module reads `BankData` and produces outputs. `accountHistory()` selects transactions that involve the account and orders them by time. `balanceAfter()` chooses the account's balance after each transaction, whether it was the sender or receiver. `renderAsciiBalanceChart()` converts those balance points into dated bars and caps the raw bar width at 40 characters. The menu formats the chart inside the terminal card."

> "The chart is deliberately read-only. Its bars are scaled against the largest balance in the selected history, so a bigger bar means a larger balance relative to the other points. The date and printed peso value remain alongside the bar because the visual length alone is only an aid, not the exact amount. An account with no transactions gets a readable empty-history message rather than an invented trend."

**Point to:** [`ReportGenerator.cpp:28`](../ReportGenerator.cpp#L28) (`accountHistory`), [`ReportGenerator.cpp:41`](../ReportGenerator.cpp#L41) (`balanceAfter`), [`ReportGenerator.cpp:109`](../ReportGenerator.cpp#L109) (`renderAsciiBalanceChart`), and [`MenuController.cpp:527`](../MenuController.cpp#L527) (`showChart`).

> "`exportStatement()` writes the account details, period, opening and closing balances, transactions, and credit and debit totals. `generateReceipt()` writes one transaction's reference, date, type, amount, and relevant account IDs and balances. `writeFile()` creates the output directory and reports write failures. In the menu, receipt export first shows this client's transactions in one-line rows so the user can copy a valid reference; the menu also checks that the chosen transaction belongs to the logged-in account."

**Presenter detail if asked:** A statement reconstructs its opening balance from the first ledger event and uses the account's stored current balance as its closing balance. A receipt selects one event by reference and includes affected account IDs and post-transaction balances. An unwritable path returns an error.

> "A statement explains an account over its recorded period; a receipt explains one ledger event. Both read the saved bank model without changing balances."

**Point to:** [`ReportGenerator.cpp:89`](../ReportGenerator.cpp#L89) (`writeFile`), [`ReportGenerator.cpp:142`](../ReportGenerator.cpp#L142) (`exportStatement`), [`ReportGenerator.cpp:195`](../ReportGenerator.cpp#L195) (`generateReceipt`), and [`MenuController.cpp:541`](../MenuController.cpp#L541) (`exportReport`).

**Member 4's GitHub walkthrough:** Open [`feature/ascii-charts-reports` PR #1](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/1) and explain its development before merge [`64a4d22`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/64a4d22).

| Commit | What changed in the PR | Where to point in the current code |
| --- | --- | --- |
| [`0fe981b`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/0fe981b) | Implemented chronological account history, bounded chart bars, account statements, transaction receipts, and report file output. | [`ReportGenerator.cpp:28`](../ReportGenerator.cpp#L28) `accountHistory`; [`ReportGenerator.cpp:109`](../ReportGenerator.cpp#L109) `renderAsciiBalanceChart`; [`ReportGenerator.cpp:142`](../ReportGenerator.cpp#L142) `exportStatement`; [`ReportGenerator.cpp:195`](../ReportGenerator.cpp#L195) `generateReceipt` |
| [`149f770`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/149f770) | Added an early verification pass for chart limits and export behavior; its temporary code was later removed. | Explain the history of the PR; no extra code file is delivered now. |
| [`e2de698`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/e2de698) | Added a cross-module verification checklist while other modules were being connected. | Historical documentation commit. |
| [`213b121`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/213b121) | Replaced the temporary code with written checks as the documentation approach changed. | Historical documentation commit. |
| [`353058e`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/353058e) | Removed that checklist; it is not a present-day deliverable. | Historical documentation commit. |
| [`227b3f5`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/227b3f5) | Synced the report branch with newer `main` before the PR merge. | Branch synchronization, not a new report feature. |
| [`9e97319`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/9e97319) | Applied automated formatting to `ReportGenerator.cpp`. | Current [`ReportGenerator.cpp`](../ReportGenerator.cpp) layout. |
| [`10a4300`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/10a4300) | Cleaned the report source before merging. | Current [`ReportGenerator.cpp`](../ReportGenerator.cpp) implementation. |

> "My first PR commit built the chart, statement, and receipt functions. The middle commits revised verification records; those temporary files are gone. I synced with `main`, formatted `ReportGenerator.cpp`, and cleaned it before merge. As leader, I then connected all members' pushed code, resolved cross-module gaps, and coordinated the final terminal UI."

### Member 4's final integration after the feature PRs

**Say in about 30–40 seconds:** "The four PRs supplied the modules, but a merged repository still needed a working end-to-end path. I led that final connection: checked where menu input met the engine, where saved IDs met report filenames, and where the UI displayed the resulting data. The following commits show the follow-up work; the shared-model correction in PR #7 was Member 1's separate integration PR."

| Commit | Concrete follow-up and where it appears now |
| --- | --- |
| [`c68a209`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/c68a209) | Aligned FX input units and report filenames with the agreed interface; see [`MenuController.cpp:456`](../MenuController.cpp#L456) `showFxQuote` and [`MenuController.cpp:541`](../MenuController.cpp#L541) `exportReport`. This came through the `fix/integration-contract-gaps` branch, merged at [`8da5bb1`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/8da5bb1). |
| [`53af3ab`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/53af3ab) | Connected persistent client profiles and account switching across model, engine, storage, and menu; see [`MenuController.cpp:349`](../MenuController.cpp#L349) and [`MenuController.cpp:377`](../MenuController.cpp#L377). |
| [`68fca72`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/68fca72) | Hid PIN entry in interactive Windows consoles; see [`InputValidator.cpp:20`](../InputValidator.cpp#L20) `HiddenInput`. |
| [`bf27794`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/bf27794) | Added one-command build and run through [`run.ps1:12`](../run.ps1#L12). |
| [`bdec326`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/bdec326), [`b3e23ad`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/b3e23ad) | Began the separate `feature/ascii-terminal-ui` branch with bordered/color screens, then made prompts and validation messages clearer; see [`MenuController.cpp:95`](../MenuController.cpp#L95) `frame` and [`InputValidator.cpp:219`](../InputValidator.cpp#L219) `getValidMoney`. |
| [`07e60f4`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/07e60f4), [`b0eb7c7`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/b0eb7c7) | Restored visible non-PIN input and cleared the terminal between screens; see [`InputValidator.cpp:188`](../InputValidator.cpp#L188) `prepareVisibleInput` and [`MenuController.cpp:52`](../MenuController.cpp#L52) `clearScreen`. |
| [`a86336e`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/a86336e), [`a110417`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/a110417) | Reordered the client menu by column, added welcome coin art, and added account-ID format guidance; see [`MenuController.cpp:240`](../MenuController.cpp#L240) `showClientMenu` and [`MenuController.cpp:696`](../MenuController.cpp#L696) `showWelcomeScreen`. |
| [`546e454`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/546e454) | Listed a client's transactions before receipt export so the reference can be copied; see [`MenuController.cpp:541`](../MenuController.cpp#L541) `exportReport`. |
| [`4c4b90c`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/4c4b90c) | Ignored generated `data/` and `reports/` directories in [`.gitignore:5`](../.gitignore#L5) and became the head when the UI branch was fast-forwarded into `main`. |

These are **post-PR integration and UI commits**, not extra commits inside Member 4's original reports PR. Show the table as a timeline; narrate the connection, prompt, and receipt-list highlights rather than reading every hash aloud.

**Merge recap while the commit graph is visible:** "The original feature PRs merged in the order [Member 1's #2](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/2), [Member 3's #3](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/3), [Member 2's #4](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/4), and [my #1](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/1). The PR number was not the merge order. [Integration PR #7](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/pull/7) followed. Later UI improvements came from `feature/ascii-terminal-ui` by a direct fast-forward into `main`, ending at [`4c4b90c`](https://github.com/VinJamesCastro/CS0053-TN31-GroupGinitaan-CaS/commit/4c4b90c)."

**Handoff:** "Those separate branches now connect in the single program we demonstrated at the start."

## 19:15–19:45 — Member 4: leader's outro

> "Can a user trust a completed transaction after the program closes? Our restart showed that the deposit remained. Member 1 created the repository and built menus and authentication. Member 2 enforced account rules. Member 3 validated input and saved the ledger. I built charts and reports and led the final connection of the pushed code. Together, these parts produce a durable, understandable banking record. Thank you."
