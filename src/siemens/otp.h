#pragma once

#include <string>

namespace siemens {

std::string esnToOtp(const std::string &esn);
std::string imeiToOtp(const std::string &imei);

}
