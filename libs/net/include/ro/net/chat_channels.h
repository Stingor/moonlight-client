#pragma once

#include <cstddef>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES CANAUX DU SERVEUR — `ZC 0x0F21`, réservé aux clients ayant donné leur
// poignée de main (ADR 0040).
//
// 🔴 IL N'EXISTE QUE PARCE QUE LE CLIENT NE PEUT PAS DEVINER. Les canaux vivent
// dans `conf/channels.conf`, filtrés par le groupe du joueur, et la seule autre
// façon de les lire en jeu est d'analyser le texte de « @channel list » — qui
// est LOCALISÉ, donc instable. Le serveur possède la liste et ses droits ; le
// client dessine ce qu'il reçoit. Même patron que la liste des coffres.
//
// ⚠ IL NE PART QU'AU LOGIN, et il n'a pas à repartir : le nom du canal de carte
// et celui de l'alliance sont ceux du gabarit — `#map`, `#ally` —, les mêmes
// sur toutes les cartes et pour toutes les guildes. Seule leur EXISTENCE varie,
// et pour l'alliance c'est un drapeau qui le dit.
//
// ⚠ ET LES CANAUX PRIVÉS EN SONT VOLONTAIREMENT ABSENTS. Cette liste dit « où
// je peux parler », pas « ce qui existe » ; « @channel list » garde ce rôle.
[[nodiscard]] FrameOutcome ApplyChatChannelFrame(const Frame& frame, sim::World& world);

// ── CE QUE LA STRUCTURE ENGENDRÉE NE PORTE PAS ──────────────────────────────
//
// 🔴 LE GÉNÉRATEUR S'ARRÊTE AU BLOC FIXE, comme pour `0x0F2D`. Le paquet déclare
// un `count` et pas ses entrées : la liste est écrite par le CODE du serveur, et
// aucune structure ne la décrit. Les décalages se nomment donc ici.
inline constexpr std::size_t kChannelListFixedSize = 5;

// `CHAN_NAME_LENGTH`, dans `src/map/channel.hpp`. ⚠ CE N'EST PAS `NAME_LENGTH` :
// vingt-quatre pour un personnage, VINGT pour un canal. Se tromper de constante
// décalerait toute la liste, en silence et dès la seconde entrée.
inline constexpr std::size_t kChannelNameSize = 20;

// Un drapeau, une couleur, deux noms.
inline constexpr std::size_t kChannelListEntrySize = 5 + 2 * kChannelNameSize;

// Ce que le serveur ne dépassera jamais : son compte tient sur un octet.
inline constexpr std::size_t kChannelsMostEntries = 255;

// Les bits de `flags`.
inline constexpr std::uint8_t kChannelNeedsGuild = 0x01;
inline constexpr std::uint8_t kChannelCanChat = 0x02;

}  // namespace ro::net
