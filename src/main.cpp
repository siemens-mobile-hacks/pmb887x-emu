#include <argparse/argparse.hpp>
#include <cstdlib>
#include <string>
#include <iostream>
#include <unordered_map>

#include "utils.h"

static std::string getBoardConfig(const std::string &device);
static std::string getQemuBin();

int main(int argc, char *argv[]) {
	argparse::ArgumentParser program("pmb887x-emu", "1.0.0");
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

	program.add_argument("--flash-otp0")
		.help("Raw NOR flash otp0 value in HEX (with lock bits)")
		.nargs(1)
		.default_value("");

	program.add_argument("--flash-otp1")
		.help("Raw NOR flash otp1 value in HEX (with lock bits)")
		.nargs(1)
		.default_value("");

	program.add_argument("--siemens-esn")
		.help("Siemens flash ESN (HEX)")
		.nargs(1)
		.default_value("");

	program.add_argument("--siemens-imei")
		.help("Siemens flash IMEI (number)")
		.nargs(1)
		.default_value("");

	program.add_group("Serial options");

	program.add_argument("--serial")
		.help("Connect host serial port to QEMU")
		.nargs(1);

	program.add_argument("--usartd")
		.help("Connect to usartd.pl in QEMU")
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
		.help("CPU IO tracing only")
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
	} catch (const std::exception& err) {
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
	auto flashOtp0 = program.get<std::string>("--flash-otp0");
	auto flashOtp1 = program.get<std::string>("--flash-otp1");

	qemuEnv["PMB887X_BOARD"] = getBoardConfig(device);

	if (program.get<bool>("--qemu-stop-on-exception"))
		qemuEnv["QEMU_ARM_STOP_ON_EXCP"] = "1";

	// Default emulator IMEI & ESN
	if (device.starts_with("siemens-") && siemensEsn == "")
		siemensEsn = "12345678";
	if (device.starts_with("siemens-") && siemensImei == "")
		siemensImei = "490154203237518"; // Nokia *trollface*

	// Flash OTP
	qemuEnv["PMB887X_FLASH_OTP0"] = siemensEsn != "" ?
		convertESNtoOTP(siemensEsn) :
		program.get<std::string>("--flash-otp0");
	qemuEnv["PMB887X_FLASH_OTP1"] = siemensImei != "" ?
		convertIMEItoOTP(siemensImei) :
		program.get<std::string>("--flash-otp1");

	if (program.get<bool>("--qemu-run-with-gdb")) {
		qemuArgs.emplace_back("gdb");
		qemuArgs.emplace_back("--args");
	}

	qemuArgs.emplace_back(qemuBin);

	qemuArgs.emplace_back("-icount");
	qemuArgs.emplace_back("precise-clocks=on");

	qemuArgs.emplace_back("-machine");
	qemuArgs.emplace_back("pmb887x");

	if (program.get<bool>("--rw")) {
		std::cout << "Write mode enabled! Your fullflash will be modified!\n";
		qemuArgs.emplace_back("-drive");
		qemuArgs.emplace_back("if=pflash,format=raw,file=" + program.get<std::string>("--fullflash"));
	} else {
		qemuArgs.emplace_back("-drive");
		qemuArgs.emplace_back("if=pflash,readonly=on,format=raw,file=" + program.get<std::string>("--fullflash"));
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

static std::string getBoardConfig(const std::string &device) {
	if (device.ends_with(".cfg") || device.ends_with(".CFG"))
		return device;

	auto file = device + ".cfg";
	std::filesystem::path exeDir = getExecutableDir();

	std::vector<std::filesystem::path> variants;
	if (isOSX()) {
		variants = {
			"/opt/homebrew/share/pmb887x-emu/" + file,
			"/usr/local/share/pmb887x-emu/boards/" + file,
		};
	} else if (isUNIX()) {
		variants = {
			"/usr/share/pmb887x-emu/boards/" + file,
			"/usr/local/share/pmb887x-emu/boards/" + file,
		};
	}

	variants.emplace_back(exeDir / ("boards/" + file));
	variants.emplace_back(exeDir / ("../bsp/lib/data/board/" + file));

	for (const auto &path : variants) {
		std::error_code ec;
		if (std::filesystem::exists(path, ec) && !ec) {
			return std::filesystem::canonical(path).string();
		}
	}

	throw std::runtime_error("QEMU configuration file not found: " + device);

	return "";
}

static std::string getQemuBin() {
	std::filesystem::path exeDir = getExecutableDir();

	std::vector<std::filesystem::path> variants;
	if (isOSX()) {
		variants = {
			"/opt/homebrew/share/pmb887x-emu/qemu-system-arm", // homebrew (arm)
			"/usr/local/share/pmb887x-emu/qemu-system-arm", // homebrew (intel)
			exeDir / "qemu-build/qemu-system-arm", // build
			exeDir / "qemu/qemu-system-arm", // portable version
		};
	} else if (isWindows()) {
		variants = {
			exeDir / "qemu-build/qemu-system-arm.exe", // build
			exeDir / "qemu/qemu-system-arm.exe", // portable version
		};
	} else {
		variants = {
			"/usr/share/pmb887x-emu/qemu-system-arm", // installed
			"/usr/local/share/pmb887x-emu/qemu-system-arm", // installed
			exeDir / "qemu-build/qemu-system-arm", // build
			exeDir / "qemu/qemu-system-arm", // portable version
		};
	}

	for (const auto &path : variants) {
		std::error_code ec;
		if (std::filesystem::exists(path, ec) && !ec) {
			return std::filesystem::canonical(path).string();
		}
	}

	throw std::runtime_error("QEMU binary not found!");

	return "";
}
