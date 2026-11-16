#pragma once

// L'ENCHANTEMENT VU PAR LA FENÊTRE : ce que le modèle devient à l'écran, ce que
// chaque refus et chaque résultat disent.
//
// ⚠ À PART DE L'ÉTAGE, ET SANS FIL NI TEXTURE : les noms, les icônes et les cases de
// carte arrivent par `ItemEnchantLooks`, ce qui laisse tout ceci s'essayer sur un banc.

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/item_enchant.h"
#include "ro/ui/item_enchant_window.h"

namespace moonclient {

// Ce que le joueur lit quand une pièce ou une demande est refusée. `group` peut
// être nul ; `item_name` nomme le matériau manquant ou l'enchantement en cause.
[[nodiscard]] std::string ItemEnchantRefusalText(ro::sim::ItemEnchantRefusal refusal,
                                                 const ro::sim::ItemEnchantGroup* group,
                                                 std::string_view item_name = {});

// Le mot du serveur, une fois sa réponse reçue. `enchant_name` nomme l'enchantement
// posé ou amélioré.
[[nodiscard]] std::string ItemEnchantResultText(const ro::sim::ItemEnchant::Result& result,
                                                std::string_view enchant_name);

// LA DEMANDE QUE LA FENÊTRE DÉSIGNE : sa page, le tirage ou le choix sûr, la case.
[[nodiscard]] ro::sim::ItemEnchantRequest ItemEnchantRequestOf(
    const ro::ui::ItemEnchantWindowState& state);

// CE QU'UN ENCHANTEMENT DONNE, EN UNE LIGNE, tiré de sa fiche : couleurs et traits
// ôtés, paliers gardés (« STR +1 · Refine Level +7: STR +1 »), arrêté à la première
// rubrique (« [Enchantable Equipment] »), au trait de séparation ou à `max_bytes`.
[[nodiscard]] std::string ItemEffectLine(std::span<const std::string> description,
                                         std::size_t max_bytes = 160);

// D'où viennent les noms, les icônes, les cases de carte et les effets des pièces.
struct ItemEnchantLooks {
  std::function<std::string(const ro::sim::InventoryItem&)> name_of_held;
  std::function<std::string(std::uint32_t)> name_of_id;
  std::function<ro::ui::SkinImage(std::uint32_t)> icon_of;
  std::function<std::uint8_t(std::uint32_t)> card_slots_of;
  // Vide : aucune ligne d'effet ne se montre.
  std::function<std::string(std::uint32_t)> effect_of;
};

// UNE TENTATIVE DÉJÀ FAITE : le mot du serveur, et s'il dit une réussite.
struct ItemEnchantAttempt {
  std::string text;
  bool good = false;
};

struct ItemEnchantScene {
  const ro::sim::ItemEnchant& enchant;
  const ro::sim::ItemEnchantGroup& group;
  const ro::sim::Inventory& inventory;
  const ItemEnchantLooks& looks;
  const ro::ui::ItemEnchantWindowState& state;
  // La pièce montrée : celle que la séance a choisie, ou que la dernière réponse a
  // touchée quand la séance est close.
  std::optional<std::uint16_t> chosen_slot;
  // La bourse du joueur, « assez » quand le serveur ne l'a pas dite.
  std::int64_t zeny = 0;
  bool zeny_known = true;
  // Le dernier mot du serveur.
  std::string_view notice;
  bool notice_good = false;
  // La séance est close par une réponse : la fenêtre montre ce qu'elle a donné, et
  // tout se retient jusqu'à ce que le PNJ la rouvre.
  bool concluded = false;
  // Les tentatives de la partie, la plus récente en tête.
  std::span<const ItemEnchantAttempt> history;
  // Le PNJ de la séance est connu : la séance close offre « Recommencer ».
  bool npc_known = false;
};

// Compose la vue, et garde ce vers quoi elle pointe jusqu'à la prochaine.
class ItemEnchantViewComposer {
 public:
  [[nodiscard]] const ro::ui::ItemEnchantView& Compose(const ItemEnchantScene& scene);

 private:
  [[nodiscard]] std::string_view Keep(std::string text);
  [[nodiscard]] ro::ui::TradeItemView HeldOf(const ItemEnchantScene& scene,
                                             const ro::sim::InventoryItem& item);
  [[nodiscard]] ro::ui::TradeItemView ItemOf(const ItemEnchantScene& scene, std::uint32_t item_id);
  [[nodiscard]] std::string_view EffectOf(const ItemEnchantScene& scene, std::uint32_t item_id);
  void ComposeCandidates(const ItemEnchantScene& scene);
  void ComposePreview(const ItemEnchantScene& scene, const ro::sim::InventoryItem& item);
  void ComposeChoice(const ItemEnchantScene& scene, const ro::sim::InventoryItem& item);
  void ComposeOperation(const ItemEnchantScene& scene, const ro::sim::InventoryItem& item);
  void ComposeCost(const ItemEnchantScene& scene, const ro::sim::ItemEnchantCost& cost);
  void ComposeTexts(const ItemEnchantScene& scene, const ro::sim::InventoryItem* item);

  // ⚠ LES VUES POINTENT DANS CES TEXTES : une file ne déplace pas ce qu'elle tient.
  std::deque<std::string> texts_;
  std::vector<ro::ui::ItemEnchantPieceView> candidates_;
  std::vector<ro::ui::ItemEnchantSlotView> slots_;
  std::vector<ro::ui::ItemEnchantOptionView> options_;
  std::vector<ro::ui::ItemEnchantMaterialView> materials_;
  std::vector<ro::ui::ItemEnchantPreviewView> preview_;
  std::vector<ro::ui::ItemEnchantHistoryView> history_;
  std::vector<std::string_view> blockers_;
  ro::ui::ItemEnchantView view_;
};

}  // namespace moonclient
