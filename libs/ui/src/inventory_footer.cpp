#include "inventory_footer.h"

#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "item_grid.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// ── CE QUE LA FENÊTRE SAIT FAIRE, ÉCRIT NOIR SUR BLANC ──────────────────────
//
// 🔴 ELLE NE LISTE QUE CE QUI MARCHE. Un geste annoncé qui ne répond pas est
// pire qu'un geste absent : le joueur le réessaie, doute de sa souris, puis du
// reste. Un geste rejoint la liste avec ce qu'il décrit, pas avant.
//
// ⚠ UNE LIGNE PAR ENTRÉE DE CATALOGUE : un pavé écrit d'un bloc n'entrerait
// dans aucune traduction. `TrText` les marque ici, `SkinHelpMarker` les traduit
// et les recolle au survol.
constexpr const char* kInventoryHelpTitle = core::TrText("Raccourcis inventory");
constexpr std::array<const char*, 7> kInventoryShortcuts = {
    core::TrText("- Double-clic gauche : utiliser / équiper"),
    core::TrText("- Ctrl + double-clic gauche : équiper en main gauche"),
    core::TrText("- Clic droit : menu contextuel"),
    core::TrText("- Maj + clic droit : (dé)favori"),
    core::TrText("- Alt + clic droit : transfert rapide (storage si ouvert, sinon cart)"),
    core::TrText("- Glisser : cart, storage, équipement, barre de raccourcis, sol"),
    core::TrText("- Glisser un item sur un onglet : l'y range, ou l'en retire"),
};

// Un pourcentage, et de quoi le calculer sans diviser par zéro.
//
// ⚠ IL NE SERT QU'AU SAC. La charrette rougit quand elle est PLEINE, et ce
// n'est pas le même seuil : le sien vient du serveur, celui du sac aussi mais
// en pour cent d'un maximum.
constexpr std::int64_t kPercent = 100;

// L'écart entre deux interrupteurs du pied, en pixels d'art.
constexpr float kFooterSwitchGapPixels = 4.0F;

// La largeur qu'un interrupteur prendra, avant de savoir où le poser.
[[nodiscard]] float SwitchWidth(const SkinImage& on, const SkinImage& off, bool active) {
  const SkinImage& shown = active ? on : off;
  return shown.usable() ? shown.width : 0.0F;
}

// ── LES INTERRUPTEURS DU PIED ────────────────────────────────────────────────
//
// Le verrou de jet sur TOUS les onglets ; le verrou de vente et le tri sur les
// FAVORIS seulement. C'est la composition du client, et elle a sa logique : les
// deux derniers ne parlent que de cette liste-là.
//
// ⚠ SUR LA LIGNE DU BAS, COLLÉS À DROITE, dans l'ordre jet - vente - tri. Les
// zenys gardent alors toute la ligne du haut, que le poids entame déjà à
// gauche. La même marge que les zenys les sépare du bord — celle que la poignée
// de redimensionnement occupe.
//
// ⚠ LE TRI DISPARAÎT QUAND LA TAILLE EST VERROUILLÉE : c'est le mode où la
// disposition appartient au joueur, et un tri viendrait la défaire.
//
// ⚠ CENTRÉS SUR LEUR LIGNE MAIS BORNÉS DANS LA BARRE, et c'est LE PLUS HAUT DES
// TROIS qui décide du bornage : borner chacun sur le sien les désalignerait, et
// le groupe paraîtrait en escalier. Un bitmap du jeu est plus haut qu'une
// demi-ligne de texte, et déborderait par le bas.
// ⚠ `tab` EST L'ONGLET CHOISI À CETTE IMAGE, pas celui que l'état porte encore.
// La bande dessine déjà le nouveau comme actif à l'image du clic, et l'état ne
// le reçoit qu'à la fin du dessin : lire l'état ferait apparaître les boutons
// des favoris une image après leur onglet.
void DrawFooterSwitches(const SkinArt& art, ItemTab tab, ImVec2 top_left, ImVec2 bottom_right,
                        float middle_y, const InventoryWindowState& state,
                        InventoryWindowAction& action) {
  const bool favorites = tab == ItemTab::kFavorite;
  const bool sort_shown = favorites && !state.size_locked;

  const SkinImage& drop_on = art.Get(SkinPiece::kDropLockOn);
  const SkinImage& drop_off = art.Get(SkinPiece::kDropLockOff);
  const SkinImage& sell_on = art.Get(SkinPiece::kSellLockOn);
  const SkinImage& sell_off = art.Get(SkinPiece::kSellLockOff);
  const SkinImage& sort_on = art.Get(SkinPiece::kSortOn);
  const SkinImage& sort_off = art.Get(SkinPiece::kSortOff);

  const float drop_width = SwitchWidth(drop_on, drop_off, state.drop_locked);
  const float sell_width = favorites ? SwitchWidth(sell_on, sell_off, state.sell_locked) : 0.0F;
  const float sort_width = sort_shown ? SwitchWidth(sort_on, sort_off, state.sorted) : 0.0F;
  float group = drop_width;
  group += sell_width > 0.0F ? kFooterSwitchGapPixels + sell_width : 0.0F;
  group += sort_width > 0.0F ? kFooterSwitchGapPixels + sort_width : 0.0F;
  if (group <= 0.0F) {
    return;
  }

  float tallest = 0.0F;
  const auto note = [&tallest](const SkinImage& shown) {
    if (shown.usable()) {
      tallest = std::max(tallest, shown.height);
    }
  };
  note(state.drop_locked ? drop_on : drop_off);
  if (favorites) {
    note(state.sell_locked ? sell_on : sell_off);
  }
  if (sort_shown) {
    note(state.sorted ? sort_on : sort_off);
  }
  const float half = tallest * 0.5F;
  const float line_y = std::clamp(middle_y, top_left.y + half, bottom_right.y - half);
  float left = bottom_right.x - kResizeGripMarginPixels - group;
  float taken = 0.0F;

  // LE MARQUEUR D'AIDE, JUSTE À GAUCHE DU GROUPE. ⚠ SA PLACE EST DANS LE PIED
  // et non au-dessus de la grille : là-haut il mangerait une ligne entière à ce
  // que la fenêtre est faite pour montrer.
  FooterHelp(kInventoryHelpTitle, kInventoryShortcuts,
             ImVec2(left - kFooterHelpGapPixels - FooterHelpWidth(), line_y));

  // ⚠ CHAQUE LIBELLÉ TIENT SUR UNE SEULE CHAÎNE : le relevé des traductions ne
  // reconnaît qu'un littéral entier, et une chaîne que le format coupe en deux
  // n'entrerait dans aucun catalogue.
  const char* drop_tip =
      state.drop_locked ? core::Tr("Verrou de jet mis : aucun item ne peut être jeté au sol.")
                        : core::Tr("Verrou de jet ôté. Cliquer pour interdire de jeter un item.");
  if (FooterToggle("##verrou_jet", drop_on, drop_off, state.drop_locked, ImVec2(left, line_y),
                   drop_tip, &taken)) {
    action.toggle_drop_lock = true;
  }
  left += taken + kFooterSwitchGapPixels;

  if (favorites) {
    const char* sell_tip =
        state.sell_locked
            ? core::Tr("Verrou de vente mis : un favori ne se vend pas à un marchand.")
            : core::Tr("Verrou de vente ôté. Cliquer pour protéger les favoris.");
    if (FooterToggle("##verrou_vente", sell_on, sell_off, state.sell_locked, ImVec2(left, line_y),
                     sell_tip, &taken)) {
      action.toggle_sell_lock = true;
    }
    left += taken + kFooterSwitchGapPixels;
  }

  if (sort_shown) {
    const char* sort_tip =
        state.sorted ? core::Tr("Vue triée par type puis par nom. Cliquer pour rendre l'ordre.")
                     : core::Tr("Vue dans l'ordre du serveur. Cliquer pour trier par type.");
    if (FooterToggle("##tri", sort_on, sort_off, state.sorted, ImVec2(left, line_y), sort_tip,
                     &taken)) {
      action.toggle_sort = true;
    }
  }
}

// LE SAC DE ZENY, À GAUCHE DU MONTANT : il ouvre la banque.
//
// ⚠ POSÉ PAR RAPPORT AU MONTANT et non au bord de la fenêtre : le nombre
// change de largeur à chaque pièce dépensée, et un bouton calé sur le bord
// finirait par lui rentrer dedans.
//
// ⚠ RIEN QUAND L'ART MANQUE : ce bitmap est un AJOUT au jeu — il vit dans
// `styleshop`, à côté de boutons qui n'ont rien à voir —, et une installation
// qui ne l'a pas garde un pied intact. Le raccourci, lui, ouvre toujours.
//
// 🔴 ET IL SE POSE À LA MOITIÉ DE SON ART. Le bitmap fait dix-neuf sur
// VINGT-QUATRE — plus haut que la ligne du pied, qui vaut un interligne de
// texte, et franchement plus gros que les interrupteurs voisins. À taille pleine
// il déborde de la barre par le haut et écrase le montant qu'il accompagne. Le
// facteur ½ exact a de plus l'avantage de moyenner quatre texels en un au
// filtrage.
constexpr float kBankButtonScale = 0.5F;

void DrawBankButton(const SkinArt& art, ImVec2 zeny_left_middle, InventoryWindowAction& action) {
  const SkinImage& out = art.Get(SkinPiece::kBankButtonOut);
  if (!out.usable()) {
    return;
  }
  float taken = 0.0F;
  const ImVec2 at(zeny_left_middle.x - kFooterSwitchGapPixels - out.width * kBankButtonScale,
                  zeny_left_middle.y);
  if (FooterImageButton("##banque", out, art.Get(SkinPiece::kBankButtonOver),
                        art.Get(SkinPiece::kBankButtonDown), at,
                        core::Tr("Banque de zeny. Ouvre, ou referme."), &taken, kBankButtonScale)) {
    action.toggle_bank = true;
  }
}

void DrawFooter(const SkinArt& art, ItemTab tab, InventoryTally tally, ImVec2 top_left,
                ImVec2 bottom_right, const InventoryWindowState& state,
                InventoryWindowAction& action) {
  ImDrawList* const into = ImGui::GetWindowDrawList();
  DrawFooterBar(into, art, top_left, bottom_right);

  const float line_height = ImGui::GetTextLineHeight();
  const float band = (bottom_right.y - top_left.y) / static_cast<float>(kFooterLines);
  const float first_y = top_left.y + band * 0.5F;
  const float second_y = top_left.y + band * 1.5F;
  const ImU32 ordinary = art.look().text;

  // Ligne 1 — le poids à gauche, les zenys à droite.
  const std::int64_t percent =
      tally.max_weight > 0 ? tally.weight * kPercent / tally.max_weight : 0;
  const bool over =
      tally.max_weight > 0 && tally.overweight_percent > 0 && percent >= tally.overweight_percent;
  std::array<char, 64> weight = {};
  std::snprintf(weight.data(), weight.size(), "%lld/%lld (%lld%%)",
                static_cast<long long>(tally.weight / kWeightTenths),
                static_cast<long long>(tally.max_weight / kWeightTenths),
                static_cast<long long>(percent));
  float x = top_left.x + kFooterInsetPixels;
  x += DrawFooterIcon(into, art.Get(SkinPiece::kWeightIcon), x, first_y) + kFooterIconGapPixels;
  into->AddText(ImVec2(x, first_y - line_height * 0.5F), over ? kItemOverweightText : ordinary,
                weight.data());

  // ⚠ RIEN TANT QUE LA BOURSE N'EST PAS CONNUE : écrire la sentinelle « pas
  // encore dit » afficherait « -1z », un montant faux plutôt qu'un vide honnête.
  //
  // 🔴 ET ELLE NE VIENT PAS DU SERVEUR DE ZONE, qui n'annonce `SP_ZENY` QUE
  // lorsqu'il change : son lot d'accueil envoie le poids et saute la bourse. Elle
  // vient de la LISTE DES PERSONNAGES, sous le nom `money`, et `app` la pose à
  // l'entrée en jeu — comme les deux niveaux et la vitesse de marche.
  if (tally.zeny >= 0) {
    const std::string zeny = GroupThousands(tally.zeny) + std::string(kZenySuffix);
    const float zeny_width = ImGui::CalcTextSize(zeny.c_str()).x;
    // ⚠ LA POIGNÉE DE REDIMENSIONNEMENT PREND LE COIN, et le montant s'arrête
    // avant elle. Aligné sur le bord, il passerait dessous et s'en trouverait
    // à moitié couvert — c'est la marge que le client réserve aussi.
    const float zeny_left = bottom_right.x - kResizeGripMarginPixels - zeny_width;
    into->AddText(ImVec2(zeny_left, first_y - line_height * 0.5F), ordinary, zeny.c_str());
    DrawBankButton(art, ImVec2(zeny_left, first_y), action);
  }

  // Ligne 2 — combien d'objets, sur combien de places.
  std::array<char, 32> count = {};
  std::snprintf(count.data(), count.size(), "%d/%d", tally.item_count, tally.max_items);
  x = top_left.x + kFooterInsetPixels;
  x += DrawFooterIcon(into, art.Get(SkinPiece::kCountIcon), x, second_y) + kFooterIconGapPixels;
  into->AddText(ImVec2(x, second_y - line_height * 0.5F), ordinary, count.data());

  DrawFooterSwitches(art, tab, top_left, bottom_right, second_y, state, action);
}

}  // namespace

void DrawInventoryFooter(const SkinArt& art, ItemTab tab, InventoryTally tally, ImVec2 top_left,
                         ImVec2 bottom_right, const InventoryWindowState& state,
                         InventoryWindowAction& action) {
  DrawFooter(art, tab, tally, top_left, bottom_right, state, action);
}

}  // namespace ro::ui
