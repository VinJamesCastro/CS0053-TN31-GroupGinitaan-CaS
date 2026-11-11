#include "Authentication.h"
#include "InputValidator.h"
#include "MenuController.h"
#include "TeamInterfaces.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace std;

namespace cas {
namespace {

constexpr size_t frameWidth = 78;
constexpr size_t contentWidth = frameWidth - 4;
bool colorEnabled = false;

const char* toneColor(UiTone tone) {
    switch (tone) {
        case UiTone::Info: return "\x1b[36m";
        case UiTone::Success: return "\x1b[32m";
        case UiTone::Warning: return "\x1b[33m";
        case UiTone::Error: return "\x1b[31m";
    }
    return "";
}

void setColor(UiTone tone) {
    if (colorEnabled) cout << toneColor(tone);
}

void resetColor() {
    if (colorEnabled) cout << "\x1b[0m" << flush;
}

string fitted(const string& value, size_t width) {
    if (value.size() <= width) return value + string(width - value.size(), ' ');
    if (width <= 3) return value.substr(0, width);
    return value.substr(0, width - 3) + "...";
}

vector<string> wrapLine(const string& text) {
    vector<string> lines;
    if (text.empty()) return {""};
    size_t start = 0;
    while (start < text.size()) {
        if (text.size() - start <= contentWidth) {
            lines.push_back(text.substr(start));
            break;
        }
        size_t end = text.rfind(' ', start + contentWidth);
        if (end == string::npos || end <= start) end = start + contentWidth;
        lines.push_back(text.substr(start, end - start));
        start = end;
        while (start < text.size() && text[start] == ' ') ++start;
    }
    return lines;
}

void frame(const string& title, const vector<string>& lines, UiTone tone = UiTone::Info) {
    const string border = "+" + string(frameWidth - 2, '-') + "+";
    cout << '\n';
    setColor(tone);
    cout << border << '\n' << "| " << fitted(title, contentWidth) << " |\n" << border << '\n';
    resetColor();
    if (tone != UiTone::Info) setColor(tone);
    for (const string& line : lines) {
        for (const string& wrapped : wrapLine(line))
            cout << "| " << fitted(wrapped, contentWidth) << " |\n";
    }
    if (tone != UiTone::Info) resetColor();
    setColor(tone);
    cout << border << '\n';
    resetColor();
}

string tableRow(const vector<string>& cells, const vector<size_t>& widths) {
    string result;
    for (size_t index = 0; index < widths.size(); ++index) {
        if (index != 0) result += " | ";
        result += fitted(index < cells.size() ? cells[index] : "", widths[index]);
    }
    return result;
}

void table(const string& title, const vector<string>& headings,
           const vector<vector<string>>& rows, const vector<size_t>& widths) {
    vector<string> lines{tableRow(headings, widths), string(contentWidth, '-')};
    if (rows.empty()) lines.push_back("No records to display.");
    for (const auto& row : rows) lines.push_back(tableRow(row, widths));
    frame(title, lines);
}

string moneyText(Money cents) {
    const bool negative = cents < 0;
    const auto magnitude = negative ? static_cast<uint64_t>(-(cents + 1)) + 1
                                    : static_cast<uint64_t>(cents);
    ostringstream out;
    if (negative) out << '-';
    out << "PHP " << magnitude / 100 << '.' << setw(2) << setfill('0') << magnitude % 100;
    return out.str();
}

string currencyText(Currency currency, Money minorUnits) {
    const char* code = "PHP";
    switch (currency) {
        case Currency::PHP: code = "PHP"; break;
        case Currency::USD: code = "USD"; break;
        case Currency::EUR: code = "EUR"; break;
        case Currency::JPY: code = "JPY"; break;
    }
    if (currency == Currency::JPY) return string(code) + " " + to_string(minorUnits);
    const bool negative = minorUnits < 0;
    const auto magnitude = negative ? static_cast<uint64_t>(-(minorUnits + 1)) + 1
                                    : static_cast<uint64_t>(minorUnits);
    ostringstream out;
    if (negative) out << '-';
    out << code << ' ' << magnitude / 100 << '.' << setw(2) << setfill('0')
        << magnitude % 100;
    return out.str();
}

string compactChartLine(const string& line) {
    const size_t first = line.find(" | ");
    const size_t last = line.rfind(" | ");
    if (first == string::npos || last == first) return line;
    const string middle = line.substr(first + 3, last - first - 3);
    const size_t amountAt = middle.find("PHP ");
    if (amountAt == string::npos) return line;
    const size_t bars = count(middle.begin(), middle.begin() + amountAt, '#');
    const size_t compactBars = bars == 0 ? 0 : max<size_t>(1, (bars * 24 + 39) / 40);
    string amount = middle.substr(amountAt);
    while (!amount.empty() && amount.back() == ' ') amount.pop_back();
    return line.substr(0, first) + " | " + fitted(string(compactBars, '#'), 24) +
           " | " + amount + " | " + line.substr(last + 3);
}

string normalizedChoice(const string& input) {
    const size_t firstNonSpace = input.find_first_not_of(" \t\r\n");
    if (firstNonSpace == string::npos) return "";
    const size_t lastNonSpace = input.find_last_not_of(" \t\r\n");
    const string trimmed = input.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
    if (!all_of(trimmed.begin(), trimmed.end(),
                                 [](unsigned char ch) { return isdigit(ch); }))
        return trimmed;
    const size_t first = trimmed.find_first_not_of('0');
    return first == string::npos ? "0" : trimmed.substr(first);
}

void promptFor(const string& label) {
    prepareVisibleInput();
    string cleanLabel = label;
    while (!cleanLabel.empty() && (cleanLabel.back() == ' ' || cleanLabel.back() == ':'))
        cleanLabel.pop_back();
    cout << '\n' << cleanLabel << "\n> " << flush;
}

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
    frame("CaS / GUEST", {"01  Login                         02  Register account",
                          "03  About CaS                     04  Exit"});
    promptFor("Select an option (1-4)");
}

void showClientMenu(const string& accountId) {
    frame("CaS / CLIENT / " + accountId,
          {"01  Account summary             02  Deposit funds",
           "03  Withdraw funds              04  Transfer funds",
           "05  FX quote                    06  Transaction history",
           "07  Balance chart               08  Export statement / receipt",
           "09  Logout                      10  Open another account",
           "11  Switch account"});
    promptFor("Select an option (1-11)");
}

void showAdminMenu() {
    frame("CaS / ADMIN", {"01  List accounts               02  Change account status",
                           "03  Read audit log              04  Logout"});
    promptFor("Select an option (1-4)");
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
    vector<string> lines{result.message};
    if (result.ok && !result.referenceId.empty())
        lines.push_back("Reference: " + result.referenceId);
    frame(result.ok ? "SUCCESS" : "ACTION NOT COMPLETED", lines,
          result.ok ? UiTone::Success : UiTone::Error);
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
    while (true) {
        string value;
        promptFor(prompt);
        if (!getline(cin, value)) return false;
        if (value.empty()) return true;
        if (validDate(value)) {
            result = value;
            return true;
        }
        showUiMessage(UiTone::Warning, "Enter a real date as YYYY-MM-DD.");
    }
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
    while (true) {
        promptFor("Holder name");
        if (!getline(cin, name)) return;
        if (name.find_first_not_of(" \t") != string::npos) break;
        showUiMessage(UiTone::Warning, "Holder name is required.");
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

void openLinkedAccount(BankData& data, const Session& session,
                       const filesystem::path& path) {
    const auto current = find_if(data.accounts.begin(), data.accounts.end(),
        [&](const Account& account) { return account.accountId == session.accountId; });
    if (current == data.accounts.end() || current->role != Role::Client) {
        showUiMessage(UiTone::Error, "Current client account is unavailable.");
        return;
    }
    const int product = getValidInt("Product (1 Checking, 2 Savings, 3 Time deposit): ", 1, 3);
    const Money opening = getValidMoney("Opening deposit in PHP: ", 0);
    const string profileId = current->profileId.empty() ? current->accountId : current->profileId;
    const string holderName = current->holderName;
    const string pinDigest = current->pinDigest;
    const OperationResult result = commitChange(data, path, [&](BankData& candidate) {
        OperationResult created = createAccount(candidate, holderName,
            static_cast<AccountType>(product - 1), pinDigest, opening);
        if (!created.ok) return created;
        const auto linked = find_if(candidate.accounts.begin(), candidate.accounts.end(),
            [&](const Account& account) { return account.accountId == created.referenceId; });
        if (linked == candidate.accounts.end())
            return OperationResult{false, "New account was not found after creation.", ""};
        linked->profileId = profileId;
        return created;
    });
    showOperation(result);
}

void switchProfileAccount(const BankData& data, Session& session) {
    const auto current = find_if(data.accounts.begin(), data.accounts.end(),
        [&](const Account& account) { return account.accountId == session.accountId; });
    if (current == data.accounts.end() || current->role != Role::Client) {
        showUiMessage(UiTone::Error, "Current client account is unavailable.");
        return;
    }
    const string profileId = current->profileId.empty() ? current->accountId : current->profileId;
    vector<vector<string>> accounts;
    for (const Account& account : data.accounts) {
        if (account.role == Role::Client && account.profileId == profileId &&
            account.pinDigest == current->pinDigest)
            accounts.push_back({account.accountId, accountTypeName(account.type),
                                statusName(account.status), moneyText(account.balanceCents)});
    }
    table("ACCOUNTS IN PROFILE " + profileId,
          {"Account ID", "Type", "Status", "Balance"}, accounts,
          {12, 18, 12, 23});
    string selectedId;
    promptFor("Account ID to use");
    if (!getline(cin, selectedId)) return;
    const auto selected = find_if(data.accounts.begin(), data.accounts.end(),
        [&](const Account& account) {
            return account.accountId == selectedId && account.role == Role::Client &&
                   account.profileId == profileId && account.pinDigest == current->pinDigest;
        });
    if (selected == data.accounts.end()) {
        showUiMessage(UiTone::Error, "Account is not in this profile.");
        return;
    }
    session.accountId = selectedId;
    showUiMessage(UiTone::Success, "Using account " + selectedId + ".");
}

void showSummary(const BankData& data, const Session& session) {
    const AccountSummary summary = getAccountSummary(data, session.accountId);
    if (!summary.found) {
        showUiMessage(UiTone::Error, "Account summary unavailable.");
        return;
    }
    ostringstream interest;
    interest << summary.annualInterestBps / 100 << '.' << setw(2) << setfill('0')
             << summary.annualInterestBps % 100 << '%';
    table("ACCOUNT SUMMARY", {"Field", "Value"},
          {{"Account ID", summary.accountId},
           {"Product", accountTypeName(summary.type)},
           {"Status", statusName(summary.status)},
           {"Available balance", moneyText(summary.balanceCents)},
           {"Minimum balance", moneyText(summary.minimumBalanceCents)},
           {"Annual interest rate", interest.str()}}, {22, 49});
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
    promptFor("Receiver account ID");
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
        showUiMessage(UiTone::Warning, "JPY amounts must be whole units.");
    }
    const Money inputMinorUnits = from == 4 ? amount / 100 : amount;
    const FxQuote quote = convertCurrency(inputMinorUnits, static_cast<Currency>(from - 1),
                                          static_cast<Currency>(to - 1));
    if (!quote.ok) {
        showUiMessage(UiTone::Error, "FX quote unavailable: " + quote.rateDescription);
        return;
    }
    frame("FX QUOTE", {"Input:     " + currencyText(quote.from, quote.inputMinorUnits),
                       "Estimated: " + currencyText(quote.to, quote.outputMinorUnits),
                       quote.rateDescription});
}

void showHistory(const BankData& data, const Session& session) {
    TransactionFilter filter;
    filter.accountId = session.accountId;
    if (!readOptionalDate("Start date (YYYY-MM-DD or blank): ", filter.startDate) ||
        !readOptionalDate("End date (YYYY-MM-DD or blank): ", filter.endDate)) return;
    if (filter.startDate && filter.endDate && *filter.startDate > *filter.endDate) {
        showUiMessage(UiTone::Warning, "Start date must not follow end date.");
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
        showUiMessage(UiTone::Warning, "Minimum amount must not exceed maximum amount.");
        return;
    }

    const vector<Transaction> rows = filterTransactions(data, filter);
    vector<vector<string>> historyRows;
    for (const Transaction& row : rows) {
        const bool credit = row.toAccountId == session.accountId;
        const string otherAccount = credit ? row.fromAccountId : row.toAccountId;
        historyRows.push_back({row.occurredAt.substr(0, 10), row.transactionId,
                               transactionTypeName(row.type),
                               string(credit ? "+" : "-") + moneyText(row.amountCents),
                               otherAccount.empty() ? "--" : otherAccount});
    }
    table("TRANSACTION HISTORY / " + session.accountId,
          {"Date", "Reference", "Type", "Amount", "Other"}, historyRows,
          {10, 12, 15, 14, 11});
}

void showChart(const BankData& data, const Session& session) {
    ostringstream chart;
    const ReportResult result = renderAsciiBalanceChart(data, session.accountId, chart);
    if (!result.ok) {
        showUiMessage(UiTone::Error, result.message);
        return;
    }
    vector<string> lines;
    istringstream input(chart.str());
    string line;
    while (getline(input, line)) lines.push_back(compactChartLine(line));
    frame("BALANCE CHART / " + session.accountId, lines);
}

void exportReport(const BankData& data, const Session& session) {
    const int kind = getValidInt("Export (1 Statement, 2 Receipt): ", 1, 2);
    if (!cin) return;
    if (kind == 1) {
        if (!validReference(session.accountId, "ACC-")) {
            showUiMessage(UiTone::Error, "Account ID is not valid for a statement filename.");
            return;
        }
        const filesystem::path path = filesystem::path("reports") /
            ("Statement_" + session.accountId + ".txt");
        const ReportResult result = exportStatement(data, session.accountId, path);
        showUiMessage(result.ok ? UiTone::Success : UiTone::Error, result.message);
        return;
    }

    string reference;
    promptFor("Transaction ID");
    if (!getline(cin, reference)) return;
    const auto owned = find_if(data.transactions.begin(), data.transactions.end(),
        [&](const Transaction& row) {
            return row.transactionId == reference &&
                   (row.fromAccountId == session.accountId ||
                    row.toAccountId == session.accountId);
        });
    if (owned == data.transactions.end() || !validReference(reference, "TXN-")) {
        showUiMessage(UiTone::Error, "Receipt not found for your account.");
        return;
    }
    const filesystem::path path = filesystem::path("reports") /
        ("Receipt_" + reference + ".txt");
    const ReportResult result = generateReceipt(data, reference, path);
    showUiMessage(result.ok ? UiTone::Success : UiTone::Error, result.message);
}

void listAccounts(const BankData& data) {
    vector<vector<string>> rows;
    for (const Account& account : data.accounts) {
        rows.push_back({account.accountId, account.holderName,
                        account.role == Role::Admin ? "Admin" : "Client",
                        statusName(account.status)});
    }
    table("ALL ACCOUNTS", {"Account ID", "Holder", "Role", "Status"}, rows,
          {12, 26, 10, 17});
}

void changeStatus(BankData& data, const Session& session,
                  const filesystem::path& path) {
    string targetId;
    promptFor("Target account ID");
    if (!getline(cin, targetId)) return;
    const auto target = find_if(data.accounts.begin(), data.accounts.end(),
        [&](const Account& account) { return account.accountId == targetId; });
    if (target == data.accounts.end()) {
        showUiMessage(UiTone::Error, "Account not found.");
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
        showUiMessage(UiTone::Warning,
                      "Status was saved, but the audit timestamp was unavailable.");
        return;
    }
    const AuditEntry entry{when, session.accountId, targetId, previous, next};
    const SaveResult audit = appendAuditLog("data/audit.log", entry);
    if (!audit.ok)
        showUiMessage(UiTone::Warning,
                      "Status was saved, but audit logging failed: " + audit.message);
}

void showAuditLog() {
    const vector<AuditEntry> entries = readAuditLogs("data/audit.log");
    vector<vector<string>> rows;
    for (const AuditEntry& entry : entries) {
        rows.push_back({entry.occurredAt, entry.adminAccountId,
                        entry.targetAccountId,
                        string(statusName(entry.previousStatus)) + " -> " +
                            statusName(entry.nextStatus)});
    }
    table("ACCOUNT STATUS AUDIT", {"UTC time", "Admin", "Account", "Change"},
          rows, {20, 12, 12, 21});
}

void runClientMenu(BankData& data, Session& session, const filesystem::path& path) {
    string choice;
    while (cin && session.role == SessionRole::Client) {
        showClientMenu(session.accountId);
        if (!getline(cin, choice)) return;
        choice = normalizedChoice(choice);
        if (choice == "1") showSummary(data, session);
        else if (choice == "2") changeBalance(data, session, path, true);
        else if (choice == "3") changeBalance(data, session, path, false);
        else if (choice == "4") transferFunds(data, session, path);
        else if (choice == "5") showFxQuote();
        else if (choice == "6") showHistory(data, session);
        else if (choice == "7") showChart(data, session);
        else if (choice == "8") exportReport(data, session);
        else if (choice == "10") openLinkedAccount(data, session, path);
        else if (choice == "11") switchProfileAccount(data, session);
        else if (choice == "9") {
            clearSession(session);
            showUiMessage(UiTone::Info, "You have signed out.");
        } else showUiMessage(UiTone::Warning, "Choose a menu option from 1 to 11.");
    }
}

void runAdminMenu(BankData& data, Session& session, const filesystem::path& path) {
    string choice;
    while (cin && session.role == SessionRole::Admin) {
        showAdminMenu();
        if (!getline(cin, choice)) return;
        choice = normalizedChoice(choice);
        if (choice == "1") listAccounts(data);
        else if (choice == "2") changeStatus(data, session, path);
        else if (choice == "3") showAuditLog();
        else if (choice == "4") {
            clearSession(session);
            showUiMessage(UiTone::Info, "You have signed out.");
        } else showUiMessage(UiTone::Warning, "Choose a menu option from 1 to 4.");
    }
}

} // namespace

void showWelcomeScreen() {
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output != INVALID_HANDLE_VALUE && output != nullptr &&
        GetConsoleMode(output, &mode) &&
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        colorEnabled = true;
#endif
    frame("WELCOME TO CENTS AND SENSIBILITY",
          {"Manage accounts, transactions, and reports."}, UiTone::Success);
}

void showUiMessage(UiTone tone, const string& message) {
    const char* title = "NOTICE";
    if (tone == UiTone::Success) title = "SUCCESS";
    else if (tone == UiTone::Warning) title = "PLEASE CHECK";
    else if (tone == UiTone::Error) title = "ERROR";
    frame(title, {message}, tone);
}

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
            showUiMessage(UiTone::Info, "Input closed. Goodbye!");
            return;
        }
        choice = normalizedChoice(choice);
        if (choice == "1") {
            string accountId;
            promptFor("Account ID");
            if (!getline(cin, accountId)) return;
            const string pin = getValidPin("PIN (four or six digits): ");
            if (!cin) return;
            const LoginResult result = login(data, accountId, pin);
            if (!result.ok) {
                showUiMessage(UiTone::Error, result.message);
                continue;
            }
            session.accountId = result.accountId;
            session.role = result.role == Role::Admin ? SessionRole::Admin
                                                      : SessionRole::Client;
            showUiMessage(UiTone::Success,
                          "Welcome back. Active account: " + session.accountId);
        } else if (choice == "2") registerAccount(data, snapshotPath);
        else if (choice == "3")
            frame("ABOUT CaS", {"Cents and Sensibility is a terminal banking",
                                 "simulation created by Group Ginitaan.",
                                 "Manage your accounts, review activity, and export records."});
        else if (choice == "4") {
            showUiMessage(UiTone::Info, "Thanks for banking with CaS. Goodbye!");
            return;
        } else showUiMessage(UiTone::Warning, "Choose a menu option from 1 to 4.");
    }
}

} // namespace cas
