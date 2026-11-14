// LA SESSION DES PROFILS DE L'IA DES COMPAGNONS : un seul basculement par
// entrée de carte, un changement de profil qui ne redémarre rien, « Mon
// profil » créé sans rien redémarrer, le renommage et le retrait du profil
// appliqué, la reconnexion, et le fichier des délais.

#include "game/companion_ai_session.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/map_mode.h"

#include "game/companion_ai_profiles.h"
#include "game/companion_ai_state_file.h"
#include "sim/companion_rule_scene.h"
#include "support/temp_directory.h"

using companion_rule_scene::Always;
using companion_rule_scene::kHomunculusId;
using companion_rule_scene::kLifJob;
using companion_rule_scene::kOwnerX;
using companion_rule_scene::kRowY;
using companion_rule_scene::kStartMs;
using companion_rule_scene::kTickMs;
using companion_rule_scene::Scene;
using moonclient::CompanionAiSession;
using moonclient::CompanionProfileBook;
using moonclient::CompanionSeat;
using moonclient::kBasicProfileName;
using moonclient::kOwnProfileName;
using moonclient::ProfileRefusal;
using moonclient::SeatedCompanion;
using ro::sim::CompanionAiContext;
using ro::sim::MapMode;

namespace {

constexpr std::uint32_t kCid = 150001;
constexpr std::int64_t kWallMs = 1760000000000;
constexpr std::uint32_t kRuleDelayMs = 10000;
constexpr std::uint32_t kLoadMs = 5000;
constexpr int kLooseFollowCells = 10;
constexpr std::uint16_t kFarCells = 6;

[[nodiscard]] MapMode Pvp() {
  MapMode mode;
  mode.kind = ro::sim::MapKind::kFreePvpZone;
  mode.attackable_beyond_party = true;
  mode.pvp_counter_shown = true;
  return mode;
}

// UNE SESSION SUR UN DOSSIER NEUF, et l'homoncule Lif de la scène des règles.
struct Bench {
  ro::test_support::TempDirectory root;
  CompanionAiSession session;
  Scene scene;
  CompanionSeat seat;
  bool owner_west = false;

  Bench() {
    session.Open(root.path(), kCid);
    session.NoteCharacterName("Stingor");
  }
  [[nodiscard]] SeatedCompanion Seated() {
    return SeatedCompanion{.companion = ro::sim::AiCompanion::kHomunculus,
                           .seat = &seat,
                           .brain = &scene.brain,
                           .settings = &scene.settings};
  }
  [[nodiscard]] CompanionProfileBook Book() {
    return CompanionProfileBook(session.profiles(), session.edits());
  }
  void Resolve(std::uint32_t now_ms, std::int64_t wall_now_ms = kWallMs) {
    session.Resolve(Seated(), kLifJob, now_ms, wall_now_ms);
  }
  [[nodiscard]] std::string Applied() const {
    return seat.applied.has_value() ? seat.applied->second : std::string();
  }
  // UN PROFIL QUI RAMÈNE AU MAÎTRE pas plus d'une fois toutes les dix secondes,
  // l'homoncule à six cases : seule la règle le fait revenir.
  void FarProfile(const std::string& name) {
    REQUIRE(Book().Create(kLifJob, name, kBasicProfileName) == ProfileRefusal::kNone);
    moonclient::CompanionProfile& profile = *session.profiles().Find(kLifJob, name);
    ro::sim::CompanionAiSettings settings =
        ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanion::kHomunculus, kLifJob);
    settings.follow_distance_cells = kLooseFollowCells;
    settings.arrival_attack_hold_ms = 0;
    settings.skills.offensive = ro::sim::OffensiveSkills::kNever;
    profile.settings = settings;
    profile.rules = {ro::sim::CompanionRule{.action = ro::sim::RuleAction::kReturnToOwner,
                                            .conditions = {Always()},
                                            .own_delay_ms = kRuleDelayMs}};
    profile.rule_extras = {""};
    session.edits().SetWhole(kLifJob, name, profile);
    REQUIRE(Book().SetActive(session.character(), kLifJob, CompanionAiContext::kPvE, name) ==
            ProfileRefusal::kNone);
    REQUIRE(
        scene.world.PlaceAt(kHomunculusId, ro::sim::CellPosition{kOwnerX + kFarCells, kRowY, 0}));
  }
  // LE MAÎTRE FAIT QUATRE PAS : un retour déjà demandé vers sa case d'avant ne
  // retient plus la règle.
  void Nudge() {
    constexpr std::uint16_t kStepCells = 4;
    owner_west = !owner_west;
    const auto x = static_cast<std::uint16_t>(owner_west ? kOwnerX - kStepCells : kOwnerX);
    REQUIRE(
        scene.world.PlaceAt(companion_rule_scene::kOwnerId, ro::sim::CellPosition{x, kRowY, 0}));
  }
  [[nodiscard]] bool RuleDecides(std::uint32_t now_ms) {
    Nudge();
    static_cast<void>(scene.Tick(now_ms));
    const std::optional<ro::sim::DecidingRule>& decided = scene.brain.deciding_rule();
    return decided.has_value() && decided->at_ms == now_ms;
  }
  [[nodiscard]] std::filesystem::path StatePath() const {
    return moonclient::CompanionStatePath(root.path());
  }
};

[[nodiscard]] std::string ReadText(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

}  // namespace

TEST_CASE("Session de l'IA : un seul basculement par entrée, même après un long chargement",
          "[app][companion_session]") {
  Bench bench;
  REQUIRE(bench.Book().Create(kLifJob, "Farm", kBasicProfileName) == ProfileRefusal::kNone);
  REQUIRE(bench.Book().Create(kLifJob, "Duel", kBasicProfileName) == ProfileRefusal::kNone);
  REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvE,
                                 "Farm") == ProfileRefusal::kNone);
  REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvP,
                                 "Duel") == ProfileRefusal::kNone);
  int applied = 0;
  std::string last;
  const auto step = [&](std::uint32_t entries, const MapMode& mode, std::uint32_t arrived_ms,
                        std::uint32_t now_ms) {
    bench.session.ObserveMap(entries, mode, arrived_ms, now_ms);
    bench.Resolve(now_ms);
    if (bench.Applied() != last) {
      ++applied;
      last = bench.Applied();
    }
  };
  std::uint32_t now = kStartMs;
  step(1, Pvp(), now, now);
  REQUIRE(last == "Duel");
  // D'UNE CARTE PVP À UNE AUTRE, au chargement long : l'arrivée est datée après
  // le début de l'image où l'entrée est vue.
  const std::uint32_t arrived = now + kLoadMs;
  step(2, MapMode{}, arrived, now += kTickMs);
  step(2, MapMode{}, arrived, arrived + kTickMs);
  step(2, Pvp(), arrived, arrived + 2 * kTickMs);
  CHECK(applied == 1);
  // VERS UNE VILLE : un seul basculement, quand le délai passe.
  const std::uint32_t town = arrived + kLoadMs;
  step(3, MapMode{}, town, town);
  step(3, MapMode{}, town, town + ro::sim::kCompanionContextSettleMs);
  CHECK(last == "Farm");
  CHECK(applied == 2);
}

TEST_CASE("Session de l'IA : changer de profil sur la carte ne redémarre rien",
          "[app][companion_session]") {
  Bench bench;
  REQUIRE(bench.Book().Create(kLifJob, "Farm", kBasicProfileName) == ProfileRefusal::kNone);
  bench.Resolve(kStartMs);
  REQUIRE(bench.Applied() == kBasicProfileName);
  SECTION("l'attente reste, même sans « garder l'attente »") {
    bench.scene.brain.ToggleStandby();
    REQUIRE_FALSE(bench.scene.settings.standby_kept_across_maps);
    REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvE,
                                   "Farm") == ProfileRefusal::kNone);
    bench.Resolve(kStartMs + kTickMs);
    CHECK(bench.Applied() == "Farm");
    CHECK(bench.scene.brain.standby());
  }
  SECTION("le combat reste") {
    bench.scene.settings.arrival_attack_hold_ms = 0;
    bench.scene.Monster(companion_rule_scene::kFirstMonsterId, kOwnerX + 2);
    bench.scene.Engage(companion_rule_scene::kFirstMonsterId, kStartMs + kTickMs);
    REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvE,
                                   "Farm") == ProfileRefusal::kNone);
    bench.Resolve(kStartMs + 2 * kTickMs);
    CHECK(bench.Applied() == "Farm");
    CHECK(bench.scene.brain.target_id() == companion_rule_scene::kFirstMonsterId);
  }
}

TEST_CASE("Session de l'IA : le premier geste crée « Mon profil » sans rien redémarrer",
          "[app][companion_session]") {
  Bench bench;
  bench.Resolve(kStartMs);
  REQUIRE(bench.Applied() == kBasicProfileName);
  bench.scene.brain.ToggleStandby();
  const moonclient::AiEditTarget target = bench.session.EditTarget(bench.Seated());
  REQUIRE(target.profile != nullptr);
  CHECK(target.profile_name == kOwnProfileName);
  CHECK(bench.Applied() == kOwnProfileName);
  CHECK(bench.scene.brain.standby());
  // SANS AUCUN CHOIX, « Mon profil » est le repli de tous les contextes : rien
  // ne rebascule.
  bench.Resolve(kStartMs + kTickMs);
  CHECK(bench.Applied() == kOwnProfileName);
  CHECK(bench.scene.brain.standby());
}

TEST_CASE("Session de l'IA : renommer le profil appliqué garde ses délais, le retirer les perd",
          "[app][companion_session]") {
  Bench bench;
  bench.FarProfile("Farm");
  std::uint32_t now = kStartMs;
  bench.Resolve(now);
  REQUIRE(bench.Applied() == "Farm");
  REQUIRE(bench.RuleDecides(now += kTickMs));
  const SeatedCompanion seated = bench.Seated();
  REQUIRE(bench.session.Rename({&seated, 1}, kLifJob, "Farm", "Chasse") == ProfileRefusal::kNone);
  CHECK(bench.Applied() == "Chasse");
  bench.Resolve(now += kTickMs);
  CHECK(bench.Applied() == "Chasse");
  CHECK_FALSE(bench.RuleDecides(now += kTickMs));

  // PASSÉ À L'IA BASIQUE, le délai se garde de côté ; le profil retiré l'emporte.
  REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvE,
                                 kBasicProfileName) == ProfileRefusal::kNone);
  bench.Resolve(now += kTickMs);
  REQUIRE(bench.Applied() == kBasicProfileName);
  REQUIRE(bench.session.Delete(kLifJob, "Chasse") == ProfileRefusal::kNone);
  bench.FarProfile("Chasse");
  REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvE,
                                 "Chasse") == ProfileRefusal::kNone);
  bench.Resolve(now += kTickMs);
  REQUIRE(bench.Applied() == "Chasse");
  CHECK(bench.RuleDecides(now += kTickMs));
}

TEST_CASE("Session de l'IA : la reconnexion garde un délai, l'absence le fait échoir",
          "[app][companion_session]") {
  constexpr std::int64_t kShortAbsenceMs = 2000;
  constexpr std::uint32_t kOtherClockMs = 7000000;
  Bench bench;
  bench.FarProfile("Farm");
  std::uint32_t now = kStartMs;
  bench.Resolve(now);
  REQUIRE(bench.RuleDecides(now += kTickMs));
  static_cast<void>(bench.session.SaveProfiles());
  REQUIRE(moonclient::LoadCompanionAi(bench.root.path()).profiles.Find(kLifJob, "Farm") != nullptr);
  const SeatedCompanion seated = bench.Seated();
  bench.session.SaveState({&seated, 1}, now, kWallMs, /*force=*/true);
  REQUIRE(std::filesystem::exists(bench.StatePath()));

  // UNE AUTRE SESSION du même personnage, une autre horloge, un compagnon neuf.
  CompanionAiSession again;
  again.Open(bench.root.path(), kCid);
  Scene scene;
  CompanionSeat seat;
  const SeatedCompanion fresh{.companion = ro::sim::AiCompanion::kHomunculus,
                              .seat = &seat,
                              .brain = &scene.brain,
                              .settings = &scene.settings};
  REQUIRE(scene.world.PlaceAt(kHomunculusId, ro::sim::CellPosition{kOwnerX + kFarCells, kRowY, 0}));
  const auto decides = [&scene](std::uint32_t now_ms) {
    static_cast<void>(scene.Tick(now_ms));
    return scene.brain.deciding_rule().has_value() && scene.brain.deciding_rule()->at_ms == now_ms;
  };
  SECTION("une absence courte") {
    again.Resolve(fresh, kLifJob, kOtherClockMs, kWallMs + kShortAbsenceMs);
    REQUIRE(seat.applied->second == "Farm");
    CHECK_FALSE(decides(kOtherClockMs + kTickMs));
  }
  SECTION("une absence plus longue que le délai") {
    again.Resolve(fresh, kLifJob, kOtherClockMs, kWallMs + kRuleDelayMs);
    CHECK(decides(kOtherClockMs + kTickMs));
  }
}

TEST_CASE("Session de l'IA : un nouveau compagnon n'hérite pas des délais du précédent",
          "[app][companion_session]") {
  constexpr std::uint32_t kOtherActorId = 201;
  constexpr std::uint16_t kLif2Job = 6009;
  Bench bench;
  bench.FarProfile("Farm");
  moonclient::FollowedCompanion followed;
  std::uint32_t now = kStartMs;
  REQUIRE(moonclient::FollowCompanion(followed, bench.scene.brain, kHomunculusId, kLifJob, now));
  bench.Resolve(now);
  REQUIRE(bench.RuleDecides(now += kTickMs));
  SECTION("le même compagnon, hors de vue un instant : sa mémoire reste") {
    CHECK_FALSE(moonclient::FollowCompanion(followed, bench.scene.brain, kHomunculusId, 0, now));
    CHECK_FALSE(
        moonclient::FollowCompanion(followed, bench.scene.brain, kHomunculusId, kLifJob, now));
    CHECK_FALSE(bench.RuleDecides(now += kTickMs));
  }
  SECTION("un autre acteur : tout repart") {
    CHECK(moonclient::FollowCompanion(followed, bench.scene.brain, kOtherActorId, kLifJob, now));
    CHECK(bench.RuleDecides(now += kTickMs));
  }
  SECTION("une autre classe : tout repart") {
    bench.scene.brain.ToggleStandby();
    CHECK(moonclient::FollowCompanion(followed, bench.scene.brain, kHomunculusId, kLif2Job, now));
    CHECK_FALSE(bench.scene.brain.standby());
  }
}

TEST_CASE("Session de l'IA : le fichier des délais", "[app][companion_session]") {
  Bench bench;
  bench.FarProfile("Farm");
  std::uint32_t now = kStartMs;
  bench.Resolve(now);
  REQUIRE(bench.RuleDecides(now += kTickMs));
  const SeatedCompanion seated = bench.Seated();
  SECTION("rien de changé : rien ne s'écrit") {
    bench.session.SaveState({&seated, 1}, now, kWallMs, true);
    REQUIRE(std::filesystem::exists(bench.StatePath()));
    std::filesystem::remove(bench.StatePath());
    bench.session.SaveState({&seated, 1}, now + kTickMs, kWallMs + kTickMs, true);
    CHECK_FALSE(std::filesystem::exists(bench.StatePath()));
    // La règle agit de nouveau : il y a de quoi écrire.
    REQUIRE(bench.RuleDecides(now += kRuleDelayMs + kTickMs));
    bench.session.SaveState({&seated, 1}, now, kWallMs + kRuleDelayMs, true);
    CHECK(std::filesystem::exists(bench.StatePath()));
  }
  SECTION("illisible : mis de côté sans écraser, puis refait") {
    const std::string broken = "version: [1\n";
    bench.root.WriteText(std::filesystem::path("savedata") / moonclient::kCompanionStateFileName,
                         broken);
    bench.session.SaveState({&seated, 1}, now, kWallMs, true);
    std::filesystem::path aside = bench.StatePath();
    aside += moonclient::kStateSetAsideSuffix;
    CHECK(ReadText(aside) == broken);
    CHECK_FALSE(moonclient::ParseCompanionState(ReadText(bench.StatePath())).unreadable);
    // UNE SECONDE FOIS : le premier mis de côté reste.
    bench.root.WriteText(std::filesystem::path("savedata") / moonclient::kCompanionStateFileName,
                         "- encore\n- illisible\n");
    REQUIRE(bench.RuleDecides(now += kRuleDelayMs + kTickMs));
    bench.session.SaveState({&seated, 1}, now, kWallMs + kRuleDelayMs, true);
    std::filesystem::path second = aside;
    second += ".2";
    CHECK(ReadText(aside) == broken);
    CHECK(std::filesystem::exists(second));
  }
  SECTION("une version inconnue ne s'écrit pas") {
    const std::string newer = "version: 99\n";
    bench.root.WriteText(std::filesystem::path("savedata") / moonclient::kCompanionStateFileName,
                         newer);
    bench.session.SaveState({&seated, 1}, now, kWallMs, true);
    CHECK(ReadText(bench.StatePath()) == newer);
  }
}
