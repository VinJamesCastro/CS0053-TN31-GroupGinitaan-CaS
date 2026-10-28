# Cents and Sensibility (CaS) — Technical Architecture & Group Member Roles

**Tagline:** *Cents and Sensibility: Proof that smart banking makes good sense—down to the last cent.*

---

## Group Member Roles & Technical Synchronization

### Member 1: Repository Coordinator & Menu/Integration Lead
- **Primary Scope:** Repository setup, state-driven CLI menu system, navigation routing, and user authentication.
- **Assigned Features:**
  - *State-Driven Modular CLI Navigation*
  - *Multi-Tier User Authentication & Role-Based Access Control (RBAC)*
- **Git Branch:** `feature/cli-menu-auth`
- **Cyclic Reviewer Role:** Assigned Reviewer for **Member 2**.

#### CLI Menu Architecture & Exact Callback Mappings:
1. **Guest Menu (Unauthenticated State):**
   - `[1] Login`: Prompts credentials via Member 3's `getValidPIN()`, then calls Member 3's `validateCredentials(accountID, inputPIN)`.
   - `[2] Register Account`: Collects info via Member 3's `getValidPIN()` and `getValidDouble()`, then calls Member 2's `createAccount(holderName, accountType, initialDeposit)`, and persists it via Member 3's `saveAccountRecord(newAccount)`.
   - `[3] About CaS`: Displays bank summary and tagline.
   - `[4] Exit`: Terminates main execution loop.
2. **Client Dashboard Menu (Post-Client Login):**
   - `[1] Account Summary`: Calls Member 2's `getAccountSummary(accountID)`.
   - `[2] Deposit Funds`: Prompts amount via Member 3's `getValidDouble()`, then calls Member 2's `deposit(accountID, amount)`.
   - `[3] Withdraw Funds`: Prompts amount via Member 3's `getValidDouble()`, then calls Member 2's `withdraw(accountID, amount)`.
   - `[4] Peer-to-Peer Transfer`: Prompts recipient & amount, then calls Member 2's `transfer(senderID, receiverID, amount)`.
   - `[5] FX Currency Converter`: Calls Member 2's `convertCurrency(amount, fromCurr, toCurr)`.
   - `[6] Filter Transaction History`: Calls Member 3's `displayFilteredTransactions(accountID, startDate, endDate, type, minAmt, maxAmt)`.
   - `[7] View Growth Chart`: Calls Member 4's `renderASCIIBalanceChart(accountID)`.
   - `[8] Export Statement / Receipt`: Calls Member 4's `exportStatement(accountID)` or `generateReceipt(transactionID)`.
   - `[9] Logout`: Resets session state to `SystemState::GUEST`.
3. **Admin Dashboard Menu (Post-Admin Login):**
   - `[1] View All Accounts`: Calls Member 3's `loadAllAccounts()`.
   - `[2] Change Account Status`: Calls Member 2's `updateAccountStatus(accountID, newStatus)` and Member 3's `saveAllAccounts()`.
   - `[3] System Audit Logs`: Calls Member 3's `readAuditLogs()`.
   - `[4] Logout`: Resets session state to `SystemState::GUEST`.

#### Defense & Quality Requirements:
- **Individual Debugging Scenario:** Debugging state-loop stuck transitions (e.g., infinite re-entry into `CLIENT_MENU` on invalid input). Sets a breakpoint inside `handleClientMenu()` using VS Code / GDB to inspect `currentState` and clear `cin` flags.
- **Refactoring & Formatting (`clang-format`) Evidence:** Captures before and after screenshots running `clang-format` on `MenuController.cpp` after refactoring monolithic `switch-case` blocks into modular handler functions.

---

### Member 2: Core Calculations & Business Logic Lead
- **Primary Scope:** Core financial transaction engine, account balance algorithms, and multi-currency mathematical operations.
- **Assigned Features:**
  - *Transaction Engine (Deposit, Withdrawal, Peer-to-Peer Transfer)*
  - *Multi-Account Type Management*
  - *Foreign Exchange (FX) & Multi-Currency Converter*
- **Git Branch:** `feature/transaction-engine`
- **Cyclic Reviewer Role:** Assigned Reviewer for **Member 3**.

#### Exact Function Implementations (Called by Member 1):
- `AccountSummary getAccountSummary(string accountID)`: Calculates balance totals, accrued interest, and active status for checking, savings, and time deposits.
- `bool deposit(string accountID, double amount)`: Adds funds to `accountID`, ensuring `amount > 0`.
- `bool withdraw(string accountID, double amount)`: Checks `(currentBalance - amount) >= minBalanceRequirement`. Rejects overdraws.
- `bool transfer(string senderID, string receiverID, double amount)`: Deducts from sender and credits receiver atomically; rolls back if receiver ID is invalid or sender balance is insufficient.
- `bool createAccount(string holderName, string accountType, double initialDeposit)`: Initializes new account objects with interest rates and minimum balance constraints.
- `double convertCurrency(double amount, string fromCurr, string toCurr)`: Calculates currency equivalents across PHP, USD, EUR, and JPY using exchange rates.
- `bool updateAccountStatus(string accountID, string newStatus)`: Changes account state between `Active`, `Frozen`, or `Closed`.

#### Defense & Quality Requirements:
- **Individual Debugging Scenario:** Debugging floating-point rounding errors or precision loss during compound interest calculations in `getAccountSummary()`. Sets breakpoints in VS Code / GDB to inspect `accruedInterest` and `double` precision variables.
- **Refactoring & Formatting (`clang-format`) Evidence:** Captures before and after screenshots running `clang-format` on `TransactionEngine.cpp` after replacing hardcoded magic numbers with constant parameters (`constexpr double MIN_CHECKING_BALANCE = 500.0;`).

---

### Member 3: File Storage, Records & Input Validation Lead
- **Primary Scope:** Persistent flat-file storage, serialization/parsing, input sanitization, and transaction filtering.
- **Assigned Features:**
  - *Persistent File Storage & Serialization*
  - *Defensive Input Validation & Sanitization Engine*
  - *Searchable Transaction History & Query Filter*
- **Git Branch:** `feature/file-storage-validation`
- **Cyclic Reviewer Role:** Assigned Reviewer for **Member 4**.

#### Exact Function Implementations (Called by Member 1):
- `int getValidInt(string prompt, int min, int max)`: Traps non-numeric inputs and range violations using `cin.fail()`, `cin.clear()`, and `cin.ignore()`.
- `double getValidDouble(string prompt, double minAmount)`: Validates double/float monetary inputs.
- `string getValidPIN(string prompt)`: Validates 4-digit or 6-digit numeric string formats.
- `bool validateCredentials(string accountID, string inputPIN)`: Reads `accounts.csv` and checks password hash matches.
- `bool saveAccountRecord(Account acc)`: Appends a newly registered account to `accounts.csv`.
- `bool saveAllAccounts()`: Overwrites `accounts.csv` with updated account states and balances.
- `vector<Account> loadAllAccounts()`: Parses `accounts.csv` into memory during startup or admin views.
- `void displayFilteredTransactions(string accountID, string startDate, string endDate, string type, double minAmt, double maxAmt)`: Reads `transactions.csv` and outputs matching entries.
- `void readAuditLogs()`: Parses `audit.log` for administrative reviews.

#### Defense & Quality Requirements:
- **Individual Debugging Scenario:** Debugging CSV parsing errors in `loadAllAccounts()` caused by trailing newlines or delimiter mismatches. Sets breakpoints inside the `stringstream` loop to inspect token extraction.
- **Refactoring & Formatting (`clang-format`) Evidence:** Captures before and after screenshots running `clang-format` on `FileStorage.cpp` and `InputValidator.cpp` after renaming cryptic variables to clean, descriptive naming conventions.

---

### Member 4: Additional Features, Quality Assurance & Testing Lead
- **Primary Scope:** Console visual outputs, exportable documentation generators, debugger investigations, static analysis, and group test suite coordination.
- **Assigned Features:**
  - *ASCII-Based Financial Data Visualization*
  - *Printable Account Statement & Receipt Generator*
- **Git Branch:** `feature/ascii-charts-reports`
- **Cyclic Reviewer Role:** Assigned Reviewer for **Member 1**.

#### Exact Function Implementations (Called by Member 1):
- `void renderASCIIBalanceChart(string accountID)`: Pulls balance history from Member 3's transactions ledger, calculates relative scale factors, and prints horizontal ASCII bar charts directly in the terminal.
- `bool exportStatement(string accountID)`: Builds a formal account summary statement and saves it as `Statement_<accountID>.txt`.
- `bool generateReceipt(string transactionID)`: Formats an individual transaction receipt and exports it as `Receipt_<transactionID>.txt`.

#### Defense & Quality Requirements:
- **Individual Debugging Scenario:** Debugging character alignment and buffer overflow issues in `renderASCIIBalanceChart()` when balance values exceed maximum bar graph scaling thresholds. Sets breakpoints to inspect `scaleFactor` and `barLength` calculations.
- **Refactoring & Formatting (`clang-format`) Evidence:** Captures before and after screenshots running `clang-format` on `ReportGenerator.cpp` after organizing stream output formatting and standardizing header borders.