#pragma once

#include <cstddef>

#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE SERVEUR DIT DES OBJETS DU COFFRE — `ZC 0x0F0F`, envoyé juste après
// sa liste, et réservé aux clients ayant donné leur poignée de main (ADR 0040).
//
// 🔴 C'EST LA SEULE SOURCE DE CES QUATRE FAITS. Le prix de revente, le sous-type
// d'une arme et le masque d'emplacement vivent dans `item_db`, côté serveur ;
// `itemInfo.lua` n'en porte aucun. Sans ce paquet, une colonne de prix ne
// pourrait qu'inventer, et les sous-catégories n'auraient rien sur quoi trier.
//
// ⚠ IL ARRIVE APRÈS LA LISTE, jamais avant : le serveur le bâtit sur le contenu
// qu'il vient d'envoyer. La fenêtre doit donc savoir se dessiner sans lui — une
// image, le temps qu'il arrive.
[[nodiscard]] FrameOutcome ApplyStorageFactsFrame(const Frame& frame, sim::World& world);

// ── CE QUE LA STRUCTURE ENGENDRÉE NE PORTE PAS ──────────────────────────────
//
// Le générateur s'arrête au bloc fixe : le paquet déclare un `count` et pas ses
// entrées. Les décalages se nomment donc ici.
inline constexpr std::size_t kStorageFactsFixedSize = 6;

// `[id:4][sell:4][subtype:1][equip:4][slots:2]`, dans cet ordre — celui où le
// serveur écrit `ZC_MOONLIGHT_STORAGE_PRICES`.
inline constexpr std::size_t kStorageFactsEntrySize = 15;

}  // namespace ro::net
