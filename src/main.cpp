#include <argparse/argparse.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.h"
#include "siemens_recalc.h"
#include "utils.h"

static std::string getBoardConfig(const std::string &device);
static std::string getQemuBin();
static void validateSimIdentity(const std::string &imsi, const std::string &operatorCode);
static std::string identityKey(const SiemensIdentity &identity);

struct FlashBankOptions {
	std::string otp0;
	std::string otp1;
	std::string otp0File;
	std::string otp1File;
	std::string efaFile;
};

static constexpr size_t SIM_IMSI_LENGTH = 15;
static constexpr size_t SIM_OPERATOR_MIN_LENGTH = 5;
static constexpr size_t SIM_OPERATOR_MAX_LENGTH = 6;
static constexpr size_t FLASH_BANK_COUNT = 4;

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("pmb887x-emu", PROJECT_VERSION);
	program.add_description("Generic emulator for PMB887X-based mobile phones.");

	program.add_group("Main options");

	program.add_argument("-d", "--device")
		.help("Device name or path to custom device.cfg file")
		.required()
		.nargs(1);

	program.add_argument("-f", "--fullflash")
		.help("Path to the fullflash.bin file")
		.required()
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
		const std::string prefix = "--flash-" + std::to_string(index) + "-";
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
		.default_value("");

	program.add_argument("--siemens-imei")
		.help("Siemens flash IMEI (number)")
		.nargs(1)
		.default_value("");

	program.add_argument("--siemens-recover-esn")
		.help("Recover the original ESN of the fullflash by brute force instead of recalculating its keys")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("--siemens-no-recalc")
		.help("Do not recalculate the fullflash security keys in memory for the emulator IMEI/ESN")
		.default_value(false)
		.implicit_value(true);

	program.add_group("SIM options");

	program.add_argument("--sim")
#if HAVE_SIM_READER
		.help("SIM source: virtual, none, or reader")
#else
		.help("SIM source: virtual or none")
#endif
		.nargs(1)
		.default_value("virtual");

#if HAVE_SIM_READER
	program.add_argument("--sim-reader-name")
		.help("Exact PC/SC reader name for --sim reader (uses the first reader with a card by default)")
		.nargs(1)
		.default_value("");
#endif

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
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		std::exit(1);
	}

	std::vector<std::string> qemuArgs;
	std::unordered_map<std::string, std::string> qemuEnv;
	auto qemuBin = getQemuBin();

	auto device = program.get<std::string>("--device");
	auto siemensEsn = program.get<std::string>("--siemens-esn");
	auto siemensImei = program.get<std::string>("--siemens-imei");
	const bool recoverEsn = program.get<bool>("--siemens-recover-esn");
	auto fullflash = program.get<std::string>("--fullflash");
	auto sim = program.get<std::string>("--sim");
#if HAVE_SIM_READER
	auto simReaderName = program.get<std::string>("--sim-reader-name");
#endif
	auto simImsi = program.get<std::string>("--sim-imsi");
	auto simOperator = program.get<std::string>("--sim-operator");
	auto startup = program.get<std::string>("--startup");

	if (sim.empty()) {
		std::cerr << "--sim must not be empty\n";
		return 1;
	}
#if HAVE_SIM_READER
	if (sim != "virtual" && sim != "none" && sim != "reader") {
		std::cerr << "--sim must be virtual, none, or reader\n";
		return 1;
	}
	if (!simReaderName.empty() && sim != "reader") {
		std::cerr << "--sim-reader-name can only be used with --sim reader\n";
		return 1;
	}
#else
	if (sim != "virtual" && sim != "none") {
		std::cerr << "--sim must be virtual or none\n";
		return 1;
	}
#endif
	if (startup.empty()) {
		std::cerr << "--startup must not be empty\n";
		return 1;
	}
	if (sim == "virtual") {
		try {
			validateSimIdentity(simImsi, simOperator);
		} catch (const std::invalid_argument &err) {
			std::cerr << err.what() << "\n";
			return 1;
		}
	}
	if (recoverEsn && (program.is_used("--siemens-esn") || program.is_used("--siemens-imei"))) {
		std::cerr << "--siemens-recover-esn reads the IMEI and ESN from the fullflash, "
			<< "so it can't be combined with --siemens-esn or --siemens-imei\n";
		return 1;
	}

	const bool siemensBoard = device.starts_with("siemens-");

	qemuEnv["PMB887X_BOARD"] = getBoardConfig(device);
	qemuEnv["PMB887X_STARTUP"] = startup;
	qemuEnv["PMB887X_SIM"] = sim;
	if (sim == "virtual") {
		qemuEnv["PMB887X_SIM_OPERATOR"] = simOperator;
		if (!simImsi.empty())
			qemuEnv["PMB887X_SIM_IMSI"] = simImsi;
#if HAVE_SIM_READER
	} else if (!simReaderName.empty()) {
		qemuEnv["PMB887X_SIM_READER_NAME"] = simReaderName;
#endif
	}

	if (program.get<bool>("--qemu-stop-on-exception"))
		qemuEnv["QEMU_ARM_STOP_ON_EXCP"] = "1";

	if (program.get<bool>("--wait-for-serial"))
		qemuEnv["PMB887X_WAIT_FOR_SERIAL"] = "1";

	std::array<FlashBankOptions, FLASH_BANK_COUNT> flashOptions;
	for (size_t index = 0; index < FLASH_BANK_COUNT; index++) {
		const std::string prefix = index == 0 ? "--flash-" : "--flash-" + std::to_string(index) + "-";
		flashOptions[index].otp0 = program.get<std::string>(prefix + "otp0");
		flashOptions[index].otp1 = program.get<std::string>(prefix + "otp1");
		flashOptions[index].otp0File = program.get<std::string>(prefix + "otp0-file");
		flashOptions[index].otp1File = program.get<std::string>(prefix + "otp1-file");
		flashOptions[index].efaFile = program.get<std::string>(prefix + "efa-file");
	}

	FlashBankOptions &flash0 = flashOptions[0];

	// A fullflash from a real phone is already consistent with the ESN of that phone. Instead of
	// rewriting its keys we can search for the ESN they were built from and present that to the firmware.
	bool esnRecovered = false;
	// A previously recovered ESN is a better match for the fullflash than recalculated keys, so an
	// existing cache file is used automatically unless the identity was given on the command line.
	const bool identityGiven = program.is_used("--siemens-esn") || program.is_used("--siemens-imei") ||
		!flash0.otp0.empty() || !flash0.otp1.empty();
	std::error_code esnCacheEc;
	const bool esnCached = !fullflash.empty() && std::filesystem::exists(esnCachePath(fullflash), esnCacheEc);
	if (siemensBoard && (recoverEsn || (esnCached && !identityGiven))) {
		if (!flash0.otp0.empty() || !flash0.otp1.empty()) {
			std::cerr << "--siemens-recover-esn can't be combined with raw OTP data\n";
			return 1;
		}
		std::vector<uint8_t> data;
		if (!readFile(fullflash, data)) {
			std::cerr << "Can't read fullflash: " << fullflash << "\n";
			return 1;
		}

		SiemensIdentity identity = siemensReadIdentity(data);
		if (!identity.ok && recoverEsn) {
			std::cerr << "Can't read the IMEI and keys from this fullflash, so the ESN can't be recovered.\n"
				<< "Run without --siemens-recover-esn to recalculate the keys instead.\n";
			return 1;
		}

		uint32_t esn = 0;
		bool haveEsn = identity.ok && readEsnCache(fullflash, identity.imei, identityKey(identity), esn);
		if (haveEsn) {
			std::cout << "[esn] Using ESN " << esnToHex(esn) << " cached in " << esnCachePath(fullflash) << "\n";
		} else if (recoverEsn) {
			std::cout << "[esn] Searching for the ESN of IMEI " << identity.imei << " (up to 2^32 keys, this can take a while)...\n";
			std::cout.flush();

			const auto started = std::chrono::steady_clock::now();
			if (!siemensRecoverEsn(identity, esn)) {
				std::cerr << "ESN not found, the keys in this fullflash are inconsistent.\n"
					<< "Run without --siemens-recover-esn to recalculate them instead.\n";
				return 1;
			}

			const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - started).count();
			std::cout << "[esn] Recovered ESN " << esnToHex(esn) << " in " << elapsed << " s\n";
			writeEsnCache(fullflash, identity.imei, identityKey(identity), esn);
			haveEsn = true;
		} else {
			// The cache is stale (different fullflash identity), fall back to recalculation.
			std::cout << "[esn] " << esnCachePath(fullflash) << " doesn't match this fullflash, recalculating the keys instead\n";
		}

		if (haveEsn) {
			siemensEsn = esnToHex(esn);
			siemensImei = identity.imei;
			esnRecovered = true;
		}
	}

	// Default emulator IMEI & ESN for FLASH0
	if (siemensBoard && siemensEsn.empty() && flash0.otp0.empty())
		siemensEsn = "12345678";
	if (siemensBoard && siemensImei.empty() && flash0.otp1.empty())
		siemensImei = "490154203237518"; // Nokia *trollface*

	if (flash0.otp0.empty() && !siemensEsn.empty())
		flash0.otp0 = convertESNtoOTP(siemensEsn);
	if (flash0.otp1.empty() && !siemensImei.empty())
		flash0.otp1 = convertIMEItoOTP(siemensImei);

	// Siemens firmware binds itself to the ESN/IMEI with keys stored in the bootcore and EEPROM.
	// Recalculate them in memory for the emulator identity unless disabled.
	const bool rw = program.get<bool>("--rw");
	const bool recalcEnabled = siemensBoard && !esnRecovered && !program.get<bool>("--siemens-no-recalc");
	std::string qemuFullflash = fullflash;
	TempFileCopy fullflashCopy;
	if (recalcEnabled) {
		if (siemensImei.size() != 15 || siemensEsn.size() != 8) {
			std::cout << "[recalc] IMEI or ESN is unknown (raw OTP data was given), fullflash keys are not recalculated\n";
		} else {
			std::vector<uint8_t> data;
			if (!readFile(fullflash, data)) {
				std::cerr << "Can't read fullflash: " << fullflash << "\n";
				return 1;
			}

			SiemensKeys keys;
			keys.imei = siemensImei;
			keys.esn = static_cast<uint32_t>(std::stoul(siemensEsn, nullptr, 16));
			// Only consistency matters for the emulator, so the PapuaUtils defaults are used.
			keys.skey = 12345678;
			keys.masterKeys.fill(12345678);

			const std::vector<uint8_t> original = rw ? data : std::vector<uint8_t>();
			SiemensRecalcResult result = siemensRecalcFullflash(data, keys);
			for (const auto &line : result.log)
				std::cout << "[recalc] " << line << "\n";

			if (!result.structureOk) {
				std::cout << "[recalc] Fullflash structure was not recognized, running it as is\n";
			} else if (result.replaced == 0) {
				std::cout << "[recalc] Fullflash keys already match IMEI " << keys.imei << " and ESN " << siemensEsn << "\n";
			} else if (rw) {
				// In write mode the fullflash on disk is the phone's flash, so recalculate it in place.
				if (!patchFile(fullflash, original, data)) {
					std::cerr << "Can't write recalculated keys to " << fullflash << "\n";
					return 1;
				}
				std::cout << "[recalc] Fullflash keys recalculated in " << fullflash << " for IMEI " << keys.imei << " and ESN " << siemensEsn
					<< (result.complete ? "" : " (some EEPROM blocks were not found)") << "\n";
			} else {
				if (!fullflashCopy.create(data)) {
					std::cerr << "Can't create an in-memory copy of the fullflash\n";
					return 1;
				}
				qemuFullflash = fullflashCopy.path();
				std::cout << "[recalc] Fullflash keys recalculated in memory for IMEI " << keys.imei << " and ESN " << siemensEsn
					<< (result.complete ? "" : " (some EEPROM blocks were not found)") << "\n";

				// QEMU derives the OTP/EFA side files from the fullflash name, keep them next to the original file.
				auto keepSideFile = [&](std::string &option, const char *suffix) {
					std::error_code ec;
					if (option.empty() && std::filesystem::exists(fullflash + suffix, ec))
						option = fullflash + suffix;
				};
				keepSideFile(flash0.otp0File, ".cfi-otp0");
				keepSideFile(flash0.otp1File, ".cfi-otp1");
				keepSideFile(flash0.efaFile, ".cfi-efa");
			}
		}
	}

	for (size_t index = 0; index < FLASH_BANK_COUNT; index++) {
		const FlashBankOptions &options = flashOptions[index];
		const std::string prefix = "PMB887X_FLASH" + std::to_string(index) + "_";
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

	if (rw) {
		std::cout << "Write mode enabled! Your fullflash will be modified!\n";
		qemuArgs.emplace_back("-drive");
		qemuArgs.emplace_back("if=pflash,format=raw,file=" + qemuFullflash);
	} else {
		qemuArgs.emplace_back("-drive");
		qemuArgs.emplace_back("if=pflash,readonly=on,format=raw,file=" + qemuFullflash);
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
		qemuArgs.emplace_back(program.get<std::string>("--monitor"));
	}

	if (program.present("--qemu-debug")) {
		qemuArgs.emplace_back("-d");
		qemuArgs.emplace_back(program.get<std::string>("--qemu-debug"));
	}

	if (isUNIX()) {
		setEnv("GTK_MODULES", "");
		setEnv("GTK2_MODULES", "");
		setEnv("GTK3_MODULES", "");
	}

	std::cout << "---------------------------------------------------\n";
	for (auto &it: qemuEnv) {
		std::cout << (isWindows() ? "set " : "export ") << it.first << "=" << it.second << "\n";
		setEnv(it.first, it.second);
	}
	std::cout << strJoin(qemuArgs, " ") << "\n";
	std::cout << "---------------------------------------------------\n";

	return exec(qemuArgs);
}

static void validateSimIdentity(const std::string &imsi, const std::string &operatorCode) {
	const bool operatorValid = (operatorCode.size() == SIM_OPERATOR_MIN_LENGTH || operatorCode.size() == SIM_OPERATOR_MAX_LENGTH) &&
		std::all_of(operatorCode.begin(), operatorCode.end(), [](uint8_t value) { return std::isdigit(value); });
	if (!operatorValid)
		throw std::invalid_argument("--sim-operator must contain MCC+MNC as 5 or 6 decimal digits");

	if (imsi.empty())
		return;
	const bool imsiValid = imsi.size() == SIM_IMSI_LENGTH &&
		std::all_of(imsi.begin(), imsi.end(), [](uint8_t value) { return std::isdigit(value); });
	if (!imsiValid)
		throw std::invalid_argument("--sim-imsi must contain exactly 15 decimal digits");
	if (!imsi.starts_with(operatorCode))
		throw std::invalid_argument("--sim-imsi must start with the MCC+MNC specified by --sim-operator");
}

static std::string getBoardConfig(const std::string &device) {
	if (device.ends_with(".toml") || device.ends_with(".TOML"))
		return device;

	auto file = device + ".toml";
	std::filesystem::path exeDir = getExecutableDir();

	std::vector<std::filesystem::path> variants;
	variants.emplace_back(exeDir / ("../bsp/lib/data/board/" + file)); // build
	variants.emplace_back(exeDir / ("../share/pmb887x-emu/boards/" + file)); // installed
	variants.emplace_back(exeDir / ("boards/" + file)); // portable version

	for (const auto &path : variants) {
		std::error_code ec;
		if (std::filesystem::exists(path, ec) && !ec)
			return std::filesystem::canonical(path).string();
	}

	throw std::runtime_error("QEMU configuration file not found: " + device);

	return "";
}

// The cache is tied to the keys it was recovered from, so it is dropped when the fullflash changes.
static std::string identityKey(const SiemensIdentity &identity) {
	return identity.hasBootKey ? toHex(identity.bootKey.data(), 16) : toHex(identity.hash.data(), 16);
}

static std::string getQemuBin() {
	std::filesystem::path exeDir = getExecutableDir();

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
		if (std::filesystem::exists(path, ec) && !ec)
			return std::filesystem::canonical(path).string();
	}

	throw std::runtime_error("QEMU binary not found!");

	return "";
}
