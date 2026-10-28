// LA MÉTA DANS LES VUES DU MODE SURVIVORS : l'accueil et ses fenêtres (Église,
// Kafra, Codex, records), le crédit de l'écran de fin et ce que le HUD montre
// du départ acheté. Les nombres viennent du carnet (`survivors_meta_desk.h`),
// dans l'ordre qu'il dit ; ici s'ajoutent les noms, les icônes et les
// illustrations des tables du jeu. Le classement s'y pose de même, lu dans la
// session par `survivors_ranking_desk.h` : ses noms viennent du serveur et ne
// se traduisent pas.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/survivors_meta.h"

#include "game/skin_image_of.h"
#include "game/survivors_views.h"
#include "survivors/arena_choice.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

// L'icône d'un succès gagné, à l'écran de fin : l'Emperium.
constexpr std::uint16_t kTrophyIconItem = 714;

[[nodiscard]] const ro::sim::SurvivorsMeta& Meta() { return ro::sim::ShippedSurvivorsMeta(); }

// Le nœud de l'arbre d'un skill nommé par son nom RO (`AL_BLESSING`).
[[nodiscard]] const ro::sim::SkillNode* NodeNamed(const char* idname,
                                                  const SurvivorsTables& tables) {
  return tables.skill_tree != nullptr ? tables.skill_tree->FindByName(idname) : nullptr;
}

}  // namespace

void SurvivorsViews::ComposeMeta(ro::ui::SurvivorsPrepareView& prepare,
                                 SurvivorsMetaWindows& windows, const SurvivorsMetaDesk& desk,
                                 const SurvivorsBookMode& book,
                                 const ro::sim::SurvivorsRules& rules,
                                 const SurvivorsTables& tables, const SurvivorsRankingMode* ranking,
                                 std::uint32_t now_ms) {
  const SurvivorsSave& save = desk.save();
  const std::int64_t now_utc_s = book.now_utc();
  ro::ui::SurvivorsPrepareMeta meta = PrepareMetaOf(rules, save, book.chosen(), now_utc_s);
  if (const std::uint16_t bolt = meta.challenge_bolt.skill_id; bolt != 0) {
    meta.challenge_bolt.icon = SurvivorsSkillIcon(bolt, tables);
    meta.challenge_bolt.name = SkillName(bolt, tables);
  }
  // LE CLASSEMENT N'A RIEN À L'ACCUEIL SERVICE FERMÉ : ni onglet, ni bouton,
  // ni compteur, ni l'onglet local des parties classées, ni le tableau.
  const bool ranking_open = ranking != nullptr && ranking->session().service_open();
  if (ranking != nullptr) {
    meta.ranking = PrepareRankingOf(ranking->session(), ranking_rows_);
    ComposeRankingWindow(*ranking, windows.ranking, windows.ranking_view, ranking_rows_);
  } else {
    windows.ranking.open = false;
  }
  // LES ARÈNES (Q6 amendée) : toute arène du catalogue en partie libre, la
  // prairie par défaut ; le défi et la partie classée nomment la leur.
  arena_names_.clear();
  arena_choices_.clear();
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    arena_names_.push_back(ArenaNameOf(rules, arena, tables.map_names));
  }
  // Après les noms : une liste qui grandit déplacerait les textes désignés.
  for (std::size_t rank = 0; rank < rules.arenas.size(); ++rank) {
    arena_choices_.push_back(ro::ui::SurvivorsArenaChoice{.arena_id = rules.arenas[rank].arena_id,
                                                          .name = arena_names_[rank].c_str()});
  }
  const ro::sim::ArenaRule* free_arena = FreeArenaOf(rules, book.free_arena_id());
  const ro::sim::ArenaRule* board_arena = DefaultArenaOf(rules);
  board_arena_name_ =
      board_arena != nullptr ? ArenaNameOf(rules, *board_arena, tables.map_names) : std::string();
  meta.free_arenas = arena_choices_;
  meta.free_arena_id = free_arena != nullptr ? free_arena->arena_id : std::uint16_t{0};
  meta.board_arena_name = board_arena_name_.c_str();
  prepare.meta = meta;
  if (windows.church.open) ComposeShop(SurvivorsMetaShop::kChurch, windows, save, tables);
  if (windows.kafra.open) ComposeShop(SurvivorsMetaShop::kKafra, windows, save, tables);
  if (windows.codex.open) ComposeCodex(windows, save, nullptr, rules, tables, now_ms);
  if (windows.records.open) {
    const std::optional<SurvivorsCredit>& credit = book.credit();
    windows.records_view = RecordsViewOf(
        rules, save, now_utc_s,
        credit.has_value() ? std::optional<std::int64_t>(credit->played_at_utc_s) : std::nullopt,
        record_rows_, book.free_arena_id());
    windows.records_view.ranked_shown = ranking_open;
  }
}

void SurvivorsViews::ComposeRanking(const SurvivorsStage& stage, const SurvivorsViewFacts& facts,
                                    SurvivorsFrameView& view) {
  hud_.ranked = false;
  if (facts.ranking == nullptr) return;
  const SurvivorsRankingSession& session = facts.ranking->session();
  const SurvivorsBoardKind board = stage.board().kind;
  if (view.hud != nullptr) hud_.ranked = HudRankedOf(session, board);
  if (view.summary != nullptr) summary_.ranking = SummaryRankOf(session, board);
  if (stage.screen() != SurvivorsScreen::kSummary || facts.meta == nullptr) return;
  SurvivorsMetaWindows& meta = *facts.meta;
  ComposeRankingWindow(*facts.ranking, meta.ranking, meta.ranking_view, ranking_rows_);
  if (!meta.ranking.open) return;
  view.ranking = &meta.ranking_view;
  view.ranking_state = &meta.ranking;
}

void SurvivorsViews::ComposeShop(SurvivorsMetaShop shop, SurvivorsMetaWindows& windows,
                                 const SurvivorsSave& save, const SurvivorsTables& tables) {
  const bool church = shop == SurvivorsMetaShop::kChurch;
  std::vector<ro::ui::SurvivorsShopRow>& rows = church ? church_rows_ : kafra_rows_;
  ShopRowsOf(save, shop, rows);
  if (church) {
    for (std::size_t rank = 0; rank < rows.size() && rank < Meta().blessings.size(); ++rank) {
      const ro::sim::BlessingRule& rule = Meta().blessings[rank];
      const ro::sim::SkillNode* node = NodeNamed(rule.skill_name, tables);
      rows[rank].icon = node != nullptr ? SurvivorsSkillIcon(node->id, tables) : ImTextureID{0};
      rows[rank].name = node != nullptr ? SkillName(node->id, tables) : rule.key;
    }
    windows.church_view = ShopViewOf(save, shop, rows);
    return;
  }
  for (std::size_t rank = 0; rank < rows.size() && rank < Meta().kafra.size(); ++rank) {
    const ro::sim::KafraRule& rule = Meta().kafra[rank];
    rows[rank].icon = SurvivorsItemIcon(rule.item_id, tables);
    rows[rank].name = CardName(rule.item_id, tables);
  }
  windows.kafra_view = ShopViewOf(save, shop, rows);
}

void SurvivorsViews::ComposeAlbum(SurvivorsMetaWindows& windows, const SurvivorsSave& save,
                                  const ro::sim::SurvivorsRules& rules,
                                  const SurvivorsTables& tables) {
  AlbumCardsOf(rules, save, album_cards_);
  const std::span<const std::uint16_t> pool =
      rules.classes.empty() ? std::span<const std::uint16_t>() : rules.classes.front().card_pool;
  for (std::size_t rank = 0; rank < album_cards_.size() && rank < pool.size(); ++rank) {
    ro::ui::SurvivorsAlbumEntry& entry = album_cards_[rank];
    const std::uint16_t card_item_id = pool[rank];
    entry.name = CardName(card_item_id, tables);
    // Une carte non trouvée ne charge même pas son illustration.
    if (entry.found > 0) {
      const ro::ui::SkinImage art = SurvivorsCardIllustration(card_item_id, tables);
      entry.illustration = art.id;
      entry.illustration_size = ImVec2(art.width, art.height);
    }
    const auto dropper =
        std::ranges::find(rules.foes, card_item_id, &ro::sim::FoeRule::card_item_id);
    if (dropper != rules.foes.end()) entry.dropped_by = FoeName(dropper->foe_id, tables);
  }
  FeatEntriesOf(save, album_feats_);
  NameFeats(album_feats_, rules, tables);
  ro::ui::SkinImage back;
  if (tables.item_icons != nullptr) {
    back = SkinImageOf(tables.item_icons->GetCollection(kGroundCardItem));
  }
  windows.album_view = ro::ui::SurvivorsAlbumView{.cards = album_cards_,
                                                  .card_back = back.id,
                                                  .card_back_size = ImVec2(back.width, back.height),
                                                  .feats = album_feats_};
}

void SurvivorsViews::NameFeats(std::span<ro::ui::SurvivorsFeatEntry> feats,
                               const ro::sim::SurvivorsRules& rules,
                               const SurvivorsTables& tables) {
  for (ro::ui::SurvivorsFeatEntry& feat : feats) {
    const std::string_view key = feat.key;
    const auto skill = std::ranges::find_if(
        rules.skills, [key](const ro::sim::SurvivorsSkillRule& rule) { return key == rule.key; });
    if (skill == rules.skills.end()) continue;
    feat.icon = SurvivorsSkillIcon(skill->skill_id, tables);
    feat.name = SkillName(skill->skill_id, tables);
  }
}

void SurvivorsViews::ComposeCredit(const SurvivorsStage& stage,
                                   const ro::sim::SurvivorsRules& rules,
                                   const SurvivorsViewFacts& facts, const SurvivorsTables& tables) {
  // Les succès d'abord : un carnet qui n'a pas pu être crédité ne les cache pas.
  TrophyRowsOf(facts.trophies, trophy_rows_);
  if (!trophy_rows_.empty()) {
    const ImTextureID trophy_icon = SurvivorsItemIcon(kTrophyIconItem, tables);
    for (ro::ui::SurvivorsTrophyRow& trophy : trophy_rows_) trophy.icon = trophy_icon;
  }
  summary_.trophies = trophy_rows_;
  const SurvivorsEnding* ending = stage.ending();
  if (facts.credit == nullptr || facts.save == nullptr || ending == nullptr) return;
  ro::ui::SurvivorsCreditRows rows =
      CreditRowsOf(*facts.credit, *ending, *facts.save, credit_rows_);
  const std::span<const std::uint16_t> found = ending->recap.cards_found;
  for (std::size_t rank = 0; rank < credit_rows_.dropped.size() && rank < found.size(); ++rank) {
    ro::ui::SurvivorsDroppedCardRow& row = credit_rows_.dropped[rank];
    const ro::ui::SkinImage art = SurvivorsCardIllustration(found[rank], tables);
    row.illustration = art.id;
    row.illustration_size = ImVec2(art.width, art.height);
    row.name = CardName(found[rank], tables);
  }
  NameFeats(credit_rows_.feats, rules, tables);
  summary_.credit = rows;
}

void SurvivorsViews::ComposeHudMeta(const SurvivorsStage& stage,
                                    const ro::sim::SurvivorsField& field,
                                    const SurvivorsTables& tables) {
  hud_charges_.clear();
  const std::array<std::pair<ro::sim::KafraService, std::uint8_t>, ro::sim::kKafraServiceCount>
      charges{{{ro::sim::KafraService::kMagnifier, field.rerolls_left},
               {ro::sim::KafraService::kFlyWing, field.skips_left},
               {ro::sim::KafraService::kButterflyWing, field.banishes_left}}};
  for (const auto& [service, count] : charges) {
    if (count == 0) continue;
    const ro::sim::KafraRule* rule = ro::sim::FindKafra(Meta(), service);
    hud_charges_.push_back(ro::ui::SurvivorsHudCharge{
        .icon = rule != nullptr ? SurvivorsItemIcon(rule->item_id, tables) : ImTextureID{0},
        .count = count});
  }
  hud_.charges = hud_charges_;
  hud_.lives = field.lives_left;
  if (hud_.lives > 0) {
    const ro::sim::BlessingRule* resurrection =
        ro::sim::FindBlessing(Meta(), ro::sim::Blessing::kResurrection);
    const ro::sim::SkillNode* node =
        resurrection != nullptr ? NodeNamed(resurrection->skill_name, tables) : nullptr;
    hud_.life_icon = node != nullptr ? SurvivorsSkillIcon(node->id, tables) : ImTextureID{0};
  }
  hud_.challenge = stage.board().kind == SurvivorsBoardKind::kChallenge;
  hud_.cursed_water = field.start.cursed_water;
}

}  // namespace moonclient
