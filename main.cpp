#include "Authentication.h"
#include "MenuController.h"
#include "TeamInterfaces.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

using namespace std;

namespace {

bool bootstrapAdmin(cas::BankData& data, const filesystem::path& snapshotPath) {
    if (!data.accounts.empty() || !data.transactions.empty()) {
        cerr << "First-run data was not empty. Admin setup stopped.\n";
        return false;
    }

    cout << "First run: create an administrator account.\n";
    string name;
    while (true) {
        cout << "Admin name: ";
        if (!getline(cin, name)) return false;
        if (name.find_first_not_of(" \t") != string::npos) break;
        cout << "Administrator name is required.\n";
    }
    const string pin = cas::getValidPin("New admin PIN (four or six digits): ");
    if (!cin) return false;

    cas::BankData candidate = data;
    cas::Account admin;
    admin.accountId = "ADM-000001";
    admin.holderName = name;
    admin.pinDigest = cas::digestPin(pin);
    admin.role = cas::Role::Admin;
    candidate.accounts.push_back(admin);

    const cas::SaveResult saved = cas::saveSnapshot(snapshotPath, candidate);
    if (!saved.ok) {
        cerr << "Admin setup was not saved: " << saved.message << '\n';
        return false;
    }
    data = move(candidate);
    cout << "Administrator account saved. Account ID: ADM-000001\n";
    return true;
}

} // namespace

int main() {
    try {
        const filesystem::path snapshotPath = "data/bank_state.txt";
        error_code fileError;
        const bool snapshotExists = filesystem::exists(snapshotPath, fileError);
        if (fileError) {
            cerr << "Cannot check bank data: " << fileError.message() << '\n';
            return 1;
        }

        cas::BankData data;
        if (snapshotExists) {
            cas::LoadResult loaded = cas::loadSnapshot(snapshotPath);
            if (!loaded.ok) {
                cerr << "Cannot load bank data: " << loaded.message << '\n';
                return 1;
            }
            data = move(loaded.data);
        } else {
            filesystem::create_directories(snapshotPath.parent_path(), fileError);
            if (fileError) {
                cerr << "Cannot create data directory: " << fileError.message() << '\n';
                return 1;
            }
            if (!bootstrapAdmin(data, snapshotPath)) return 1;
        }

        cas::runMenu(data, snapshotPath);
        return 0;
    } catch (const exception& error) {
        cerr << "Cannot continue: " << error.what() << '\n';
        return 1;
    }
}
