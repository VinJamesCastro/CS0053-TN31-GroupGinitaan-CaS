#pragma once

#include "BankModel.h"

#include <filesystem>

namespace cas {

void runMenu(BankData& data, const std::filesystem::path& snapshotPath);

} // namespace cas
