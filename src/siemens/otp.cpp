#include "otp.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace SiemensFW {

std::string imeiToOtp(const std::string &imei) {
	if (imei.length() != 15)
		throw std::invalid_argument("Invalid IMEI: " + imei);
	if (!std::all_of(imei.begin(), imei.end(), [](char digit) { return digit >= '0' && digit <= '9'; }))
		throw std::invalid_argument("Invalid IMEI: " + imei);

	std::string otpImei;
	for (size_t index = 0; index < imei.length() - 1; index += 2) {
		otpImei += imei[index + 1];
		otpImei += imei[index];
	}

	return "0000" + otpImei + "FF";
}

std::string esnToOtp(const std::string &esn) {
	if (esn.length() != 8)
		throw std::invalid_argument("Invalid ESN: " + esn);
	if (!std::all_of(esn.begin(), esn.end(), [](uint8_t digit) { return std::isxdigit(digit); }))
		throw std::invalid_argument("Invalid ESN: " + esn);

	static const uint8_t ESN_KEY[] = { 0x32, 0xE5, 0xF7, 0x03 };
	static const char HEX_DIGITS[] = "0123456789ABCDEF";
	std::string otpEsn;

	for (size_t index = 0; index < 4; index++) {
		auto byteString = esn.substr((3 - index) * 2, 2);
		uint8_t value = std::stoul(byteString, nullptr, 16);
		uint8_t result = (value ^ ESN_KEY[index]);

		otpEsn += HEX_DIGITS[(result >> 4)];
		otpEsn += HEX_DIGITS[(result & 0x0F)];
	}

	return "0200" + otpEsn + "00000000";
}

}
