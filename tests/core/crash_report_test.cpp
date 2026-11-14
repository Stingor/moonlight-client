// Le rapport de plantage, sur ce qui se vérifie SANS planter, puis le débordement
// de pile, qui plante un processus enfant : la même suite relancée sur un essai
// caché.

#include "ro/core/crash_report.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <thread>
#include <vector>

#include <windows.h>
// ⚠ APRÈS `windows.h` : `dbghelp.h` emploie ses types.
#include <dbghelp.h>

#include "support/temp_directory.h"
#endif

TEST_CASE("une faute connue se dit en français, pas en hexadécimal", "[core]") {
  // Le rapport est écrit pour être LU, souvent par qui ne l'a pas écrit : un
  // code brut oblige à chercher ailleurs ce qu'une phrase dit tout de suite.
#if defined(_WIN32)
  CHECK(ro::core::DescribeFault(EXCEPTION_ACCESS_VIOLATION) == "violation d'acces");
  CHECK(ro::core::DescribeFault(EXCEPTION_STACK_OVERFLOW) == "pile debordee");
  CHECK(ro::core::DescribeFault(EXCEPTION_INT_DIVIDE_BY_ZERO) == "division entiere par zero");
#endif
}

TEST_CASE("une faute inconnue se dit par son NUMÉRO plutôt que de se taire", "[core]") {
  // ⚠ La règle qui compte : nommer toutes les fautes ferait une table que
  // personne ne relit, mais taire celles qu'on ne nomme pas rendrait le rapport
  // muet précisément dans le cas rare, c'est-à-dire le cas intéressant.
  const std::string said = ro::core::DescribeFault(0xDEADBEEF);
  CHECK(said.find("deadbeef") != std::string::npos);
  CHECK_FALSE(said.empty());
}

TEST_CASE("une liste de modules ne se lit pas au-delà de sa place", "[core]") {
  // Le système annonce ce qu'il lui aurait fallu, même quand il n'a pas tout écrit.
  constexpr std::size_t kHandleBytes = 8;
  constexpr std::size_t kCapacityBytes = 256 * kHandleBytes;
  CHECK(ro::core::ListedModuleCount(3 * kHandleBytes, kCapacityBytes, kHandleBytes) == 3);
  CHECK(ro::core::ListedModuleCount(kCapacityBytes, kCapacityBytes, kHandleBytes) == 256);
  CHECK(ro::core::ListedModuleCount(300 * kHandleBytes, kCapacityBytes, kHandleBytes) == 256);
}

// ASan prend lui-même le débordement de pile, avant le filtre.
#if defined(_WIN32) && !defined(__SANITIZE_ADDRESS__)
namespace {

// Où l'enfant dépose son rapport. Sans cette variable, l'essai caché ne fait rien :
// un filtre par fichier le sélectionne aussi.
constexpr const char* kChildFolderVariable = "RO_TEST_CRASH_FOLDER";
constexpr const char* kChildOnMainThreadTag = "[plantage-pile-fil-principal]";
constexpr const char* kChildOnWorkerThreadTag = "[plantage-pile-fil-secondaire]";
constexpr DWORD kChildPatienceMs = 120'000;
constexpr std::size_t kRecursionFrameBytes = 512;

[[nodiscard]] std::optional<std::filesystem::path> ChildFolder() {
  char folder[MAX_PATH] = {};
  const DWORD size = GetEnvironmentVariableA(kChildFolderVariable, folder, sizeof(folder));
  if (size == 0 || size >= sizeof(folder)) return std::nullopt;
  return std::filesystem::path(folder);
}

std::size_t RecurseForever(std::size_t depth) {
  volatile char frame[kRecursionFrameBytes] = {};
  frame[depth % kRecursionFrameBytes] = 1;
  if (depth == SIZE_MAX) return 0;
  return RecurseForever(depth + 1) + static_cast<std::size_t>(frame[0]);
}

// Lance cette suite sur `tag` et rend son code de sortie.
[[nodiscard]] DWORD RunChild(const char* tag, const std::filesystem::path& folder) {
  char own_path[MAX_PATH] = {};
  REQUIRE(GetModuleFileNameA(nullptr, own_path, sizeof(own_path)) != 0);
  std::string command = std::string("\"") + own_path + "\" " + tag;
  REQUIRE(SetEnvironmentVariableA(kChildFolderVariable, folder.string().c_str()) != 0);
  STARTUPINFOA startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION child = {};
  const BOOL created = CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child);
  SetEnvironmentVariableA(kChildFolderVariable, nullptr);
  REQUIRE(created != 0);
  const DWORD waited = WaitForSingleObject(child.hProcess, kChildPatienceMs);
  if (waited == WAIT_TIMEOUT) TerminateProcess(child.hProcess, EXIT_FAILURE);
  DWORD exit_code = 0;
  GetExitCodeProcess(child.hProcess, &exit_code);
  CloseHandle(child.hThread);
  CloseHandle(child.hProcess);
  REQUIRE(waited == WAIT_OBJECT_0);
  return exit_code;
}

[[nodiscard]] std::filesystem::path OnlyFileWithExtension(const std::filesystem::path& folder,
                                                          const char* extension) {
  std::vector<std::filesystem::path> found;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::directory_iterator(folder)) {
    if (entry.path().extension() == extension) found.push_back(entry.path());
  }
  REQUIRE(found.size() == 1);
  return found.front();
}

[[nodiscard]] std::string ReadWhole(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Le numéro que le rapport donne au fil fautif, sur sa ligne « fil     : ».
[[nodiscard]] unsigned long ReportedThreadId(const std::string& report) {
  constexpr std::string_view kThreadLine = "fil     : ";
  const std::size_t at = report.find(kThreadLine);
  REQUIRE(at != std::string::npos);
  return std::strtoul(report.c_str() + at + kThreadLine.size(), nullptr, 10);
}

void CheckOverflowReport(const char* tag) {
  const ro::test_support::TempDirectory folder;
  const DWORD exit_code = RunChild(tag, folder.path());
  CHECK(exit_code != 0);

  const std::string report = ReadWhole(OnlyFileWithExtension(folder.path(), ".txt"));
  CHECK(report.find("pile debordee") != std::string::npos);
  CHECK(report.find("REGISTRES") != std::string::npos);
  CHECK(report.find("MODULES") != std::string::npos);
  CHECK(report.find("DERNIERES LIGNES DU JOURNAL") != std::string::npos);
  // La pile décrite est bien celle de la récursion : la même image, en boucle.
  constexpr std::string_view kStackTitle = "PILE D'APPELS DU FIL FAUTIF";
  REQUIRE(report.find(kStackTitle) != std::string::npos);
  CHECK(report.find("ro_tests.exe+", report.find(kStackTitle)) != std::string::npos);

  // Le vidage désigne la faute et le fil qui l'a commise, pas l'auxiliaire.
  std::string dump = ReadWhole(OnlyFileWithExtension(folder.path(), ".dmp"));
  void* stream = nullptr;
  ULONG stream_size = 0;
  REQUIRE(MiniDumpReadDumpStream(dump.data(), ExceptionStream, nullptr, &stream, &stream_size) !=
          FALSE);
  const auto* exception = static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
  CHECK(exception->ExceptionRecord.ExceptionCode == EXCEPTION_STACK_OVERFLOW);
  CHECK(exception->ThreadId == ReportedThreadId(report));
}

}  // namespace

// Les deux enfants : le fil qui a appelé l'installation, avec sa réserve, et un
// fil qui n'a que celle du système.
TEST_CASE("enfant : la pile du fil principal déborde", "[.][plantage-pile-fil-principal]") {
  const std::optional<std::filesystem::path> folder = ChildFolder();
  if (!folder.has_value()) return;
  ro::core::InstallCrashReporter(*folder, ro::core::CrashNotice::kSilent);
  static_cast<void>(RecurseForever(0));
}

TEST_CASE("enfant : la pile d'un autre fil déborde", "[.][plantage-pile-fil-secondaire]") {
  const std::optional<std::filesystem::path> folder = ChildFolder();
  if (!folder.has_value()) return;
  ro::core::InstallCrashReporter(*folder, ro::core::CrashNotice::kSilent);
  std::thread worker([] { static_cast<void>(RecurseForever(0)); });
  worker.join();
}

TEST_CASE("le relevé d'une pile remonte depuis les registres", "[core]") {
  CONTEXT here = {};
  RtlCaptureContext(&here);
  std::array<std::uint64_t, 8> addresses = {};
  const std::size_t walked = ro::core::WalkStackFrom(here, addresses);
  CHECK(walked > 1);
  CHECK(addresses[0] == here.Rip);
}

TEST_CASE("une pile illisible arrête le relevé et garde ce qui est relevé", "[core]") {
  // UNE PAGE DE GARDE tient lieu de pile illisible : la lire lève une faute, sans
  // être de celles que Catch2 prend pour la mort de l'essai.
  void* guarded = VirtualAlloc(nullptr, 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE | PAGE_GUARD);
  REQUIRE(guarded != nullptr);

  // Aucun code à cette adresse : le relevé la croit feuille, et lit le sommet.
  constexpr std::uint64_t kNoCodeAddress = 0x1000;
  CONTEXT broken = {};
  broken.Rip = kNoCodeAddress;
  broken.Rsp = reinterpret_cast<DWORD64>(guarded);
  std::array<std::uint64_t, 8> addresses = {};
  const std::size_t walked = ro::core::WalkStackFrom(broken, addresses);
  VirtualFree(guarded, 0, MEM_RELEASE);

  CHECK(walked == 1);
  CHECK(addresses[0] == kNoCodeAddress);
}

TEST_CASE("un débordement de pile écrit son rapport et son vidage", "[core]") {
  CheckOverflowReport(kChildOnMainThreadTag);
}

TEST_CASE("un débordement de pile sur un autre fil écrit aussi son rapport", "[core]") {
  CheckOverflowReport(kChildOnWorkerThreadTag);
}
#endif
