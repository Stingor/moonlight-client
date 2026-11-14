// LES PROFILS NOMMÉS DE L'IA DES COMPAGNONS : leurs opérations et leurs bornes,
// le profil actif par contexte et ses replis, l'import avec un skill absent, la
// proposition de recopie après une évolution, et leur fichier.

#include "game/companion_ai_profiles.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_players.h"
#include "ro/sim/companion_rules.h"

#include "game/companion_ai_file.h"

using moonclient::CharacterKey;
using moonclient::CompanionProfile;
using moonclient::CompanionProfileBook;
using moonclient::CompanionProfileEdits;
using moonclient::CompanionProfiles;
using moonclient::EvolutionProposal;
using moonclient::kBasicProfileName;
using moonclient::kOwnProfileName;
using moonclient::ProfileRefusal;
using ro::sim::CompanionAiContext;

namespace {

constexpr std::uint16_t kLifJob = 6001;
constexpr std::uint16_t kLif2Job = 6009;
constexpr std::uint16_t kArcherJob = 6017;
constexpr std::uint32_t kCid = 150001;
constexpr std::uint16_t kHealId = 8001;
constexpr std::uint16_t kArcherSkillId = 8207;

struct Bench {
  CompanionProfiles profiles;
  CompanionProfileEdits edits;
  CompanionProfileBook book{profiles, edits};
  CharacterKey me{.cid = kCid, .name = "Stingor"};
};

[[nodiscard]] ro::sim::CompanionRule CastRule(std::uint16_t skill_id) {
  return ro::sim::CompanionRule{
      .action = ro::sim::RuleAction::kCastSkill,
      .aim = ro::sim::RuleAim::kOwner,
      .skill_id = skill_id,
      .conditions = {ro::sim::RuleCondition{.subject = ro::sim::RuleSubject::kOwner,
                                            .predicate = ro::sim::RulePredicate::kHpPercent,
                                            .comparison = ro::sim::RuleComparison::kBelow,
                                            .number = 50}}};
}

}  // namespace

TEST_CASE("Profils nommés : créer, renommer, dupliquer, supprimer", "[app][companion_profiles]") {
  Bench bench;
  CompanionProfileBook& book = bench.book;
  REQUIRE(book.Create(kLifJob, "  Chasse ", kBasicProfileName) == ProfileRefusal::kNone);
  REQUIRE(bench.profiles.Find(kLifJob, "Chasse") != nullptr);
  CHECK(book.Create(kLifJob, "Chasse", kBasicProfileName) == ProfileRefusal::kNameTaken);
  CHECK(book.Create(kLifJob, "", kBasicProfileName) == ProfileRefusal::kEmptyName);
  CHECK(book.Create(kLifJob, std::string(moonclient::kProfileNameMostBytes + 1, 'a'),
                    kBasicProfileName) == ProfileRefusal::kNameTooLong);
  CHECK(book.Create(kLifJob, "Tab\there", kBasicProfileName) == ProfileRefusal::kBadCharacter);
  CHECK(book.Create(kLifJob, kBasicProfileName, kBasicProfileName) ==
        ProfileRefusal::kReservedName);
  CHECK(book.Create(kLifJob, "Copie", "Absent") == ProfileRefusal::kUnknownProfile);

  // DUPLIQUER, C'EST CRÉER DEPUIS UN PROFIL : la copie porte ses règles.
  bench.profiles.Find(kLifJob, "Chasse")->rules = {CastRule(kHealId)};
  bench.profiles.Find(kLifJob, "Chasse")->rule_extras = {""};
  REQUIRE(book.Create(kLifJob, "Chasse 2", "Chasse") == ProfileRefusal::kNone);
  CHECK(bench.profiles.Find(kLifJob, "Chasse 2")->rules ==
        bench.profiles.Find(kLifJob, "Chasse")->rules);

  // RENOMMER : la fiche qui le désigne suit.
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvP, "Chasse") ==
          ProfileRefusal::kNone);
  CHECK(book.Rename(kLifJob, "Chasse", "Chasse 2") == ProfileRefusal::kNameTaken);
  REQUIRE(book.Rename(kLifJob, "Chasse", "Arène") == ProfileRefusal::kNone);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvP) == "Arène");
  CHECK(book.Rename(kLifJob, kBasicProfileName, "X") == ProfileRefusal::kBasicLocked);

  // SUPPRIMER : la fiche retombe sur son repli.
  REQUIRE(book.Delete(kLifJob, "Arène") == ProfileRefusal::kNone);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvP) == kBasicProfileName);
  CHECK(book.Delete(kLifJob, kBasicProfileName) == ProfileRefusal::kBasicLocked);
  CHECK(book.Delete(kLifJob, "Arène") == ProfileRefusal::kUnknownProfile);

  // LE PLAFOND PAR TYPE.
  for (std::size_t made = bench.profiles.by_job.at(kLifJob).size();
       made < moonclient::kCompanionProfileMost; ++made) {
    REQUIRE(book.Create(kLifJob, "P" + std::to_string(made), kBasicProfileName) ==
            ProfileRefusal::kNone);
  }
  CHECK(book.Create(kLifJob, "De trop", kBasicProfileName) == ProfileRefusal::kTypeFull);
  // Un nom libre se trouve toujours, dans sa longueur.
  CHECK(moonclient::FreeProfileName(bench.profiles, kLifJob, "Chasse 2") == "Chasse 2 (2)");
}

TEST_CASE("Profils nommés : le profil actif par contexte, et ses replis",
          "[app][companion_profiles]") {
  Bench bench;
  CompanionProfileBook& book = bench.book;
  REQUIRE(book.Create(kLifJob, "Farm", kBasicProfileName) == ProfileRefusal::kNone);
  REQUIRE(book.Create(kLifJob, "Duel", kBasicProfileName) == ProfileRefusal::kNone);
  // Sans fiche : l'IA basique partout.
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kWoE) == kBasicProfileName);
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvE, "Farm") ==
          ProfileRefusal::kNone);
  // UN CONTEXTE SANS CHOIX PREND CELUI DU PVE.
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvP) == "Farm");
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvP, "Duel") ==
          ProfileRefusal::kNone);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvP) == "Duel");
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kWoE) == "Farm");
  // L'IA basique se choisit explicitement ; un profil inconnu, non.
  CHECK(book.SetActive(bench.me, kLifJob, CompanionAiContext::kWoE, kBasicProfileName) ==
        ProfileRefusal::kNone);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kWoE) == kBasicProfileName);
  CHECK(book.SetActive(bench.me, kLifJob, CompanionAiContext::kWoE, "Absent") ==
        ProfileRefusal::kUnknownProfile);
  // UN AUTRE PERSONNAGE a ses choix à lui.
  const CharacterKey other{.cid = kCid + 1, .name = "Autre"};
  CHECK(book.Active(other, kLifJob, CompanionAiContext::kPvE) == kBasicProfileName);
  // SANS CID, LE NOM SERT DE CLÉ.
  const CharacterKey by_name{.name = "Stingor"};
  CHECK(book.Active(by_name, kLifJob, CompanionAiContext::kPvE) == kBasicProfileName);
  REQUIRE(book.SetActive(by_name, kLifJob, CompanionAiContext::kPvE, "Duel") ==
          ProfileRefusal::kNone);
  CHECK(book.Active(by_name, kLifJob, CompanionAiContext::kPvE) == "Duel");
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvE) == "Farm");
}

TEST_CASE("Profils nommés : l'import d'un autre type garde un skill absent",
          "[app][companion_profiles]") {
  Bench bench;
  CompanionProfileBook& book = bench.book;
  REQUIRE(book.Create(kArcherJob, "Tir", kBasicProfileName) == ProfileRefusal::kNone);
  CompanionProfile& archer = *bench.profiles.Find(kArcherJob, "Tir");
  archer.rules = {CastRule(kArcherSkillId), CastRule(kHealId)};
  archer.rule_extras = {"", ""};

  REQUIRE(book.Import(kLifJob, "Tir", kArcherJob, "Tir") == ProfileRefusal::kNone);
  const CompanionProfile* copied = bench.profiles.Find(kLifJob, "Tir");
  REQUIRE(copied != nullptr);
  // RIEN N'EST RETIRÉ : la règle d'un skill que Lif n'a pas reste, infaisable.
  CHECK(copied->rules == archer.rules);
  const std::vector<std::uint16_t> lif_skills = {kHealId};
  CHECK(moonclient::RulesMissingSkill(copied->rules, lif_skills) == std::vector<std::size_t>{0});
  // DES RÉGLAGES ABSENTS VALENT L'USINE DE LEUR TYPE : un archer de guilde
  // n'attaque pas de lui-même, et la copie le garde.
  REQUIRE(copied->settings.has_value());
  CHECK(copied->settings->attack_on_own ==
        ro::sim::BasicAggression(ro::sim::AiCompanion::kMercenary, kArcherJob));
  // L'import écrit le profil en entier.
  const CompanionProfileEdits::ProfileEdits* written = bench.edits.Find(kLifJob, "Tir");
  REQUIRE(written != nullptr);
  CHECK(written->whole);
  CHECK(book.Import(kLifJob, "Tir", kArcherJob, "Tir") == ProfileRefusal::kNameTaken);
  CHECK(book.Import(kLifJob, "Rien", kArcherJob, "Absent") == ProfileRefusal::kUnknownProfile);
}

TEST_CASE("Profils nommés : la proposition après évolution, une fois, refus retenu",
          "[app][companion_profiles]") {
  Bench bench;
  CompanionProfileBook& book = bench.book;
  // UNE PREMIÈRE FORME SANS PROFIL : elle devient la forme vue, sans proposition.
  CHECK_FALSE(book.NoteForm(bench.me, kLifJob).has_value());
  REQUIRE(book.Create(kLifJob, "Soins", kBasicProfileName) == ProfileRefusal::kNone);
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvE, "Soins") ==
          ProfileRefusal::kNone);
  // UN MERCENAIRE ne se confond pas avec l'homoncule : son genre a sa forme.
  CHECK_FALSE(book.NoteForm(bench.me, kArcherJob).has_value());

  SECTION("acceptée : les profils et les choix suivent") {
    const std::optional<EvolutionProposal> proposal = book.NoteForm(bench.me, kLif2Job);
    REQUIRE(proposal == EvolutionProposal{.from_job = kLifJob, .to_job = kLif2Job});
    // Tant qu'elle attend, elle se redit.
    CHECK(book.NoteForm(bench.me, kLif2Job) == proposal);
    book.Answer(bench.me, *proposal, true);
    CHECK(bench.profiles.Find(kLif2Job, "Soins") != nullptr);
    CHECK(book.Active(bench.me, kLif2Job, CompanionAiContext::kPvE) == "Soins");
    CHECK_FALSE(book.NoteForm(bench.me, kLif2Job).has_value());
  }
  SECTION("refusée : elle ne revient pas, même après un aller-retour") {
    const std::optional<EvolutionProposal> proposal = book.NoteForm(bench.me, kLif2Job);
    REQUIRE(proposal.has_value());
    book.Answer(bench.me, *proposal, false);
    CHECK(bench.profiles.Find(kLif2Job, "Soins") == nullptr);
    CHECK_FALSE(book.NoteForm(bench.me, kLif2Job).has_value());
    // La forme vue est maintenant la nouvelle : revenir à l'ancienne ne propose
    // rien (elle a des profils), et la paire refusée reste refusée.
    CHECK_FALSE(book.NoteForm(bench.me, kLifJob).has_value());
    CHECK_FALSE(book.NoteForm(bench.me, kLif2Job).has_value());
  }
  SECTION("le refus survit au fichier") {
    const std::optional<EvolutionProposal> proposal = book.NoteForm(bench.me, kLif2Job);
    REQUIRE(proposal.has_value());
    book.Answer(bench.me, *proposal, false);
    const moonclient::CompanionAiFileRead read =
        moonclient::ParseCompanionAi(moonclient::FormatCompanionAi(bench.profiles));
    CHECK(read.profiles == bench.profiles);
    CompanionProfiles reread = read.profiles;
    CompanionProfileEdits more;
    CompanionProfileBook again(reread, more);
    // REVENU À L'ANCIENNE FORME, puis évolué de nouveau : seule la paire refusée,
    // relue du fichier, retient la proposition.
    CHECK_FALSE(again.NoteForm(bench.me, kLifJob).has_value());
    CHECK_FALSE(again.NoteForm(bench.me, kLif2Job).has_value());
    // Le témoin : sans le refus, elle reviendrait.
    CompanionProfiles forgotten = read.profiles;
    forgotten.characters.front().declined.clear();
    CompanionProfileEdits none;
    CompanionProfileBook blank(forgotten, none);
    CHECK_FALSE(blank.NoteForm(bench.me, kLifJob).has_value());
    CHECK(blank.NoteForm(bench.me, kLif2Job).has_value());
  }
  SECTION("posée, elle reste même si un profil naît pour la nouvelle classe") {
    const std::optional<EvolutionProposal> proposal = book.NoteForm(bench.me, kLif2Job);
    REQUIRE(proposal.has_value());
    REQUIRE(book.Create(kLif2Job, kOwnProfileName, kBasicProfileName) == ProfileRefusal::kNone);
    CHECK(book.NoteForm(bench.me, kLif2Job) == proposal);
    // Elle survit au fichier.
    CompanionProfiles reread =
        moonclient::ParseCompanionAi(moonclient::FormatCompanionAi(bench.profiles)).profiles;
    CompanionProfileEdits more;
    CompanionProfileBook again(reread, more);
    CHECK(again.NoteForm(bench.me, kLif2Job) == proposal);
    // ACCEPTÉE : le profil pris garde le sien, la copie prend un nom libre, et
    // chaque contexte prend le profil actif de l'ancienne forme.
    book.Answer(bench.me, *proposal, true);
    CHECK(bench.profiles.Find(kLif2Job, kOwnProfileName) != nullptr);
    CHECK(bench.profiles.Find(kLif2Job, "Soins") != nullptr);
    CHECK(book.Active(bench.me, kLif2Job, CompanionAiContext::kWoE) == "Soins");
    CHECK_FALSE(book.NoteForm(bench.me, kLif2Job).has_value());
  }
}

TEST_CASE("Profils nommés : sans aucun choix, « Mon profil » avant l'IA basique",
          "[app][companion_profiles]") {
  Bench bench;
  CompanionProfileBook& book = bench.book;
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvE) == kBasicProfileName);
  REQUIRE(book.Create(kLifJob, kOwnProfileName, kBasicProfileName) == ProfileRefusal::kNone);
  for (const CompanionAiContext context :
       {CompanionAiContext::kPvE, CompanionAiContext::kPvP, CompanionAiContext::kWoE}) {
    CHECK(book.Active(bench.me, kLifJob, context) == kOwnProfileName);
  }
  // UN CHOIX EXPLICITE l'emporte.
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvE, kBasicProfileName) ==
          ProfileRefusal::kNone);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvP) == kBasicProfileName);
}

TEST_CASE("Profils nommés : l'import d'un genre à l'autre", "[app][companion_profiles]") {
  constexpr std::uint16_t kFilirJob = 6003;
  Bench bench;
  CompanionProfileBook& book = bench.book;
  // L'IA BASIQUE d'un Filir, qui attaque de lui-même, vers un archer de guilde,
  // qui ne le fait pas d'usine : la copie garde l'usine du Filir.
  REQUIRE(book.Import(kArcherJob, "Filir", kFilirJob, kBasicProfileName) == ProfileRefusal::kNone);
  const CompanionProfile* filir = bench.profiles.Find(kArcherJob, "Filir");
  REQUIRE(filir != nullptr);
  REQUIRE(filir->settings.has_value());
  CHECK(filir->settings->attack_on_own);
  // ET D'UN MERCENAIRE VERS UN HOMONCULE, règle et skill absent compris.
  bench.profiles.Find(kArcherJob, "Filir")->rules = {CastRule(kArcherSkillId)};
  bench.profiles.Find(kArcherJob, "Filir")->rule_extras = {""};
  REQUIRE(book.Import(kLifJob, "Archer", kArcherJob, "Filir") == ProfileRefusal::kNone);
  const CompanionProfile* lif = bench.profiles.Find(kLifJob, "Archer");
  REQUIRE(lif != nullptr);
  CHECK(lif->rules.size() == 1);
  CHECK(moonclient::RulesMissingSkill(lif->rules, std::vector<std::uint16_t>{kHealId}).size() == 1);
  CHECK(lif->settings->attack_on_own);
}

TEST_CASE("Profils nommés : les libellés réservés et les contrôles C1",
          "[app][companion_profiles]") {
  Bench bench;
  for (const char* reserved : {"IA basique", "IA básica", "Mon profil", "Mi perfil", "Basic AI"}) {
    CHECK(bench.book.Create(kLifJob, reserved, kBasicProfileName) == ProfileRefusal::kReservedName);
    CHECK_FALSE(moonclient::ValidProfileName(reserved));
  }
  // La clé « My profile » elle-même reste permise : c'est ce profil.
  CHECK(bench.book.Create(kLifJob, kOwnProfileName, kBasicProfileName) == ProfileRefusal::kNone);
  // U+0085 (NEL), un contrôle C1, en UTF-8.
  const std::string c1 = std::string("Nom") + "\xC2\x85" + "suite";
  CHECK(bench.book.Create(kLifJob, c1, kBasicProfileName) == ProfileRefusal::kBadCharacter);
  // Un caractère accentué n'en est pas un.
  CHECK(bench.book.Create(kLifJob, "Arène", kBasicProfileName) == ProfileRefusal::kNone);
}

TEST_CASE(
    "Profils nommés : la fusion garde les nœuds illisibles d'une fiche et une conduite en "
    "double",
    "[app][companion_profiles]") {
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6001\n"
      "    profiles:\n"
      "      - name: Farm\n"
      "        players:\n"
      "          - job: 8\n"
      "            rank: high\n"
      "          - job: 8\n"
      "            rank: low\n"
      "characters:\n"
      "  - cid: 150001\n"
      "    name: Stingor\n"
      "    companions:\n"
      "      - class: not-a-class\n"
      "        pve: Farm\n"
      "      - class: 6001\n"
      "        pve: Farm\n"
      "    previous_forms:\n"
      "      homunculus: 6001\n"
      "      dragon: 7\n"
      "    declined:\n"
      "      - from: nowhere\n";
  Bench bench;
  bench.profiles = moonclient::ParseCompanionAi(current).profiles;
  const CompanionProfile* farm = bench.profiles.Find(kLifJob, "Farm");
  REQUIRE(farm != nullptr);
  REQUIRE(farm->conducts.lines().size() == 1);
  CHECK(farm->conducts.lines().front().rank == ro::sim::TacticRank::kHigh);
  REQUIRE(bench.book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvP, "Farm") ==
          ProfileRefusal::kNone);
  bench.edits.SetConducts(kLifJob, "Farm", farm->conducts);
  const std::optional<std::string> merged = moonclient::MergeCompanionAi(current, bench.edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("not-a-class") != std::string::npos);
  CHECK(merged->find("dragon: 7") != std::string::npos);
  CHECK(merged->find("nowhere") != std::string::npos);
  CHECK(merged->find("rank: low") != std::string::npos);
  moonclient::CompanionAiFileRead read = moonclient::ParseCompanionAi(*merged);
  CHECK(read.profiles.Find(kLifJob, "Farm")->conducts.lines().front().rank ==
        ro::sim::TacticRank::kHigh);
  CHECK(CompanionProfileBook(read.profiles, bench.edits)
            .Active(bench.me, kLifJob, CompanionAiContext::kPvP) == "Farm");
}

TEST_CASE("Profils nommés : le fichier porte les profils, les fiches et les conduites",
          "[app][companion_profiles]") {
  Bench bench;
  CompanionProfileBook& book = bench.book;
  REQUIRE(book.Create(kLifJob, kOwnProfileName, kBasicProfileName) == ProfileRefusal::kNone);
  REQUIRE(book.Create(kLifJob, "Guerre", kBasicProfileName) == ProfileRefusal::kNone);
  CompanionProfile& woe = *bench.profiles.Find(kLifJob, "Guerre");
  REQUIRE(woe.conducts.Set(ro::sim::CompanionPlayerConduct{
      .key = ro::sim::PlayerConductKey{.kind = ro::sim::PlayerConductKeyKind::kJob, .job = 8},
      .rank = ro::sim::TacticRank::kAbsolute,
      .trigger = ro::sim::TacticTrigger::kIfAttacks,
      .skill_use = ro::sim::TacticSkillUse::kTimes,
      .skill_use_count = 2,
      .opening_skill_id = kHealId,
      .sp_reserve_points = 30,
      .chase = ro::sim::TacticChase::kNever}));
  REQUIRE(woe.conducts.Set(ro::sim::CompanionPlayerConduct{
      .key = ro::sim::PlayerConductKey{.kind = ro::sim::PlayerConductKeyKind::kPlayer,
                                       .name = "Bob l'Éponge"}}));
  REQUIRE(woe.conducts.Set(ro::sim::CompanionPlayerConduct{
      .key = ro::sim::PlayerConductKey{.kind = ro::sim::PlayerConductKeyKind::kOwnerGuild},
      .rank = ro::sim::TacticRank::kIgnore}));
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kWoE, "Guerre") ==
          ProfileRefusal::kNone);
  REQUIRE(book.SetActive(bench.me, kLifJob, CompanionAiContext::kPvE, kOwnProfileName) ==
          ProfileRefusal::kNone);

  const std::string text = moonclient::FormatCompanionAi(bench.profiles);
  CHECK(text.find("version: 1") != std::string::npos);
  const moonclient::CompanionAiFileRead read = moonclient::ParseCompanionAi(text);
  CHECK(read.rejected.empty());
  CHECK(read.profiles == bench.profiles);
}

TEST_CASE("Profils nommés : la fusion renomme et retire sans rien perdre d'autre",
          "[app][companion_profiles]") {
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6001\n"
      "    profiles:\n"
      "      - name: Farm\n"
      "        note: keep-me\n"
      "        tactics:\n"
      "          - monster: 1002\n"
      "            rank: high\n"
      "      - name: Old\n"
      "      - name: Basic AI\n"
      "characters:\n"
      "  - cid: 150001\n"
      "    name: Stingor\n"
      "    companions:\n"
      "      - class: 6001\n"
      "        pve: Farm\n"
      "        woe: Old\n"
      "  - cid: 150002\n"
      "    name: Autre\n"
      "    mood: unknown-field\n"
      "    companions:\n"
      "      - class: 6001\n"
      "        pvp: Farm\n";
  Bench bench;
  bench.profiles = moonclient::ParseCompanionAi(current).profiles;
  // « Basic AI » est un nom réservé : écarté, mais gardé dans le fichier.
  CHECK(bench.profiles.by_job.at(kLifJob).size() == 2);
  REQUIRE(bench.book.Rename(kLifJob, "Farm", "Leveling") == ProfileRefusal::kNone);
  REQUIRE(bench.book.Delete(kLifJob, "Old") == ProfileRefusal::kNone);

  const std::optional<std::string> merged = moonclient::MergeCompanionAi(current, bench.edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("note: keep-me") != std::string::npos);
  CHECK(merged->find("mood: unknown-field") != std::string::npos);
  CHECK(merged->find("name: Basic AI") != std::string::npos);
  const moonclient::CompanionAiFileRead read = moonclient::ParseCompanionAi(*merged);
  const CompanionProfile* leveling = read.profiles.Find(kLifJob, "Leveling");
  REQUIRE(leveling != nullptr);
  CHECK(leveling->tactics.lines().size() == 1);
  CHECK(read.profiles.Find(kLifJob, "Old") == nullptr);
  CHECK(read.profiles.Find(kLifJob, "Farm") == nullptr);
  // LES DEUX FICHES SUIVENT : la mienne et celle d'un personnage non modifié.
  CompanionProfileEdits none;
  CompanionProfiles reread = read.profiles;
  const CompanionProfileBook book(reread, none);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvE) == "Leveling");
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kWoE) == "Leveling");
  CHECK(book.Active(CharacterKey{.cid = 150002}, kLifJob, CompanionAiContext::kPvP) == "Leveling");
}

TEST_CASE("Profils nommés : renommer vers un nom qu'un autre client vient d'écrire",
          "[app][companion_profiles]") {
  // Ce que la session a lu : « Farm » seul.
  const std::string read_before =
      "version: 1\n"
      "types:\n"
      "  - class: 6001\n"
      "    profiles:\n"
      "      - name: Farm\n"
      "        tactics:\n"
      "          - monster: 1002\n"
      "            rank: high\n"
      "characters:\n"
      "  - cid: 150001\n"
      "    name: Stingor\n"
      "    companions:\n"
      "      - class: 6001\n"
      "        pve: Farm\n"
      "        woe: Farm\n";
  // Ce qu'un autre client a écrit entre-temps : un « Leveling » à lui, que
  // l'un de ses personnages a choisi.
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6001\n"
      "    profiles:\n"
      "      - name: Farm\n"
      "        tactics:\n"
      "          - monster: 1002\n"
      "            rank: high\n"
      "      - name: Leveling\n"
      "        note: theirs\n"
      "characters:\n"
      "  - cid: 150001\n"
      "    name: Stingor\n"
      "    companions:\n"
      "      - class: 6001\n"
      "        pve: Farm\n"
      "        woe: Farm\n"
      "  - cid: 150002\n"
      "    name: Autre\n"
      "    companions:\n"
      "      - class: 6001\n"
      "        pvp: Leveling\n";
  Bench bench;
  bench.profiles = moonclient::ParseCompanionAi(read_before).profiles;
  REQUIRE(bench.book.Rename(kLifJob, "Farm", "Leveling") == ProfileRefusal::kNone);
  bench.edits.SetFriends(kLifJob, "Leveling", {"Ami"});
  // Une fiche de la session en attente, qui désigne le profil par son nouveau nom.
  const moonclient::CompanionCharacter* mine = bench.profiles.FindCharacter(bench.me);
  REQUIRE(mine != nullptr);
  bench.edits.SetCharacter(*mine);

  std::vector<moonclient::CompanionRenameShift> shifts;
  const std::optional<std::string> merged =
      moonclient::MergeCompanionAi(current, bench.edits, &shifts);
  REQUIRE(merged.has_value());
  // UN SEUL « Leveling », celui de l'autre client, intact.
  std::size_t leveling_count = 0;
  for (std::size_t at = merged->find("name: Leveling\n"); at != std::string::npos;
       at = merged->find("name: Leveling\n", at + 1)) {
    ++leveling_count;
  }
  CHECK(leveling_count == 1);
  CHECK(merged->find("note: theirs") != std::string::npos);
  REQUIRE(shifts.size() == 1);
  CHECK(shifts[0].job == kLifJob);
  CHECK(shifts[0].wanted == "Leveling");
  CHECK(shifts[0].written == "Leveling (2)");

  // LE PROFIL RENOMMÉ, sous le nom libre : ses lignes, et la modification que
  // la session lui a portée.
  const moonclient::CompanionAiFileRead read = moonclient::ParseCompanionAi(*merged);
  const CompanionProfile* renamed = read.profiles.Find(kLifJob, "Leveling (2)");
  REQUIRE(renamed != nullptr);
  CHECK(renamed->tactics.lines().size() == 1);
  CHECK(renamed->friends == std::vector<std::string>{"Ami"});
  const CompanionProfile* theirs = read.profiles.Find(kLifJob, "Leveling");
  REQUIRE(theirs != nullptr);
  CHECK(theirs->tactics.lines().empty());
  CHECK(theirs->friends.empty());
  CHECK(read.profiles.Find(kLifJob, "Farm") == nullptr);

  // Chaque fiche garde le profil qu'elle désignait.
  CompanionProfileEdits none;
  CompanionProfiles reread = read.profiles;
  const CompanionProfileBook book(reread, none);
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kPvE) == "Leveling (2)");
  CHECK(book.Active(bench.me, kLifJob, CompanionAiContext::kWoE) == "Leveling (2)");
  CHECK(book.Active(CharacterKey{.cid = 150002}, kLifJob, CompanionAiContext::kPvP) == "Leveling");
}
