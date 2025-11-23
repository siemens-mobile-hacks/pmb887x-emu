class Pmb887xEmu < Formula
	desc "Infineon PMB887x-based phones emulator"
	homepage "https://github.com/siemens-mobile-hacks/pmb887x-emu"
	url "https://github.com/siemens-mobile-hacks/pmb887x-emu.git",
      tag:      "RELEASE_TAG_NAME",
	  revision: "RELEASE_TAG_HASH"
	license "MIT"
	head "https://github.com/siemens-mobile-hacks/pmb887x-emu.git", branch: "main"

	livecheck do
		url :stable
		strategy :github_latest
	end

	depends_on "cmake" => :build
	depends_on "meson" => :build
	depends_on "ninja" => :build
	depends_on "pkg-config" => :build
	depends_on "coreutils" => :build
	depends_on "llvm"
	depends_on "libffi"
	depends_on "gettext"
	depends_on "glib"
	depends_on "pixman"

	def install
		system "cmake", "-S", ".", "-B", "build", *std_cmake_args
		system "cmake", "--build", "build"
		system "cmake", "--install", "build"
	end

	test do
		assert_match "Usage: pmb887x-emu", shell_output("#{bin}/pmb887x-emu -h 2>&1", 1)
	end
end
