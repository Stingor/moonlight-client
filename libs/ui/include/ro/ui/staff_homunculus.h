#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// UN HOMONCULE QU'UNE COMMANDE DU STAFF PEUT DÉSIGNER : sa classe, et le
// libellé que la liste affiche (« 6001 Lif »).
//
// ⚠ LA LISTE VIENT D'`app`, qui sait ce que le serveur sait créer ou muter ; la
// fenêtre ne fait que la montrer et rendre la classe choisie.
struct StaffHomunculusChoice {
  std::uint16_t job = 0;
  std::string label;
};

// La largeur de cette liste, en hauteurs de police : « 6004 Vanilmirth » y
// tient.
inline constexpr float kStaffHomunculusComboEms = 12.0F;

// LA LISTE HABILLÉE DE CES HOMONCULES. `rank` est le rang choisi dans
// `choices`, ramené dans la liste quand elle a raccourci. Rend vrai sur un
// changement de choix.
[[nodiscard]] bool StaffHomunculusCombo(const char* id, int* rank,
                                        std::span<const StaffHomunculusChoice> choices,
                                        const SkinArt& art);

// La classe au rang `rank`, zéro hors de la liste.
[[nodiscard]] std::uint16_t StaffHomunculusJobAt(std::span<const StaffHomunculusChoice> choices,
                                                 int rank);

}  // namespace ro::ui
