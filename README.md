# Cents and Sensibility (CaS)

CaS is a C++17 terminal banking simulation made by Group Ginitaan for CS0053. It demonstrates account management, transaction processing, PIN authentication, local file storage, and role based menus. It does not connect to a real bank or handle real money.

## Features

- Guest menu for login, client registration, About, and exit.
- Client menu for account summaries, deposits, withdrawals, transfers, FX quotes, filtered transaction history, balance charts, statements, and receipts.
- Multiple accounts per client profile, with account opening and account switching.
- Admin menu for listing accounts, changing account status, and reading the audit log.
- Persistent account and transaction snapshots, plus an audit log for administrator status changes.
- Four or six digit PINs, preserving leading zeroes; balances use integer minor units and accept PHP amounts with up to two decimal places.
- Fixed demonstration FX rates. Time deposit accounts cannot withdraw or transfer funds because the project does not define a maturity rule.

## Requirements

- Windows 10 or later
- MSYS2 UCRT64 `g++` with C++17 support, and PowerShell
- Windows CNG (`bcrypt`), used by the PIN hashing code

On Linux or macOS, install a C++17 capable `g++` and OpenSSL development headers and library (`libcrypto`).

## Compile and run

From the repository directory on Windows, run these exact commands in PowerShell (using MSYS2 UCRT64 `g++`):

```powershell
g++ -std=c++17 -Wall -Wextra main.cpp MenuController.cpp TransactionEngine.cpp FileStorage.cpp InputValidator.cpp ReportGenerator.cpp Authentication.cpp -lbcrypt -o cas_app.exe
.\cas_app.exe
```

On Linux or macOS, compile and launch from the repository directory with: On a fresh installation, create the administrator name and a four or six digit PIN when prompted. CaS saves the administrator before showing its ID, `ADM-000001`; there is no default administrator credential. From the guest menu, register a client before logging in as one.

## Sample interaction

The exact colors and screen formatting depend on the terminal. On first launch, the prompts and responses follow this pattern (the continue prompt is omitted here):

```text
First run: create an administrator account to secure this installation.

Admin name
> Casey
New admin PIN (four or six digits): ****
Administrator account saved. Account ID: ADM-000001
```

After setup, the guest menu offers login, client registration, About, and exit. Choose client registration and enter a name, PIN, and initial deposit; the program saves the account and displays its generated account ID. Amount inputs use PHP with up to two decimal places, for example `500.00`.

## Local files

- `data/bank_state.txt` stores accounts and transactions.
- `data/audit.log` stores administrator account status changes.
- `reports/Statement_<accountId>.txt` and `reports/Receipt_<transactionId>.txt` are created when reports are exported.

These paths are relative to the repository directory. Keep a backup if you need to preserve locally generated data.

## Project files

- `main.cpp`, `MenuController.*`, and `Authentication.*` handle startup, menus, sessions, and PIN authentication.
- `TransactionEngine.*` applies account and transaction rules.
- `FileStorage.*` and `InputValidator.*` handle local data, audit records, and input validation.
- `ReportGenerator.cpp` creates charts, statements, and receipts.
- `BankModel.h` contains the shared data types.

See [the project architecture and member roles](docs/cas_project_architecture_roles.md), [the shared development contract](docs/cas_shared_development_contract.md), and [the presentation script](docs/presentation_script.md) for more detail.
