#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/render/item_icons.h"
#include "ro/sim/inventory.h"
#include "ro/sim/skill_tree.h"
#include "ro/ui/craft_atlas_window.h"
#include "ro/vfs/item_stacking.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/vfs.h"

#include "items/craft_book.h"

namespace moonclient {

// Ce que les réglages décident de ce que l'Atlas montre.
struct CraftAtlasFilters {
  // Montrer aussi les recettes dont aucune classe de ce serveur n'apprend le
  // skill : le serveur les refusera toujours.
  bool show_unlearnable = false;
  bool only_craftable = false;
  // La fenêtre de chat peut recevoir un lien de recette.
  bool can_share = false;
};

// L'ATLAS, VU DE LA PARTIE : le livre des recettes, et ce que la fenêtre en
// montre — croisé avec la table des items, leurs icônes, les noms de skill et
// le contenu du sac.
//
// ⚠ LES LISTES SE TRIENT UNE FOIS, par NOM : le joueur cherche « Emperium »,
// pas 714. Les noms viennent de la table de l'installation, qui ne change pas en
// cours de partie ; seuls le sac et la fiche se recomposent à chaque image.
class CraftAtlasView {
 public:
  // Tous désignés, et tous facultatifs : sans table des items, les lignes
  // portent « #id » ; sans icônes, un blanc ; sans arbre, le numéro du skill.
  CraftAtlasView(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                 const ro::sim::SkillTree* skill_tree);

  // Lit le livre dans l'installation. Une absence se dit UNE fois au journal,
  // et la fenêtre la dit au joueur.
  void Load(ro::vfs::Vfs& vfs);
  // Pour un banc : un livre déjà lu.
  void UseBook(CraftBook book);

  [[nodiscard]] const CraftBook* book() const noexcept {
    return book_.has_value() ? &*book_ : nullptr;
  }

  // Compose la vue. À appeler à chaque image où la fenêtre est ouverte, APRÈS
  // avoir noté ce qu'elle a dessiné.
  void Refresh(const ro::ui::CraftAtlasWindowState& state, const ro::sim::Inventory& bag,
               const CraftAtlasFilters& filters);
  [[nodiscard]] const ro::ui::CraftAtlasView& view() const noexcept { return view_; }

  // Le catalogue du vote shop, pour l'entrée du menu d'un item. ⚠ À POSER APRÈS
  // `Refresh`, qui remet la vue à zéro.
  void NoteVoteShop(std::span<const ro::ui::VoteShopOffer> offers) noexcept {
    view_.vote_shop = offers;
  }
  // Le droit du staff, pour la ligne « fabrications », et ce qui borne ses
  // demandes : l'empilement de chaque ingrédient de la fiche et les cases libres
  // du sac — vides quand on ne les sait pas. Même règle : après `Refresh`.
  void NoteStaff(bool staff_tools, const ro::vfs::ItemStacking& stacking,
                 std::optional<int> free_bag_slots);

  // Les items dont la fenêtre a dessiné une ligne : leurs icônes se chargent.
  void NoteDrawn(std::span<const std::uint32_t> item_ids);

  // UN LIEN DE RECETTE SURVOLÉ, dans le fil ou dans un dialogue.
  void NoteHoveredRecipe(std::uint32_t product_id) noexcept { hovered_recipe_ = product_id; }
  // Son aperçu, UNE fois : la note se consomme. Nul quand rien n'est survolé —
  // ou quand ce livre ne connaît pas la recette : un aperçu vide ne dirait
  // rien que le clic ne dise mieux.
  [[nodiscard]] const ro::ui::CraftRecipePreview* TakePreview();

  // LE LIBELLÉ D'UN SKILL DE RECETTE. Zéro n'est pas un skill manquant mais la
  // fabrication PAR UN ITEM — et 🔴 encore faut-il dire lequel : les soixante
  // plats sont dans ce cas, et les ranger sous « sans skill » à côté d'un
  // « Mix Cooking » bien nommé se lirait comme une donnée manquante. Un
  // `item_level` de plat les nomme donc « Cuisine (kit) ».
  [[nodiscard]] std::string_view SkillLabel(int skill_id, int item_level = 0);

  // Le nom d'un item, jamais vide.
  [[nodiscard]] std::string_view NameOf(std::uint32_t item_id);

  // L'ÉTIQUETTE « CRAFT » D'UNE DESCRIPTION et son infobulle, déjà traduites.
  // Faux quand l'item n'a rien à voir avec la fabrication, ou quand le livre
  // manque : l'absence d'une entrée ne prouve rien quand la table entière est
  // absente.
  //
  // ⚠ LE LIBELLÉ PORTE L'INFORMATION LA PLUS UTILE : la RECETTE d'abord — savoir
  // qu'on peut fabriquer l'item, et par quel skill, prime sur le fait qu'il
  // serve ailleurs —, puis le nombre de recettes, puis les flèches.
  [[nodiscard]] bool CraftTagOf(std::uint32_t item_id, std::string* label, std::string* tip);

 private:
  void BuildIndex();
  [[nodiscard]] ro::ui::CraftAtlasRow RowOf(std::uint32_t item_id);
  [[nodiscard]] int Owned(std::uint32_t item_id) const;
  [[nodiscard]] bool Playable(const CraftRecipe& recipe, const CraftAtlasFilters& filters) const;
  // Combien de recettes JOUABLES consomment ce matériau. ⚠ LE MÊME COMPTE pour
  // la liste et pour la fiche : deux chiffres pour la même question enverraient
  // chercher des recettes qui n'existent pas.
  [[nodiscard]] int PlayableUses(std::uint32_t material_id, const CraftAtlasFilters& filters) const;
  void ComposeLists(const CraftAtlasFilters& filters);
  void ComposeSheet(std::uint32_t item_id, const CraftAtlasFilters& filters);

  const ro::vfs::ItemTable* items_;
  ro::render::ItemIcons* icons_;
  const ro::sim::SkillTree* skill_tree_;
  std::optional<CraftBook> book_;

  // ── CE QUI SE BÂTIT UNE FOIS ──────────────────────────────────────────────
  bool indexed_ = false;
  std::unordered_map<std::uint32_t, std::string> names_;
  std::unordered_map<int, std::string> skill_names_;
  std::unordered_map<std::uint32_t, ro::ui::SkinImage> icons_seen_;
  // La génération des icônes que `icons_seen_` a copiées : voir
  // `ItemIcons::Clear`.
  std::uint64_t icons_generation_ = 0;
  std::vector<std::uint32_t> sorted_products_;
  std::vector<std::uint32_t> sorted_materials_;
  std::vector<std::uint32_t> sorted_arrow_sources_;

  // ── CE QUI SE RECOMPOSE À CHAQUE IMAGE ────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs
  // remplis, jamais avant — un vecteur qui grandit déplace ce qu'il porte.
  std::unordered_map<std::uint32_t, int> owned_;
  std::vector<ro::ui::CraftAtlasRow> group_rows_;
  std::vector<ro::ui::CraftAtlasGroup> groups_;
  std::vector<ro::ui::CraftAtlasRow> products_;
  std::vector<ro::ui::CraftAtlasRow> materials_;
  std::vector<ro::ui::CraftAtlasRow> arrow_sources_;
  std::vector<ro::ui::CraftAtlasRow> ingredients_;
  std::vector<ro::ui::CraftAtlasBulkView> bulk_;
  std::vector<ro::ui::CraftAtlasRow> arrow_yields_;
  std::vector<ro::ui::CraftAtlasRow> uses_;
  std::vector<ro::ui::CraftAtlasRow> sheet_arrow_sources_;
  ro::ui::CraftAtlasView view_;

  // ── L'APERÇU D'UN LIEN ────────────────────────────────────────────────────
  std::uint32_t hovered_recipe_ = 0;
  std::vector<ro::ui::CraftAtlasRow> preview_ingredients_;
  ro::ui::CraftRecipePreview preview_;
};

}  // namespace moonclient
