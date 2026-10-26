// LE CODEX DANS LES VUES DU MODE SURVIVORS : les fiches, composées en nombres,
// clés et identifiants par `survivors_codex_desk.h`, reçoivent ici leurs noms,
// leurs icônes et le sprite de l'entrée choisie, lus dans les tables du jeu.
//
// 🔴 UNE ENTRÉE NON VUE NE CHARGE QUE CE QUE SA SILHOUETTE DEMANDE : l'icône
// ou le sprite, que la fenêtre peint en noir. Jamais l'illustration d'une
// carte : le Codex n'en charge aucune.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/render/actor_vignette.h"

#include "game/skin_image_of.h"
#include "game/survivors_views.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

using ro::ui::SurvivorsCodexSheet;

// L'icône d'un sort, à sa taille : la fenêtre la ramène dans sa boîte.
[[nodiscard]] ro::ui::SkinImage SkillIconImage(std::uint16_t skill_id,
                                               const SurvivorsTables& tables) {
  if (tables.skill_icons == nullptr || tables.skill_tree == nullptr) return {};
  const ro::sim::SkillNode* node = tables.skill_tree->Find(skill_id);
  if (node == nullptr) return {};
  return SkinImageOf(tables.skill_icons->Get(skill_id, node->idname));
}

[[nodiscard]] ro::ui::SkinImage ItemIconImage(std::uint16_t item_id,
                                              const SurvivorsTables& tables) {
  return tables.items != nullptr ? tables.items->IconOf(item_id) : ro::ui::SkinImage{};
}

// La fiche se dessine avec le sprite de son monstre, de sa plante ou de son boss.
[[nodiscard]] bool ShowsSprite(SurvivorsCodexSheet sheet) noexcept {
  return sheet == SurvivorsCodexSheet::kFoe || sheet == SurvivorsCodexSheet::kBoss ||
         sheet == SurvivorsCodexSheet::kPlant;
}

}  // namespace

void SurvivorsViews::ComposeCodex(SurvivorsMetaWindows& windows, const SurvivorsSave& save,
                                  const SurvivorsCodexSeen* live,
                                  const ro::sim::SurvivorsRules& rules,
                                  const SurvivorsTables& tables, std::uint32_t now_ms) {
  windows.codex_view = ComposeSurvivorsCodex(rules, CodexSeenOf(rules, save, live), codex_rows_,
                                             FoundCardItemsOf(rules, save));
  for (std::vector<ro::ui::SurvivorsCodexEntry>& tab : codex_rows_.entries) {
    for (ro::ui::SurvivorsCodexEntry& entry : tab) NameCodexEntry(entry, tables);
  }
  for (std::vector<ro::ui::SurvivorsCodexSource>& sources : codex_rows_.sources) {
    for (ro::ui::SurvivorsCodexSource& source : sources) {
      NameCodexLink(source.bringer, CodexLinkKind::kFoe, tables);
    }
  }
  for (std::vector<ro::ui::SurvivorsCodexMove>& moves : codex_rows_.moves) {
    for (ro::ui::SurvivorsCodexMove& move : moves) {
      NameCodexLink(move.foe, CodexLinkKind::kFoe, tables);
    }
  }
  ComposeCodexSprite(windows.codex, tables, now_ms);
  // L'ONGLET CARTES EST L'ALBUM, composé comme il l'a toujours été.
  ComposeAlbum(windows, save, rules, tables);
}

void SurvivorsViews::NameCodexEntry(ro::ui::SurvivorsCodexEntry& entry,
                                    const SurvivorsTables& tables) {
  switch (entry.sheet) {
    case SurvivorsCodexSheet::kSkill:
      entry.name = SkillName(entry.ro_id, tables);
      entry.icon = SkillIconImage(entry.ro_id, tables);
      NameCodexLink(entry.evolution.card, CodexLinkKind::kItem, tables);
      NameCodexLink(entry.evolution.result, CodexLinkKind::kSkill, tables);
      NameCodexLink(entry.evolved_from.card, CodexLinkKind::kItem, tables);
      NameCodexLink(entry.evolved_from.result, CodexLinkKind::kSkill, tables);
      return;
    case SurvivorsCodexSheet::kItem:
    case SurvivorsCodexSheet::kChest:
    case SurvivorsCodexSheet::kGem:
      entry.name = CardName(entry.ro_id, tables);
      entry.icon = ItemIconImage(entry.ro_id, tables);
      return;
    case SurvivorsCodexSheet::kGroundCard:
      // La carte au sol est l'objet que l'arène pose : son icône, jamais une
      // illustration.
      entry.name = CardName(kGroundCardItem, tables);
      entry.icon = ItemIconImage(kGroundCardItem, tables);
      return;
    case SurvivorsCodexSheet::kPlant:
      entry.name = FoeName(entry.ro_id, tables);
      NameCodexLink(entry.drop, CodexLinkKind::kItem, tables);
      return;
    case SurvivorsCodexSheet::kFoe:
    case SurvivorsCodexSheet::kBoss:
      entry.name = FoeName(entry.ro_id, tables);
      NameCodexLink(entry.card.card, CodexLinkKind::kItem, tables);
      NameCodexLink(entry.escort, CodexLinkKind::kFoe, tables);
      return;
    case SurvivorsCodexSheet::kEvent:
      NameCodexLink(entry.event_foe, CodexLinkKind::kFoe, tables);
      return;
  }
}

void SurvivorsViews::NameCodexLink(ro::ui::SurvivorsCodexLink& link, CodexLinkKind kind,
                                   const SurvivorsTables& tables) {
  // Un lien sans identifiant (un boss sans escorte) reste vide ; un lien non vu
  // n'a que l'icône de sa silhouette, jamais son nom.
  if (link.ro_id == 0) return;
  switch (kind) {
    case CodexLinkKind::kSkill:
      if (link.seen) link.name = SkillName(link.ro_id, tables);
      link.icon = SkillIconImage(link.ro_id, tables);
      return;
    case CodexLinkKind::kItem:
      if (link.seen) link.name = CardName(link.ro_id, tables);
      link.icon = ItemIconImage(link.ro_id, tables);
      return;
    case CodexLinkKind::kFoe:
      if (link.seen) link.name = FoeName(link.ro_id, tables);
      return;
  }
}

void SurvivorsViews::ComposeCodexSprite(const ro::ui::SurvivorsCodexState& state,
                                        const SurvivorsTables& tables, std::uint32_t now_ms) {
  // UNE AUTRE GARDE-ROBE, UN AUTRE SPRITE : celui d'avant la désigne encore.
  if (codex_sprite_wardrobe_ != tables.wardrobe) {
    codex_sprite_.reset();
    codex_sprite_wardrobe_ = tables.wardrobe;
  }
  const std::size_t tab = static_cast<std::size_t>(state.tab);
  if (tab >= codex_rows_.entries.size() || tables.wardrobe == nullptr) return;
  std::vector<ro::ui::SurvivorsCodexEntry>& entries = codex_rows_.entries[tab];
  if (entries.empty()) return;
  ro::ui::SurvivorsCodexEntry& entry = entries[std::min(state.selected[tab], entries.size() - 1)];
  // La boîte est relevée au dessin : la première image n'en a pas encore.
  const int box_width = static_cast<int>(state.sprite_box_width);
  const int box_height = static_cast<int>(state.sprite_box_height);
  if (!ShowsSprite(entry.sheet) || box_width <= 0 || box_height <= 0) return;
  if (!codex_sprite_.has_value()) codex_sprite_.emplace(*tables.wardrobe);
  const MonsterSprite::Shown shown =
      codex_sprite_->Compose(MonsterSprite::Framing{.sprite_class = entry.ro_id,
                                                    .box_width = box_width,
                                                    .box_height = box_height,
                                                    .fit = ro::render::VignetteFit::kStanding},
                             now_ms);
  // La fiche ne parle pas : les sons des images se jettent.
  static_cast<void>(codex_sprite_->TakeSounds());
  if (!shown.missing) entry.icon = shown.image;
}

}  // namespace moonclient
