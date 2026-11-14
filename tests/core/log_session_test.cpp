#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/log.h"

#include "support/temp_directory.h"

// ⚠ CES ESSAIS INSTALLENT LE JOURNAL DU PROCESSUS : chacun le referme par
// `EndLogFile`, sans quoi le dossier temporaire ne s'effacerait pas.

namespace {

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

// Vrai si une ligne écrite DEPUIS `first_serial` contient `fragment`.
[[nodiscard]] bool SaidSince(std::uint64_t first_serial, std::string_view fragment) {
  for (const ro::core::LoggedLine& line : ro::core::RecentLogLines()) {
    if (line.serial >= first_serial && line.text.find(fragment) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Un puits placé APRÈS le fichier, qui relit le disque au moment où il reçoit
// la ligne : c'est l'instant où une console qui bloque ou qui plante arrêterait
// le processus.
class DiskProbe final : public spdlog::sinks::base_sink<std::mutex> {
 public:
  explicit DiskProbe(std::filesystem::path log_path) : log_path_(std::move(log_path)) {}

  [[nodiscard]] const std::string& seen_on_disk() const { return seen_on_disk_; }

 protected:
  void sink_it_(const spdlog::details::log_msg& /*msg*/) override {
    seen_on_disk_ = Contents(log_path_);
  }
  void flush_() override {}

 private:
  std::filesystem::path log_path_;
  std::string seen_on_disk_;
};

constexpr std::string_view kSilentDeath = "arrêté SANS PRÉVENIR";
constexpr std::string_view kEmptyLog = "est VIDE";
// La première ligne d'un journal qui annonce sa ligne de fin.
constexpr std::string_view kFirstLine =
    "[info] moonclient 1.0 — revision abc (arbre propre) — journal clos par une ligne de fin\n";

}  // namespace

TEST_CASE("un journal précédent sans ligne de fin se signale avec sa dernière ligne",
          "[core][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path log_path = folder.path() / "client.log";
  folder.WriteText("client.log", std::string(kFirstLine) +
                                     "[info] démarrage : interface\n"
                                     "[info] vidéo : contexte demandé\n");

  const std::uint64_t first = ro::core::LoggedLineCount() + 1;
  REQUIRE(ro::core::InitLogging(log_path).has_value());
  ro::core::EndLogFile("fin de l'essai");

  CHECK(SaidSince(first, kSilentDeath));
  CHECK(SaidSince(first, "« [info] vidéo : contexte demandé »"));
  // L'ancien est mis de côté, pas écrasé.
  CHECK(Contents(folder.path() / "client.precedent.log").find("contexte demandé") !=
        std::string::npos);
}

TEST_CASE("un journal fermé par EndLogFile ne se signale pas au lancement suivant",
          "[core][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path log_path = folder.path() / "client.log";

  REQUIRE(ro::core::InitLogging(log_path).has_value());
  ro::core::EndLogFile("première session");
  CHECK(Contents(log_path).find("journal fermé proprement : première session") !=
        std::string::npos);

  const std::uint64_t first = ro::core::LoggedLineCount() + 1;
  REQUIRE(ro::core::InitLogging(log_path).has_value());
  ro::core::EndLogFile("seconde session");
  CHECK_FALSE(SaidSince(first, kSilentDeath));
  CHECK_FALSE(SaidSince(first, kEmptyLog));
}

TEST_CASE("un journal d'une version qui n'annonçait pas sa ligne de fin ne se juge pas",
          "[core][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path log_path = folder.path() / "client.log";
  folder.WriteText(
      "client.log",
      "[info] moonclient 0.9 — revision abc (arbre propre)\n[info] carte : prontera\n");

  const std::uint64_t first = ro::core::LoggedLineCount() + 1;
  REQUIRE(ro::core::InitLogging(log_path).has_value());
  ro::core::EndLogFile("fin de l'essai");

  CHECK_FALSE(SaidSince(first, kSilentDeath));
  // Et le journal neuf, lui, l'annonce : c'est ce que relira le suivant.
  CHECK(Contents(log_path).find("journal clos par une ligne de fin") != std::string::npos);
}

TEST_CASE("un journal précédent vide se signale comme tel", "[core][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path log_path = folder.path() / "client.log";
  folder.WriteText("client.log", "");

  const std::uint64_t first = ro::core::LoggedLineCount() + 1;
  REQUIRE(ro::core::InitLogging(log_path).has_value());
  ro::core::EndLogFile("fin de l'essai");

  CHECK(SaidSince(first, kEmptyLog));
  CHECK_FALSE(SaidSince(first, kSilentDeath));
}

TEST_CASE("une ligne atteint le disque avant le puits qui suit le fichier", "[core][journal]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path log_path = folder.path() / "client.log";
  REQUIRE(ro::core::InitLogging(log_path).has_value());
  const auto probe = std::make_shared<DiskProbe>(log_path);
  spdlog::default_logger()->sinks().push_back(probe);

  ro::core::LogInfo("ligne à trouver sur disque");
  spdlog::default_logger()->sinks().pop_back();
  ro::core::EndLogFile("fin de l'essai");

  CHECK(probe->seen_on_disk().find("ligne à trouver sur disque") != std::string::npos);
}
