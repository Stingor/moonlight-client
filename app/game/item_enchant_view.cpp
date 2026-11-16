// L'enchantement vu par la fenêtre. Voir l'en-tête.

#include "game/item_enchant_view.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/core/text.h"
#include "ro/sim/item_reform.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::core::Tr;
using ro::sim::InventoryItem;
using ro::sim::ItemEnchantMode;
using ro::sim::ItemEnchantRefusal;

// Les cases s'affichent numérotées à partir de un, comme dans le jeu.
constexpr int kShownSlotOffset = 1;
// Une chance en cent-millièmes, écrite en pour cent.
constexpr double kPercentScale = 1000.0;

[[nodiscard]] std::string PercentText(std::uint32_t chance) {
  return Printed("%.2f %%", static_cast<double>(chance) / kPercentScale);
}

// Deux tirages qui posent la même chose, aux mêmes chances.
[[nodiscard]] bool SameDraws(const ro::sim::ItemEnchantDrawGrade& left,
                             const ro::sim::ItemEnchantDrawGrade& right) {
  return std::ranges::equal(
      left.enchants, right.enchants,
      [](const ro::sim::ItemEnchantDraw& a, const ro::sim::ItemEnchantDraw& b) {
        return a.item_id == b.item_id && a.odds == b.odds;
      });
}

// LE TIRAGE DU GRADE SUIVANT, quand il diffère de celui de la pièce ; nul sinon.
[[nodiscard]] const ro::sim::ItemEnchantDrawGrade* OtherGradeDraws(
    const ro::sim::ItemEnchantSlot& entry, std::uint8_t grade) {
  if (grade == std::numeric_limits<std::uint8_t>::max()) {
    return nullptr;
  }
  const ro::sim::ItemEnchantDrawGrade* next = entry.DrawsFor(static_cast<std::uint8_t>(grade + 1));
  const ro::sim::ItemEnchantDrawGrade* now = entry.DrawsFor(grade);
  if (next == nullptr || (now != nullptr && SameDraws(*now, *next))) {
    return nullptr;
  }
  return next;
}

// Les octets d'un caractère UTF-8 qui suivent le premier.
constexpr unsigned char kUtf8ContinuationMask = 0xC0;
constexpr unsigned char kUtf8Continuation = 0x80;

void CutAtCharacter(std::string& text, std::size_t at) {
  while (at > 0 &&
         (static_cast<unsigned char>(text[at]) & kUtf8ContinuationMask) == kUtf8Continuation) {
    --at;
  }
  text.resize(at);
}

}  // namespace

std::string ItemEffectLine(std::span<const std::string> description, std::size_t max_bytes) {
  std::string out;
  for (const std::string& raw : description) {
    std::string plain;
    for (const ro::core::ColorRun& run : ro::core::SplitColorRuns(std::string_view(raw))) {
      plain += run.text;
    }
    // Le « # » de fin de ligne vaut un trait : il ne dit rien de l'effet.
    while (!plain.empty() && (plain.back() == '#' || plain.back() == ' ')) {
      plain.pop_back();
    }
    const std::size_t first = plain.find_first_not_of(' ');
    if (first == std::string::npos) {
      continue;
    }
    plain.erase(0, first);
    // LA PREMIÈRE RUBRIQUE CLÔT L'EFFET : ce qui suit dit où il se pose, pas ce qu'il donne.
    if (plain.starts_with('[') || plain.starts_with("____")) {
      break;
    }
    if (!out.empty()) {
      out += out.back() == ':' ? " " : " · ";
    }
    out += plain;
    if (out.size() > max_bytes) {
      CutAtCharacter(out, max_bytes);
      out += "…";
      break;
    }
  }
  return out;
}

std::string ItemEnchantRefusalText(ItemEnchantRefusal refusal,
                                   const ro::sim::ItemEnchantGroup* group,
                                   std::string_view item_name) {
  switch (refusal) {
    case ItemEnchantRefusal::kNone:
      return {};
    case ItemEnchantRefusal::kNotOpen:
      return Tr("La fenêtre est fermée.");
    case ItemEnchantRefusal::kWaiting:
      return Tr("En attente de la réponse du serveur.");
    case ItemEnchantRefusal::kNothingChosen:
      return Tr("Choisissez la pièce à enchanter.");
    case ItemEnchantRefusal::kPieceGone:
      return Tr("La pièce choisie n'est plus dans l'inventory.");
    case ItemEnchantRefusal::kNotTargeted:
      return Tr("Cette pièce ne s'enchante pas ici.");
    case ItemEnchantRefusal::kWorn:
      return Tr("Cette pièce est portée : ôtez-la d'abord.");
    case ItemEnchantRefusal::kDamaged:
      return Tr("Cette pièce est endommagée : faites-la réparer d'abord.");
    case ItemEnchantRefusal::kRefineTooLow:
      return Printed(Tr("Raffinement trop bas : +%d au moins."),
                     group != nullptr ? static_cast<int>(group->least_refine) : 0);
    case ItemEnchantRefusal::kGradeTooLow:
      return Tr("Le grade de cette pièce est trop bas.");
    case ItemEnchantRefusal::kRandomOptionsForbidden:
      return Tr("Une pièce à options aléatoires n'est pas acceptée.");
    case ItemEnchantRefusal::kNoFreeSlot:
      return Tr("Aucune case libre pour un enchantement.");
    case ItemEnchantRefusal::kSlotClosed:
      return Tr("Cette case ne s'enchante pas ici.");
    case ItemEnchantRefusal::kNoDrawForGrade:
      return Tr("Aucun tirage pour le grade de cette pièce.");
    case ItemEnchantRefusal::kNoSuchEnchant:
      return Tr("Choisissez l'enchantement à poser.");
    case ItemEnchantRefusal::kNothingToUpgrade:
      return Tr("Choisissez une case dont l'enchantement s'améliore.");
    case ItemEnchantRefusal::kNothingToReset:
      return Tr("Aucun enchantement à ôter.");
    case ItemEnchantRefusal::kMissingMaterial:
      return Printed(Tr("Matériau insuffisant : %s."), std::string(item_name).c_str());
    case ItemEnchantRefusal::kNotEnoughZeny:
      return Tr("Pas assez de zenys.");
  }
  return {};
}

std::string ItemEnchantResultText(const ro::sim::ItemEnchant::Result& result,
                                  std::string_view enchant_name) {
  const std::string name(enchant_name);
  if (!result.succeeded()) {
    return result.request.mode == ItemEnchantMode::kReset
               ? std::string(Tr("La remise à zéro a échoué : les enchantements restent."))
               : std::string(Tr("L'enchantement a échoué."));
  }
  switch (result.request.mode) {
    case ItemEnchantMode::kRandom:
    case ItemEnchantMode::kPerfect:
      return Printed(Tr("Enchantement posé : %s."), name.c_str());
    case ItemEnchantMode::kUpgrade:
      return Printed(Tr("Enchantement amélioré : %s."), name.c_str());
    case ItemEnchantMode::kReset:
      return Tr("Tous les enchantements ont été ôtés.");
  }
  return {};
}

ro::sim::ItemEnchantRequest ItemEnchantRequestOf(const ro::ui::ItemEnchantWindowState& state) {
  switch (state.page) {
    case ro::ui::ItemEnchantPage::kEnchant:
      return state.perfect ? ro::sim::ItemEnchantRequest{.mode = ItemEnchantMode::kPerfect,
                                                         .enchant_item_id = state.enchant_item_id}
                           : ro::sim::ItemEnchantRequest{.mode = ItemEnchantMode::kRandom};
    case ro::ui::ItemEnchantPage::kUpgrade:
      return ro::sim::ItemEnchantRequest{.mode = ItemEnchantMode::kUpgrade,
                                         .slot = state.upgrade_slot};
    case ro::ui::ItemEnchantPage::kReset:
      return ro::sim::ItemEnchantRequest{.mode = ItemEnchantMode::kReset};
  }
  return {};
}

std::string_view ItemEnchantViewComposer::Keep(std::string text) {
  return texts_.emplace_back(std::move(text));
}

ro::ui::TradeItemView ItemEnchantViewComposer::HeldOf(const ItemEnchantScene& scene,
                                                      const InventoryItem& item) {
  ro::ui::TradeItemView view;
  view.item_id = item.item_id;
  view.icon = scene.looks.icon_of(item.item_id);
  view.damaged = item.damaged;
  view.name = Keep(scene.looks.name_of_held(item));
  return view;
}

ro::ui::TradeItemView ItemEnchantViewComposer::ItemOf(const ItemEnchantScene& scene,
                                                      std::uint32_t item_id) {
  ro::ui::TradeItemView view;
  view.item_id = item_id;
  view.icon = scene.looks.icon_of(item_id);
  view.name = Keep(scene.looks.name_of_id(item_id));
  return view;
}

std::string_view ItemEnchantViewComposer::EffectOf(const ItemEnchantScene& scene,
                                                   std::uint32_t item_id) {
  if (!scene.looks.effect_of || item_id == 0) {
    return {};
  }
  return Keep(scene.looks.effect_of(item_id));
}

void ItemEnchantViewComposer::ComposeCandidates(const ItemEnchantScene& scene) {
  for (const InventoryItem& item : scene.inventory.items()) {
    if (!scene.group.Targets(item.item_id)) {
      continue;
    }
    ro::ui::ItemEnchantPieceView row;
    row.client_slot = item.client_slot;
    row.item = HeldOf(scene, item);
    row.refusal = Keep(
        ItemEnchantRefusalText(ro::sim::JudgeItemEnchantPiece(scene.group, item), &scene.group));
    row.chosen = scene.chosen_slot == item.client_slot;
    // OÙ EN SONT SES CASES : celles que le groupe remplit, hors cases de carte. Un
    // objet forgé n'en a aucune, et ne dit rien.
    const std::uint8_t card_slots = scene.looks.card_slots_of(item.item_id);
    const auto total = std::ranges::count_if(scene.group.order, [card_slots](std::uint8_t slot) {
      return slot >= card_slots && slot < ro::sim::kItemCardSlots;
    });
    if (total > 0 && !ro::sim::ItemIsForged(item)) {
      const auto done = static_cast<int>(ro::sim::EnchantedSlots(item, card_slots).size());
      row.full = done > 0 && !ro::sim::NextEnchantSlot(scene.group, item, card_slots).has_value();
      row.progress = row.full
                         ? Keep(Tr("pleine"))
                         : Keep(Printed(Tr("%d/%d enchantée(s)"), done, static_cast<int>(total)));
    }
    candidates_.push_back(row);
  }
}

// LA PIÈCE AVANT ET APRÈS : les seules cases que la tentative changerait.
void ItemEnchantViewComposer::ComposePreview(const ItemEnchantScene& scene,
                                             const InventoryItem& item) {
  const std::uint8_t card_slots = scene.looks.card_slots_of(item.item_id);
  const ro::sim::ItemEnchantRequest request = ItemEnchantRequestOf(scene.state);
  const std::optional<std::uint8_t> next = ro::sim::NextEnchantSlot(scene.group, item, card_slots);
  switch (request.mode) {
    case ItemEnchantMode::kRandom:
      if (next.has_value() && view_.random_open) {
        preview_.push_back(ro::ui::ItemEnchantPreviewView{.slot = *next, .drawn = true});
      }
      break;
    case ItemEnchantMode::kPerfect: {
      const ro::sim::ItemEnchantSlot* entry =
          next.has_value() ? scene.group.FindSlot(*next) : nullptr;
      if (entry != nullptr && entry->FindPerfect(request.enchant_item_id) != nullptr) {
        preview_.push_back(
            ro::ui::ItemEnchantPreviewView{.slot = *next,
                                           .after = ItemOf(scene, request.enchant_item_id),
                                           .effect = EffectOf(scene, request.enchant_item_id)});
      }
      break;
    }
    case ItemEnchantMode::kUpgrade: {
      if (request.slot >= ro::sim::kItemCardSlots) {
        break;
      }
      const ro::sim::ItemEnchantSlot* entry = scene.group.FindSlot(request.slot);
      const ro::sim::ItemEnchantUpgrade* upgrade =
          entry != nullptr ? entry->FindUpgrade(item.cards[request.slot]) : nullptr;
      if (upgrade != nullptr) {
        preview_.push_back(
            ro::ui::ItemEnchantPreviewView{.slot = request.slot,
                                           .before = ItemOf(scene, upgrade->from_item_id),
                                           .after = ItemOf(scene, upgrade->to_item_id),
                                           .effect = EffectOf(scene, upgrade->to_item_id)});
      }
      break;
    }
    case ItemEnchantMode::kReset:
      if (!view_.reset_open) {
        break;
      }
      for (const std::uint8_t slot : ro::sim::EnchantedSlots(item, card_slots)) {
        preview_.push_back(ro::ui::ItemEnchantPreviewView{
            .slot = slot, .before = ItemOf(scene, item.cards[slot])});
      }
      break;
  }
}

void ItemEnchantViewComposer::ComposeChoice(const ItemEnchantScene& scene,
                                            const InventoryItem& item) {
  const std::uint8_t card_slots = scene.looks.card_slots_of(item.item_id);
  const std::optional<std::uint8_t> next = ro::sim::NextEnchantSlot(scene.group, item, card_slots);
  const std::vector<std::uint8_t> enchanted = ro::sim::EnchantedSlots(item, card_slots);
  const bool forged = ro::sim::ItemIsForged(item);
  view_.chosen = HeldOf(scene, item);
  for (std::uint8_t slot = 0; slot < ro::sim::kItemCardSlots; ++slot) {
    ro::ui::ItemEnchantSlotView row;
    row.slot = slot;
    if (!forged && item.cards[slot] != 0) {
      row.item = ItemOf(scene, item.cards[slot]);
    }
    row.card_slot = slot < card_slots;
    row.next = next == slot;
    if (std::ranges::find(enchanted, slot) != enchanted.end()) {
      const ro::sim::ItemEnchantSlot* entry = scene.group.FindSlot(slot);
      row.selectable = entry != nullptr && entry->FindUpgrade(item.cards[slot]) != nullptr;
    }
    row.selected = row.selectable && scene.state.upgrade_slot == slot;
    view_.upgrade_open = view_.upgrade_open || row.selectable;
    slots_.push_back(row);
  }
  const ro::sim::ItemEnchantSlot* entry = next.has_value() ? scene.group.FindSlot(*next) : nullptr;
  view_.random_open = entry != nullptr && entry->DrawsFor(item.grade) != nullptr;
  view_.perfect_open = entry != nullptr && !entry->perfect.empty();
  view_.enchant_open = view_.random_open || view_.perfect_open;
  view_.reset_open =
      scene.group.reset.has_value() && scene.group.reset->chance > 0 && !enchanted.empty();
}

void ItemEnchantViewComposer::ComposeCost(const ItemEnchantScene& scene,
                                          const ro::sim::ItemEnchantCost& cost) {
  for (const ro::sim::ItemEnchantMaterial& material : cost.materials) {
    ro::ui::ItemEnchantMaterialView row;
    row.item = ItemOf(scene, material.item_id);
    row.owned = static_cast<int>(ro::sim::CountedMaterial(scene.inventory, material.item_id));
    row.needed = material.amount;
    materials_.push_back(row);
  }
  view_.zeny = cost.zeny;
}

void ItemEnchantViewComposer::ComposeOperation(const ItemEnchantScene& scene,
                                               const InventoryItem& item) {
  const std::uint8_t card_slots = scene.looks.card_slots_of(item.item_id);
  const ro::sim::ItemEnchantRequest request = ItemEnchantRequestOf(scene.state);
  const std::optional<std::uint8_t> next = ro::sim::NextEnchantSlot(scene.group, item, card_slots);
  const ro::sim::ItemEnchantSlot* entry = next.has_value() ? scene.group.FindSlot(*next) : nullptr;
  const int shown_next = next.has_value() ? *next + kShownSlotOffset : 0;
  switch (request.mode) {
    case ItemEnchantMode::kRandom: {
      if (entry == nullptr) {
        break;
      }
      const ro::sim::ItemEnchantDrawGrade* draws = entry->DrawsFor(item.grade);
      const std::size_t count = draws != nullptr ? draws->enchants.size() : 0;
      view_.summary = Keep(Printed(Tr("Case %d : un enchantement tiré au sort parmi %d."),
                                   shown_next, static_cast<int>(count)));
      view_.options_title = Keep(Tr("Ce que le tirage peut poser"));
      // LE GRADE SUIVANT TIRE AUTRE CHOSE : la case à cocher le montre, sans que la
      // tentative change — elle tire toujours au grade de la pièce.
      const ro::sim::ItemEnchantDrawGrade* other = OtherGradeDraws(*entry, item.grade);
      if (other != nullptr) {
        view_.other_grade_label = Keep(Tr("Voir le tirage du grade suivant"));
        if (scene.state.other_grade) {
          draws = other;
          view_.options_title =
              Keep(Tr("Ce que le tirage poserait au grade suivant (pas cette tentative)"));
        }
      }
      if (draws != nullptr) {
        for (const ro::sim::ItemEnchantDraw& draw : draws->enchants) {
          options_.push_back(
              ro::ui::ItemEnchantOptionView{.item = ItemOf(scene, draw.item_id),
                                            .odds = Keep(PercentText(draw.odds)),
                                            .effect = EffectOf(scene, draw.item_id)});
        }
      }
      view_.chance = entry->chance < ro::sim::kItemEnchantCertain
                         ? Keep(Printed(Tr("Réussite : %s"), PercentText(entry->chance).c_str()))
                         : Keep(Tr("Réussite assurée."));
      break;
    }
    case ItemEnchantMode::kPerfect: {
      if (entry == nullptr) {
        break;
      }
      view_.summary =
          Keep(Printed(Tr("Case %d : l'enchantement choisi, sans risque."), shown_next));
      view_.options_title = Keep(Tr("Enchantements au choix"));
      for (const ro::sim::ItemEnchantPerfect& perfect : entry->perfect) {
        const bool short_of =
            (scene.zeny_known && scene.zeny < perfect.cost.zeny) ||
            ro::sim::MissingEnchantMaterial(perfect.cost, scene.inventory) != nullptr;
        options_.push_back(
            ro::ui::ItemEnchantOptionView{.item = ItemOf(scene, perfect.item_id),
                                          .effect = EffectOf(scene, perfect.item_id),
                                          .selectable = true,
                                          .chosen = scene.state.enchant_item_id == perfect.item_id,
                                          .short_of = short_of});
      }
      view_.chance = Keep(Tr("Réussite assurée."));
      break;
    }
    case ItemEnchantMode::kUpgrade: {
      if (request.slot >= ro::sim::kItemCardSlots) {
        view_.summary = Keep(Tr("Choisissez la case dont l'enchantement doit monter."));
        break;
      }
      const ro::sim::ItemEnchantSlot* upgraded = scene.group.FindSlot(request.slot);
      const ro::sim::ItemEnchantUpgrade* upgrade =
          upgraded != nullptr ? upgraded->FindUpgrade(item.cards[request.slot]) : nullptr;
      if (upgrade == nullptr) {
        break;
      }
      view_.summary = Keep(Printed(Tr("Case %d : %s devient %s."), request.slot + kShownSlotOffset,
                                   scene.looks.name_of_id(upgrade->from_item_id).c_str(),
                                   scene.looks.name_of_id(upgrade->to_item_id).c_str()));
      view_.options_title = Keep(Tr("Devient"));
      options_.push_back(
          ro::ui::ItemEnchantOptionView{.item = ItemOf(scene, upgrade->to_item_id),
                                        .effect = EffectOf(scene, upgrade->to_item_id)});
      view_.chance = Keep(Tr("Réussite assurée."));
      break;
    }
    case ItemEnchantMode::kReset: {
      if (!scene.group.reset.has_value()) {
        break;
      }
      const std::size_t enchanted = ro::sim::EnchantedSlots(item, card_slots).size();
      view_.summary =
          Keep(Printed(Tr("Ôte les %d enchantement(s) de la pièce ; les cartes restent."),
                       static_cast<int>(enchanted)));
      view_.chance =
          scene.group.reset->chance < ro::sim::kItemEnchantCertain
              ? Keep(Printed(Tr("Réussite : %s"), PercentText(scene.group.reset->chance).c_str()))
              : Keep(Tr("Réussite assurée."));
      break;
    }
  }
  if (const ro::sim::ItemEnchantCost* cost =
          ro::sim::ItemEnchantCostOf(scene.group, item, request, card_slots);
      cost != nullptr) {
    ComposeCost(scene, *cost);
  }
}

void ItemEnchantViewComposer::ComposeTexts(const ItemEnchantScene& scene,
                                           const InventoryItem* item) {
  view_.title = Keep(Tr("Enchantement"));
  view_.instruction = Keep(
      Tr("Choisissez une pièce de l'inventory, ôtée, ou glissez-la sur la fenêtre, puis ce qu'il "
         "faut y faire. Chaque tentative ferme la séance : « Recommencer » reparle au PNJ pour la "
         "suivante."));
  view_.notice = Keep(std::string(scene.notice));
  view_.notice_good = scene.notice_good;
  const ro::sim::ItemEnchantRequest request = ItemEnchantRequestOf(scene.state);
  const std::string piece = item != nullptr ? std::string(view_.chosen.name) : std::string();
  switch (request.mode) {
    case ItemEnchantMode::kRandom:
      view_.action_label = Keep(Tr("Tenter l'enchantement"));
      view_.confirmation = Keep(
          Printed(Tr("Tenter un enchantement au hasard sur %s ? Les zenys et les matériaux seront "
                     "consommés, même en cas d'échec."),
                  piece.c_str()));
      break;
    case ItemEnchantMode::kPerfect:
      view_.action_label = Keep(Tr("Poser l'enchantement"));
      view_.confirmation =
          Keep(Printed(Tr("Poser %s sur %s ? Les zenys et les matériaux seront consommés."),
                       scene.looks.name_of_id(request.enchant_item_id).c_str(), piece.c_str()));
      break;
    case ItemEnchantMode::kUpgrade:
      view_.action_label = Keep(Tr("Améliorer"));
      view_.confirmation = Keep(Printed(
          Tr("Améliorer l'enchantement de la case %d de %s ? Les zenys et les matériaux seront "
             "consommés."),
          request.slot + kShownSlotOffset, piece.c_str()));
      break;
    case ItemEnchantMode::kReset:
      view_.action_label = Keep(Tr("Tout ôter"));
      view_.confirmation = Keep(Printed(
          Tr("Ôter tous les enchantements de %s ? Les zenys et les matériaux seront consommés, "
             "même en cas d'échec."),
          piece.c_str()));
      break;
  }

  blockers_.clear();
  if (scene.concluded) {
    return;
  }
  const std::int64_t purse =
      scene.zeny_known ? scene.zeny : std::numeric_limits<std::int64_t>::max();
  const std::uint8_t card_slots = item != nullptr ? scene.looks.card_slots_of(item->item_id) : 0;
  const ro::sim::ItemEnchantCheck check = ro::sim::CheckItemEnchantRequest(
      scene.enchant, scene.group, request, scene.inventory, card_slots, purse);
  if (!check.passed() && check.refusal != ItemEnchantRefusal::kWaiting) {
    const std::string named =
        check.item_id != 0 ? scene.looks.name_of_id(check.item_id) : std::string();
    blockers_.push_back(Keep(ItemEnchantRefusalText(check.refusal, &scene.group, named)));
  }
}

const ro::ui::ItemEnchantView& ItemEnchantViewComposer::Compose(const ItemEnchantScene& scene) {
  texts_.clear();
  candidates_.clear();
  slots_.clear();
  options_.clear();
  materials_.clear();
  preview_.clear();
  history_.clear();
  view_ = ro::ui::ItemEnchantView{};

  ComposeCandidates(scene);
  const InventoryItem* item =
      scene.chosen_slot.has_value() ? scene.inventory.Find(*scene.chosen_slot) : nullptr;
  if (item != nullptr && scene.group.Targets(item->item_id)) {
    ComposeChoice(scene, *item);
    ComposeOperation(scene, *item);
    // UNE SÉANCE CLOSE N'A PLUS RIEN À PRÉVOIR : la pièce montrée est déjà l'« après ».
    if (!scene.concluded) {
      ComposePreview(scene, *item);
    }
  } else {
    item = nullptr;
  }
  ComposeTexts(scene, item);
  view_.candidates = candidates_;
  view_.slots = slots_;
  view_.options = options_;
  view_.materials = materials_;
  view_.preview = preview_;
  for (const ItemEnchantAttempt& attempt : scene.history) {
    history_.push_back(ro::ui::ItemEnchantHistoryView{.text = attempt.text, .good = attempt.good});
  }
  view_.history = history_;
  view_.retry_open = scene.concluded && scene.npc_known;
  view_.blockers = blockers_;
  view_.purse = scene.zeny_known ? scene.zeny : -1;
  view_.choice_seq = scene.enchant.choice_seq();
  // UNE SÉANCE CLOSE SE MONTRE SANS SE TOUCHER : tout se retient comme en attente.
  view_.waiting = scene.enchant.waiting() || scene.concluded;
  return view_;
}

}  // namespace moonclient
