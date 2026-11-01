# Member 4 integration checklist

Member 4 implements `ReportGenerator.cpp`. The agreed shared types and report function signatures are in [the development contract](cas_shared_development_contract.md). This checklist is for the group to run after the four modules are present.

## Local report verification

Record these as written test cases with the input, expected result, observed result, and pass/fail status. No test-code file is required.

- Compile `ReportGenerator.cpp` with C++17 and record whether the compiler reports errors or warnings.
- Check chronological chart rows, dates, bounded bars, zero or empty history, and very large balances.
- Check statement account details, period, opening and closing balances, transaction amounts, and credit/debit totals against the in-memory ledger.
- Check receipt reference, UTC date, amount, participant IDs, and resulting balances.
- Check missing IDs and unwritable output paths return failed results. Check that chart and exports leave `BankData` unchanged.

## Cross-module smoke test

1. Build the complete C++17 application. Resolve any mismatch between the C++ types and signatures used by the modules against the Markdown contract.
2. Register a client with an opening deposit, restart, log in, and confirm the saved account and opening transaction remain available.
3. Deposit, withdraw, and transfer. Verify each resulting chart point and statement row matches the ledger and stored balance. A transfer must appear once in each participant's statement with the correct direction and balance.
4. Filter transaction history and compare its results with the unfiltered statement for the same account.
5. Export a statement to `reports/Statement_<accountId>.txt` and a receipt to `reports/Receipt_<transactionId>.txt`. Check directory creation, file contents, and readable error messages for failed writes.
6. Confirm a client can request reports only for their authenticated account; Member 1 enforces this scope before calling Member 4.
7. Change an account's status as admin, inspect the audit log, and confirm rejected banking operations leave the saved snapshot and subsequent reports unchanged.

Report defects to the owner of the affected module. Member 4 fixes report formatting and lookup defects in its own branch or a reviewed follow-up.
