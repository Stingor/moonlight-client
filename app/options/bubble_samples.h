#pragma once

#include "ro/ui/broadcast_balloon.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/options_window.h"

namespace moonclient {

// ── LES APERÇUS DES DEUX PAGES DE BULLES ────────────────────────────────────
//
// Un cadre épinglé en haut de la page, où les bulles se peignent comme en jeu
// avec le réglage du moment : on règle une couleur ou une marge en la
// regardant, sans attendre qu'un joueur parle ou que le serveur annonce.
//
// ⚠ LE FOND DU CADRE EST COUPÉ EN DEUX, sombre puis clair : un voile trop léger
// se lit sur l'un et se perd sur l'autre, et c'est justement ce qu'on règle.
//
// ⚠ UNE BULLE DE CHAQUE CADRE VIT ET MEURT EN BOUCLE : la durée et le fondu ne
// se voient qu'en mouvement. Les autres restent fixes, pour l'apparence.

// Le cadre des bulles de chat : deux têtes, une parole fixe et une qui vit.
[[nodiscard]] ro::ui::OptionRowView ChatBubbleSampleRow(const ro::ui::ChatBubbleLook& look);

// Le cadre des bulles d'annonce : autant d'annonces que le réglage en garde, au
// plus trois, puis l'avancée d'une chasse.
[[nodiscard]] ro::ui::OptionRowView BroadcastBalloonSampleRow(
    const ro::ui::BroadcastBalloonLook& look);

}  // namespace moonclient
