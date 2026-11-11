#include "Authentication.h"
#include "InputValidator.h"
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
        cas::showUiMessage(cas::UiTone::Error,
                           "First-run data was not empty. Admin setup stopped.");
        return false;
    }

    cas::showUiMessage(cas::UiTone::Info,
                       "First run: create an administrator account to secure this installation.");
    string name;
    while (true) {
        cas::prepareVisibleInput();
        cout << "\nAdmin name\n> " << flush;
        if (!getline(cin, name)) return false;
        if (name.find_first_not_of(" \t") != string::npos) break;
        cas::showUiMessage(cas::UiTone::Warning, "Administrator name is required.");
    }
    const string pin = cas::getValidPin("New admin PIN (four or six digits): ");
    if (!cin) return false;

    cas::BankData candidate = data;
    cas::Account admin;
    admin.accountId = "ADM-000001";
    admin.profileId = admin.accountId;
    admin.holderName = name;
    admin.pinDigest = cas::digestPin(pin);
    admin.role = cas::Role::Admin;
    candidate.accounts.push_back(admin);

    const cas::SaveResult saved = cas::saveSnapshot(snapshotPath, candidate);
    if (!saved.ok) {
        cas::showUiMessage(cas::UiTone::Error,
                           "Admin setup was not saved: " + saved.message);
        return false;
    }
    data = move(candidate);
    cas::showUiMessage(cas::UiTone::Success,
                       "Administrator account saved. Account ID: ADM-000001");
    cas::waitForContinue();
    return true;
}

} // namespace

int main() {
    try {
        cas::showWelcomeScreen();
        cas::waitForContinue();
        const filesystem::path snapshotPath = "data/bank_state.txt";
        error_code fileError;
        const bool snapshotExists = filesystem::exists(snapshotPath, fileError);
        if (fileError) {
            cas::showUiMessage(cas::UiTone::Error,
                               "Cannot check bank data: " + fileError.message());
            return 1;
        }

        cas::BankData data;
        if (snapshotExists) {
            cas::LoadResult loaded = cas::loadSnapshot(snapshotPath);
            if (!loaded.ok) {
                cas::showUiMessage(cas::UiTone::Error,
                                   "Cannot load bank data: " + loaded.message);
                return 1;
            }
            data = move(loaded.data);
        } else {
            filesystem::create_directories(snapshotPath.parent_path(), fileError);
            if (fileError) {
                cas::showUiMessage(cas::UiTone::Error,
                                   "Cannot create data directory: " + fileError.message());
                return 1;
            }
            if (!bootstrapAdmin(data, snapshotPath)) return 1;
        }

        cas::runMenu(data, snapshotPath);
        return 0;
    } catch (const exception& error) {
        cas::showUiMessage(cas::UiTone::Error,
                           "Cannot continue: " + string(error.what()));
        return 1;
    }
}
