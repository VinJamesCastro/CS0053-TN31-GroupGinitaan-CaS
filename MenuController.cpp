#include "Authentication.h"
#include "MenuController.h"
#include "TeamInterfaces.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

using namespace std;

namespace cas {
namespace {

enum class SessionRole { Guest, Client, Admin };

struct Session {
    SessionRole role = SessionRole::Guest;
    string accountId;
};

void clearSession(Session& session) {
    session.role = SessionRole::Guest;
    session.accountId.clear();
}

const char* accountTypeName(AccountType type) {
    switch (type) {
        case AccountType::Checking: return "Checking";
        case AccountType::Savings: return "Savings";
        case AccountType::TimeDeposit: return "Time deposit";
    }
    return "Unknown";
}

const char* statusName(AccountStatus status) {
    switch (status) {
        case AccountStatus::Active: return "Active";
        case AccountStatus::Frozen: return "Frozen";
        case AccountStatus::Closed: return "Closed";
    }
    return "Unknown";
}

const char* transactionTypeName(TransactionType type) {
    switch (type) {
        case TransactionType::OpeningDeposit: return "Opening deposit";
        case TransactionType::Deposit: return "Deposit";
        case TransactionType::Withdrawal: return "Withdrawal";
        case TransactionType::Transfer: return "Transfer";
    }
    return "Unknown";
}

void showGuestMenu() {
    cout << "\n=== Cents and Sensibility ===\n"
         << "1. Login\n2. Register account\n3. About\n4. Exit\n"
         << "Choose an option: ";
}

void showClientMenu() {
    cout << "\n=== Client Menu ===\n"
         << "1. Account summary\n2. Deposit\n3. Withdraw\n4. Transfer\n"
         << "5. FX quote\n6. Transaction history\n7. Balance chart\n"
         << "8. Export statement or receipt\n9. Logout\n"
         << "Choose an option: ";
}

void showAdminMenu() {
    cout << "\n=== Admin Menu ===\n"
         << "1. List accounts\n2. Change account status\n"
         << "3. Read audit log\n4. Logout\n"
         << "Choose an option: ";
}

template <typename Change>
OperationResult commitChange(BankData& data, const filesystem::path& path,
                             Change change) {
    BankData candidate = data;
    OperationResult result = change(candidate);
    if (!result.ok) return result;
    const SaveResult saved = saveSnapshot(path, candidate);
    if (!saved.ok) return {false, "Save failed: " + saved.message, ""};
    data = move(candidate);
    return result;
}

void showOperation(const OperationResult& result) {
    cout << result.message << '\n';
    if (result.ok && !result.referenceId.empty())
        cout << "Reference: " << result.referenceId << '\n';
}

bool validDate(const string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (size_t i = 0; i < date.size(); ++i) {
        if (i != 4 && i != 7 && !isdigit(static_cast<unsigned char>(date[i])))
            return false;
    }
    const int year = stoi(date.substr(0, 4));
    const int month = stoi(date.substr(5, 2));
    const int day = stoi(date.substr(8, 2));
    if (year < 1 || month < 1 || month > 12) return false;
    const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day >= 1 && day <= daysInMonth[month - 1] + (month == 2 && leap);
}

bool readOptionalDate(const string& prompt, optional<string>& result) {
    string value;
    cout << prompt;
    if (!getline(cin, value)) return false;
    if (value.empty()) return true;
    if (!validDate(value)) {
        cout << "Enter a real date as YYYY-MM-DD.\n";
        return false;
    }
    result = value;
    return true;
}

bool validReference(const string& value, const string& prefix) {
    return value.size() >= prefix.size() + 6 &&
           value.compare(0, prefix.size(), prefix) == 0 &&
           all_of(value.begin() + prefix.size(), value.end(),
                  [](unsigned char digit) { return isdigit(digit); });
}

string utcNow() {
    const time_t now = time(nullptr);
    tm utc{};
    if (gmtime_s(&utc, &now) != 0) return "";
    ostringstream output;
    output << put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

void registerAccount(BankData& data, const filesystem::path& path) {
    string name;
    cout << "Holder name: ";
    if (!getline(cin, name)) return;
    if (name.find_first_not_of(" \t") == string::npos) {
        cout << "Holder name is required.\n";
        return;
    }
    const int product = getValidInt("Product (1 Checking, 2 Savings, 3 Time deposit): ", 1, 3);
    if (!cin) return;
    const string pin = getValidPin("New PIN (four or six digits): ");
    if (!cin) return;
    const Money opening = getValidMoney("Opening deposit in PHP (e.g. 500.00): ", 0);
    if (!cin) return;

    const AccountType type = static_cast<AccountType>(product - 1);
    const string digest = digestPin(pin);
    const OperationResult result = commitChange(data, path, [&](BankData& candidate) {
        OperationResult created = createAccount(candidate, name, type, digest, opening);
        if (created.ok && created.referenceId.empty())
            return OperationResult{false, "Account creation returned no ID.", ""};
        return created;
    });
    showOperation(result); // A new ID appears only after the snapshot was saved.
}

void showSummary(const BankData& data, const Session& session) {
    const AccountSummary summary = getAccountSummary(data, session.accountId);
    if (!summary.found) {
        cout << "Account summary unavailable.\n";
        return;
    }
    cout << "Account: " << summary.accountId << '\n'
         << "Product: " << accountTypeName(summary.type) << '\n'
         << "Status: " << statusName(summary.status) << '\n'
         << "Balance (centavos): " << summary.balanceCents << '\n'
         << "Minimum balance (centavos): " << summary.minimumBalanceCents << '\n'
         << "Annual interest (basis points): " << summary.annualInterestBps << '\n';
}

void changeBalance(BankData& data, const Session& session,
                   const filesystem::path& path, bool isDeposit) {
    const Money amount = getValidMoney("Amount in PHP (e.g. 10.50): ", 1);
    if (!cin) return;
    const OperationResult result = commitChange(data, path, [&](BankData& candidate) {
        return isDeposit ? deposit(candidate, session.accountId, amount)
                         : withdraw(candidate, session.accountId, amount);
    });
    showOperation(result);
}

void transferFunds(BankData& data, const Session& session,
                   const filesystem::path& path) {
    string receiverId;
    cout << "Receiver account ID: ";
    if (!getline(cin, receiverId)) return;
    const Money amount = getValidMoney("Amount in PHP (e.g. 10.50): ", 1);
    if (!cin) return;
    const OperationResult result = commitChange(data, path, [&](BankData& candidate) {
        return transfer(candidate, session.accountId, receiverId, amount);
    });
    showOperation(result);
}

void showFxQuote() {
    const int from = getValidInt("From (1 PHP, 2 USD, 3 EUR, 4 JPY): ", 1, 4);
    if (!cin) return;
    const int to = getValidInt("To (1 PHP, 2 USD, 3 EUR, 4 JPY): ", 1, 4);
    if (!cin) return;
    Money amount;
    while (true) {
        amount = getValidMoney("Amount in source currency (e.g. 1.00; whole units for JPY): ",
                               from == 4 ? 100 : 1);
        if (!cin) return;
        if (from != 4 || amount % 100 == 0) break;
        cout << "JPY amounts must be whole units.\n";
    }
    const Money inputMinorUnits = from == 4 ? amount / 100 : amount;
    const FxQuote quote = convertCurrency(inputMinorUnits, static_cast<Currency>(from - 1),
                                          static_cast<Currency>(to - 1));
    if (!quote.ok) {
        cout << "FX quote unavailable.\n";
        return;
    }
    cout << "Estimated output minor units: " << quote.outputMinorUnits << '\n'
         << "Rate: " << quote.rateDescription << '\n';
}

void showHistory(const BankData& data, const Session& session) {
    TransactionFilter filter;
    filter.accountId = session.accountId;
    if (!readOptionalDate("Start date (YYYY-MM-DD or blank): ", filter.startDate) ||
        !readOptionalDate("End date (YYYY-MM-DD or blank): ", filter.endDate)) return;
    if (filter.startDate && filter.endDate && *filter.startDate > *filter.endDate) {
        cout << "Start date must not follow end date.\n";
        return;
    }

    const int type = getValidInt("Type (0 All, 1 Opening, 2 Deposit, 3 Withdrawal, 4 Transfer): ", 0, 4);
    if (!cin) return;
    if (type != 0) filter.type = static_cast<TransactionType>(type - 1);
    const int useMinimum = getValidInt("Minimum amount filter? (0 No, 1 Yes): ", 0, 1);
    if (!cin) return;
    if (useMinimum)
        filter.minAmountCents = getValidMoney("Minimum amount in PHP: ", 0);
    if (!cin) return;
    const int useMaximum = getValidInt("Maximum amount filter? (0 No, 1 Yes): ", 0, 1);
    if (!cin) return;
    if (useMaximum)
        filter.maxAmountCents = getValidMoney("Maximum amount in PHP: ", 0);
    if (!cin) return;
    if (filter.minAmountCents && filter.maxAmountCents &&
        *filter.minAmountCents > *filter.maxAmountCents) {
        cout << "Minimum amount must not exceed maximum amount.\n";
        return;
    }

    const vector<Transaction> rows = filterTransactions(data, filter);
    if (rows.empty()) {
        cout << "No matching transactions.\n";
        return;
    }
    for (const Transaction& row : rows) {
        cout << row.occurredAt << "  " << row.transactionId << "  "
             << transactionTypeName(row.type) << "  " << row.amountCents
             << " centavos  " << row.fromAccountId << " -> "
             << row.toAccountId << '\n';
    }
}

void showChart(const BankData& data, const Session& session) {
    const ReportResult result = renderAsciiBalanceChart(data, session.accountId, cout);
    if (!result.ok) cout << result.message << '\n';
}

void exportReport(const BankData& data, const Session& session) {
    const int kind = getValidInt("Export (1 Statement, 2 Receipt): ", 1, 2);
    if (!cin) return;
    if (kind == 1) {
        if (!validReference(session.accountId, "ACC-")) {
            cout << "Account ID is not valid for a statement filename.\n";
            return;
        }
        const filesystem::path path = filesystem::path("reports") /
            ("Statement_" + session.accountId + ".txt");
        const ReportResult result = exportStatement(data, session.accountId, path);
        cout << result.message << '\n';
        if (result.ok) cout << "Saved to " << path.string() << '\n';
        return;
    }

    string reference;
    cout << "Transaction ID: ";
    if (!getline(cin, reference)) return;
    const auto owned = find_if(data.transactions.begin(), data.transactions.end(),
        [&](const Transaction& row) {
            return row.transactionId == reference &&
                   (row.fromAccountId == session.accountId ||
                    row.toAccountId == session.accountId);
        });
    if (owned == data.transactions.end() || !validReference(reference, "TXN-")) {
        cout << "Receipt not found for your account.\n";
        return;
    }
    const filesystem::path path = filesystem::path("reports") /
        ("Receipt_" + reference + ".txt");
    const ReportResult result = generateReceipt(data, reference, path);
    cout << result.message << '\n';
    if (result.ok) cout << "Saved to " << path.string() << '\n';
}

void listAccounts(const BankData& data) {
    if (data.accounts.empty()) cout << "No accounts.\n";
    for (const Account& account : data.accounts) {
        cout << account.accountId << "  " << account.holderName << "  "
             << (account.role == Role::Admin ? "Admin" : "Client") << "  "
             << statusName(account.status) << '\n';
    }
}

void changeStatus(BankData& data, const Session& session,
                  const filesystem::path& path) {
    string targetId;
    cout << "Target account ID: ";
    if (!getline(cin, targetId)) return;
    const auto target = find_if(data.accounts.begin(), data.accounts.end(),
        [&](const Account& account) { return account.accountId == targetId; });
    if (target == data.accounts.end()) {
        cout << "Account not found.\n";
        return;
    }
    const AccountStatus previous = target->status;
    const int selected = getValidInt("New status (1 Active, 2 Frozen, 3 Closed): ", 1, 3);
    if (!cin) return;
    const AccountStatus next = static_cast<AccountStatus>(selected - 1);
    const OperationResult result = commitChange(data, path, [&](BankData& candidate) {
        return updateAccountStatus(candidate, targetId, next);
    });
    showOperation(result);
    if (!result.ok) return;

    const string when = utcNow();
    if (when.empty()) {
        cout << "Status was saved, but the audit timestamp was unavailable.\n";
        return;
    }
    const AuditEntry entry{when, session.accountId, targetId, previous, next};
    const SaveResult audit = appendAuditLog("data/audit.log", entry);
    if (!audit.ok)
        cout << "Status was saved, but audit logging failed: " << audit.message << '\n';
}

void showAuditLog() {
    const vector<AuditEntry> entries = readAuditLogs("data/audit.log");
    if (entries.empty()) cout << "No audit entries.\n";
    for (const AuditEntry& entry : entries) {
        cout << entry.occurredAt << "  " << entry.adminAccountId << "  "
             << entry.targetAccountId << "  " << statusName(entry.previousStatus)
             << " -> " << statusName(entry.nextStatus) << '\n';
    }
}

void runClientMenu(BankData& data, Session& session, const filesystem::path& path) {
    string choice;
    while (cin && session.role == SessionRole::Client) {
        showClientMenu();
        if (!getline(cin, choice)) return;
        if (choice == "1") showSummary(data, session);
        else if (choice == "2") changeBalance(data, session, path, true);
        else if (choice == "3") changeBalance(data, session, path, false);
        else if (choice == "4") transferFunds(data, session, path);
        else if (choice == "5") showFxQuote();
        else if (choice == "6") showHistory(data, session);
        else if (choice == "7") showChart(data, session);
        else if (choice == "8") exportReport(data, session);
        else if (choice == "9") {
            clearSession(session);
            cout << "Logged out.\n";
        } else cout << "Invalid choice. Please enter 1 to 9.\n";
    }
}

void runAdminMenu(BankData& data, Session& session, const filesystem::path& path) {
    string choice;
    while (cin && session.role == SessionRole::Admin) {
        showAdminMenu();
        if (!getline(cin, choice)) return;
        if (choice == "1") listAccounts(data);
        else if (choice == "2") changeStatus(data, session, path);
        else if (choice == "3") showAuditLog();
        else if (choice == "4") {
            clearSession(session);
            cout << "Logged out.\n";
        } else cout << "Invalid choice. Please enter 1 to 4.\n";
    }
}

} // namespace

void runMenu(BankData& data, const filesystem::path& snapshotPath) {
    Session session;
    string choice;
    while (cin) {
        if (session.role == SessionRole::Client) {
            runClientMenu(data, session, snapshotPath);
            continue;
        }
        if (session.role == SessionRole::Admin) {
            runAdminMenu(data, session, snapshotPath);
            continue;
        }

        showGuestMenu();
        if (!getline(cin, choice)) {
            cout << "\nInput closed. Goodbye!\n";
            return;
        }
        if (choice == "1") {
            string accountId;
            cout << "Account ID: ";
            if (!getline(cin, accountId)) return;
            const string pin = getValidPin("PIN (four or six digits): ");
            if (!cin) return;
            const LoginResult result = login(data, accountId, pin);
            if (!result.ok) {
                cout << result.message << '\n';
                continue;
            }
            session.accountId = result.accountId;
            session.role = result.role == Role::Admin ? SessionRole::Admin
                                                      : SessionRole::Client;
            cout << "Logged in as " << session.accountId << ".\n";
        } else if (choice == "2") registerAccount(data, snapshotPath);
        else if (choice == "3")
            cout << "CaS is a C++ banking simulation created by Group Ginitaan.\n";
        else if (choice == "4") {
            cout << "Goodbye!\n";
            return;
        } else cout << "Invalid choice. Please enter 1, 2, 3, or 4.\n";
    }
}

} // namespace cas
