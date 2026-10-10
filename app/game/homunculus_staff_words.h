#pragma once

// LES COMMANDES DU STAFF SUR L'HOMONCULE : les listes de classes que les
// fenêtres montrent, et le texte de la commande qu'une intention devient.
//
// 🔴 LE VOCABULAIRE DU SERVEUR EST ICI, PAS DANS LA FENÊTRE : celle-ci émet
// « régler la faim », c'est `app` qui sait que cela s'écrit « @homhungry ».
//
// ⚠ RIEN N'Y EST VALIDÉ : le serveur revalide selon le groupe du compte, et
// son refus arrive dans la conversation.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/ui/homunculus_tab.h"
#include "ro/ui/staff_homunculus.h"

namespace moonclient {

// LES HOMONCULES QUE `@makehomun` SAIT CRÉER : les huit espèces non évoluées,
// puis les cinq formes mutées, chacune en « classe nom ».
[[nodiscard]] std::vector<ro::ui::StaffHomunculusChoice> CreatableHomunculi();

// LES FORMES QUE `@hommutate` PEUT DONNER : les cinq formes mutées.
[[nodiscard]] std::vector<ro::ui::StaffHomunculusChoice> MutatedHomunculi();

// VRAI POUR UNE FORME MUTÉE (« S »), qui a son propre plafond de niveau.
[[nodiscard]] bool IsMutatedHomunculus(std::uint32_t job) noexcept;

// ÉVOLUÉ OU NON, pour une classe d'homoncule connue ; vide pour une classe que
// le client ne connaît pas, ou pas encore.
[[nodiscard]] std::optional<bool> HomunculusEvolved(std::uint32_t job) noexcept;

// « @makehomun <classe> ».
[[nodiscard]] std::string MakeHomunculusCommand(std::uint16_t job);

// LA COMMANDE QU'UNE INTENTION DE LA SECTION « STAFF » DEVIENT, vide quand il
// n'y a rien à envoyer : aucun ordre, aucun niveau à ajouter, aucune phrase.
[[nodiscard]] std::string HomunculusStaffCommand(const ro::ui::HomunculusTabAction& asked);

}  // namespace moonclient
