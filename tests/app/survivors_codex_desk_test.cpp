// LES FICHES DU CODEX, tirées des règles livrées : chaque nombre vient du
// catalogue, chaque entrée a sa fiche, et une entrée non vue le reste tant que
// ni le carnet, ni la partie en cours, ni ce que le carnet permet de déduire
// ne l'ont montrée. Ouvert sur la pause, le Codex prend Échap et la molette.

#include "game/survivors_codex_desk.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_rules.h"
#include "ro/ui/survivors_codex_window.h"

#include "survivors/arena_night.h"
#include "survivors/survivors_codex_keys.h"
#include "survivors/survivors_gems.h"
#include "survivors/survivors_route.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_sightings.h"
#include "survivors/survivors_stage.h"

using moonclient::CodexCategory;
using moonclient::CodexSeenOf;
using moonclient::ComposeSurvivorsCodex;
using moonclient::SurvivorsCodexRowsHeld;
using moonclient::SurvivorsCodexSeen;
using moonclient::SurvivorsSave;
using ro::ui::SurvivorsCodexEntry;
using ro::ui::SurvivorsCodexFigureLabel;
using ro::ui::SurvivorsCodexSheet;
using ro::ui::SurvivorsCodexSourceKind;
using ro::ui::SurvivorsCodexTab;
using ro::ui::SurvivorsCodexView;

namespace {

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

constexpr std::uint32_t kSecondMs = 1000;
constexpr std::uint32_t kMinuteMs = 60 * kSecondMs;
constexpr std::uint32_t kHalfMinuteMs = 30 * kSecondMs;

constexpr std::uint16_t kFireBolt = 19;
constexpr std::uint16_t kMeteorStorm = 83;
constexpr std::uint16_t kHornetCard = 4019;
constexpr std::uint16_t kLunatic = 1063;
constexpr std::uint16_t kPoring = 1002;
constexpr std::uint16_t kChonchon = 1011;
constexpr std::uint16_t kPupa = 1008;
constexpr std::uint16_t kHornet = 1004;
constexpr std::uint16_t kGoblin = 1122;
constexpr std::uint16_t kGhoul = 1036;
constexpr std::uint16_t kWhisper = 1179;
constexpr std::uint16_t kBaphometJr = 1101;
constexpr std::uint16_t kMastering = 1090;
constexpr std::uint16_t kBaphomet = 1039;
constexpr std::size_t kBaphometMoves = 6;
constexpr std::uint32_t kEliteIntervalMs = 75 * kSecondMs;

// Les rangs des éléments, dans l'ordre du serveur.
constexpr std::uint8_t kNeutral = 0;
constexpr std::uint8_t kWater = 1;
constexpr std::uint8_t kFire = 3;
constexpr std::uint8_t kWind = 4;

struct Composed {
  SurvivorsCodexRowsHeld held;
  SurvivorsCodexView view;
};

[[nodiscard]] std::span<const SurvivorsCodexEntry> TabOf(const SurvivorsCodexView& view,
                                                         SurvivorsCodexTab tab) {
  return view.entries[static_cast<std::size_t>(tab)];
}

void Compose(Composed& composed, const SurvivorsCodexSeen& seen = {}) {
  composed.view = ComposeSurvivorsCodex(Rules(), seen, composed.held);
}

[[nodiscard]] const SurvivorsCodexEntry& EntryOf(const SurvivorsCodexView& view,
                                                 SurvivorsCodexTab tab, std::uint16_t ro_id,
                                                 SurvivorsCodexSheet sheet) {
  const auto entries = TabOf(view, tab);
  const auto found = std::ranges::find_if(entries, [&](const SurvivorsCodexEntry& entry) {
    return entry.ro_id == ro_id && entry.sheet == sheet;
  });
  REQUIRE(found != entries.end());
  return *found;
}

[[nodiscard]] const SurvivorsCodexEntry& EntryByKey(const SurvivorsCodexView& view,
                                                    SurvivorsCodexTab tab, std::string_view key) {
  const auto entries = TabOf(view, tab);
  const auto found = std::ranges::find_if(
      entries, [&](const SurvivorsCodexEntry& entry) { return key == entry.key; });
  REQUIRE(found != entries.end());
  return *found;
}

[[nodiscard]] bool HasFigure(const SurvivorsCodexEntry& entry, SurvivorsCodexFigureLabel label) {
  return std::ranges::any_of(entry.figures,
                             [label](const auto& figure) { return figure.label == label; });
}

[[nodiscard]] std::optional<std::int32_t> PctOf(
    std::span<const ro::ui::SurvivorsCodexAffinity> list, std::uint8_t element) {
  for (const auto& affinity : list) {
    if (affinity.element == element) return affinity.pct;
  }
  return std::nullopt;
}

[[nodiscard]] std::size_t IndexOf(std::span<const SurvivorsCodexEntry> entries,
                                  std::string_view key) {
  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (key == entries[index].key) return index;
  }
  // Absente : la taille de l'onglet, après toute entrée présente.
  return entries.size();
}

}  // namespace

TEST_CASE("Codex : la fiche de Fire Bolt, une colonne par niveau, et son évolution",
          "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const SurvivorsCodexEntry& fire_bolt =
      EntryOf(composed.view, SurvivorsCodexTab::kSkills, kFireBolt, SurvivorsCodexSheet::kSkill);
  const ro::sim::SurvivorsSkillRule& rule = *ro::sim::FindSkill(Rules(), kFireBolt);
  CHECK(HasFigure(fire_bolt, SurvivorsCodexFigureLabel::kDamage));
  CHECK(HasFigure(fire_bolt, SurvivorsCodexFigureLabel::kCooldown));
  for (const auto& figure : fire_bolt.figures) {
    CHECK(figure.count == rule.levels.size());
    CHECK(std::any_of(figure.values.begin(), figure.values.begin() + figure.count,
                      [](std::int64_t value) { return value != 0; }));
  }
  CHECK(fire_bolt.figures.front().values[0] == rule.levels[0].damage);
  CHECK(std::string_view(fire_bolt.description) == rule.description);
  REQUIRE(fire_bolt.has_evolution);
  CHECK(fire_bolt.evolution.card.ro_id == kHornetCard);
  CHECK(fire_bolt.evolution.result.ro_id == kMeteorStorm);
  CHECK(fire_bolt.evolution.max_level == rule.levels.size());
  const SurvivorsCodexEntry& meteor =
      EntryOf(composed.view, SurvivorsCodexTab::kSkills, kMeteorStorm, SurvivorsCodexSheet::kSkill);
  REQUIRE(meteor.evolved);
  CHECK(meteor.evolved_from.result.ro_id == kFireBolt);
  CHECK(meteor.evolved_from.card.ro_id == kHornetCard);
}

TEST_CASE("Codex : une posture chiffre son bonus, dans l'unité de sa statistique",
          "[app][survivors][codex]") {
  constexpr std::uint16_t kEndure = 8;
  constexpr std::uint16_t kTwoHandQuicken = 60;
  constexpr std::uint16_t kFrenzy = 359;
  using Label = SurvivorsCodexFigureLabel;
  using Unit = ro::ui::SurvivorsCodexUnit;
  const struct {
    std::uint16_t skill_id;
    Label label;
    Unit unit;
  } kStances[] = {{kEndure, Label::kStanceArmor, Unit::kPlain},
                  {kTwoHandQuicken, Label::kStanceCooldown, Unit::kPercent},
                  {kFrenzy, Label::kStanceDamage, Unit::kPercent}};
  for (const auto& stance : kStances) {
    const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(Rules(), stance.skill_id);
    REQUIRE(rule != nullptr);
    const auto figures = moonclient::SkillFiguresOf(*rule);
    REQUIRE_FALSE(figures.empty());
    CHECK(figures.front().label == stance.label);
    CHECK(figures.front().unit == stance.unit);
    for (std::size_t level = 0; level < rule->levels.size(); ++level) {
      CHECK(figures.front().values[level] == rule->levels[level].stance_bonus);
    }
    // Une posture ne se compte pas : la quantité par défaut se tait.
    CHECK(std::ranges::none_of(figures,
                               [](const auto& figure) { return figure.label == Label::kAmount; }));
  }
  // Un skill qui n'est pas une posture n'a aucune de ces lignes.
  const auto bolt = moonclient::SkillFiguresOf(*ro::sim::FindSkill(Rules(), kFireBolt));
  CHECK(bolt.front().label == Label::kDamage);
}

TEST_CASE("Codex : l'union se lit sur ses deux fiches, l'évolué dit ses deux sources",
          "[app][survivors][codex]") {
  constexpr std::uint16_t kBowlingBash = 62;
  constexpr std::uint16_t kBrandishSpear = 57;
  constexpr std::uint16_t kIgnitionBreak = 2006;
  const ro::sim::EvolutionRule* joined = ro::sim::FindEvolutionOf(Rules(), kBowlingBash);
  REQUIRE(joined != nullptr);
  REQUIRE(joined->partner_skill_id == kBrandishSpear);
  SurvivorsCodexRowsHeld held;
  const SurvivorsCodexView view = ComposeSurvivorsCodex(Rules(), SurvivorsCodexSeen{}, held, {});
  const auto skill_entry = [&](std::uint16_t skill_id) -> const SurvivorsCodexEntry& {
    return EntryOf(view, SurvivorsCodexTab::kSkills, skill_id, SurvivorsCodexSheet::kSkill);
  };
  const SurvivorsCodexEntry& first = skill_entry(kBowlingBash);
  REQUIRE(first.has_evolution);
  CHECK(first.evolution.union_of_two);
  CHECK(first.evolution.card.ro_id == kBrandishSpear);
  CHECK(first.evolution.result.ro_id == joined->evolved_skill_id);
  const SurvivorsCodexEntry& second = skill_entry(kBrandishSpear);
  CHECK_FALSE(second.has_evolution);
  REQUIRE(second.joins_union);
  CHECK(second.joined_union.union_of_two);
  CHECK(second.joined_union.card.ro_id == kBowlingBash);
  CHECK(second.joined_union.result.ro_id == joined->evolved_skill_id);
  const SurvivorsCodexEntry& evolved = skill_entry(joined->evolved_skill_id);
  REQUIRE(evolved.evolved);
  CHECK(evolved.evolved_from.union_of_two);
  CHECK(evolved.evolved_from.result.ro_id == kBowlingBash);
  CHECK(evolved.evolved_from.card.ro_id == kBrandishSpear);
  CHECK(joined->evolved_skill_id == kIgnitionBreak);
}

TEST_CASE("Codex : le second d'une union nomme aussi sa propre évolution",
          "[app][survivors][codex]") {
  constexpr std::uint16_t kColdBolt = 14;
  const ro::sim::EvolutionRule* own = ro::sim::FindEvolutionOf(Rules(), kColdBolt);
  REQUIRE(own != nullptr);
  const auto joined =
      std::ranges::find(Rules().evolutions, kColdBolt, &ro::sim::EvolutionRule::partner_skill_id);
  REQUIRE(joined != Rules().evolutions.end());
  SurvivorsCodexRowsHeld held;
  const SurvivorsCodexView view = ComposeSurvivorsCodex(Rules(), SurvivorsCodexSeen{}, held, {});
  const SurvivorsCodexEntry& bolt =
      EntryOf(view, SurvivorsCodexTab::kSkills, kColdBolt, SurvivorsCodexSheet::kSkill);
  REQUIRE(bolt.has_evolution);
  CHECK_FALSE(bolt.evolution.union_of_two);
  CHECK(bolt.evolution.card.ro_id == own->card_item_id);
  CHECK(bolt.evolution.result.ro_id == own->evolved_skill_id);
  REQUIRE(bolt.joins_union);
  CHECK(bolt.joined_union.union_of_two);
  CHECK(bolt.joined_union.card.ro_id == joined->skill_id);
  CHECK(bolt.joined_union.result.ro_id == joined->evolved_skill_id);
}

TEST_CASE("Codex : les skills se suivent classe après classe, évolués compris",
          "[app][survivors][codex]") {
  constexpr std::uint16_t kMageClass = 1;
  constexpr std::uint16_t kKnightClass = 2;
  constexpr std::uint16_t kMerchantClass = 5;
  constexpr std::uint16_t kBash = 5;
  constexpr std::uint16_t kFrenzy = 359;
  SurvivorsCodexRowsHeld held;
  const SurvivorsCodexView view = ComposeSurvivorsCodex(Rules(), SurvivorsCodexSeen{}, held, {});
  const auto entries = TabOf(view, SurvivorsCodexTab::kSkills);
  CHECK(
      EntryOf(view, SurvivorsCodexTab::kSkills, kFireBolt, SurvivorsCodexSheet::kSkill).class_id ==
      kMageClass);
  CHECK(EntryOf(view, SurvivorsCodexTab::kSkills, kMeteorStorm, SurvivorsCodexSheet::kSkill)
            .class_id == kMageClass);
  CHECK(EntryOf(view, SurvivorsCodexTab::kSkills, kBash, SurvivorsCodexSheet::kSkill).class_id ==
        kKnightClass);
  CHECK(EntryOf(view, SurvivorsCodexTab::kSkills, kFrenzy, SurvivorsCodexSheet::kSkill).class_id ==
        kKnightClass);
  CHECK(std::ranges::is_sorted(entries, {}, &SurvivorsCodexEntry::class_id));
  CHECK(entries.front().class_id == kMageClass);
  CHECK(entries.back().class_id == kMerchantClass);
}

TEST_CASE("Codex : l'heure d'arrivée de chaque monstre, par sa première source",
          "[app][survivors][codex]") {
  using moonclient::FoeArrivalOf;
  CHECK(FoeArrivalOf(Rules(), kLunatic).arrives_ms == 0U);
  // La nuée de 1:30 avant sa vague de 2:00.
  const auto chonchon = FoeArrivalOf(Rules(), kChonchon);
  CHECK(chonchon.arrives_ms == kMinuteMs + kHalfMinuteMs);
  REQUIRE_FALSE(chonchon.sources.empty());
  CHECK(chonchon.sources.front().kind == SurvivorsCodexSourceKind::kEvent);
  CHECK(FoeArrivalOf(Rules(), kPupa).arrives_ms == 2 * kMinuteMs + kHalfMinuteMs);
  CHECK(FoeArrivalOf(Rules(), kGoblin).arrives_ms == 10 * kMinuteMs + kHalfMinuteMs);
  const auto ghoul = FoeArrivalOf(Rules(), kGhoul);
  CHECK(ghoul.arrives_ms == 12 * kMinuteMs);
  CHECK(ghoul.sources.front().kind == SurvivorsCodexSourceKind::kWave);
  const auto baphomet_jr = FoeArrivalOf(Rules(), kBaphometJr);
  REQUIRE(baphomet_jr.sources.size() == 1);
  CHECK(baphomet_jr.sources.front().kind == SurvivorsCodexSourceKind::kSummon);
  CHECK(baphomet_jr.sources.front().bringer.ro_id == kBaphomet);
  // La vague silencieuse ne fait venir personne : aucune source à son heure.
  const ro::sim::WaveRule& silent = Rules().waves.back();
  REQUIRE(silent.floor == 0);
  for (const ro::sim::FoeRule& foe : Rules().foes) {
    for (const auto& source : FoeArrivalOf(Rules(), foe.foe_id).sources) {
      if (source.kind == SurvivorsCodexSourceKind::kWave) CHECK(source.at_ms != silent.from_ms);
    }
  }
}

TEST_CASE("Codex : faiblesses et résistances tirées de la table des éléments",
          "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const SurvivorsCodexEntry& poring =
      EntryOf(composed.view, SurvivorsCodexTab::kFoes, kPoring, SurvivorsCodexSheet::kFoe);
  CHECK(PctOf(poring.weaknesses, kWind) == 175);
  CHECK(PctOf(poring.resistances, kFire) == 50);
  CHECK(PctOf(poring.resistances, kWater) == 25);
  REQUIRE_FALSE(poring.resistances.empty());
  CHECK(poring.resistances.front().element == kWater);
  const SurvivorsCodexEntry& whisper =
      EntryOf(composed.view, SurvivorsCodexTab::kFoes, kWhisper, SurvivorsCodexSheet::kFoe);
  CHECK(PctOf(whisper.resistances, kNeutral) == 25);
}

TEST_CASE("Codex : la carte d'un monstre, une chance sur 300 ou garantie",
          "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const SurvivorsCodexEntry& hornet =
      EntryOf(composed.view, SurvivorsCodexTab::kFoes, kHornet, SurvivorsCodexSheet::kFoe);
  REQUIRE(hornet.drops_card);
  CHECK(hornet.card.odds == ro::sim::kCommonCardOdds);
  CHECK_FALSE(hornet.card.guaranteed);
  const SurvivorsCodexEntry& mastering =
      EntryOf(composed.view, SurvivorsCodexTab::kBosses, kMastering, SurvivorsCodexSheet::kBoss);
  REQUIRE(mastering.drops_card);
  CHECK(mastering.card.guaranteed);
}

TEST_CASE("Codex : les élites disent ce qui s'ajoute, et non le facteur des règles",
          "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const SurvivorsCodexEntry& elites =
      EntryByKey(composed.view, SurvivorsCodexTab::kBosses, moonclient::kCodexElitesKey);
  const ro::sim::EliteRule& rule = Rules().elites;
  // La partie multiplie PV, contact et XP par le facteur des règles, en % : la
  // fiche dit le facteur moins cent.
  constexpr std::int64_t kWhole = 100;
  const auto value_of = [&](SurvivorsCodexFigureLabel label) {
    const auto found =
        std::ranges::find(elites.figures, label, &ro::ui::SurvivorsCodexFigure::label);
    REQUIRE(found != elites.figures.end());
    CHECK(found->unit == ro::ui::SurvivorsCodexUnit::kPercent);
    return found->values[0];
  };
  CHECK(value_of(SurvivorsCodexFigureLabel::kHpBoost) == rule.hp_pct - kWhole);
  CHECK(value_of(SurvivorsCodexFigureLabel::kTouchBoost) == rule.touch_pct - kWhole);
  CHECK(value_of(SurvivorsCodexFigureLabel::kExpBoost) == rule.exp_pct - kWhole);
  // Les règles livrées : dix fois les PV, une fois et demie le contact, cinq
  // fois l'XP.
  CHECK(value_of(SurvivorsCodexFigureLabel::kHpBoost) == 900);
  CHECK(value_of(SurvivorsCodexFigureLabel::kTouchBoost) == 50);
  CHECK(value_of(SurvivorsCodexFigureLabel::kExpBoost) == 400);
}

TEST_CASE("Codex : le coffre dit la chance de chaque compte de gains", "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const ro::sim::ChestRule& chest = Rules().chests;
  const SurvivorsCodexEntry& box =
      EntryOf(composed.view, SurvivorsCodexTab::kItems, chest.item_id, SurvivorsCodexSheet::kChest);
  REQUIRE(box.figures.size() >= 2);
  const ro::ui::SurvivorsCodexFigure& triple = box.figures[0];
  const ro::ui::SurvivorsCodexFigure& quintuple = box.figures[1];
  CHECK(triple.label == SurvivorsCodexFigureLabel::kGainsOdds);
  CHECK(quintuple.label == SurvivorsCodexFigureLabel::kGainsOdds);
  CHECK(triple.gain_count == chest.triple);
  CHECK(quintuple.gain_count == chest.quintuple);
  // Le tirage teste les cinq gains d'abord : la plage des trois est ce qui
  // reste sous leur seuil.
  CHECK(triple.values[0] == chest.triple_per_10000 - chest.quintuple_per_10000);
  CHECK(quintuple.values[0] == chest.quintuple_per_10000);
  // Les règles livrées : 10 % de trois gains, 2 % de cinq.
  CHECK(triple.values[0] == 1000);
  CHECK(quintuple.values[0] == 200);
}

TEST_CASE("Codex : la fiche d'une entrée vue ne révèle pas ses liens non vus",
          "[app][survivors][codex]") {
  SurvivorsCodexSeen seen;
  seen.Add(CodexCategory::kFoes, moonclient::CodexFoeKey(kLunatic));
  seen.Add(CodexCategory::kFoes, moonclient::CodexFoeKey(kPoring));
  seen.Add(CodexCategory::kFoes, moonclient::CodexFoeKey(kBaphomet));
  seen.Add(CodexCategory::kSkills, "fire_bolt");
  Composed composed;
  composed.view = ComposeSurvivorsCodex(Rules(), seen, composed.held);

  for (const std::uint16_t foe_id : {kLunatic, kPoring}) {
    const SurvivorsCodexEntry& foe =
        EntryOf(composed.view, SurvivorsCodexTab::kFoes, foe_id, SurvivorsCodexSheet::kFoe);
    for (const auto& source : foe.sources) {
      if (source.bringer.ro_id != 0) CHECK_FALSE(source.bringer.seen);
    }
  }
  const SurvivorsCodexEntry& fire_bolt =
      EntryOf(composed.view, SurvivorsCodexTab::kSkills, kFireBolt, SurvivorsCodexSheet::kSkill);
  CHECK(fire_bolt.seen);
  CHECK_FALSE(fire_bolt.evolution.card.seen);
  CHECK_FALSE(fire_bolt.evolution.result.seen);
  const SurvivorsCodexEntry& poring =
      EntryOf(composed.view, SurvivorsCodexTab::kFoes, kPoring, SurvivorsCodexSheet::kFoe);
  REQUIRE(poring.drops_card);
  CHECK_FALSE(poring.card.card.seen);
  const SurvivorsCodexEntry& baphomet =
      EntryOf(composed.view, SurvivorsCodexTab::kBosses, kBaphomet, SurvivorsCodexSheet::kBoss);
  const auto summons = [&] {
    return std::ranges::find(baphomet.moves, ro::ui::SurvivorsCodexMoveKind::kSummon,
                             &ro::ui::SurvivorsCodexMove::kind);
  };
  REQUIRE(summons() != baphomet.moves.end());
  CHECK_FALSE(summons()->foe.seen);

  // Vus à leur tour — Baphomet Jr. né, Meteor Storm offert, Poring Card
  // trouvée —, les liens se révèlent.
  seen.Add(CodexCategory::kFoes, moonclient::CodexFoeKey(kBaphometJr));
  seen.Add(CodexCategory::kSkills, "meteor_storm");
  SurvivorsSave save;
  for (const ro::sim::CardRule& card : Rules().cards) {
    if (card.card_item_id == poring.card.card.ro_id) {
      save.album.cards.push_back(moonclient::SurvivorsAlbumCard{.key = card.key, .found = 1});
    }
  }
  REQUIRE_FALSE(save.album.cards.empty());
  const std::vector<std::uint16_t> found = moonclient::FoundCardItemsOf(Rules(), save);
  Composed known;
  known.view = ComposeSurvivorsCodex(Rules(), seen, known.held, found);
  CHECK(EntryOf(known.view, SurvivorsCodexTab::kFoes, kPoring, SurvivorsCodexSheet::kFoe)
            .card.card.seen);
  const SurvivorsCodexEntry& known_bolt =
      EntryOf(known.view, SurvivorsCodexTab::kSkills, kFireBolt, SurvivorsCodexSheet::kSkill);
  CHECK(known_bolt.evolution.result.seen);
  const SurvivorsCodexEntry& known_baphomet =
      EntryOf(known.view, SurvivorsCodexTab::kBosses, kBaphomet, SurvivorsCodexSheet::kBoss);
  CHECK(std::ranges::find(known_baphomet.moves, ro::ui::SurvivorsCodexMoveKind::kSummon,
                          &ro::ui::SurvivorsCodexMove::kind)
            ->foe.seen);
}

TEST_CASE("Codex : la chronologie des boss et des événements", "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const auto bosses = TabOf(composed.view, SurvivorsCodexTab::kBosses);
  for (std::size_t index = 1; index < bosses.size(); ++index) {
    CHECK(bosses[index - 1].at_ms <= bosses[index].at_ms);
  }
  CHECK(IndexOf(bosses, moonclient::CodexFoeKey(kMastering)) <
        IndexOf(bosses, "swarm_" + moonclient::CodexFoeKey(kHornet)));
  const SurvivorsCodexEntry& baphomet =
      EntryOf(composed.view, SurvivorsCodexTab::kBosses, kBaphomet, SurvivorsCodexSheet::kBoss);
  CHECK(baphomet.at_ms == 15 * kMinuteMs);
  CHECK(baphomet.final);
  REQUIRE(baphomet.moves.size() == kBaphometMoves);
  CHECK(baphomet.moves[0].from_hp_pct == ro::sim::kWholeHpPct);
  CHECK(baphomet.moves[2].kind == ro::ui::SurvivorsCodexMoveKind::kQuake);
  const SurvivorsCodexEntry& elites =
      EntryByKey(composed.view, SurvivorsCodexTab::kBosses, moonclient::kCodexElitesKey);
  CHECK(elites.at_ms == 2 * kMinuteMs);
  CHECK(elites.interval_ms == kEliteIntervalMs);
  CHECK(elites.until_ms == 14 * kMinuteMs + kHalfMinuteMs);
  const SurvivorsCodexEntry& night =
      EntryByKey(composed.view, SurvivorsCodexTab::kBosses, moonclient::kCodexNightKey);
  CHECK(night.at_ms == moonclient::kArenaNightFallsMs);
  CHECK(night.until_ms == moonclient::kArenaMidnightMs);
  // LA LISTE NOMME CHAQUE LIGNE : un boss par son espèce, un événement par sa
  // sorte et l'espèce qu'il amène ; les élites et la nuit par leur sorte seule.
  for (const SurvivorsCodexEntry& entry : bosses) {
    if (entry.sheet == SurvivorsCodexSheet::kBoss) {
      CHECK(entry.ro_id != 0);
      continue;
    }
    REQUIRE(entry.sheet == SurvivorsCodexSheet::kEvent);
    const bool alone = entry.event_kind == ro::ui::SurvivorsCodexEventKind::kElites ||
                       entry.event_kind == ro::ui::SurvivorsCodexEventKind::kNight;
    CHECK(alone == (entry.event_foe.ro_id == 0));
  }
}

TEST_CASE("Codex : une entrée n'est vue que par le carnet ou la partie en cours",
          "[app][survivors][codex]") {
  SurvivorsSave save;
  SurvivorsCodexSeen live;
  live.Add(CodexCategory::kFoes, moonclient::CodexFoeKey(kPoring));
  Composed blank;
  Compose(blank, CodexSeenOf(Rules(), save, nullptr));
  CHECK_FALSE(
      EntryOf(blank.view, SurvivorsCodexTab::kFoes, kPoring, SurvivorsCodexSheet::kFoe).seen);
  Composed lively;
  Compose(lively, CodexSeenOf(Rules(), save, &live));
  CHECK(EntryOf(lively.view, SurvivorsCodexTab::kFoes, kPoring, SurvivorsCodexSheet::kFoe).seen);
  CHECK_FALSE(
      EntryOf(lively.view, SurvivorsCodexTab::kFoes, kHornet, SurvivorsCodexSheet::kFoe).seen);
}

TEST_CASE("Codex : le carnet d'avant le Codex déduit ce qu'il a forcément vu (Q2)",
          "[app][survivors][codex]") {
  SurvivorsSave save;
  save.album.counters.best_survived_ms = 4 * kMinuteMs;
  save.album.cards.push_back(moonclient::SurvivorsAlbumCard{.key = "hornet_card", .found = 1});
  moonclient::SurvivorsBoard board{
      .class_key = "mage", .arena_key = "prt_fild08", .board_key = "free"};
  ro::sim::SurvivorsRecord record;
  record.first_skill_key = "cold_bolt";
  board.runs.push_back(record);
  save.records.push_back(board);
  const SurvivorsCodexSeen seen = CodexSeenOf(Rules(), save, nullptr);
  CHECK(seen.Has(CodexCategory::kFoes, moonclient::CodexFoeKey(kLunatic)));
  CHECK(seen.Has(CodexCategory::kFoes, moonclient::CodexFoeKey(kMastering)));
  CHECK(seen.Has(CodexCategory::kEvents, "swarm_" + moonclient::CodexFoeKey(kChonchon)));
  CHECK(seen.Has(CodexCategory::kEvents, moonclient::kCodexElitesKey));
  CHECK_FALSE(seen.Has(CodexCategory::kEvents, moonclient::kCodexNightKey));
  // La nuée de Hornet tombe à 4:30, au-delà du meilleur temps ; Hornet est vu
  // par sa carte trouvée.
  CHECK_FALSE(seen.Has(CodexCategory::kEvents, "swarm_" + moonclient::CodexFoeKey(kHornet)));
  CHECK(seen.Has(CodexCategory::kFoes, moonclient::CodexFoeKey(kHornet)));
  CHECK(seen.Has(CodexCategory::kSkills, "cold_bolt"));
  // Rien ne s'écrit : le carnet reste tel quel.
  CHECK(save.codex == SurvivorsCodexSeen{});
  // Un carnet vierge ne déduit rien.
  CHECK(CodexSeenOf(Rules(), SurvivorsSave{}, nullptr) == SurvivorsCodexSeen{});
}

TEST_CASE("Codex : un sort verrouillé par un exploit en porte la clé (Q3)",
          "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const SurvivorsCodexEntry& sight = EntryByKey(composed.view, SurvivorsCodexTab::kSkills, "sight");
  CHECK(std::string_view(sight.unlock_feat) == "sight");
  const SurvivorsCodexEntry& fire_bolt =
      EntryOf(composed.view, SurvivorsCodexTab::kSkills, kFireBolt, SurvivorsCodexSheet::kSkill);
  CHECK(std::string_view(fire_bolt.unlock_feat).empty());
}

TEST_CASE("Codex : une fiche par entrée des règles livrées", "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const auto count = [&](SurvivorsCodexTab tab, SurvivorsCodexSheet sheet) {
    return static_cast<std::size_t>(std::ranges::count_if(
        TabOf(composed.view, tab),
        [sheet](const SurvivorsCodexEntry& entry) { return entry.sheet == sheet; }));
  };
  const auto& rules = Rules();
  CHECK(count(SurvivorsCodexTab::kSkills, SurvivorsCodexSheet::kSkill) == rules.skills.size());
  CHECK(TabOf(composed.view, SurvivorsCodexTab::kCards).empty());
  const auto commons = static_cast<std::size_t>(std::ranges::count_if(
      rules.foes, [](const auto& foe) { return foe.rank == ro::sim::FoeRank::kCommon; }));
  CHECK(count(SurvivorsCodexTab::kFoes, SurvivorsCodexSheet::kFoe) == commons);
  CHECK(TabOf(composed.view, SurvivorsCodexTab::kFoes).size() == commons);
  // Les boss et les événements de chaque contenu : celui des règles, et
  // celui de chaque arène qui a le sien.
  std::size_t bosses = 0;
  std::size_t events = 0;
  for (const ro::sim::ArenaContent& content : ro::sim::AllContents(rules)) {
    bosses += content.bosses.size();
    events += content.events.size();
  }
  CHECK(count(SurvivorsCodexTab::kBosses, SurvivorsCodexSheet::kBoss) == bosses);
  // Les événements, plus les élites et la nuit.
  constexpr std::size_t kElitesAndNight = 2;
  CHECK(count(SurvivorsCodexTab::kBosses, SurvivorsCodexSheet::kEvent) == events + kElitesAndNight);
  CHECK(count(SurvivorsCodexTab::kPlants, SurvivorsCodexSheet::kPlant) == rules.plants.size());
  const std::size_t items = count(SurvivorsCodexTab::kItems, SurvivorsCodexSheet::kItem) +
                            count(SurvivorsCodexTab::kItems, SurvivorsCodexSheet::kChest);
  CHECK(items == rules.pickups.size());
  CHECK(count(SurvivorsCodexTab::kItems, SurvivorsCodexSheet::kGem) ==
        moonclient::kGemColours.size());
  CHECK(count(SurvivorsCodexTab::kItems, SurvivorsCodexSheet::kGroundCard) == 1);
  // Chaque mini-boss et boss des règles a sa ligne dans la chronologie.
  for (const ro::sim::FoeRule& foe : rules.foes) {
    if (foe.rank == ro::sim::FoeRank::kCommon) continue;
    const bool scripted = std::ranges::any_of(
        ro::sim::AllContents(rules), [&foe](const ro::sim::ArenaContent& content) {
          return std::ranges::any_of(content.bosses, [&foe](const ro::sim::BossRule& boss) {
            return boss.foe_id == foe.foe_id;
          });
        });
    CHECK(scripted);
  }
}

TEST_CASE("Codex : chaque entrée des règles livrées a sa phrase", "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  for (const auto& entries : composed.view.entries) {
    for (const SurvivorsCodexEntry& entry : entries) {
      INFO(entry.key);
      const bool described = entry.description != nullptr && entry.description[0] != '\0';
      CHECK((described || !std::string_view(ro::ui::SurvivorsCodexPhrase(entry.key)).empty()));
    }
  }
  // L'onglet Cartes est l'Album : chaque carte garde la phrase de son catalogue.
  for (const ro::sim::CardRule& card : Rules().cards) {
    INFO(card.key);
    CHECK(card.description != nullptr);
    CHECK_FALSE(std::string_view(card.description).empty());
  }
}

TEST_CASE("Codex : aucune phrase n'est orpheline", "[app][survivors][codex]") {
  Composed composed;
  Compose(composed);
  const auto phrases = ro::ui::SurvivorsCodexPhrases();
  for (std::size_t index = 0; index < phrases.size(); ++index) {
    const std::string_view key = phrases[index].key;
    INFO(key);
    CHECK_FALSE(std::string_view(phrases[index].french).empty());
    // Une entrée des règles la lit : elle n'a pas de phrase de catalogue.
    bool read = false;
    for (const auto& entries : composed.view.entries) {
      read = read || std::ranges::any_of(entries, [key](const SurvivorsCodexEntry& entry) {
               return key == entry.key && std::string_view(entry.description).empty();
             });
    }
    CHECK(read);
    for (std::size_t other = index + 1; other < phrases.size(); ++other) {
      CHECK(key != phrases[other].key);
    }
  }
}

// ── LE CODEX PAR-DESSUS LA PAUSE ────────────────────────────────────────────

TEST_CASE("Codex : Échap sur la pause le ferme sans reprendre, puis reprend",
          "[app][survivors][codex]") {
  ro::ui::SurvivorsCodexState codex;
  moonclient::SurvivorsGestures gestures;
  CHECK_FALSE(moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPaused, true, gestures,
                                               codex));
  CHECK(codex.open);
  // Codex ouvert : Échap le ferme et ne va pas à l'étage.
  gestures.escape = true;
  CHECK(moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPaused, false, gestures,
                                         codex));
  CHECK_FALSE(codex.open);
  CHECK_FALSE(gestures.escape);
  // Codex fermé : le second Échap va à l'étage, qui reprend.
  gestures.escape = true;
  CHECK_FALSE(moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPaused, false,
                                               gestures, codex));
  CHECK(gestures.escape);
}

TEST_CASE("Codex : la molette lui revient tant qu'il est ouvert", "[app][survivors][codex]") {
  constexpr float kOneNotch = 1.0F;
  ro::ui::SurvivorsCodexState codex{.open = true};
  moonclient::SurvivorsGestures gestures{.wheel_notches = kOneNotch};
  (void)moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPaused, false, gestures,
                                         codex);
  CHECK(gestures.wheel_notches == 0.0F);
  codex.open = false;
  gestures.wheel_notches = kOneNotch;
  (void)moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPaused, false, gestures,
                                         codex);
  CHECK(gestures.wheel_notches == kOneNotch);
}

TEST_CASE("Codex : l'étage hors de la pause le ferme, la pause qui demeure le garde",
          "[app][survivors][codex]") {
  ro::ui::SurvivorsCodexState codex{.open = true};
  moonclient::SurvivorsGestures gestures{.focus_lost = true};
  (void)moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPaused, false, gestures,
                                         codex);
  CHECK(codex.open);
  gestures = {};
  (void)moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPlaying, false, gestures,
                                         codex);
  CHECK_FALSE(codex.open);
  // Hors de la pause, « Codex » n'ouvre rien : la pause n'est plus là pour l'avoir demandé.
  (void)moonclient::FollowCodexOverPause(moonclient::SurvivorsScreen::kPlaying, true, gestures,
                                         codex);
  CHECK_FALSE(codex.open);
}
