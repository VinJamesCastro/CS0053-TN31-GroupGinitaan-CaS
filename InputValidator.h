#ifndef CAS_INPUT_VALIDATOR_H
#define CAS_INPUT_VALIDATOR_H

#include "BankModel.h"

#include <string>

namespace cas {

int getValidInt(const std::string& prompt,
                int minInclusive,
                int maxInclusive);

Money getValidMoney(const std::string& prompt,
                    Money minInclusive);

std::string getValidPin(const std::string& prompt);

}

#endif
