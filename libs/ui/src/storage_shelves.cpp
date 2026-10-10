#include "storage_shelves.h"

#include <algorithm>
#include <cstring>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "item_grid.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// La largeur du champ de saisie, en cadratins de la police courante : un nom de
// coffre en fait au plus vingt-trois, et la mesure suit l'échelle de
// l'interface.
constexpr float kShelfNameFieldEms = 12.0F;

// LE GABARIT D'UN ONGLET DE COFFRE quand l'installation n'a pas son art : les
// mesures du client (tabh_sto1.bmp), pour que la disposition ne bouge pas d'une
// installation à l'autre.
constexpr float kShelfTileWidthPixels = 27.0F;
constexpr float kShelfTileHeightPixels = 25.0F;
// Ce que le cadre garde pour lui, de chaque côté de ce qu'il porte.
constexpr float kShelfTileInsetPixels = 2.0F;
// Les onglets sont JOINTIFS : leurs cadres partagent un pixel de bord.
constexpr float kShelfTabOverlapPixels = -1.0F;
// Le pont de l'onglet ouvert, et le corps de son art — le gris clair du cadre.
constexpr float kShelfBridgeInsetPixels = 1.0F;
constexpr float kShelfBridgeOverlapPixels = 2.0F;
constexpr ImU32 kShelfBridgeColor = IM_COL32(0xD1, 0xDC, 0xE8, 255);

// CE QUE LE MENU PREND EN PLUS DU THÈME, en cadratins de la police courante.
//
// ⚠ EN PLUS, ET NON À LA PLACE : il suit l'habillage comme le reste de la
// fenêtre, il s'en écarte juste d'un cheveu. Il porte trois réglages et un
// aperçu là où une liste de menu ne porte que des lignes, et la même marge y
// serre.
constexpr float kShelfMenuExtraRoomEms = 0.35F;

// LA MARGE ET L'ESPACEMENT DU THÈME, relevés AVANT que la bande ne les écrase.
//
// 🔴 LE MENU NAÎT DANS LA BANDE D'ONGLETS, qui met sa marge à zéro et son
// espacement au recouvrement d'un pixel pour coller ses tuiles. Le surgissant
// en HÉRITERAIT : son contenu au ras du cadre, ses lignes jointives. Les
// valeurs ne se codent pas en dur ici — relevées, elles suivent l'habillage.
struct ShelfMenuRoom {
  ImVec2 padding{};
  ImVec2 spacing{};
};

// LE MENU D'UN ONGLET DE COFFRE : son nom, et son icône.
//
// 🔴 CE QUE LE SERVEUR APPELLE « Storage Alt 3 » NE DIT RIEN au joueur qui en a
// fait son coffre à consommables. Le nom et l'icône sont donc à NOUS — aucun
// paquet ne part d'ici, et le serveur continue d'appeler ce coffre comme il
// veut.
//
// 🔴 DEUX FAÇONS DE DONNER L'ICÔNE, ET IL EN FAUT DEUX. On GLISSE un objet du
// coffre sur l'onglet quand on l'a sous les yeux ; on TAPE son identifiant quand
// on ne l'a pas — une icône qu'on ne possède plus, ou qu'on n'a jamais eue. Le
// glisser seul rendrait la seconde moitié impossible.
//
// ⚠ ET LE MENU ÉCRIT EN DIRECT, sans bouton de validation : l'onglet derrière
// le menu change sous les yeux, donc on voit ce qu'on règle. Le tampon part de
// ce que le coffre porte déjà.
//
// ⚠ UN NOM VIDE REND LE COFFRE À CELUI DU SERVEUR, et un identifiant nul lui
// retire son icône : c'est ainsi qu'on annule l'un ou l'autre, sans bouton de
// plus. « Réinitialiser » fait les deux d'un coup.
void DrawShelfMenu(const StorageShelfView& shelf, StorageWindowState& state, const SkinArt& art,
                   ShelfMenuRoom room, StorageWindowAction& action) {
  if (state.naming_shelf != static_cast<int>(shelf.id)) {
    return;
  }
  // ⚠ `WindowPadding` N'EST LU QU'À LA CRÉATION de la fenêtre, `ItemSpacing` à
  // CHAQUE élément : les deux restent donc poussés jusqu'à `EndPopup`, et non
  // relâchés juste après `BeginPopup`.
  const float extra = ImGui::GetFontSize() * kShelfMenuExtraRoomEms;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(room.padding.x + extra, room.padding.y + extra));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(room.spacing.x, room.spacing.y + extra));
  if (!BeginSkinPopup("nommer")) {
    ImGui::PopStyleVar(2);
    return;
  }
  // 🔴 LE NOM DU SERVEUR EN TÊTE, AVEC SON NUMÉRO : une fois le coffre renommé,
  // c'est la seule chose qui dise lequel on est en train de régler.
  ImGui::TextDisabled(core::Tr("%s (id %u)"), std::string(shelf.name).c_str(),
                      static_cast<unsigned>(shelf.id));
  ImGui::Separator();

  const float field_width = ImGui::GetFontSize() * kShelfNameFieldEms;
  // ⚠ LE NOM DU SERVEUR SERT DE FILIGRANE : laisser le champ vide, c'est le
  // garder, et le filigrane le dit sans qu'une ligne de plus l'explique.
  ImGui::SetNextItemWidth(field_width);
  if (ImGui::InputTextWithHint(core::Tr("Nom"), std::string(shelf.name).c_str(), state.shelf_name,
                               sizeof(state.shelf_name))) {
    action.renamed_shelf = static_cast<int>(shelf.id);
  }

  // 🔴 LE CHAMP SANS SES BOUTONS (`step` nul), et les deux pas DESSINÉS ICI :
  // ceux d'ImGui sont du thème par défaut, bleus, et ils trahiraient l'habillage
  // au milieu d'une fenêtre qui n'en porte aucun. Le libellé se pose alors à la
  // main, à la droite du tout — la place que le champ lui donnait.
  ImGui::SetNextItemWidth(field_width);
  bool icon_changed = ImGui::InputInt("##icone", &state.shelf_icon, /*step=*/0, /*step_fast=*/0);
  // ⚠ CARRÉS ET DE MÊME TAILLE : « - » est plus étroit que « + », et deux
  // boutons de largeurs différentes se liraient comme deux gestes différents.
  const float step_side = SkinSmallButtonHeight(art);
  const ImVec2 step_size(step_side, step_side);
  ImGui::SameLine();
  if (SkinSmallButton("-", art, step_size)) {
    --state.shelf_icon;
    icon_changed = true;
  }
  ImGui::SameLine();
  if (SkinSmallButton("+", art, step_size)) {
    ++state.shelf_icon;
    icon_changed = true;
  }
  ImGui::SameLine();
  // ⚠ CENTRÉ SUR LA LIGNE, PAS POSÉ EN HAUT : les deux pas sont plus hauts qu'une
  // ligne de texte, et le libellé pendrait à leur sommet.
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Icône (id d'item)"));
  if (icon_changed) {
    // ⚠ UN IDENTIFIANT NÉGATIF N'EXISTE PAS, et le pas descend sous zéro : on
    // borne, plutôt que de demander une icône impossible.
    state.shelf_icon = std::max(state.shelf_icon, 0);
    action.decorated_shelf = static_cast<int>(shelf.id);
    action.decorate_item_id = static_cast<std::uint32_t>(state.shelf_icon);
  }

  // CE QUE L'ONGLET PORTERA, montré ici même.
  //
  // 🔴 TROIS ÉTATS ET NON DEUX : aucune icône demandée, une icône demandée que
  // l'installation n'a pas, et l'icône. Le deuxième se lirait « le réglage n'a
  // pas pris » s'il ressemblait au premier — alors que c'est l'identifiant qui
  // est faux.
  if (shelf.icon_item_id == 0) {
    ImGui::TextDisabled("%s", core::Tr("Glissez-y un item pour prendre son icône."));
  } else if (shelf.icon.usable()) {
    ImGui::Image(shelf.icon.id, ImVec2(shelf.icon.width, shelf.icon.height));
  } else {
    ImGui::TextDisabled("%s", core::Tr("Aucune icône pour cet identifiant."));
  }

  if (SkinButton(core::Tr("Réinitialiser"), art)) {
    action.reset_shelf = static_cast<int>(shelf.id);
    state.naming_shelf = -1;
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
  ImGui::PopStyleVar(2);  // WindowPadding et ItemSpacing du surgissant
}

// LA RANGÉE DES COFFRES : UN ONGLET par coffre que le personnage peut ouvrir.
//
// 🔴 DES ONGLETS, ET NON DES BOUTONS. L'installation livre leur art —
// `tabh_sto1/2.bmp` — et c'est ce que montre le client : une rangée de cadres
// jointifs, dont celui qui est ouvert touche ce qu'il ouvre. En petits boutons
// du thème, la rangée passait pour une barre d'outils, et rien ne disait plus
// lequel était ouvert sinon la teinte d'un bouton.
//
// 🔴 CE SONT DES SESSIONS, ET NON DES ONGLETS DE FILTRE. Cliquer ici parle au
// SERVEUR — il referme l'un et ouvre l'autre —, là où la rangée du dessous ne
// fait que trier ce qui est déjà là. Les deux se ressemblent à l'œil et n'ont
// pas le même prix ; celle-ci est donc au-dessus, séparée.
//
// ⚠ ELLE NE PARAÎT PAS SOUS DEUX COFFRES. Un onglet unique n'offre aucun choix
// et prendrait une ligne à la table, qui en a six cents à montrer.
//
// ⚠ ET LE COFFRE OUVERT NE SE RECLIQUE PAS : le serveur ignore la demande, et
// l'onglet ouvert le dit déjà.
// LE GABARIT D'UN ONGLET, PRIS SUR L'ART — et le repli sur les mesures du
// client quand l'installation ne l'a pas, pour que la disposition ne bouge pas
// de l'une à l'autre.
//
// ⚠ DEUX JEUX D'IMAGES, ET CE NE SONT PAS LES MÊMES TOURNÉES : `tabh_sto*` pour
// la rangée (27 × 25), `tab_sto*` pour la colonne (23 × 27). Leur bord ouvert
// n'est pas du même côté.
[[nodiscard]] ImVec2 ShelfTileSize(const SkinArt& art, bool lying) {
  const SkinImage& open_tile =
      art.Get(lying ? SkinPiece::kTabHStorageOn : SkinPiece::kTabStorageOn);
  if (open_tile.usable()) {
    return {static_cast<float>(open_tile.width), static_cast<float>(open_tile.height)};
  }
  return lying ? ImVec2(kShelfTileWidthPixels, kShelfTileHeightPixels)
               : ImVec2(kShelfTileHeightPixels, kShelfTileWidthPixels);
}

void DrawShelfRow(StorageWindowState& state, const StorageWindowView& view, const SkinArt& art,
                  bool lying, float length, StorageWindowAction& action) {
  if (view.shelves.size() < 2) {
    return;
  }
  // 🔴 RELEVÉ AVANT TOUTE POUSSÉE : ce qui suit met la marge à zéro et
  // l'espacement au recouvrement pour coller les tuiles, et le menu d'un onglet
  // naît DEDANS.
  const ShelfMenuRoom room{.padding = ImGui::GetStyle().WindowPadding,
                           .spacing = ImGui::GetStyle().ItemSpacing};
  const SkinImage& open_tile =
      art.Get(lying ? SkinPiece::kTabHStorageOn : SkinPiece::kTabStorageOn);
  const SkinImage& shut_tile =
      art.Get(lying ? SkinPiece::kTabHStorageOff : SkinPiece::kTabStorageOff);
  const ImVec2 tile = ShelfTileSize(art, lying);

  // 🔴 DEBOUT, LA COLONNE EST SON PROPRE ENFANT, de la hauteur de la table
  // qu'elle longe — comme la bande des catégories. Posée dans le flux, elle
  // s'arrêterait au premier onglet et la table descendrait sans elle.
  if (!lying) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
    if (!ImGui::BeginChild("coffres", ImVec2(tile.x, length), ImGuiChildFlags_None,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
      ImGui::EndChild();
      ImGui::PopStyleVar();
      return;
    }
  }

  ImDrawList* const into = ImGui::GetWindowDrawList();
  // ⚠ LE MÊME RECOUVREMENT D'UN PIXEL QUE LES ONGLETS DE CATÉGORIE : les cadres
  // partagent leur bord, et un espacement positif ouvrirait une rainure entre
  // chaque. Sur l'axe de la bande, donc l'un ou l'autre selon qu'elle est
  // couchée ou debout.
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, lying ? ImVec2(kShelfTabOverlapPixels, 0.0F)
                                                       : ImVec2(0.0F, kShelfTabOverlapPixels));
  bool first = true;
  ImVec2 open_min;
  ImVec2 open_max;
  bool open_shown = false;
  for (const StorageShelfView& shelf : view.shelves) {
    if (!first && lying) {
      ImGui::SameLine();
    }
    first = false;
    ImGui::PushID(static_cast<int>(shelf.id));
    const bool active = shelf.id == view.open_shelf;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 corner(at.x + tile.x, at.y + tile.y);
    ImGui::InvisibleButton("coffre", tile);
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && !active) {
      action.chose_shelf = static_cast<int>(shelf.id);
    }
    if (const SkinImage& shown = active ? open_tile : shut_tile; shown.usable()) {
      into->AddImage(shown.id, at, corner);
    } else {
      into->AddRectFilled(at, corner, active ? art.look().active_tab : art.look().idle_tab);
    }
    // 🔴 L'ICÔNE CHOISIE PREND TOUTE LA PLACE, et c'est tout l'objet du réglage :
    // on reconnaît son coffre à une potion rouge sans lire un mot. Sans icône,
    // c'est le nom du joueur — ou le NUMÉRO, qui tient toujours.
    if (shelf.icon.usable()) {
      const ImVec2 middle((at.x + corner.x) * 0.5F, (at.y + corner.y) * 0.5F);
      const float side = std::min(tile.x, tile.y) - 2.0F * kShelfTileInsetPixels;
      into->AddImage(shelf.icon.id, ImVec2(middle.x - side * 0.5F, middle.y - side * 0.5F),
                     ImVec2(middle.x + side * 0.5F, middle.y + side * 0.5F));
    } else {
      const std::string label = shelf.own_name.empty() ? std::to_string(static_cast<int>(shelf.id))
                                                       : std::string(shelf.own_name);
      const ImVec2 text = ImGui::CalcTextSize(label.c_str());
      // ⚠ ROGNÉ AU CADRE : un nom de joueur peut dépasser, et il déborderait sur
      // l'onglet voisin.
      into->PushClipRect(ImVec2(at.x + kShelfTileInsetPixels, at.y),
                         ImVec2(corner.x - kShelfTileInsetPixels, corner.y), true);
      into->AddText(ImVec2((at.x + corner.x - text.x) * 0.5F, (at.y + corner.y - text.y) * 0.5F),
                    active || hovered ? art.look().text : art.look().faint_text, label.c_str());
      into->PopClipRect();
    }
    // Le survol éclaircit, comme une case d'objet — jamais le bleu du thème.
    if (hovered && !active) {
      into->AddRectFilled(at, corner, kItemHoverVeil);
    }
    // 🔴 L'INFOBULLE PORTE LE NOM DU SERVEUR, et le nom du joueur quand il y en
    // a un : c'est la seule chose qui dise quel coffre est sous le curseur,
    // puisque l'onglet, lui, ne montre qu'une icône ou un numéro.
    if (hovered) {
      if (shelf.own_name.empty()) {
        SkinTooltip(std::string(shelf.name).c_str());
      } else {
        SkinTooltip((std::string(shelf.own_name) + " (" + std::string(shelf.name) + ")").c_str());
      }
    }
    // ⚠ LA CIBLE SE POSE SUR LE DERNIER ÉLÉMENT SOUMIS, donc juste après le
    // bouton : on LÂCHE un objet du coffre sur un onglet pour lui prendre son
    // icône, sans avoir à taper son identifiant.
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
          dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(ItemDrag))) {
        ItemDrag carried;
        std::memcpy(&carried, dropped->Data, sizeof(carried));
        action.decorated_shelf = static_cast<int>(shelf.id);
        action.decorate_item_id = carried.item_id;
      }
      ImGui::EndDragDropTarget();
    }
    // LE CLIC DROIT OUVRE LE RENOMMAGE. ⚠ AU RELÂCHEMENT, comme partout dans
    // cette fenêtre : le surgissant naît à l'image suivante, et parti de
    // l'enfoncement il naîtrait derrière la fenêtre qui reprend le premier plan.
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
      state.naming_shelf = static_cast<int>(shelf.id);
      // ⚠ LE TAMPON PART DU NOM DU JOUEUR, pas de celui du serveur : on corrige
      // ce qu'on a écrit, et une page blanche est ce qu'on obtient quand on n'a
      // rien écrit.
      const std::size_t fits = std::min(shelf.own_name.size(), sizeof(state.shelf_name) - 1);
      std::memcpy(state.shelf_name, shelf.own_name.data(), fits);
      state.shelf_name[fits] = '\0';
      state.shelf_icon = static_cast<int>(shelf.icon_item_id);
      ImGui::OpenPopup("nommer");
    }
    if (active) {
      open_min = at;
      open_max = corner;
      open_shown = true;
    }
    DrawShelfMenu(shelf, state, art, room, action);
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
  // LE PONT DE L'ONGLET OUVERT : il mange le bord qui le sépare de ce qu'il
  // ouvre. ⚠ SANS LUI, L'OUVERT EST FERMÉ COMME LES AUTRES — son art ouvre un
  // bord, et ce qui suit repasse par-dessus.
  //
  // ⚠ LE BORD DÉPEND DE LA DISPOSITION : le BAS pour une rangée, la DROITE pour
  // une colonne — du côté de la table, toujours.
  if (open_shown) {
    if (lying) {
      into->AddRectFilled(
          ImVec2(open_min.x + kShelfBridgeInsetPixels, open_max.y - 1.0F),
          ImVec2(open_max.x - kShelfBridgeInsetPixels, open_max.y + kShelfBridgeOverlapPixels),
          kShelfBridgeColor);
    } else {
      into->AddRectFilled(
          ImVec2(open_max.x - 1.0F, open_min.y + kShelfBridgeInsetPixels),
          ImVec2(open_max.x + kShelfBridgeOverlapPixels, open_max.y - kShelfBridgeInsetPixels),
          kShelfBridgeColor);
    }
  }
  if (!lying) {
    ImGui::EndChild();
    ImGui::PopStyleVar();
  }
}

}  // namespace

void DrawStorageShelfRow(StorageWindowState& state, const StorageWindowView& view,
                         const SkinArt& art, bool lying, float length,
                         StorageWindowAction& action) {
  DrawShelfRow(state, view, art, lying, length, action);
}

float StorageShelfThickness(const SkinArt& art, bool lying) {
  const ImVec2 tile = ShelfTileSize(art, lying);
  return lying ? tile.y : tile.x;
}

}  // namespace ro::ui
