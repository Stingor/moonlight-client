// LES GESTES DE L'EN-TÊTE DES PROFILS ET DE L'ONGLET « JOUEURS » : créer,
// renommer, dupliquer, supprimer, importer, choisir le profil d'un contexte,
// répondre à la proposition, et les conduites ; le repli d'un PvE sans choix ;
// les noms montrés.

#include "game/companion_ai_profile_gestures.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_editor.h"
#include "game/companion_ai_names.h"
#include "game/companion_ai_profiles.h"
#include "game/companion_tactic_fields.h"
#include "sim/companion_rule_scene.h"
#include "support/temp_directory.h"

using companion_rule_scene::kLifJob;
using companion_rule_scene::kStartMs;
using moonclient::CompanionAiSession;
using moonclient::CompanionProfileBook;
using moonclient::CompanionSeat;
using moonclient::kBasicProfileName;
using moonclient::kOwnProfileName;
using moonclient::ProfileGestureResult;
using moonclient::SeatedCompanion;
using ro::sim::CompanionAiContext;
using ro::ui::CompanionAiVerb;
using ro::ui::CompanionAiWindowAction;

namespace {

constexpr std::uint32_t kCid = 150001;
constexpr std::int64_t kWallMs = 1760000000000;
constexpr std::uint16_t kArcherJob = 6017;

struct Bench {
  ro::test_support::TempDirectory root;
  CompanionAiSession session;
  companion_rule_scene::Scene scene;
  CompanionSeat seat;

  Bench() {
    session.Open(root.path(), kCid);
    session.NoteCharacterName("Stingor");
    session.Resolve(Seated(), kLifJob, kStartMs, kWallMs);
  }
  [[nodiscard]] SeatedCompanion Seated() {
    return SeatedCompanion{.companion = ro::sim::AiCompanion::kHomunculus,
                           .seat = &seat,
                           .brain = &scene.brain,
                           .settings = &scene.settings};
  }
  [[nodiscard]] std::vector<std::string> Keys() const {
    std::vector<std::string> keys{std::string(kBasicProfileName)};
    if (const auto type = session.profiles().by_job.find(kLifJob);
        type != session.profiles().by_job.end()) {
      for (const moonclient::NamedProfile& profile : type->second) {
        keys.push_back(profile.name);
      }
    }
    return keys;
  }
  ProfileGestureResult Do(CompanionAiVerb verb, std::string name = {}, std::string profile = {},
                          int choice = -1, int context = 0,
                          const std::vector<moonclient::ProfileSource>& imports = {}) {
    CompanionAiWindowAction action;
    action.verb = verb;
    action.name = std::move(name);
    action.profile = std::move(profile);
    action.choice = choice;
    action.context = context;
    const std::vector<std::string> keys = Keys();
    return moonclient::ApplyProfileGesture(action, session, Seated(), keys, imports);
  }
};

}  // namespace

TEST_CASE("Gestes des profils : créer, dupliquer, renommer, supprimer", "[app][profile_gestures]") {
  Bench bench;
  ProfileGestureResult made = bench.Do(CompanionAiVerb::kNewProfile, " Farm ");
  REQUIRE(made.accepted);
  CHECK(made.edited == "Farm");
  // UN NOM PRIS se refuse, avec sa raison.
  const ProfileGestureResult taken = bench.Do(CompanionAiVerb::kNewProfile, "Farm");
  CHECK_FALSE(taken.accepted);
  CHECK_FALSE(taken.refusal.empty());
  REQUIRE(bench.Do(CompanionAiVerb::kDuplicateProfile, "Farm 2", "Farm").accepted);
  const ProfileGestureResult renamed = bench.Do(CompanionAiVerb::kRenameProfile, "Chasse", "Farm");
  REQUIRE(renamed.accepted);
  CHECK(renamed.edited == "Chasse");
  CHECK(bench.session.profiles().Find(kLifJob, "Farm") == nullptr);
  // L'IA BASIQUE ne se renomme ni ne se supprime.
  CHECK_FALSE(
      bench.Do(CompanionAiVerb::kDeleteProfile, {}, std::string(kBasicProfileName)).accepted);
  const ProfileGestureResult deleted = bench.Do(CompanionAiVerb::kDeleteProfile, {}, "Chasse");
  REQUIRE(deleted.accepted);
  CHECK(deleted.edited == "");
  CHECK(bench.session.profiles().Find(kLifJob, "Chasse") == nullptr);
  // Ce qui n'est pas de l'en-tête ne s'y traite pas.
  CHECK_FALSE(bench.Do(CompanionAiVerb::kAddRule).handled);
}

TEST_CASE("Gestes des profils : le choix d'un contexte, et son repli", "[app][profile_gestures]") {
  Bench bench;
  REQUIRE(bench.Do(CompanionAiVerb::kNewProfile, "Duel").accepted);
  const std::vector<std::string> keys = bench.Keys();
  const int duel = 1;
  REQUIRE(keys[static_cast<std::size_t>(duel)] == "Duel");
  REQUIRE(bench
              .Do(CompanionAiVerb::kSetContextProfile, {}, {}, duel,
                  static_cast<int>(CompanionAiContext::kPvP))
              .accepted);
  CompanionProfileBook book(bench.session.profiles(), bench.session.edits());
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kPvP) == "Duel");
  // 🔴 UN PVE SANS CHOIX prend « Mon profil » s'il existe, même quand le PvP en
  // a un ; le PvP sans choix prend le PvE.
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kPvE) ==
        kBasicProfileName);
  REQUIRE(bench.Do(CompanionAiVerb::kNewProfile, std::string(kOwnProfileName)).accepted);
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kPvE) ==
        kOwnProfileName);
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kWoE) ==
        kOwnProfileName);
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kPvP) == "Duel");
  // LE REPLI SE RECHOISIT.
  REQUIRE(bench
              .Do(CompanionAiVerb::kSetContextProfile, {}, {}, -1,
                  static_cast<int>(CompanionAiContext::kPvP))
              .accepted);
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kPvP) ==
        kOwnProfileName);
}

TEST_CASE("Gestes des profils : l'import d'un autre type et d'un autre personnage",
          "[app][profile_gestures]") {
  Bench bench;
  CompanionProfileBook book(bench.session.profiles(), bench.session.edits());
  REQUIRE(book.Create(kArcherJob, "Tir", kBasicProfileName) == moonclient::ProfileRefusal::kNone);
  REQUIRE(book.Create(kArcherJob, "Arène", kBasicProfileName) == moonclient::ProfileRefusal::kNone);
  const moonclient::CharacterKey other{.cid = kCid + 1, .name = "Autre"};
  REQUIRE(book.SetActive(other, kArcherJob, CompanionAiContext::kPvE, "Arène") ==
          moonclient::ProfileRefusal::kNone);
  const std::vector<moonclient::ProfileSource> sources =
      moonclient::ImportSources(bench.session.profiles(), bench.session.character(), kLifJob);
  // LE PROFIL ACTIF D'UN AUTRE PERSONNAGE vient d'abord, sous son nom, et ne se
  // répète pas parmi les profils de son type.
  REQUIRE(sources.size() == 2);
  CHECK(sources.front().name == "Arène");
  CHECK(sources.front().label == "Autre · Archer, rang 1 — Arène");
  CHECK(sources.back().label == "Archer, rang 1 — Tir");
  const ProfileGestureResult imported =
      bench.Do(CompanionAiVerb::kImportProfile, "Tir", {}, 1, 0, sources);
  REQUIRE(imported.accepted);
  CHECK(imported.edited == "Tir");
  CHECK(bench.session.profiles().Find(kLifJob, "Tir") != nullptr);
  // Un import hors de la liste ne fait rien.
  CHECK_FALSE(bench.Do(CompanionAiVerb::kImportProfile, "X", {}, 5, 0, sources).accepted);
}

TEST_CASE("Gestes des conduites : poser, régler, retirer", "[app][profile_gestures]") {
  Bench bench;
  REQUIRE(bench.Do(CompanionAiVerb::kNewProfile, "Arène").accepted);
  const moonclient::AiEditTarget target = bench.session.NamedTarget(bench.Seated(), "Arène");
  REQUIRE(target.profile != nullptr);
  // L'IA BASIQUE ne se vise pas.
  CHECK(bench.session.NamedTarget(bench.Seated(), kBasicProfileName).profile == nullptr);
  CompanionAiWindowAction action;
  action.verb = CompanionAiVerb::kAddGenericConduct;
  action.generic = ro::ui::kConductGenericOwnerGuild;
  REQUIRE(moonclient::ApplyAiEdit(action, target, {}).accepted);
  action.verb = CompanionAiVerb::kAddJobConduct;
  action.choice = 8;
  REQUIRE(moonclient::ApplyAiEdit(action, target, {}).accepted);
  REQUIRE(target.profile->conducts.lines().size() == 2);
  std::vector<ro::sim::PlayerConductKey> keys;
  for (const ro::sim::CompanionPlayerConduct& line : target.profile->conducts.lines()) {
    keys.push_back(line.key);
  }
  action.verb = CompanionAiVerb::kSetConduct;
  action.index = 1;
  action.rank = static_cast<int>(ro::sim::TacticRank::kIgnore);
  action.trigger = static_cast<int>(ro::sim::TacticTrigger::kIfAttacks);
  action.advanced.chase = static_cast<int>(ro::sim::TacticChase::kNever);
  // Un champ qui ne vaut pas pour un joueur ne revient pas.
  action.advanced.harass = true;
  REQUIRE(moonclient::ApplyAiEdit(action, target, {}, keys).accepted);
  const ro::sim::CompanionPlayerConduct* priest = target.profile->conducts.Find(keys[1]);
  REQUIRE(priest != nullptr);
  CHECK(priest->rank == ro::sim::TacticRank::kIgnore);
  CHECK(priest->trigger == ro::sim::TacticTrigger::kIfAttacks);
  CHECK(priest->chase == ro::sim::TacticChase::kNever);
  CHECK(moonclient::ConductFieldsOf(*priest).harass == false);
  REQUIRE(bench.session.edits().Find(kLifJob, "Arène")->conducts.has_value());
  action.verb = CompanionAiVerb::kRemoveConduct;
  action.index = 0;
  REQUIRE(moonclient::ApplyAiEdit(action, target, {}, keys).accepted);
  CHECK(target.profile->conducts.lines().size() == 1);
}

TEST_CASE("Noms de l'IA : les classes de compagnon et les profils", "[app][profile_gestures]") {
  CHECK(moonclient::CompanionClassName(6001) == "Lif");
  CHECK(moonclient::CompanionClassName(6012) == "Vanilmirth évolué");
  CHECK(moonclient::CompanionClassName(6036) == "Lancer, rang 10");
  CHECK(moonclient::CompanionClassName(6048) == "Eira");
  CHECK(moonclient::CompanionClassName(6052) == "Eleanor");
  CHECK(moonclient::CompanionClassName(7000) == "Classe 7000");
  CHECK(moonclient::ProfileLabelOf(kBasicProfileName) == "IA basique");
  CHECK(moonclient::ProfileLabelOf(kOwnProfileName) == "Mon profil");
  CHECK(moonclient::ProfileLabelOf("Farm") == "Farm");
}
