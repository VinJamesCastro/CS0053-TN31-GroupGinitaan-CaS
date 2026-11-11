#pragma once

#include "BankModel.h"

#include <filesystem>
#include <string>

namespace cas {

enum class UiTone { Info, Success, Warning, Error };

void showWelcomeScreen();
void showUiMessage(UiTone tone, const std::string& message);

void runMenu(BankData& data, const std::filesystem::path& snapshotPath);

} // namespace cas
