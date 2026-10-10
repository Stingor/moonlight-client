#include "ro/ui/craft_atlas_window.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// ── LES MESURES, en largeurs de police ──────────────────────────────────────
//
// ⚠ EN `em` ET NON EN PIXELS : la fenêtre suit la police choisie, sinon elle
// rétrécit à l'œil quand la police grossit.
constexpr float kFirstWidthEm = 54.0F;
constexpr float kFirstHeightEm = 28.0F;
// 🔴 LA LARGEUR MINIMALE LAISSE TENIR LES QUATRE ONGLETS ET LA COLONNE DE
// GAUCHE : en dessous, la barre d'onglets défile et les deux derniers
// disparaissent — on ne découvre pas un onglet qu'on ne voit pas.
constexpr float kLeastWidthEm = 38.0F;
constexpr float kLeastHeightEm = 15.0F;
constexpr float kFilterWidthEm = 14.0F;
// La colonne des listes. ⚠ PLAFONNÉE EN `em`, PAS EN POURCENTAGE : sur une
// fenêtre élargie, c'est la FICHE qui doit gagner la place — elle porte des
// lignes longues, la liste n'a que des noms. Et le plafond compte aussi les
// quatre onglets, la flèche d'un arbre, l'icône et un libellé composé comme
// « Cuisine (kit) (60) » : seize `em` rogneraient tout, onglets compris.
constexpr float kListWidthEm = 24.0F;
constexpr float kListWidthShare = 0.55F;
// L'indentation de l'arbre des skills : celle d'ImGui vaut une largeur de
// police entière, prise sur CHAQUE ligne alors que l'arbre n'a qu'un niveau.
constexpr float kTreeIndentEm = 0.75F;
// L'icône de l'en-tête de la fiche, en hauteurs de ligne.
constexpr float kSheetIconLines = 2.0F;
// La largeur de l'aperçu d'un lien de recette, avant qu'il ne revienne à la
// ligne.
constexpr float kPreviewWrapEm = 22.0F;

// Le champ du nombre de fabrications du staff, boutons compris, et le pas que
// Ctrl tenu fait faire.
constexpr float kStaffCraftsWidthEm = 7.0F;
constexpr int kStaffCraftsFastStep = 10;

// Les pour mille d'un pour cent, pour écrire la décimale d'une probabilité.
constexpr int kPermillePerPercent = 10;

constexpr const char* kRowMenuId = "##atlas_item_menu";

constexpr std::size_t kLabelBytes = 256;
constexpr std::size_t kSuffixBytes = 128;
constexpr std::size_t kIdTextBytes = 16;

[[nodiscard]] float Em() { return ImGui::GetFontSize(); }

// ── LA RECHERCHE ────────────────────────────────────────────────────────────

// Insensible à la casse, sur de l'ASCII. Un octet de continuation UTF-8 n'est
// jamais confondu avec une lettre ASCII, donc la comparaison reste juste pour
// ce à quoi elle sert : trouver « jellopy » en tapant « JELL ».
[[nodiscard]] bool ContainsFolded(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  const auto folded_equal = [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  };
  return !std::ranges::search(haystack, needle, folded_equal).empty();
}

// Le filtre retient-il cette ligne ? Par le nom ET par l'identifiant : on
// cherche « Jellopy » comme « 909 », et les deux se croisent dans les mêmes
// conversations.
[[nodiscard]] bool Matches(const CraftAtlasWindowState& state, const CraftAtlasRow& row) {
  const std::string_view filter(state.filter.data());
  if (filter.empty() || ContainsFolded(row.name, filter)) {
    return true;
  }
  std::array<char, kIdTextBytes> id_text{};
  std::snprintf(id_text.data(), id_text.size(), "%u", static_cast<unsigned>(row.item_id));
  return ContainsFolded(id_text.data(), filter);
}

// ── LE TEXTE GRIS D'UN ÉTAT VIDE, une fois ──────────────────────────────────
//
// 🔴 L'ENJEU EST L'APPARIEMENT : une couleur poussée dont le retrait manque
// déteint sur tout ce que la frame dessine ENSUITE, loin de sa cause. Ici les
// deux vont ensemble par construction.
void DimNotice(const char* message) {
  ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().faint_text);
  ImGui::TextWrapped("%s", message);
  ImGui::PopStyleColor();
}

void ColoredWrapped(ImU32 color, const char* message) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%s", message);
  ImGui::PopStyleColor();
}

// ── LA NAVIGATION ───────────────────────────────────────────────────────────

// Aller à la fiche d'un item, en empilant la précédente. Le retour est ce qui
// rend l'index inverse utilisable : on descend dans un matériau pour voir où il
// sert, puis on revient à la recette d'où l'on venait.
void GoTo(CraftAtlasWindowState& state, std::uint32_t item_id) {
  if (item_id == 0 || item_id == state.selected_id) {
    return;
  }
  if (state.selected_id != 0) {
    state.back.push_back(state.selected_id);
  }
  if (state.back.size() > kCraftAtlasBackDepth) {
    state.back.erase(state.back.begin());
  }
  state.selected_id = item_id;
}

void GoBack(CraftAtlasWindowState& state) {
  if (state.back.empty()) {
    return;
  }
  state.selected_id = state.back.back();
  state.back.pop_back();
}

// ── UNE LIGNE D'ITEM ────────────────────────────────────────────────────────
//
// Icône et libellé, avec les gestes d'une cellule d'objet (`ItemLinkCell`) —
// sauf le clic gauche, qui SUIT l'objet dans l'Atlas au lieu de le décrire.
// `selected` souligne la ligne.
void DrawRow(CraftAtlasWindowState& state, CraftAtlasAnswer& answer, const CraftAtlasRow& row,
             const char* suffix, bool selected, ImU32 color) {
  std::array<char, kLabelBytes> label{};
  const int name_bytes = static_cast<int>(row.name.size());
  if (suffix != nullptr && suffix[0] != '\0') {
    std::snprintf(label.data(), label.size(), "%.*s %s", name_bytes, row.name.data(), suffix);
  } else {
    std::snprintf(label.data(), label.size(), "%.*s", name_bytes, row.name.data());
  }
  const ChatLinkGesture gesture = DrawItemLinkCell(ItemLinkCell{.item_id = row.item_id,
                                                                .text = label.data(),
                                                                .menu_label = row.name,
                                                                .icon = row.icon,
                                                                .keep_icon_place = true,
                                                                .underlined = selected,
                                                                .color = color},
                                                   state.menu, answer.item);
  if (ImGui::IsItemVisible()) {
    answer.drawn.push_back(row.item_id);
  }
  if (gesture == ChatLinkGesture::kDescribe) {
    answer.item.gesture = ChatLinkGesture::kNothing;
    GoTo(state, row.item_id);
  }
}

// Le suffixe « xN » d'un produit réalisable, et sa couleur.
void DrawProductRow(CraftAtlasWindowState& state, CraftAtlasAnswer& answer,
                    const CraftAtlasRow& row) {
  std::array<char, kSuffixBytes> suffix{};
  if (row.craftable > 0) {
    std::snprintf(suffix.data(), suffix.size(), "x%d", row.craftable);
  }
  DrawRow(state, answer, row, suffix.data(), row.item_id == state.selected_id,
          row.craftable > 0 ? PaintingLook().good_text : PaintingLook().text);
}

// ── LES QUATRE LISTES ───────────────────────────────────────────────────────

void DrawSkillTree(CraftAtlasWindowState& state, const CraftAtlasView& view,
                   CraftAtlasAnswer& answer) {
  const bool filtering = state.filter[0] != '\0';
  int shown = 0;
  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, Em() * kTreeIndentEm);
  for (const CraftAtlasGroup& group : view.groups) {
    // Compter d'abord ce qui passe les filtres : un skill dont plus rien ne
    // ressort ne doit pas s'afficher comme un dossier vide à ouvrir pour rien.
    const auto kept = [&](const CraftAtlasRow& row) {
      return Matches(state, row) && (!view.only_craftable || row.craftable > 0);
    };
    const auto matching = std::ranges::count_if(group.products, kept);
    if (matching == 0) {
      continue;
    }
    ++shown;
    // UN FILTRE ACTIF OUVRE les skills qui ont un résultat : sinon la recherche
    // ne montrerait que des dossiers fermés.
    if (filtering || view.only_craftable) {
      ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }
    const int label_bytes = static_cast<int>(group.label.size());
    std::array<char, kLabelBytes> header{};
    std::snprintf(header.data(), header.size(), "%.*s (%d)###skill%d", label_bytes,
                  group.label.data(), static_cast<int>(matching), group.skill_id);
    if (!ImGui::TreeNode(header.data())) {
      continue;
    }
    for (const CraftAtlasRow& row : group.products) {
      if (kept(row)) {
        DrawProductRow(state, answer, row);
      }
    }
    ImGui::TreePop();
  }
  ImGui::PopStyleVar();
  if (shown == 0) {
    DimNotice(core::Tr("Aucun skill ne correspond."));
  }
}

void DrawProductList(CraftAtlasWindowState& state, const CraftAtlasView& view,
                     CraftAtlasAnswer& answer) {
  int shown = 0;
  for (const CraftAtlasRow& row : view.products) {
    if (!Matches(state, row) || (view.only_craftable && row.craftable <= 0)) {
      continue;
    }
    DrawProductRow(state, answer, row);
    ++shown;
  }
  if (shown == 0) {
    DimNotice(core::Tr("Aucun produit ne correspond."));
  }
}

void DrawMaterialList(CraftAtlasWindowState& state, const CraftAtlasView& view,
                      CraftAtlasAnswer& answer) {
  int shown = 0;
  for (const CraftAtlasRow& row : view.materials) {
    // « Réalisable » n'a pas de sens pour un matériau, qui n'a pas de recette à
    // réaliser : il se lit ici « ceux que j'ai », la question équivalente de ce
    // côté-ci de l'index. Et un matériau qui ne sert que dans des recettes
    // injouables n'a rien à faire là : sa fiche serait vide.
    if (!Matches(state, row) || (view.only_craftable && row.owned <= 0) || row.uses == 0) {
      continue;
    }
    std::array<char, kSuffixBytes> suffix{};
    if (row.owned > 0) {
      std::snprintf(suffix.data(), suffix.size(), "(%d)  [%d]", row.owned, row.uses);
    } else {
      std::snprintf(suffix.data(), suffix.size(), "[%d]", row.uses);
    }
    DrawRow(state, answer, row, suffix.data(), row.item_id == state.selected_id,
            row.owned > 0 ? PaintingLook().good_text : PaintingLook().text);
    ++shown;
  }
  if (shown == 0) {
    DimNotice(core::Tr("Aucun matériau ne correspond."));
  }
}

void DrawArrowList(CraftAtlasWindowState& state, const CraftAtlasView& view,
                   CraftAtlasAnswer& answer) {
  int shown = 0;
  for (const CraftAtlasRow& row : view.arrow_sources) {
    if (!Matches(state, row) || (view.only_craftable && row.owned <= 0)) {
      continue;
    }
    std::array<char, kSuffixBytes> suffix{};
    if (row.owned > 0) {
      std::snprintf(suffix.data(), suffix.size(), "(%d)", row.owned);
    }
    DrawRow(state, answer, row, suffix.data(), row.item_id == state.selected_id,
            row.owned > 0 ? PaintingLook().good_text : PaintingLook().text);
    ++shown;
  }
  if (shown == 0) {
    DimNotice(core::Tr("Aucune source de flèches ne correspond."));
  }
}

// ── LA BARRE D'OUTILS ───────────────────────────────────────────────────────

void DrawToolbar(CraftAtlasWindowState& state, const CraftAtlasView& view, const SkinArt& art,
                 CraftAtlasAnswer& answer) {
  ImGui::SetNextItemWidth(Em() * kFilterWidthEm);
  ImGui::InputTextWithHint("##atlasfilter", core::Tr("Filtrer par nom ou par id…"),
                           state.filter.data(), state.filter.size());
  ImGui::SameLine();
  if (SkinButton(core::Tr("Effacer"), art)) {
    state.filter[0] = '\0';
  }
  ImGui::SameLine();
  bool only_craftable = view.only_craftable;
  if (SkinCheckBox(core::Tr("Réalisable maintenant"), &only_craftable, art)) {
    answer.only_craftable = only_craftable;
  }
  SkinTooltip(
      core::Tr("Ne garder que les recettes dont TOUS les matériaux sont dans l'inventory. Le "
               "filtre ne regarde pas les skills appris : le serveur, lui, refusera une recette "
               "dont le skill manque."));
  ImGui::SameLine();
  ImGui::BeginDisabled(state.back.empty());
  if (SkinButton(core::Tr("Retour"), art)) {
    GoBack(state);
  }
  ImGui::EndDisabled();
}

// ── LA FICHE ────────────────────────────────────────────────────────────────

void DrawYield(const CraftAtlasRecipeView& recipe) {
  // 🔴 LE TAIRE REVIENDRAIT À ANNONCER « 1 » : c'est ce que tout le monde
  // suppose, et une rune en rend deux à six.
  switch (recipe.yield_mode) {
    case CraftAtlasYieldMode::kFixed:
      if (recipe.yield_least == 1 && recipe.yield_most == 1) {
        return;
      }
      [[fallthrough]];
    case CraftAtlasYieldMode::kAllOrNone:
      ImGui::TextColored(
          ImGui::ColorConvertU32ToFloat4(PaintingLook().good_text),
          core::Tr("Rendement : %d à %d exemplaires d'un coup, selon le niveau et la difficulté."),
          recipe.yield_least, recipe.yield_most);
      return;
    case CraftAtlasYieldMode::kTable:
      ColoredWrapped(PaintingLook().warn_text,
                     core::Tr("Le résultat n'est pas ce produit : le serveur tire dans sa propre "
                              "table de transmutation, avec ses taux et ses quantités."));
      return;
    case CraftAtlasYieldMode::kBonusRoll:
      // 🔴 LE PLUS INTÉRESSANT À AFFICHER DE TOUS : il est propre à ce serveur,
      // et un joueur ne peut pas le deviner — il faudrait fabriquer des
      // centaines de fois pour soupçonner que la pile monte plus vite que le
      // compte. La probabilité s'écrit avec sa décimale.
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PaintingLook().good_text),
                         core::Tr("Rendement : 1, et %d,%d %% de chances d'en obtenir %d à %d de "
                                  "plus (bonus propre à ce serveur)."),
                         recipe.yield_bonus_permille / kPermillePerPercent,
                         recipe.yield_bonus_permille % kPermillePerPercent, 1,
                         recipe.yield_most - 1);
      return;
    case CraftAtlasYieldMode::kPerUnit:
      // « Jusqu'à » : chaque exemplaire est tiré séparément, donc le bas de la
      // fourchette n'est pas un plancher.
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PaintingLook().good_text),
                         core::Tr("Rendement : %d à %d exemplaires, chacun tiré séparément (on "
                                  "peut en obtenir moins)."),
                         recipe.yield_least, recipe.yield_most);
      return;
  }
}

// La bulle d'un bouton du staff : son motif, puis ce que le sac a retranché.
void GrantTooltip(const char* purpose, StaffItemRoom room) {
  const char* bag = nullptr;
  if (room == StaffItemRoom::kBounded) {
    bag = core::Tr(
        "Un ingrédient qui ne s'empile pas prend une case par exemplaire : il est "
        "ramené aux cases libres de l'inventory.");
  } else if (room == StaffItemRoom::kFull) {
    bag = core::Tr(
        "L'inventory est plein : un ingrédient qui ne s'empile pas prend une case par "
        "exemplaire, et n'est pas demandé.");
  }
  SkinTooltipFormat("%s%s%s", purpose, bag != nullptr ? "\n\n" : "", bag != nullptr ? bag : "");
}

// LA LIGNE DU STAFF : se faire donner les ingrédients de N fabrications.
//
//     Staff — fabrications [-][ N ][+]
//                          [Obtenir tout] [Obtenir le manque]
//
// ⚠ LES BOUTONS S'ALIGNENT SOUS LE CHAMP : c'est lui qu'ils multiplient.
void DrawStaffGrants(CraftAtlasWindowState& state, const SkinArt& art,
                     const CraftAtlasRecipeView& recipe, std::optional<int> free_bag_slots,
                     CraftAtlasAnswer& answer) {
  state.staff_crafts = std::clamp(state.staff_crafts, 1, kCraftAtlasMostCrafts);
  ImGui::Spacing();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Staff — fabrications"));
  ImGui::SameLine();
  const float field_x = ImGui::GetCursorPosX();
  ImGui::SetNextItemWidth(Em() * kStaffCraftsWidthEm);
  static_cast<void>(SkinInputInt("##atlas_staff_crafts", &state.staff_crafts, 1,
                                 kCraftAtlasMostCrafts, 1, kStaffCraftsFastStep, art));
  ImGui::SetCursorPosX(field_x);

  // ⚠ LES DEMANDES SE CALCULENT AVANT LES BOUTONS : un bouton qui n'enverrait
  // rien se grise avec son motif, plutôt que de répondre au clic par un silence.
  CraftAtlasGrantPlan all = CraftAtlasGrants(recipe.ingredients, state.staff_crafts,
                                             CraftAtlasGrantKind::kAll, free_bag_slots);
  ImGui::BeginDisabled(all.grants.empty());
  if (SkinSmallButton(core::Tr("Obtenir tout"), art)) {
    answer.staff_items = std::move(all.grants);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    GrantTooltip(core::Tr("Envoie « @item » pour chaque ingrédient, en quantité multipliée par "
                          "le nombre de fabrications, sans regarder l'inventory."),
                 all.room);
  }
  ImGui::SameLine();
  CraftAtlasGrantPlan missing = CraftAtlasGrants(recipe.ingredients, state.staff_crafts,
                                                 CraftAtlasGrantKind::kMissing, free_bag_slots);
  const bool nothing_missing = missing.grants.empty() && missing.room == StaffItemRoom::kFits;
  ImGui::BeginDisabled(missing.grants.empty());
  if (SkinSmallButton(core::Tr("Obtenir le manque"), art)) {
    answer.staff_items = std::move(missing.grants);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    GrantTooltip(nothing_missing ? core::Tr("Rien ne manque : l'inventory suffit pour ce nombre "
                                            "de fabrications.")
                                 : core::Tr("N'envoie « @item » que pour ce qui manque à "
                                            "l'inventory pour ce nombre de fabrications."),
                 missing.room);
  }
}

void DrawRecipe(CraftAtlasWindowState& state, const CraftAtlasView& view, const SkinArt& art,
                const CraftAtlasRecipeView& recipe, CraftAtlasAnswer& answer) {
  ImGui::SeparatorText(core::Tr("Comment le fabriquer"));
  const ImVec4 text = ImGui::ColorConvertU32ToFloat4(PaintingLook().text);
  const ImVec4 dim = ImGui::ColorConvertU32ToFloat4(PaintingLook().faint_text);
  if (recipe.skill_id == 0) {
    // 🔴 PAS UNE DONNÉE MANQUANTE : la recette s'ouvre par le script d'un item,
    // sans qu'aucun skill n'y donne accès. Le dire évite de chercher un job qui
    // n'existe pas.
    ColoredWrapped(PaintingLook().warn_text,
                   core::Tr("Aucun skill : cette recette s'ouvre en UTILISANT un item (fourneau, "
                            "marteau, kit)."));
  } else {
    const int label_bytes = static_cast<int>(recipe.skill_label.size());
    ImGui::TextColored(text, core::Tr("Skill : %.*s (id %d), niveau %d"), label_bytes,
                       recipe.skill_label.data(), recipe.skill_id, recipe.skill_level);
  }
  if (recipe.item_level > 0) {
    ImGui::TextColored(dim, core::Tr("Niveau de recette (itemlv) : %d"), recipe.item_level);
  }
  if (!recipe.learnable) {
    // Montré SEULEMENT quand le joueur a demandé à voir ces recettes : sinon
    // elles ne sont pas là.
    ColoredWrapped(PaintingLook().warn_text,
                   core::Tr("Injouable ici : aucune classe de ce serveur n'apprend ce skill."));
  }
  DrawYield(recipe);
  if (recipe.craftable > 0) {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PaintingLook().good_text),
                       core::Tr("Réalisable %d fois avec l'inventory actuel."), recipe.craftable);
  } else if (recipe.craftable == 0) {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PaintingLook().bad_text), "%s",
                       core::Tr("Matériaux insuffisants."));
  }

  // PARTAGER LA RECETTE, pas l'item : le lien montre skill et composants au
  // survol, et ouvre l'Atlas au clic. ⚠ GRISÉ AVEC SON MOTIF plutôt qu'un clic
  // sans effet que rien n'explique.
  ImGui::BeginDisabled(!view.can_share_recipe);
  if (SkinSmallButton(core::Tr("Partager la recette"), art)) {
    answer.share_recipe = true;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (view.can_share_recipe) {
      const int name_bytes = static_cast<int>(view.sheet.item.name.size());
      SkinTooltipFormat(core::Tr("Pose « [Recette: %.*s] » dans la barre de chat. Au survol, le "
                                 "lien montrera le skill et les composants ; au clic, il ouvrira "
                                 "l'Atlas."),
                        name_bytes, view.sheet.item.name.data());
    } else {
      SkinTooltipFormat("%s", core::Tr("Ouvrez la fenêtre de chat : le lien de recette se pose "
                                       "dans sa barre de saisie."));
    }
  }

  ImGui::Spacing();
  // 🔴 LES QUANTITÉS SUIVENT LES FABRICATIONS DE LA LIGNE DU STAFF, qui se
  // montre sous la liste : le manque affiché est celui que « Obtenir le manque »
  // comblerait. Hors vue staff, une seule fabrication.
  const int crafts =
      view.staff_items ? std::clamp(state.staff_crafts, 1, kCraftAtlasMostCrafts) : 1;
  if (crafts > 1) {
    ImGui::TextColored(dim, core::Tr("Quantités pour %d fabrications :"), crafts);
  }
  for (const CraftAtlasRow& ingredient : recipe.ingredients) {
    const int missing = CraftIngredientShortfall(ingredient, crafts);
    const int needed = CraftIngredientNeed(ingredient, crafts);
    const bool enough = missing == 0;
    std::array<char, kSuffixBytes> suffix{};
    if (ingredient.kept) {
      std::snprintf(suffix.data(), suffix.size(),
                    core::Tr("(requis, non consommé — inventory : %d)"), ingredient.owned);
    } else if (!enough) {
      // LE MANQUE plutôt que le seul stock : c'est le chiffre qu'on vient
      // chercher — combien aller ramasser.
      std::snprintf(suffix.data(), suffix.size(), core::Tr("x%d  (%d — il en manque %d)"), needed,
                    ingredient.owned, missing);
    } else {
      std::snprintf(suffix.data(), suffix.size(), "x%d  (%d)", needed, ingredient.owned);
    }
    DrawRow(state, answer, ingredient, suffix.data(), false,
            enough ? PaintingLook().good_text : PaintingLook().bad_text);
  }
  if (view.staff_items) {
    DrawStaffGrants(state, art, recipe, view.free_bag_slots, answer);
  }
}

void DrawSheet(CraftAtlasWindowState& state, const CraftAtlasView& view, const SkinArt& art,
               CraftAtlasAnswer& answer) {
  const CraftAtlasSheet& sheet = view.sheet;
  if (state.selected_id == 0 || sheet.item.item_id != state.selected_id) {
    DimNotice(
        core::Tr("Choisissez un item à gauche.\n\nClic gauche : suivre l'item dans l'Atlas.\nClic "
                 "droit : le menu contextuel (description, @whodrops…).\nMaj + clic : poser son "
                 "lien dans le chat."));
    return;
  }

  // ── L'EN-TÊTE : L'ITEM LUI-MÊME ─────────────────────────────────────────
  const float side = ImGui::GetTextLineHeight() * kSheetIconLines;
  if (sheet.item.icon.usable()) {
    ImGui::Image(sheet.item.icon.id, ImVec2(side, side));
  } else {
    ImGui::Dummy(ImVec2(side, side));
  }
  if (ImGui::IsItemVisible()) {
    answer.drawn.push_back(sheet.item.item_id);
  }
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().text);
  ImGui::TextUnformatted(sheet.item.name.data(), sheet.item.name.data() + sheet.item.name.size());
  ImGui::PopStyleColor();
  ImGui::TextColored(
      ImGui::ColorConvertU32ToFloat4(sheet.item.owned > 0 ? PaintingLook().good_text
                                                          : PaintingLook().faint_text),
      core::Tr("id %u  ·  inventory : %d"), static_cast<unsigned>(sheet.item.item_id),
      sheet.item.owned);
  ImGui::EndGroup();
  ImGui::Separator();

  bool said_something = false;

  // ── 1. SA RECETTE ─────────────────────────────────────────────────────────
  if (sheet.recipe.has_value()) {
    said_something = true;
    DrawRecipe(state, view, art, *sheet.recipe, answer);
  }

  // ── 1 bis. LA PRODUCTION EN MASSE ─────────────────────────────────────────
  if (!sheet.bulk.empty()) {
    said_something = true;
    ImGui::SeparatorText(core::Tr("Production en masse"));
    const ImVec4 ok = ImGui::ColorConvertU32ToFloat4(PaintingLook().good_text);
    for (const CraftAtlasBulkView& source : sheet.bulk) {
      // « JUSQU'À » : chaque exemplaire de la fournée est tiré au taux de
      // réussite, et un chiffre ferme promettrait ce que le serveur ne
      // garantit pas.
      const int label_bytes = static_cast<int>(source.skill_label.size());
      if (source.least == source.most) {
        ImGui::TextColored(ok, core::Tr("%.*s : jusqu'à %d par lancement"), label_bytes,
                           source.skill_label.data(), source.most);
      } else {
        ImGui::TextColored(ok, core::Tr("%.*s : de %d à %d par lancement"), label_bytes,
                           source.skill_label.data(), source.least, source.most);
      }
    }
  }

  // ── 2. LES FLÈCHES QU'IL DONNE ────────────────────────────────────────────
  if (!sheet.arrow_yields.empty()) {
    said_something = true;
    ImGui::SeparatorText(core::Tr("Transformable en flèches"));
    // ⚠ UN EXEMPLAIRE CONSOMMÉ REND TOUT CE QUI SUIT, ce n'est pas un tirage
    // entre les lignes — et une liste suggère naturellement le contraire.
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PaintingLook().faint_text), "%s",
                       core::Tr("Un exemplaire consommé donne tout ce qui suit, en une fois :"));
    for (const CraftAtlasRow& yield : sheet.arrow_yields) {
      std::array<char, kSuffixBytes> suffix{};
      std::snprintf(suffix.data(), suffix.size(), core::Tr("x%d obtenues"), yield.quantity);
      DrawRow(state, answer, yield, suffix.data(), false, PaintingLook().text);
    }
  }

  // ── 3. L'INDEX INVERSE : OÙ CET ITEM SERT ─────────────────────────────────
  if (!sheet.uses.empty()) {
    said_something = true;
    std::array<char, kSuffixBytes> header{};
    // ⚠ LE MÊME COMPTE QUE LA LISTE DES MATÉRIAUX : deux chiffres pour la même
    // question enverraient chercher des recettes qui n'existent pas.
    std::snprintf(header.data(), header.size(), core::Tr("Sert dans %d recettes"),
                  static_cast<int>(sheet.uses.size()));
    ImGui::SeparatorText(header.data());
    for (const CraftAtlasRow& use : sheet.uses) {
      const int label_bytes = static_cast<int>(use.skill_label.size());
      std::array<char, kSuffixBytes> suffix{};
      if (use.kept) {
        std::snprintf(suffix.data(), suffix.size(), core::Tr("(à posséder — %.*s)"), label_bytes,
                      use.skill_label.data());
      } else {
        std::snprintf(suffix.data(), suffix.size(), core::Tr("x%d — %.*s"), use.quantity,
                      label_bytes, use.skill_label.data());
      }
      DrawRow(state, answer, use, suffix.data(), false, PaintingLook().text);
    }
  }

  // ── 4. D'OÙ VIENNENT CES FLÈCHES ──────────────────────────────────────────
  if (!sheet.arrow_sources.empty()) {
    said_something = true;
    ImGui::SeparatorText(core::Tr("S'obtient en transformant"));
    for (const CraftAtlasRow& source : sheet.arrow_sources) {
      std::array<char, kSuffixBytes> suffix{};
      std::snprintf(suffix.data(), suffix.size(), core::Tr("-> x%d  (inventory : %d)"),
                    source.quantity, source.owned);
      DrawRow(state, answer, source, suffix.data(), false, PaintingLook().text);
    }
  }

  if (!said_something) {
    // Cas réel : le produit d'une transformation de flèches qu'on a suivi, sans
    // rien d'autre qui le concerne. Dire QUOI manque vaut mieux qu'une page vide.
    DimNotice(
        core::Tr("Cet item n'a pas de recette, n'entre dans aucune et ne se transforme pas en "
                 "flèches. Le fichier de recettes ne couvre que la fabrication : quêtes, échanges "
                 "et drops de monstres n'y sont pas."));
  }
}

// Le menu d'une ligne, ouvert HORS des listes : la ligne a changé la pile
// d'ids, et le surgissant ne s'y retrouverait pas.
void DrawRowMenu(CraftAtlasWindowState& state, const CraftAtlasView& view,
                 CraftAtlasAnswer& answer) {
  DrawItemLinkMenu(kRowMenuId, state.menu, answer.item, view.vote_shop);
}

void DrawBody(CraftAtlasWindowState& state, const CraftAtlasView& view, const SkinArt& art,
              CraftAtlasAnswer& answer) {
  if (!view.available) {
    // DIRE L'ABSENCE, ET LAQUELLE : une fenêtre vide se lirait « rien ne se
    // fabrique sur ce serveur », ce qui est faux.
    ColoredWrapped(PaintingLook().warn_text,
                   core::Tr("Le fichier de recettes est introuvable ou illisible "
                            "(data\\yaml_files\\craft_recipes.yml). L'Atlas ne peut rien montrer "
                            "tant qu'il manque — le reste du client fonctionne normalement."));
    return;
  }
  DrawToolbar(state, view, art, answer);
  ImGui::Separator();

  const float list_width =
      std::min(Em() * kListWidthEm, ImGui::GetContentRegionAvail().x * kListWidthShare);
  if (ImGui::BeginChild("##atlas_list", ImVec2(list_width, 0.0F), ImGuiChildFlags_Borders)) {
    if (BeginSkinTabBar("##atlas_tabs", art, ImGuiTabBarFlags_FittingPolicyScroll)) {
      if (ImGui::BeginTabItem(core::Tr("Skills"))) {
        DrawSkillTree(state, view, answer);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(core::Tr("Produits"))) {
        DrawProductList(state, view, answer);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(core::Tr("Matériaux"))) {
        DrawMaterialList(state, view, answer);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(core::Tr("Flèches"))) {
        DrawArrowList(state, view, answer);
        ImGui::EndTabItem();
      }
      EndSkinTabBar();
    }
  }
  ImGui::EndChild();

  ImGui::SameLine();
  if (ImGui::BeginChild("##atlas_sheet", ImVec2(0.0F, 0.0F), ImGuiChildFlags_Borders)) {
    DrawSheet(state, view, art, answer);
  }
  ImGui::EndChild();

  // 🔴 ICI ET PAS AU CLIC : l'identifiant d'un popup se hache avec la pile
  // d'ids, et les lignes cliquent sous celle d'un enfant, d'un onglet, d'un
  // arbre et d'un `PushID`. Les deux colonnes arment le même menu.
  DrawRowMenu(state, view, answer);
}

}  // namespace

void DrawCraftRecipePreview(const CraftRecipePreview& preview, const SkinArt& art) {
  // 🔴 L'INFOBULLE HABILLÉE, ET NON CELLE D'IMGUI : seule la première a le fond
  // clair des panneaux de description. Celle d'ImGui est sombre, et le texte
  // sombre de l'Atlas y deviendrait illisible — le nom de la recette
  // disparaîtrait.
  if (!BeginSkinnedTooltip(art)) {
    return;
  }
  ImGui::PushTextWrapPos(Em() * kPreviewWrapEm);
  ImGui::TextUnformatted(preview.product_name.data(),
                         preview.product_name.data() + preview.product_name.size());
  ImGui::Separator();
  const ImVec4 dim = ImGui::ColorConvertU32ToFloat4(PaintingLook().faint_text);
  const int label_bytes = static_cast<int>(preview.skill_label.size());
  if (preview.skill_id == 0) {
    ImGui::TextColored(dim, "%.*s", label_bytes, preview.skill_label.data());
  } else {
    ImGui::TextColored(dim, core::Tr("%.*s  (niveau %d)"), label_bytes, preview.skill_label.data(),
                       preview.skill_level);
  }
  if (preview.ingredients.empty()) {
    ImGui::TextColored(dim, "%s", core::Tr("(aucun matériau connu)"));
  } else {
    ImGui::Spacing();
    for (const CraftAtlasRow& ingredient : preview.ingredients) {
      const int name_bytes = static_cast<int>(ingredient.name.size());
      // Un guide n'est PAS consommé : « x0 » serait absurde, et « x1 »
      // laisserait croire qu'on le perd.
      if (ingredient.kept) {
        ImGui::BulletText(core::Tr("%.*s  (requis, non consommé)"), name_bytes,
                          ingredient.name.data());
      } else {
        ImGui::BulletText("%d x %.*s", ingredient.quantity, name_bytes, ingredient.name.data());
      }
    }
  }
  ImGui::Spacing();
  ImGui::TextColored(dim, "%s", core::Tr("Clic : ouvrir dans l'Atlas des recettes"));
  ImGui::PopTextWrapPos();
  EndSkinnedTooltip(art);
}

void OpenCraftAtlasOn(CraftAtlasWindowState& state, std::uint32_t item_id) {
  state.open = true;
  state.raise = true;
  state.back.clear();
  state.selected_id = item_id;
}

CraftAtlasAnswer DrawCraftAtlasWindow(CraftAtlasWindowState& state, const CraftAtlasView& view,
                                      const SkinArt& art) {
  CraftAtlasAnswer answer;
  if (!state.open) {
    return answer;
  }
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(Em() * kFirstWidthEm, Em() * kFirstHeightEm),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Em() * kLeastWidthEm, Em() * kLeastHeightEm),
                                      ImVec2(FLT_MAX, FLT_MAX));
  // LA PUCE MÈNE AUX RÉGLAGES DE L'ATLAS, comme celles des fenêtres d'items.
  SetNextWindowBulletClickable(core::Tr("Réglages de l'Atlas"));
  // ⚠ L'IDENTITÉ, PAS LE TITRE : la place se retient sous un nom qui ne dépend
  // pas de la langue.
  const std::string title =
      std::string(core::Tr("Atlas des recettes")) + "###" + kCraftAtlasWindowId;
  const bool body_shown = BeginSkinnedWindow(title.c_str(), &state.open, art);
  // ⚠ LUE HORS DU CORPS : la puce vaut aussi pour une fenêtre repliée.
  answer.open_settings = TitleBulletClicked();
  if (body_shown) {
    DrawBody(state, view, art, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
