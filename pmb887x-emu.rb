class Pmb887xEmu < Formula
  desc "Infineon PMB887x-based phones emulator"
  homepage "https://github.com/siemens-mobile-hacks/pmb887x-emu"
  url "https://github.com/siemens-mobile-hacks/pmb887x-emu.git",
      tag:      "RELEASE_TAG_NAME",
      revision: "RELEASE_TAG_HASH"
  license "GPL-2.0-only"
  head "https://github.com/siemens-mobile-hacks/pmb887x-emu.git", branch: "master"

  livecheck do
    url :stable
    strategy :github_latest
  end

  depends_on "cmake" => :build
  depends_on "coreutils" => :build
  depends_on "ninja" => :build
  depends_on "pkgconf" => :build
  depends_on "python@3.14" => :build
  depends_on "capstone"
  depends_on "gettext"
  depends_on "glib"
  depends_on "libslirp"
  depends_on "pixman"

  uses_from_macos "bison" => :build
  uses_from_macos "flex" => :build
  uses_from_macos "zlib"

  def install
    system "cmake", "-S", ".", "-B", "build", "-DBUILD_TESTING=OFF", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/pmb887x-emu --version")
  end
end
