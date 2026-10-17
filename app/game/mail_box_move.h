#pragma once

#include <optional>

#include "ro/sim/mailbox.h"

namespace moonclient {

// CE QUE LA BOÎTE DU COURRIER FAIT À CETTE IMAGE.
enum class MailBoxMove {
  kNone,
  // Ouvrir, et demander la liste.
  kOpen,
  // Fermer, sans un octet : le serveur ne tient aucune session de boîte.
  kClose,
};

// LES DEUX CHEMINS QUI OUVRENT LA BOÎTE N'EN FONT QU'UN. L'icône du menu
// bascule ; l'ordre du serveur dit l'état voulu, et l'emporte sur la bascule de
// la même image. Ouvrir une boîte ouverte, ou fermer une boîte fermée, ne fait
// rien : la liste n'est pas redemandée.
[[nodiscard]] MailBoxMove MailBoxMoveFor(bool box_open, bool toggle_asked,
                                         std::optional<ro::sim::MailWindowOrder> order) noexcept;

}  // namespace moonclient
