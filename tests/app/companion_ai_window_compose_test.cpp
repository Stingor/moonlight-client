// LA FENÊTRE « IA DU COMPAGNON » VUE DES PROFILS : l'en-tête composé (choix,
// replis, profil qui n'agit nulle part), la proposition dite une fois, la
// conduite posée depuis le menu d'un joueur, les classes proposées, le
// renommage de « Mon profil », et un geste entier relu après enregistrement.

#include "game/companion_ai_window_compose.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_profiles.h"
#include "sim/companion_rule_scene.h"
#include "support/temp_directory.h"

using companion_rule_scene::kLifJob;
using companion_rule_scene::kStartMs;
using moonclient::CharacterKey;
using moonclient::CompanionAiSession;
using moonclient::CompanionProfileBook;
using moonclient::CompanionSeat;
using moonclient::EvolutionProposal;
using moonclient::kBasicProfileName;
using moonclient::kOwnProfileName;
using moonclient::ProfileRefusal;
using moonclient::SeatedCompanion;
using ro::sim::CompanionAiContext;

namespace {

constexpr std::uint32_t kCid = 150002;
constexpr std::int64_t kWallMs = 1760000000000;
constexpr std::uint16_t kEvolvedLifJob = 6009;

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
  [[nodiscard]] CompanionProfileBook Book() {
    return CompanionProfileBook(session.profiles(), session.edits());
  }
};

[[nodiscard]] std::size_t Context(CompanionAiContext context) {
  return static_cast<std::size_t>(context);
}

}  // namespace

TEST_CASE("Composition des profils : choix, replis et profil qui n'agit nulle part",
          "[app][companion_ai_compose]") {
  Bench bench;
  REQUIRE(bench.Book().Create(kLifJob, "Farm", kBasicProfileName) == ProfileRefusal::kNone);
  moonclient::ProfilesViewStore store;
  ro::ui::CompanionProfilesView view;
  moonclient::ComposeProfilesView(bench.session, kLifJob, "Farm", "", store, view);
  REQUIRE(view.profiles.size() == 2);
  CHECK(view.profiles[0].label == "IA basique");
  CHECK(view.edited == 1);
  CHECK(view.current_context == static_cast<int>(CompanionAiContext::kPvE));
  CHECK(view.chosen[Context(CompanionAiContext::kPvE)] == -1);
  CHECK(view.fallback[Context(CompanionAiContext::kPvE)] == "Par défaut (IA basique)");
  CHECK(view.fallback[Context(CompanionAiContext::kPvP)] == "Comme en PvE (IA basique)");
  // CRÉÉ, LE PROFIL N'EST CHOISI NULLE PART : la vue le dit.
  CHECK(view.unused);
  REQUIRE(bench.Book().SetActive(bench.session.character(), kLifJob, CompanionAiContext::kPvP,
                                 "Farm") == ProfileRefusal::kNone);
  moonclient::ComposeProfilesView(bench.session, kLifJob, "Farm", "", store, view);
  CHECK(view.chosen[Context(CompanionAiContext::kPvP)] == 1);
  CHECK_FALSE(view.unused);
  // L'IA BASIQUE, repli du PvE, agit.
  moonclient::ComposeProfilesView(bench.session, kLifJob, kBasicProfileName, "", store, view);
  CHECK_FALSE(view.unused);
}

TEST_CASE("Composition des profils : l'import se garde jusqu'à la péremption",
          "[app][companion_ai_compose]") {
  Bench bench;
  constexpr std::uint16_t kArcherJob = 6017;
  moonclient::ProfilesViewStore store;
  ro::ui::CompanionProfilesView view;
  moonclient::ComposeProfilesView(bench.session, kLifJob, kBasicProfileName, "", store, view);
  CHECK(view.imports.empty());
  REQUIRE(bench.Book().Create(kArcherJob, "Tir", kBasicProfileName) == ProfileRefusal::kNone);
  moonclient::ComposeProfilesView(bench.session, kLifJob, kBasicProfileName, "", store, view);
  CHECK(view.imports.empty());
  store.imports_stale = true;
  moonclient::ComposeProfilesView(bench.session, kLifJob, kBasicProfileName, "", store, view);
  REQUIRE(view.imports.size() == 1);
  CHECK(view.imports.front() == "Archer, rang 1 — Tir");
}

TEST_CASE("Proposition : la phrase, et le chat une seule fois d'une connexion à l'autre",
          "[app][companion_ai_compose]") {
  const EvolutionProposal proposal{.from_job = kLifJob, .to_job = kEvolvedLifJob};
  CHECK(moonclient::ProposalLine(proposal) == "Recopier le profil de Lif ?");
  CHECK(moonclient::ProposalChatLine(proposal).find("Lif") != std::string::npos);
  ro::test_support::TempDirectory root;
  const CharacterKey key{.cid = kCid, .name = "Stingor"};
  {
    CompanionAiSession session;
    session.Open(root.path(), kCid);
    CompanionProfileBook book(session.profiles(), session.edits());
    CHECK(book.MarkTold(key, proposal));
    CHECK_FALSE(book.MarkTold(key, proposal));
    REQUIRE(session.SaveProfiles());
  }
  CompanionAiSession session;
  session.Open(root.path(), kCid);
  CompanionProfileBook book(session.profiles(), session.edits());
  // RETENU DANS LA FICHE : la connexion suivante ne le redit pas.
  CHECK_FALSE(book.MarkTold(key, proposal));
  // UNE RÉPONSE l'efface : une nouvelle proposition pareille se redirait.
  book.Answer(key, proposal, false);
  CHECK(book.MarkTold(key, proposal));
  // Sans personnage connu, rien ne se retient.
  CHECK_FALSE(book.MarkTold(CharacterKey{}, proposal));
}

TEST_CASE("Conduite depuis le menu d'un joueur : l'IA basique cède la place à « Mon profil »",
          "[app][companion_ai_compose]") {
  Bench bench;
  REQUIRE(bench.seat.applied.has_value());
  CHECK(bench.seat.applied->second == kBasicProfileName);
  const moonclient::AiEditResult set = moonclient::SetPlayerConduct(
      bench.session, bench.Seated(), "Rival", ro::sim::TacticRank::kIgnore, std::nullopt);
  REQUIRE(set.accepted);
  CHECK(bench.seat.applied->second == kOwnProfileName);
  const moonclient::CompanionProfile* own = bench.session.profiles().Find(kLifJob, kOwnProfileName);
  REQUIRE(own != nullptr);
  const ro::sim::PlayerConductKey rival{.kind = ro::sim::PlayerConductKeyKind::kPlayer,
                                        .name = "Rival"};
  const ro::sim::CompanionPlayerConduct* line = own->conducts.Find(rival);
  REQUIRE(line != nullptr);
  CHECK(line->rank == ro::sim::TacticRank::kIgnore);
  CHECK(bench.scene.settings.player_conducts.Find(rival) != nullptr);
  REQUIRE(moonclient::RemovePlayerConduct(bench.session, bench.Seated(), "Rival").accepted);
  CHECK(own->conducts.Find(rival) == nullptr);
  // Retirer ce qui n'existe pas ne fait rien.
  CHECK_FALSE(moonclient::RemovePlayerConduct(bench.session, bench.Seated(), "Rival").accepted);
}

TEST_CASE("Classes de joueur proposées : ni monture ni costume, un libellé une fois",
          "[app][companion_ai_compose]") {
  const std::unordered_map<std::uint32_t, std::string> names{
      {1, "Swordman"},       {4002, "Swordman"},    {7, "Knight"},   {13, "Knight"},
      {4008, "Lord Knight"}, {4014, "Lord Knight"}, {22, "Wedding"}, {8, ""}};
  const std::vector<ro::ui::RuleChoice> choices = moonclient::PlayerJobChoices(names);
  REQUIRE(choices.size() == 3);
  CHECK(choices[0].label == "Knight");
  CHECK(choices[0].value == 7);
  CHECK(choices[1].value == 4008);
  CHECK(choices[2].label == "Swordman");
  CHECK(choices[2].value == 1);
}

TEST_CASE("Renommer « Mon profil » : un personnage sans choix PvE garde son profil",
          "[app][companion_ai_compose]") {
  Bench bench;
  CompanionProfileBook book = bench.Book();
  REQUIRE(book.Create(kLifJob, kOwnProfileName, kBasicProfileName) == ProfileRefusal::kNone);
  const CharacterKey other{.cid = kCid + 1, .name = "Autre"};
  book.Know(other);
  REQUIRE(book.Active(other, kLifJob, CompanionAiContext::kPvE) == kOwnProfileName);
  REQUIRE(book.Rename(kLifJob, kOwnProfileName, "Principal") == ProfileRefusal::kNone);
  CHECK(book.Active(other, kLifJob, CompanionAiContext::kPvE) == "Principal");
  CHECK(book.Active(bench.session.character(), kLifJob, CompanionAiContext::kPvE) == "Principal");
}

TEST_CASE("Un geste de la fenêtre : le nombre de règles survit à l'enregistrement",
          "[app][companion_ai_compose]") {
  Bench bench;
  REQUIRE(bench.Book().Create(kLifJob, "Farm", kBasicProfileName) == ProfileRefusal::kNone);
  ro::ui::CompanionAiWindowAction action;
  action.verb = ro::ui::CompanionAiVerb::kAddRule;
  action.profile = "Farm";
  const moonclient::AiWindowOutcome outcome =
      moonclient::ApplyAiWindowGesture(action, bench.session, bench.Seated(), {}, {}, {}, {});
  REQUIRE(outcome.edit.accepted);
  CHECK_FALSE(outcome.gesture.handled);
  CHECK(outcome.rule_count == 1);
  // L'ENREGISTREMENT REMPLACE LES PROFILS EN MÉMOIRE : le compte relevé avant
  // dit toujours le profil relu.
  REQUIRE(bench.session.SaveProfiles());
  const moonclient::CompanionProfile* farm = bench.session.profiles().Find(kLifJob, "Farm");
  REQUIRE(farm != nullptr);
  CHECK(farm->rules.size() == outcome.rule_count);
}
