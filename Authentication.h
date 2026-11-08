#pragma once

#include "BankModel.h"

namespace cas {

std::string digestPin(const std::string& pin);
LoginResult login(const BankData& data, const std::string& accountId,
                  const std::string& pin);

} // namespace cas
