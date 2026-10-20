#pragma once

// CE QUE LA GRILLE D'ICÔNES DE MENU MONTRE : quelles icônes, leur art, leur
// raccourci, enfoncées ou allumées. Ne lit que ce qu'on lui donne — les faits
// des fenêtres sont relevés à part, par `MenuIconFactsOf` — et s'éprouve donc
// sur un banc.

#include <optional>
#include <string_view>
#include <vector>

#include "ro/render/game_images.h"
#include "ro/ui/menu_icons.h"

#include "input/hotkeys.h"

namespace moonclient {

// L'ACTION DE RACCOURCI QUE CETTE ICÔNE DÉCLENCHE, ou rien.
//
// ⚠ LE STATUT ET L'ÉQUIPEMENT OUVRENT LE MÊME ONGLET : ce client montre les
// caractéristiques À CÔTÉ du mannequin, là où le jeu en fait deux fenêtres.
[[nodiscard]] std::optional<GameAction> ActionOfMenuCommand(ro::ui::MenuCommand command);

// LA FENÊTRE DE CETTE ICÔNE EXISTE-T-ELLE DANS CE CLIENT ? Faux, son clic ouvre
// la fenêtre d'attente.
[[nodiscard]] bool MenuCommandPorted(ro::ui::MenuCommand command);

// Ce que la partie et ses fenêtres disent à la grille.
struct MenuIconFacts {
  // Le métier du personnage est-il doram ? Le dessin du statut en dépend.
  bool doram = false;
  std::string_view map_name;
  // Le carnet de chasse MVP, éteint, n'a pas d'icône.
  bool mvp_tracker_enabled = false;
  // Les bascules dont la fenêtre est ouverte : leur icône reste enfoncée.
  std::vector<ro::ui::MenuCommand> open;
  // Les icônes qui s'allument : du nouveau les attend.
  std::vector<ro::ui::MenuCommand> lit;
  // DES POINTS À PLACER : une pastille sur le Statut, sur les Skills. Voir
  // `ro::sim::StatusPointsSpendable`.
  bool status_points_waiting = false;
  bool skill_points_waiting = false;
  // En file pour un champ de bataille : une pastille, et le terrain nommé.
  bool battleground_queued = false;
  std::string_view battleground_queued_name;
  // Le nom que la table du serveur donne à sa boutique : l'infobulle de son
  // icône le porte.
  std::string_view cash_shop_name;
  // LA TÊTE DU PERSONNAGE, dans le bouton du Statut, à double densité — voir
  // `ro::render::StatusHeadIcon`. Elle passe avant toute autre image du Statut ;
  // vide — aucun personnage chargé, ou un rendu raté —, le Statut garde la sienne.
  ro::ui::SkinImage status_head;
  ro::ui::SkinImage status_head_pressed;
};

// Recompose les icônes MONTRÉES, dans l'ordre de la grille.
//
// `images` nul laisse les icônes sans art : elles se dessinent alors en carrés
// de secours, à leur place.
void ComposeMenuIcons(std::vector<ro::ui::MenuIconView>& into, ro::render::GameImages* images,
                      const Hotkeys& bindings, const MenuIconFacts& facts);

}  // namespace moonclient
