#pragma once

#include "ro/vfs/vfs.h"

#include "game/game_pieces.h"

namespace moonclient {

// ── LE SKIN, LU ET RELU ─────────────────────────────────────────────────────
//
// Un skin est un dossier posé par-dessus le dossier d'interface du VFS : il ne
// change que des images. Le changer EN JEU, c'est relire tout ce qui en a été
// tiré et gardé.

// Lit les pièces de l'habillage des fenêtres et les envoie à la carte
// graphique. ⚠ LES ANCIENNES TEXTURES MEURENT : l'interface doit reprendre les
// nouvelles avant la prochaine image (`UiStage::UseSkin`).
void LoadSkinTextures(ro::vfs::Vfs& vfs, GamePieces& pieces);

// RELIT CE QUE LES PIÈCES ONT TIRÉ DU DOSSIER D'INTERFACE, après un changement
// de surcharge : l'habillage, les caches d'images et d'icônes — vidés SANS
// ÊTRE DÉPLACÉS, d'autres les désignent —, les textures du sol, et, si le monde
// est chargé, les insignes de boss, la marque du champ de bataille et les
// enseignes.
//
// ⚠ ENTRE DEUX IMAGES : des textures meurent.
//
// ⚠ CE QUE LES ÉTAGES EN ONT COPIÉ se relit seul : ils comparent la génération
// du cache dont ils l'ont tiré (`GameImages::generation`,
// `ItemIcons::generation`) ou la révision des surcharges du VFS.
void ReloadSkinPieces(ro::vfs::Vfs& vfs, GamePieces& pieces, bool world_loaded);

}  // namespace moonclient
