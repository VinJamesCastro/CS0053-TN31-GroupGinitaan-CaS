# Cents and Sensibility (CaS) — Project README

**Tagline:** *Cents and Sensibility: Proof that smart banking makes good sense—down to the last cent.*

---

## 1. General Description & Project Overview

### General Description
**Cents and Sensibility (CaS)** is a robust, terminal-based banking application developed in C++ designed with a clean software architecture, modular design, and persistent record management. Balancing financial precision ("Cents") with bulletproof execution, defensive programming, and user-centric safety ("Sensibility"), CaS provides users with a complete banking experience—ranging from basic ledger operations to automated financial analytics and account governance—all within an intuitive CLI environment.

### Project Overview
While traditional banking institutions often bog down customers with bloated interfaces, hidden fees, and bureaucratic delays, **Cents and Sensibility (CaS)** offers a refreshing, streamlined alternative that puts clarity, speed, and safety first. Built on the principle that modern finance should be both mathematically precise and user-centric, CaS delivers a complete banking experience without the noise. Clients can seamlessly oversee multiple account types—from standard checking to high-yield savings and time deposits—all under a single unified profile. Whether performing real-time deposits, withdrawals, or instant peer-to-peer transfers, users operate within a protected ecosystem backed by multi-tier authentication and strict role-based access controls for both clients and administrators.

Beyond basic ledger operations, **Cents and Sensibility** equips users with clear financial visibility that leaves conventional bank records behind. Account holders can query and filter their transaction history by date or amount, analyze monthly balance fluctuations using visual terminal charts, and calculate multi-currency conversions across PHP, USD, EUR, and JPY. When official documentation is required, the system instantly formats clean, printable account statements and receipts. By combining bulletproof input safety with automated financial tools, **Cents and Sensibility** proves that managing your wealth doesn't require endless red tape—just smart tools, total transparency, and a little common sense.

---

## 2. Features Summary

### Core Features Overview
The core functionality of **Cents and Sensibility (CaS)** forms the resilient foundation of the entire banking application, engineered to handle essential financial operations with unyielding stability and precision. Powered by a modular, state-driven CLI menu system, CaS provides a seamless interface where users can execute real-time deposits, withdrawals, and peer-to-peer transfers through an atomic transaction engine. To guarantee absolute data integrity, all account balances, credentials, and ledger entries are continuously serialized and saved to local file storage across application restarts. Surrounding these capabilities is a centralized defensive input validation system that proactively filters bad inputs and prevents program crashes, alongside a searchable transaction ledger that gives users complete control and visibility over their past financial activities.

### Additional Features Overview
Beyond standard ledger operations, the additional features of **Cents and Sensibility (CaS)** elevate the application into an advanced financial dashboard tailored for both clients and administrators. System security is reinforced through multi-tier authentication and role-based access control, allowing customers to manage diverse account types—including checking, savings, and time deposits—while unlocking elevated system controls for administrators. CaS further enriches the user experience by turning terminal output into visual insights through ASCII-based financial trend charts and real-time multi-currency conversion across PHP, USD, EUR, and JPY. Finally, an automated statement and receipt generator allows users to instantly export formatted text documents of their account summaries for offline proof and record-keeping.

---

## 3. Cyclic PR Review Chain Workflow

To satisfy course collaboration requirements, maintain peer accountability, and enforce code quality before merging into `main`, the group follows a **Cyclic Pull Request (PR) Review Workflow**:

```
[Member 1]  ---> (Reviews PRs from) --->  [Member 2]
    ^                                         |
    |                                         v
[Member 4]  <--- (Reviews PRs from) <---  [Member 3]
```

- **Member 1** reviews and approves **Member 2's** PRs
- **Member 2** reviews and approves **Member 3's** PRs
- **Member 3** reviews and approves **Member 4's** PRs
- **Member 4** reviews and approves **Member 1's** PRs

---

---

## 4. Prerequisites & Build Instructions

### Required Tools
- **Compiler:** `g++` (C++17 or later)
- **Editor:** VS Code with C/C++ Extensions
- **Quality Tools:** `clang-format` and `clang-tidy` / `Cppcheck`
- **Version Control:** Git & GitHub

### Compilation Instructions

```bash
# Compile using G++
g++ -std=c++17 -Wall main.cpp MenuController.cpp TransactionEngine.cpp FileStorage.cpp InputValidator.cpp ReportGenerator.cpp -o cas_app

# Run application
./cas_app
```

---

## 5. Sample CLI Output

```text
+--------------------------------------------------+
|           CENTS AND SENSIBILITY (CaS)            |
|       Smart banking that makes good sense.       |
+--------------------------------------------------+
|  1. Login                                        |
|  2. Register Account                             |
|  3. About CaS                                    |
|  4. Exit                                         |
+--------------------------------------------------+
  Select an option [1-4]: 1

  Account ID: ACC-000001
  PIN: ****

  Login successful. Welcome, Client!
```
