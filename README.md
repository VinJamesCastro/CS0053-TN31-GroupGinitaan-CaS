# Cents and Sensibility (CaS)

CaS is a C++17 terminal banking simulation made by Group Ginitaan for CS0053. It demonstrates account management, transaction processing, PIN authentication, local file storage, and role-based menus. It does not connect to a real bank or handle real money.

## Features

- Guest menu for login, client registration, About, and exit.
- Client menu for account summaries, deposits, withdrawals, transfers, FX quotes, filtered transaction history, balance charts, statements, and receipts.
- Client profiles can hold more than one account. Clients can open another account and switch between accounts in their profile.
- Admin menu for listing accounts, changing account status, and reading the audit log.
- State changes are written to the snapshot before the program reports success.
- PINs are four or six digits. The program preserves leading zeroes and hides PIN input in an interactive Windows console.
- Balances use integer minor units. Enter PHP amounts with up to two decimal places, such as `500.00`.

FX conversion uses fixed demonstration rates. Time deposit accounts cannot withdraw or transfer funds because the project does not define a maturity rule.

## Requirements

- Windows
- MSYS2 UCRT64 `g++` with C++17 support
- Windows CNG (`bcrypt`), used by the PIN hashing code
- PowerShell

## Build and run

From the repository folder, run:

```powershell
.\run.ps1
```

The script compiles the application and starts it. It uses `C:\msys64\ucrt64\bin\g++.exe` when available, or finds `g++` on `PATH`. To compile manually in PowerShell:

```powershell
g++ -std=c++17 -Wall -Wextra main.cpp MenuController.cpp TransactionEngine.cpp FileStorage.cpp InputValidator.cpp ReportGenerator.cpp Authentication.cpp -lbcrypt -o cas_app.exe
```

Then start the program with:

```powershell
.\cas_app.exe
```

## First launch

On a fresh installation, CaS asks you to create an administrator name and PIN. It saves the account before showing its ID, `ADM-000001`. There is no default administrator password or PIN.

After setup, use the guest menu to register a client account. The new account ID appears after the account is saved. Use that ID and the PIN you chose to log in.

## Local files

- `data/bank_state.txt` stores accounts and transactions.
- `data/audit.log` stores administrator account-status changes.
- `reports/Statement_<accountId>.txt` and `reports/Receipt_<transactionId>.txt` are created when reports are exported.

These files are created relative to the current working directory. `run.ps1` sets it to the repository folder. The generated files are ignored by Git. Keep a backup if you need to preserve local data.

## Project files

- `main.cpp`, `MenuController.*`, and `Authentication.*` handle startup, menus, sessions, and PIN authentication.
- `TransactionEngine.*` applies account and transaction rules.
- `FileStorage.*` and `InputValidator.*` handle local data, audit records, and input validation.
- `ReportGenerator.cpp` creates charts, statements, and receipts.
- `BankModel.h` contains the shared data types.

See [the project architecture and member roles](docs/cas_project_architecture_roles.md), [the shared development contract](docs/cas_shared_development_contract.md), and [the presentation script](docs/presentation_script.md) for more detail.
