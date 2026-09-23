#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "siemens/fullflash.h"
#include "siemens/otp.h"
#include "siemens/recalc.h"
#include "utils/file.h"
#include "utils/process.h"
#include "utils/string.h"
#include "utils/temp_file.h"

static std::string getBoardConfigPath(const std::string &device);
static std::string getQemuBinaryPath();
static void validateSimIdentity(const std::string &imsi, const std::string &operatorCode);

struct FlashBankOptions {
	std::string otp0;
	std::string otp1;
	std::string otp0File;
	std::string otp1File;
	std::string efaFile;
};

static constexpr char DEFAULT_IMEI[] = "490154203237518";
static constexpr char DEFAULT_ESN[] = "12345678";

static constexpr size_t SIM_IMSI_LENGTH = 15;
static constexpr size_t SIM_OPERATOR_MIN_LENGTH = 5;
static constexpr size_t SIM_OPERATOR_MAX_LENGTH = 6;
static constexpr size_t FLASH_BANK_COUNT = 4;

int main(int argc, char *argv[]) {
	TempFileCopy fullflashCopy;

	spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
	spdlog::flush_on(spdlog::level::info);

	argparse::ArgumentParser program("pmb887x-emu", PROJECT_VERSION);
	program.add_description("Generic emulator for PMB887X-based mobile phones.");

	program.add_group("Main options");

	program.add_argument("-d", "--device")
		.help("Device name or path to a custom board TOML file (detected for Siemens fullflashes by default)")
		.nargs(1)
		.default_value("");

	program.add_argument("-f", "--fullflash")
		.help("Path to the fullflash.bin file")
		.default_value("")
		.nargs(1);

	program.add_argument("--rw")
		.help("Allow writing to fullflash.bin (dangerous!)")
		.default_value(false)
		.implicit_value(true);

	program.add_group("OTP options");

	program.add_argument("--flash-otp0", "--flash-0-otp0")
		.help("Raw NOR flash otp0 value in HEX (with lock bits)")
		.nargs(1)
		.default_value("");

	program.add_argument("--flash-otp1", "--flash-0-otp1")
		.help("Raw NOR flash otp1 value in HEX (with lock bits)")
		.nargs(1)
		.default_value("");

	program.add_argument("--flash-otp0-file", "--flash-0-otp0-file")
		.help("Raw NOR flash OTP0 file")
		.nargs(1)
		.default_value("");

	program.add_argument("--flash-otp1-file", "--flash-0-otp1-file")
		.help("Raw NOR flash OTP1 file")
		.nargs(1)
		.default_value("");

	program.add_argument("--flash-efa-file", "--flash-0-efa-file")
		.help("Raw NOR flash EFA file")
		.nargs(1)
		.default_value("");

	for (size_t index = 1; index < FLASH_BANK_COUNT; index++) {
		const auto prefix = "--flash-" + std::to_string(index) + "-";
		program.add_argument(prefix + "otp0")
			.help("Raw NOR flash otp0 value in HEX (with lock bits)")
			.nargs(1)
			.default_value("");
		program.add_argument(prefix + "otp1")
			.help("Raw NOR flash otp1 value in HEX (with lock bits)")
			.nargs(1)
			.default_value("");
		program.add_argument(prefix + "otp0-file")
			.help("Raw NOR flash OTP0 file")
			.nargs(1)
			.default_value("");
		program.add_argument(prefix + "otp1-file")
			.help("Raw NOR flash OTP1 file")
			.nargs(1)
			.default_value("");
		program.add_argument(prefix + "efa-file")
			.help("Raw NOR flash EFA file")
			.nargs(1)
			.default_value("");
	}

	program.add_argument("--siemens-esn")
		.help("Siemens flash ESN (HEX)")
		.nargs(1)
		.default_value(DEFAULT_ESN);

	program.add_argument("--siemens-imei")
		.help("Siemens flash IMEI (number)")
		.nargs(1)
		.default_value(DEFAULT_IMEI);

	program.add_argument("--siemens-recalc")
		.help("Recalculate the fullflash security keys instead of recovering its original OTP")
		.default_value(false)
		.implicit_value(true);

	program.add_group("SIM options");

	program.add_argument("--sim")
		.help("SIM source: virtual, none, or reader")
		.nargs(1)
		.default_value("virtual");

	program.add_argument("--sim-reader-name")
		.help("Exact PC/SC reader name for --sim reader (uses the first reader with a card by default)")
		.nargs(1)
		.default_value("");

	program.add_argument("--sim-imsi")
		.help("Virtual SIM IMSI (15 decimal digits; derived from --sim-operator by default)")
		.nargs(1)
		.default_value("");

	program.add_argument("--sim-operator")
		.help("Virtual SIM operator code as MCC+MNC (5 or 6 decimal digits)")
		.nargs(1)
		.default_value("00101");

	program.add_group("Startup options");

	program.add_argument("--startup")
		.help("Startup scenario from the board config")
		.nargs(1)
		.default_value("ONLINE");

	program.add_group("Serial options");

	program.add_argument("--serial")
		.help("Connect host serial port to QEMU")
		.nargs(1);

	program.add_argument("--usartd")
		.help("Connect to usartd.pl in QEMU")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("-W", "--wait-for-serial")
		.help("Wait for first byte on serial port")
		.default_value(false)
		.implicit_value(true);

	program.add_group("Trace options");

	program.add_argument("--gdb")
		.help("Run firmware with GDB")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("-D", "--trace")
		.help("CPU IO + CPU emulation log")
		.nargs(1);

	program.add_argument("--trace-io")
		.help("I/O tracing")
		.nargs(1);

	program.add_argument("--trace-log")
		.help("CPU emulation logs only")
		.nargs(1);

	program.add_group("QEMU options");

	program.add_argument("--headless")
		.help("Run QEMU without a display")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("--qemu-monitor")
		.help("QEMU monitor")
		.nargs(1);

	program.add_argument("--qemu-run-with-gdb")
		.help("Run emulator using GDB (debug)")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("-E", "--qemu-stop-on-exception")
		.help("Stop QEMU on ARM exception")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("--qemu-debug")
		.help("QEMU debug options")
		.nargs(1);

	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &err) {
		spdlog::error("{}", err.what());
		std::cerr << program;
		std::exit(1);
	}

	std::vector<std::string> qemuArgs;
	std::unordered_map<std::string, std::string> qemuEnv;
	std::string qemuBin;
	try {
		qemuBin = getQemuBinaryPath();
	} catch (const std::exception &error) {
		spdlog::error("{}", error.what());
		return 1;
	}

	auto device = program.get<std::string>("--device");
	auto siemensEsn = program.get<std::string>("--siemens-esn");
	auto siemensImei = program.get<std::string>("--siemens-imei");
	auto fullflash = program.get<std::string>("--fullflash");
	auto sim = program.get<std::string>("--sim");
	auto simReaderName = program.get<std::string>("--sim-reader-name");
	auto simImsi = program.get<std::string>("--sim-imsi");
	auto simOperator = program.get<std::string>("--sim-operator");
	auto startup = program.get<std::string>("--startup");
	bool rw = program.get<bool>("--rw");

	if (sim != "virtual" && sim != "none" && sim != "reader") {
		spdlog::error("--sim must be virtual, none, or reader");
		return 1;
	}

	if (!simReaderName.empty() && sim != "reader") {
		spdlog::error("--sim-reader-name can only be used with --sim reader");
		return 1;
	}

	if (startup.empty()) {
		spdlog::error("--startup must not be empty");
		return 1;
	}

	if (sim == "virtual") {
		try {
			validateSimIdentity(simImsi, simOperator);
		} catch (const std::invalid_argument &err) {
			spdlog::error("{}", err.what());
			return 1;
		}
	}

	if (device.empty()) {
		if (fullflash.empty()) {
			spdlog::error("Specify --device or --fullflash");
			return 1;
		}
		auto info = SiemensFW::probeFullflash(fullflash);
		if (!info) {
			spdlog::error("Can't detect device from fullflash, specify --device");
			return 1;
		}
		device = info->device;
		spdlog::info("Detected device: {} ({} {})", device, info->vendor, info->model);
	}

	try {
		qemuEnv["PMB887X_BOARD"] = getBoardConfigPath(device);
	} catch (const std::exception &error) {
		spdlog::error("{}", error.what());
		return 1;
	}
	qemuEnv["PMB887X_STARTUP"] = startup;
	qemuEnv["PMB887X_SIM"] = sim;
	if (sim == "virtual") {
		qemuEnv["PMB887X_SIM_OPERATOR"] = simOperator;
		if (!simImsi.empty())
			qemuEnv["PMB887X_SIM_IMSI"] = simImsi;
	} else if (!simReaderName.empty()) {
		qemuEnv["PMB887X_SIM_READER_NAME"] = simReaderName;
	}

	if (program.get<bool>("--qemu-stop-on-exception"))
		qemuEnv["QEMU_ARM_STOP_ON_EXCP"] = "1";

	if (program.get<bool>("--wait-for-serial"))
		qemuEnv["PMB887X_WAIT_FOR_SERIAL"] = "1";

	bool hasOTP = false;
	std::array<FlashBankOptions, FLASH_BANK_COUNT> flashOptions;
	for (size_t index = 0; index < FLASH_BANK_COUNT; index++) {
		const auto prefix = index == 0 ? "--flash-" : "--flash-" + std::to_string(index) + "-";
		FlashBankOptions &opt = flashOptions[index];

		opt.otp0 = program.get<std::string>(prefix + "otp0");
		opt.otp1 = program.get<std::string>(prefix + "otp1");
		opt.otp0File = program.get<std::string>(prefix + "otp0-file");
		opt.otp1File = program.get<std::string>(prefix + "otp1-file");
		opt.efaFile = program.get<std::string>(prefix + "efa-file");

		if (!opt.otp0.empty() || !opt.otp1.empty())
			hasOTP = true;
		if (!opt.otp0File.empty() || !opt.otp1File.empty())
			hasOTP = true;
		if (!opt.efaFile.empty())
			hasOTP = true;
	}

	if (program.is_used("--siemens-esn") || program.is_used("--siemens-imei")) {
		FlashBankOptions &flash0 = flashOptions[0];
		try {
			if (!siemensEsn.empty())
				flash0.otp0 = SiemensFW::esnToOtp(siemensEsn);
			if (!siemensImei.empty())
				flash0.otp1 = SiemensFW::imeiToOtp(siemensImei);
		} catch (const std::invalid_argument &error) {
			spdlog::error("{}", error.what());
			return 1;
		}
		hasOTP = true;
	}

	if (device.starts_with("siemens-") && !hasOTP) {
		FlashBankOptions &flash0 = flashOptions[0];
		flash0.otp0 = SiemensFW::esnToOtp(DEFAULT_ESN);
		flash0.otp1 = SiemensFW::imeiToOtp(DEFAULT_IMEI);

		if (program.get<bool>("--siemens-recalc")) {
			try {
				std::vector<uint8_t> data;
				if (!readFile(fullflash, data))
					throw std::runtime_error("Can't read fullflash: " + fullflash);

				SiemensFW::Keys keys;
				keys.imei = DEFAULT_IMEI;
				keys.esn = (uint32_t) std::stoul(DEFAULT_ESN, nullptr, 16);
				keys.skey = 12345678;
				keys.masterKeys.fill(12345678);
				auto result = SiemensFW::recalculateFullflash(data, keys);
				if (result && result->changed) {
					if (rw) {
						if (!replaceFile(fullflash, data))
							throw std::runtime_error("Can't replace fullflash: " + fullflash);
					} else {
						fullflash = fullflashCopy.create(data);
					}
				}
			} catch (const std::exception &err) {
				spdlog::error("{}", err.what());
				return 1;
			}
		} else if (!fullflash.empty()) {
			try {
				if (auto otp = SiemensFW::recoverOtp(fullflash)) {
					flash0.otp0 = otp->otp0;
					flash0.otp1 = otp->otp1;
				}
			} catch (const std::exception &err) {
				spdlog::warn("{}", err.what());
			}
		}
	}

	for (size_t index = 0; index < FLASH_BANK_COUNT; index++) {
		const FlashBankOptions &options = flashOptions[index];
		const auto prefix = "PMB887X_FLASH" + std::to_string(index) + "_";
		if (!options.otp0.empty())
			qemuEnv[prefix + "OTP0"] = options.otp0;
		if (!options.otp1.empty())
			qemuEnv[prefix + "OTP1"] = options.otp1;
		if (!options.otp0File.empty())
			qemuEnv[prefix + "OTP0_FILE"] = options.otp0File;
		if (!options.otp1File.empty())
			qemuEnv[prefix + "OTP1_FILE"] = options.otp1File;
		if (!options.efaFile.empty())
			qemuEnv[prefix + "EFA_FILE"] = options.efaFile;
	}

	if (program.get<bool>("--qemu-run-with-gdb")) {
		qemuArgs.emplace_back("gdb");
		qemuArgs.emplace_back("--args");
	}

	qemuArgs.emplace_back(qemuBin);

	qemuArgs.emplace_back("-icount");
	qemuArgs.emplace_back("precise-clocks=on");

	qemuArgs.emplace_back("-machine");
	qemuArgs.emplace_back("pmb887x");

	if (program.get<bool>("--headless")) {
		qemuArgs.emplace_back("-display");
		qemuArgs.emplace_back("none");
	}

	if (!fullflash.empty()) {
		if (rw) {
			spdlog::warn("Write mode enabled! Your fullflash will be modified!");
			qemuArgs.emplace_back("-drive");
			qemuArgs.emplace_back("if=pflash,format=raw,file=" + fullflash);
		} else {
			qemuArgs.emplace_back("-drive");
			qemuArgs.emplace_back("if=pflash,readonly=on,format=raw,file=" + fullflash);
		}
	}

	if (program.present("--trace"))
		qemuEnv["PMB887X_TRACE_ALL"] = program.get<std::string>("--trace");

	if (program.present("--trace-io"))
		qemuEnv["PMB887X_TRACE_IO"] = program.get<std::string>("--trace-io");

	if (program.present("--trace-log"))
		qemuEnv["PMB887X_TRACE_LOG"] = program.get<std::string>("--trace-log");

	if (program.get<bool>("--gdb")) {
		qemuArgs.emplace_back("-s");
		qemuArgs.emplace_back("-S");
	}

	if (program.present("--serial")) {
		qemuArgs.emplace_back("-serial");
		qemuArgs.emplace_back(program.get<std::string>("--serial"));
	} else if (program.get<bool>("--usartd")) {
		if (isWindows()) {
			qemuArgs.emplace_back("-serial");
			qemuArgs.emplace_back("tcp:127.0.0.1:11111");
		} else {
			qemuArgs.emplace_back("-serial");
			qemuArgs.emplace_back("unix:/tmp/siemens.sock");
		}
	}

	if (program.present("--qemu-monitor")) {
		qemuArgs.emplace_back("-monitor");
		qemuArgs.emplace_back(program.get<std::string>("--qemu-monitor"));
	}

	if (program.present("--qemu-debug")) {
		qemuArgs.emplace_back("-d");
		qemuArgs.emplace_back(program.get<std::string>("--qemu-debug"));
	}

#if !defined(_WIN32) && !defined(__APPLE__)
	setEnvironmentVariable("GTK_MODULES", "");
	setEnvironmentVariable("GTK2_MODULES", "");
	setEnvironmentVariable("GTK3_MODULES", "");
#endif

	spdlog::info("---------------------------------------------------");
	for (const auto &[name, value] : qemuEnv) {
		spdlog::info("{}{}={}", isWindows() ? "set " : "export ", name, value);
		setEnvironmentVariable(name, value);
	}
	spdlog::info("{}", joinStrings(qemuArgs, " "));
	spdlog::info("---------------------------------------------------");

	try {
		return executeProcess(qemuArgs);
	} catch (const std::exception &error) {
		spdlog::error("{}", error.what());
		return 1;
	}
}

static void validateSimIdentity(const std::string &imsi, const std::string &operatorCode) {
	if (operatorCode.size() != SIM_OPERATOR_MIN_LENGTH && operatorCode.size() != SIM_OPERATOR_MAX_LENGTH)
		throw std::invalid_argument("--sim-operator must contain MCC+MNC as 5 or 6 decimal digits");
	if (!std::all_of(operatorCode.begin(), operatorCode.end(), [](uint8_t value) { return std::isdigit(value); }))
		throw std::invalid_argument("--sim-operator must contain MCC+MNC as 5 or 6 decimal digits");

	if (imsi.empty())
		return;

	if (imsi.size() != SIM_IMSI_LENGTH)
		throw std::invalid_argument("--sim-imsi must contain exactly 15 decimal digits");
	if (!std::all_of(imsi.begin(), imsi.end(), [](uint8_t value) { return std::isdigit(value); }))
		throw std::invalid_argument("--sim-imsi must contain exactly 15 decimal digits");
	if (!imsi.starts_with(operatorCode))
		throw std::invalid_argument("--sim-imsi must start with the MCC+MNC specified by --sim-operator");
}

static std::string getBoardConfigPath(const std::string &device) {
	if (device.ends_with(".toml") || device.ends_with(".TOML"))
		return device;

	auto file = device + ".toml";
	auto exeDir = getExecutableDirectory();

	std::vector<std::filesystem::path> variants;
	variants.emplace_back(exeDir / ("../bsp/lib/data/board/" + file)); // build
	variants.emplace_back(exeDir / ("../share/pmb887x-emu/boards/" + file)); // installed
	variants.emplace_back(exeDir / ("boards/" + file)); // portable version

	for (const auto &path : variants) {
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
			return std::filesystem::canonical(path).string();
	}

	throw std::runtime_error("QEMU configuration file not found: " + device);
}

static std::string getQemuBinaryPath() {
	auto exeDir = getExecutableDirectory();

	std::vector<std::filesystem::path> variants;
	if (isWindows()) {
		variants = {
			exeDir / "qemu-build/qemu-system-arm.exe", // build
			exeDir / "qemu/qemu-system-arm.exe", // portable version
		};
	} else {
		variants = {
			exeDir / "qemu-install/bin/qemu-system-arm", // build
			exeDir / "../share/pmb887x-emu/qemu/bin/qemu-system-arm", // installed
			exeDir / "qemu/bin/qemu-system-arm", // portable version
		};
	}

	for (const auto &path : variants) {
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
			return std::filesystem::canonical(path).string();
	}

	throw std::runtime_error("QEMU binary not found!");
}
