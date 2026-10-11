#pragma once

#include <array>

#include "ro/core/i18n.h"
#include "ro/sim/chat_line.h"

namespace moonclient {

// LES LIBELLÉS DES VINGT-HUIT CASES DE FILTRE, dans l'ordre des numéros.
//
// Le français est celui de la traduction communautaire du client, l'anglais le
// libellé d'origine : c'est ce que les joueurs ont sous les yeux depuis des
// années. Le jargon du jeu reste en anglais (« item », « skill »).
//
// ⚠ MARQUÉS ICI, TRADUITS À LA LECTURE par `ChatTypeLabel` : la langue peut
// changer en cours de partie, et un libellé traduit une fois pour toutes
// resterait dans l'ancienne.
inline constexpr std::array<const char*, ro::sim::kChatTypeCount> kChatTypeLabels = {
    ro::core::TrText("Public"),
    ro::core::TrText("Chat public"),
    ro::core::TrText("Message privé"),
    ro::core::TrText("Chat de groupe"),
    ro::core::TrText("Chat de guilde"),
    ro::core::TrText("Chat d'alliance"),
    ro::core::TrText("Items ramassés/lâchés"),
    ro::core::TrText("Équipement mis/retiré"),
    ro::core::TrText("Altérations d'état"),
    ro::core::TrText("Items obtenus par le groupe"),
    ro::core::TrText("Altérations d'état du groupe"),
    ro::core::TrText("Échecs de skill"),
    ro::core::TrText("Configuration du groupe"),
    ro::core::TrText("Équipement endommagé"),
    ro::core::TrText("Informations WOE"),
    ro::core::TrText("Message de recherche de groupe"),
    ro::core::TrText("Message de combat"),
    ro::core::TrText("Message de combat du groupe"),
    ro::core::TrText("Message d'expérience"),
    ro::core::TrText("Informations de quête"),
    ro::core::TrText("Message du champ de bataille"),
    ro::core::TrText("Chat du clan"),
    ro::core::TrText("Messages d'appel"),
    ro::core::TrText("EXP de contribution"),
    ro::core::TrText("Attributs d'équipement"),
    ro::core::TrText("Annonces du serveur"),
    ro::core::TrText("Discord"),
    ro::core::TrText("Mes skills (visée et lancer)"),
};

}  // namespace moonclient
