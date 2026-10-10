// LA PARTIE PVP DE LA SECTION INTERFACE : où le joueur pose ses deux pièces.
//
// ⚠ À PART DU CATALOGUE, et c'est le plafond de huit cents lignes qui l'a
// décidé — mais la couture suit une frontière réelle, celle que `app/options`
// emploie déjà pour les jauges, les barres, les dégâts et les objets : une
// FAMILLE par fichier. Celle-ci est « la place des deux pièces du classement ».
//
// 🔴 CE FICHIER NE RÈGLE PAS CE QUE LES PIÈCES MONTRENT, et il ne le peut pas :
// ce qu'elles montrent, c'est le SERVEUR qui le dit, et il ne parle qu'en zone
// PVP. Les cadres, eux, se posent n'importe où — autrement on ne pourrait
// composer son écran qu'au milieu d'une mêlée.

#include "ro/core/i18n.h"

#include "options/game_options.h"
#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;

}  // namespace

void GameOptions::ComposePvpHudRows() {
  // ⚠ AUCUN RÉGLAGE DE TAILLE : la taille EST le cadre, qu'on tire par ses
  // bords. Un curseur d'échelle en plus dirait la même chose deux fois, et les
  // deux se contrediraient dès qu'on emploie l'une puis l'autre.
  display_rows_.push_back(
      Toggle(GameOption::kPvpHudLocked, Tr("Verrouiller"),
             Tr("Verrouillés, le compteur de rang et le chronomètre se figent et laissent "
                "passer les clics au jeu. Déverrouillés, on les déplace au glisser et on "
                "les retaille par un bord — les chiffres suivent la taille du cadre."),
             nullptr, pvp_hud_.locked));
  display_rows_.back().heading = Tr("Classement PVP");
  display_rows_.push_back(
      Toggle(GameOption::kPvpHudBorder, Tr("Montrer le filet pendant la pose"),
             Tr("Les deux cadres n'ont pas de fond : sans filet, on ne voit pas ce qu'on "
                "attrape tant qu'on ne l'a pas attrapé. Il disparaît au verrouillage."),
             nullptr, pvp_hud_.border));
  display_rows_.back().disabled = pvp_hud_.locked;
  display_rows_.push_back(
      Toggle(GameOption::kPvpHudSticky, Tr("Aimanter entre eux"),
             Tr("Les bords du cadre qu'on déplace s'alignent sur ceux de l'autre. Deux "
                "cadres qui se touchent partent ensemble au CTRL."),
             nullptr, pvp_hud_.sticky));
  display_rows_.back().disabled = pvp_hud_.locked;
  display_rows_.push_back(
      Toggle(GameOption::kPvpHudCenterMark, Tr("Repère de centre"),
             Tr("Une croix au milieu de chaque cadre déverrouillé : centrer à l'œil est "
                "faux de quelques pixels, et l'erreur ne se voit qu'une fois reverrouillé."),
             nullptr, pvp_hud_.center_mark));
  display_rows_.back().disabled = pvp_hud_.locked;
}

}  // namespace moonclient
