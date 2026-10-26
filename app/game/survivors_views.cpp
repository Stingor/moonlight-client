#include "game/survivors_views.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ro/core/build_flavor.h"
#include "ro/core/i18n.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"

#include "game/skin_image_of.h"

namespace moonclient {
namespace {

using ro::sim::SurvivorsField;

// LES LIBELLÉS DES MESURES ET DES COMPTES DE LA PAUSE, marqués pour le
// catalogue : la fenêtre les traduit à la lecture.
constexpr std::array<const char*, kArenaMeasureCount> kMeasureLabels = {
    ro::core::TrText("Simulation"), ro::core::TrText("Composition"), ro::core::TrText("Scène"),
    ro::core::TrText("Effets"), ro::core::TrText("Interface")};
constexpr const char* kFoesCountLabel = ro::core::TrText("Monstres");
constexpr const char* kBodiesCountLabel = ro::core::TrText("Corps");
constexpr const char* kEffectsCountLabel = ro::core::TrText("Effets");
constexpr const char* kVoicesCountLabel = ro::core::TrText("Sons");

[[nodiscard]] std::string NumberedName(std::uint16_t id) { return "#" + std::to_string(id); }

[[nodiscard]] float Fill(std::int64_t part, std::int64_t whole) noexcept {
  if (whole <= 0) return 0.0F;
  return std::clamp(static_cast<float>(part) / static_cast<float>(whole), 0.0F, 1.0F);
}

[[nodiscard]] ro::ui::SurvivorsPauseReason ReasonOf(PauseCause cause) noexcept {
  switch (cause) {
    case PauseCause::kPlayer:
      return ro::ui::SurvivorsPauseReason::kPlayer;
    case PauseCause::kFocusLost:
      return ro::ui::SurvivorsPauseReason::kFocusLost;
    case PauseCause::kWorldAlert:
      return ro::ui::SurvivorsPauseReason::kWorldAlert;
  }
  return ro::ui::SurvivorsPauseReason::kPlayer;
}

}  // namespace

ImTextureID SurvivorsSkillIcon(std::uint16_t skill_id, const SurvivorsTables& tables) {
  if (tables.skill_icons == nullptr || tables.skill_tree == nullptr) return ImTextureID{0};
  const ro::sim::SkillNode* node = tables.skill_tree->Find(skill_id);
  if (node == nullptr) return ImTextureID{0};
  return SkinImageOf(tables.skill_icons->Get(skill_id, node->idname)).id;
}

ImTextureID SurvivorsItemIcon(std::uint16_t item_id, const SurvivorsTables& tables) {
  return tables.items != nullptr ? tables.items->IconOf(item_id).id : ImTextureID{0};
}

ro::ui::SkinImage SurvivorsCardIllustration(std::uint16_t card_item_id,
                                            const SurvivorsTables& tables) {
  if (tables.item_icons == nullptr) return {};
  return SkinImageOf(tables.item_icons->GetCardIllustration(card_item_id));
}

const char* SurvivorsViews::SkillName(std::uint16_t skill_id, const SurvivorsTables& tables) {
  auto [found, fresh] = skill_names_.try_emplace(skill_id);
  if (fresh) {
    const ro::sim::SkillNode* node =
        tables.skill_tree != nullptr ? tables.skill_tree->Find(skill_id) : nullptr;
    found->second = node != nullptr && !node->display_name.empty() ? node->display_name
                                                                   : NumberedName(skill_id);
  }
  return found->second.c_str();
}

const char* SurvivorsViews::CardName(std::uint16_t card_item_id, const SurvivorsTables& tables) {
  auto [found, fresh] = card_names_.try_emplace(card_item_id);
  if (fresh) {
    found->second =
        tables.items != nullptr ? tables.items->NameOf(card_item_id) : NumberedName(card_item_id);
  }
  return found->second.c_str();
}

const char* SurvivorsViews::FoeName(std::uint16_t foe_id, const SurvivorsTables& tables) {
  auto [found, fresh] = foe_names_.try_emplace(foe_id);
  if (fresh) {
    const ro::vfs::MonsterListEntry* entry =
        tables.monsters != nullptr ? tables.monsters->Find(foe_id) : nullptr;
    found->second = entry != nullptr ? entry->name : NumberedName(foe_id);
  }
  return found->second.c_str();
}

const char* SurvivorsViews::JobName(std::uint16_t job_id, const SurvivorsTables& tables) {
  auto [found, fresh] = job_names_.try_emplace(job_id);
  if (fresh) {
    const std::string_view name =
        tables.jobs != nullptr ? tables.jobs->Find(job_id) : std::string_view();
    found->second = name.empty() ? NumberedName(job_id) : std::string(name);
  }
  return found->second.c_str();
}

void SurvivorsViews::ComposePrepare(ro::ui::SurvivorsPrepareView& view,
                                    const ro::sim::SurvivorsRules& rules,
                                    std::uint16_t chosen_skill_id, const char* refusal,
                                    const std::array<std::string, 4>& key_labels,
                                    const SurvivorsTables& tables) {
  bolts_.clear();
  if (!rules.classes.empty()) {
    for (const std::uint16_t skill_id : rules.classes.front().starter_skills) {
      const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(rules, skill_id);
      bolts_.push_back(
          ro::ui::SurvivorsBoltChoice{.skill_id = skill_id,
                                      .icon = SurvivorsSkillIcon(skill_id, tables),
                                      .name = SkillName(skill_id, tables),
                                      .description = rule != nullptr ? rule->description : ""});
    }
  }
  view = ro::ui::SurvivorsPrepareView{.bolts = bolts_,
                                      .chosen_skill_id = chosen_skill_id,
                                      .refusal = refusal,
                                      .key_north = key_labels[0].c_str(),
                                      .key_west = key_labels[1].c_str(),
                                      .key_south = key_labels[2].c_str(),
                                      .key_east = key_labels[3].c_str()};
}

SurvivorsFrameView SurvivorsViews::Compose(const SurvivorsStage& stage,
                                           const ro::sim::SurvivorsRules& rules,
                                           const SurvivorsViewFacts& facts,
                                           const SurvivorsTables& tables) {
  SurvivorsFrameView view;
  const ro::sim::SurvivorsPlay* play = stage.play();
  const SurvivorsField* field = play != nullptr ? &play->run().field() : nullptr;
  const SurvivorsScreen screen = stage.screen();
  if (field != nullptr && screen != SurvivorsScreen::kSummary) {
    ComposeHud(*field, rules, stage.presentation_ms(), facts, tables);
    ComposeHudMeta(stage, *field, tables);
    view.hud = &hud_;
  }
  switch (screen) {
    case SurvivorsScreen::kChoosing:
      if (field == nullptr) break;
      ComposeOffer(*field, rules, stage.accepts_input(), facts.pressed_card_key, tables);
      view.offer = &offer_;
      view.offer_state = &offer_state_;
      break;
    case SurvivorsScreen::kChest:
      if (field == nullptr) break;
      ComposeChest(stage, tables);
      view.chest = &chest_;
      break;
    case SurvivorsScreen::kPaused:
      ComposePause(stage, field, facts);
      view.pause = &pause_;
      view.pause_state = &pause_state_;
      // LE CODEX DE LA PAUSE montre déjà ce que la partie a vu, avant tout
      // crédit au carnet.
      if (facts.meta != nullptr && facts.meta->codex.open && facts.save != nullptr) {
        SurvivorsMetaWindows& meta = *facts.meta;
        ComposeCodex(meta, *facts.save, &stage.sightings(), rules, tables, facts.now_ms);
        view.codex = &meta.codex_view;
        view.codex_state = &meta.codex;
        view.album = &meta.album_view;
        view.album_state = &meta.album;
      }
      break;
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
    case SurvivorsScreen::kSummary:
      ComposeSummary(stage, rules, tables);
      ComposeCredit(stage, rules, facts, tables);
      view.summary = &summary_;
      if (screen == SurvivorsScreen::kSummary && facts.meta != nullptr && facts.meta->church.open &&
          facts.save != nullptr) {
        ComposeShop(SurvivorsMetaShop::kChurch, *facts.meta, *facts.save, tables);
        view.church = &facts.meta->church_view;
        view.church_state = &facts.meta->church;
      }
      break;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
      break;
  }
  ComposeRanking(stage, facts, view);
  return view;
}

void SurvivorsViews::ComposeHud(const SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                                std::uint64_t clock_ms, const SurvivorsViewFacts& facts,
                                const SurvivorsTables& tables) {
  skill_slots_.clear();
  for (const ro::sim::OwnedSkill& skill : field.hero.skills) {
    skill_slots_.push_back(
        {.icon = SurvivorsSkillIcon(skill.skill_id, tables), .level = skill.level});
  }
  card_slots_.clear();
  for (const ro::sim::OwnedCard& card : field.hero.cards) {
    card_slots_.push_back(
        {.icon = SurvivorsItemIcon(card.card_item_id, tables), .level = card.level});
  }
  hud_ = ro::ui::SurvivorsHudView{
      .level = field.hero.level,
      .exp_fill = Fill(field.hero.exp, ro::sim::ExpToNext(rules, field.hero.level)),
      .clock_ms = clock_ms,
      .kills = field.tally.kills,
      .zeny = field.hero.zeny,
      .skills = skill_slots_,
      .cards = card_slots_,
      .unread_whispers = static_cast<int>(facts.unread_whispers),
      .hero_fill = Fill(field.hero.hp, field.hero.max_hp)};
  if (const std::optional<CardVignetteShown> card = card_vignettes_.ShownAt(clock_ms)) {
    const ro::ui::SkinImage art = SurvivorsCardIllustration(card->card_item_id, tables);
    if (art.usable()) {
      hud_.found_card = ro::ui::SurvivorsHudCard{.illustration = art.id,
                                                 .size = ImVec2(art.width, art.height),
                                                 .fraction = card->fraction};
    }
  }
  // LE BOSS VIVANT : sa barre et son nom ; le premier suivi s'il y en a deux.
  edge_targets_.clear();
  const auto screen_of = [&facts](ro::sim::FieldPoint point) -> std::optional<glm::vec2> {
    if (facts.arena == nullptr) return std::nullopt;
    return facts.arena->ScreenOf(facts.arena->GroundAt(point), facts.width, facts.height);
  };
  for (const ro::sim::SurvivorsBossTrack& track : field.boss_tracks) {
    const auto foe = std::ranges::find(field.foes, track.serial, &ro::sim::SurvivorsFoe::serial);
    if (foe == field.foes.end()) continue;
    if (hud_.boss_name == nullptr) {
      hud_.boss_name = FoeName(foe->foe_id, tables);
      hud_.boss_fill = Fill(foe->hp, foe->max_hp);
    }
    if (const std::optional<glm::vec2> at = screen_of(foe->position)) {
      edge_targets_.push_back(
          {.screen_px = ImVec2(at->x, at->y), .kind = ro::ui::SurvivorsEdgeKind::kBoss});
    }
  }
  for (const ro::sim::SurvivorsPickup& pickup : field.pickups) {
    if (pickup.kind != ro::sim::PickupKind::kChest) continue;
    if (const std::optional<glm::vec2> at = screen_of(pickup.position)) {
      edge_targets_.push_back(
          {.screen_px = ImVec2(at->x, at->y), .kind = ro::ui::SurvivorsEdgeKind::kChest});
    }
  }
  hud_.edge_targets = edge_targets_;
  if (facts.arena != nullptr) {
    if (const std::optional<glm::vec2> hero_cell = facts.arena->hero_cell()) {
      if (const std::optional<glm::vec2> feet = facts.arena->ScreenOf(
              facts.arena->GroundAtCell(*hero_cell), facts.width, facts.height)) {
        hud_.hero_feet_px = ImVec2(feet->x, feet->y);
      }
    }
  }
}

void SurvivorsViews::ComposeOffer(const SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                                  bool accepts_input, int pressed_card_key,
                                  const SurvivorsTables& tables) {
  offer_cards_.clear();
  for (const ro::sim::SurvivorsOffer& offer : field.offers) {
    ro::ui::SurvivorsOfferCard card;
    card.next_level = offer.next_level;
    switch (offer.kind) {
      case ro::sim::OfferKind::kNewSkill:
      case ro::sim::OfferKind::kSkillLevel: {
        const ro::sim::SurvivorsSkillRule* rule = ro::sim::FindSkill(rules, offer.id);
        card.icon = SurvivorsSkillIcon(offer.id, tables);
        card.name = SkillName(offer.id, tables);
        card.description = rule != nullptr ? rule->description : "";
        break;
      }
      case ro::sim::OfferKind::kNewCard:
      case ro::sim::OfferKind::kCardLevel: {
        const ro::sim::CardRule* rule = ro::sim::FindCard(rules, offer.id);
        const ro::ui::SkinImage art = SurvivorsCardIllustration(offer.id, tables);
        card.icon = art.usable() ? art.id : SurvivorsItemIcon(offer.id, tables);
        card.icon_size = art.usable() ? ImVec2(art.width, art.height) : ImVec2(0.0F, 0.0F);
        card.name = CardName(offer.id, tables);
        card.description = rule != nullptr ? rule->description : "";
        break;
      }
      case ro::sim::OfferKind::kHeal:
        card.name = ro::core::Tr("Soin");
        card.next_level = 1;
        break;
      case ro::sim::OfferKind::kZeny:
        card.name = ro::core::Tr("Zeny");
        card.next_level = 1;
        break;
    }
    const bool levels_up = offer.kind == ro::sim::OfferKind::kSkillLevel ||
                           offer.kind == ro::sim::OfferKind::kCardLevel;
    card.owned_level = levels_up ? std::max(0, offer.next_level - 1) : 0;
    offer_cards_.push_back(card);
  }
  offer_ = ro::ui::SurvivorsOfferView{.cards = offer_cards_,
                                      .accepts_input = accepts_input,
                                      .pressed_card_key = pressed_card_key,
                                      .rerolls = field.rerolls_left,
                                      .skips = field.skips_left,
                                      .banishes = field.banishes_left};
}

void SurvivorsViews::ComposeChest(const SurvivorsStage& stage, const SurvivorsTables& tables) {
  chest_gains_.clear();
  for (const ro::sim::SurvivorsChestNews& chest : stage.chests()) {
    for (std::size_t rank = 0; rank < chest.gain_count && rank < chest.gains.size(); ++rank) {
      const ro::sim::SurvivorsChestGain& gain = chest.gains[rank];
      ro::ui::SurvivorsChestGainRow row{.level = gain.level};
      switch (gain.kind) {
        case ro::sim::ChestGainKind::kEvolution:
          row.icon = SurvivorsSkillIcon(gain.id, tables);
          row.name = SkillName(gain.id, tables);
          row.look = ro::ui::SurvivorsChestGainLook::kEvolution;
          break;
        case ro::sim::ChestGainKind::kSkillLevel:
          row.icon = SurvivorsSkillIcon(gain.id, tables);
          row.name = SkillName(gain.id, tables);
          break;
        case ro::sim::ChestGainKind::kCardLevel:
          row.icon = SurvivorsItemIcon(gain.id, tables);
          row.name = CardName(gain.id, tables);
          break;
        case ro::sim::ChestGainKind::kZeny:
          row.look = ro::ui::SurvivorsChestGainLook::kZeny;
          break;
      }
      chest_gains_.push_back(row);
    }
  }
  roulette_icons_.clear();
  for (const ro::ui::SurvivorsHudSlot& slot : skill_slots_) roulette_icons_.push_back(slot.icon);
  for (const ro::ui::SurvivorsHudSlot& slot : card_slots_) roulette_icons_.push_back(slot.icon);
  chest_ = ro::ui::SurvivorsChestView{.gains = chest_gains_,
                                      .roulette_icons = roulette_icons_,
                                      .elapsed_ms = stage.screen_elapsed_ms(),
                                      .list_shown = stage.chest_list_shown(),
                                      .accepts_input = stage.accepts_input()};
}

void SurvivorsViews::ComposePause(const SurvivorsStage& stage, const SurvivorsField* field,
                                  const SurvivorsViewFacts& facts) {
  measure_rows_.clear();
  count_rows_.clear();
  const bool measures_shown = MeasuresShown(facts.options, ro::core::kDevTools);
  if (measures_shown && facts.measures != nullptr) {
    for (std::size_t rank = 0; rank < kArenaMeasureCount; ++rank) {
      const MeasureSummary summary = facts.measures->Window(static_cast<ArenaMeasure>(rank));
      measure_rows_.push_back({.label = kMeasureLabels[rank],
                               .p50_ms = summary.p50_ms,
                               .p95_ms = summary.p95_ms,
                               .max_ms = summary.max_ms});
    }
    if (field != nullptr) {
      count_rows_.push_back(
          {.label = kFoesCountLabel, .value = static_cast<long long>(field->foes.size())});
    }
    if (facts.arena != nullptr) {
      count_rows_.push_back(
          {.label = kBodiesCountLabel,
           .value = static_cast<long long>(facts.arena->composer().drawn().size())});
      count_rows_.push_back({.label = kEffectsCountLabel,
                             .value = static_cast<long long>(facts.arena->effects().living())});
    }
    count_rows_.push_back(
        {.label = kVoicesCountLabel, .value = static_cast<long long>(facts.busy_voices)});
  }
  pause_ = ro::ui::SurvivorsPauseView{
      .reason = ReasonOf(stage.pause_cause()),
      .alert_banner = facts.banner.has_value() ? BannerText(*facts.banner) : nullptr,
      .accepts_input = stage.accepts_input(),
      .reduced_effects = facts.options.reduced_effects,
      .measures_offered = ro::core::kDevTools,
      .show_measures = measures_shown,
      .horde_sun_shadows = facts.options.horde_sun_shadows,
      .measures = measure_rows_,
      .counts = count_rows_};
}

void SurvivorsViews::ComposeSummary(const SurvivorsStage& stage,
                                    const ro::sim::SurvivorsRules& rules,
                                    const SurvivorsTables& tables) {
  const SurvivorsEnding* ending = stage.ending();
  summary_ = ro::ui::SurvivorsSummaryView{};
  summary_.accepts_input = stage.accepts_input();
  switch (stage.screen()) {
    case SurvivorsScreen::kDying:
      summary_.phase = ro::ui::SurvivorsEndPhase::kDeathVeil;
      break;
    case SurvivorsScreen::kWon:
      summary_.phase = ro::ui::SurvivorsEndPhase::kVictoryBanner;
      break;
    default:
      summary_.phase = ro::ui::SurvivorsEndPhase::kSummary;
      break;
  }
  if (ending == nullptr) return;
  summary_.outcome = EndOutcomeOf(ending->tally.outcome);
  summary_.survived_ms = std::uint64_t{ending->tally.survived_steps} * ro::sim::kSurvivorsStepMs;
  summary_.level = ending->tally.level;
  const auto hero_class =
      std::ranges::find(rules.classes, ending->class_id, &ro::sim::ClassRule::class_id);
  if (hero_class != rules.classes.end()) {
    summary_.job_name = JobName(ending->tally.evolutions > 0 && hero_class->evolved_job_id != 0
                                    ? hero_class->evolved_job_id
                                    : hero_class->job_id,
                                tables);
  }
  foe_rows_.clear();
  for (const ro::sim::SurvivorsCount& count : ending->recap.kills_by_foe) {
    foe_rows_.push_back({.name = FoeName(count.id, tables), .kills = count.value});
  }
  skill_rows_.clear();
  for (const ro::sim::SurvivorsCount& count : ending->recap.damage_by_skill) {
    skill_rows_.push_back({.icon = SurvivorsSkillIcon(count.id, tables),
                           .name = SkillName(count.id, tables),
                           .damage = count.value});
  }
  card_rows_.clear();
  for (const ro::sim::OwnedCard& card : ending->cards) {
    const ro::ui::SkinImage art = SurvivorsCardIllustration(card.card_item_id, tables);
    card_rows_.push_back({.illustration = art.id,
                          .illustration_size = ImVec2(art.width, art.height),
                          .name = CardName(card.card_item_id, tables),
                          .level = card.level});
  }
  summary_.foes = foe_rows_;
  summary_.skills = skill_rows_;
  summary_.cards = card_rows_;
  summary_.zeny = ending->zeny;
}

}  // namespace moonclient
