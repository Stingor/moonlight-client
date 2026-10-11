#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_palette.h"

namespace ro::ui {

// ── LA CELLULE D'UN OBJET CLIQUABLE ─────────────────────────────────────────
//
// Un objet montré hors du sac — une récompense, un butin, un matériau, la
// pièce d'un autre joueur — se lit et se manie partout de la même façon : son
// icône, son nom en lien, et les gestes de tout lien. Le clic gauche le
// décrit, Maj + clic le repose dans la saisie, le clic droit ouvre LE menu
// commun des liens (`DrawChatLinkMenu`), et le survol demande son aperçu.
// Une cellule de MONSTRE suit les mêmes règles, avec le menu d'un monstre.
//
// ⚠ `ui` N'EN FAIT RIEN : les gestes s'écrivent dans un `ChatLinkAsk`, que
// `app` joue comme tout lien de la même surface.

// CE QUE LA FENÊTRE GARDE du menu d'une de ses cellules, d'une image à l'autre.
struct ItemLinkMenu {
  // Un clic droit l'a demandé ; il s'ouvre au prochain `DrawItemLinkMenu`.
  bool armed = false;
  // Ctrl + clic droit : le prochain `DrawItemLinkMenu` rend la description,
  // sans ouvrir le menu.
  bool describe_at_once = false;
  ChatLinkKind kind = ChatLinkKind::kItem;
  std::uint32_t item_id = 0;
  std::uint32_t token = 0;
  // La tête du menu : le nom tel que la cellule le montrait.
  std::string label;
};

// UNE CELLULE : ce qu'elle désigne et ce qu'on lit.
struct ItemLinkCell {
  // L'objet, ou le monstre pour `ChatLinkKind::kMonster`.
  std::uint32_t item_id = 0;
  ChatLinkKind kind = ChatLinkKind::kItem;
  // Ce que le lien porte de plus que l'identifiant — raffinement, cartes —,
  // opaque pour `ui` et rendu tel quel ; zéro pour un objet de catalogue.
  std::uint32_t token = 0;
  std::string_view text;
  // La tête du menu, quand `text` porte plus que le nom — une quantité, une
  // chance ; vide : `text`.
  std::string_view menu_label;
  SkinImage icon;
  // Le côté du carré de l'icône, en pixels d'écran ; zéro : la hauteur d'une
  // ligne. Plus haut qu'une ligne, le nom se centre sur l'icône.
  float icon_side = 0.0F;
  // La place de l'icône se garde même sans icône : une colonne reste alignée.
  bool keep_icon_place = false;
  // Un équipement cassé : le nom prend son ombre rouge, l'icône sa teinte.
  bool damaged = false;
  // Souligné même sans survol : la ligne choisie d'une liste.
  bool underlined = false;
  // Nulle : celle des liens de l'habillage, lue au dessin.
  ImU32 color = 0;
};

// Dessine la cellule sur la ligne courante. Son survol s'écrit dans `asked` ;
// le clic gauche et Maj + clic aussi, un clic droit arme `menu`. Rend le geste
// joué ce tour-ci, pour l'appelant qui donne au clic un autre sens : il remet
// alors `asked.gesture` à `kNothing`.
ChatLinkGesture DrawItemLinkCell(const ItemLinkCell& cell, ItemLinkMenu& menu, ChatLinkAsk& asked);

// LE MENU qu'une cellule a armé, à appeler UNE FOIS par image au niveau de la
// fenêtre — hors de tout `PushID`, de toute table et de tout groupe : un
// surgissant se nomme dans la pile d'identifiants, et celui que la cellule
// aurait ouvert sous la sienne ne se retrouverait pas. Ce qu'on y choisit
// s'écrit dans `asked`. `vote_shop` : le catalogue du vote shop, quand la
// surface le connaît.
void DrawItemLinkMenu(const char* popup_id, ItemLinkMenu& menu, ChatLinkAsk& asked,
                      std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
