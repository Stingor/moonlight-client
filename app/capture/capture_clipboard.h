#pragma once

#include <cstdint>
#include <span>

#include "ro/formats/image.h"

namespace moonclient {

// ── CE QUE LA CAPTURE DEVIENT : UN COLLAGE, PAS UN FICHIER ──────────────────
//
// 🔴 LE PRESSE-PAPIER EST LA LIVRAISON, ET LA SEULE. Le trajet réel d'une
// capture est « je filme, je colle dans le salon » ; un fichier déposé dans un
// dossier ajoute deux gestes — le retrouver, le glisser — et laisse derrière lui
// des dizaines de prises que personne ne rouvrira.
//
// ⚠ ÉCRASER LE PRESSE-PAPIER EST UNE ACTION, ET ELLE NE SE DÉFAIT PAS. Ce qui
// s'y trouvait est perdu ; c'est le prix, et il est dit au joueur à l'endroit où
// il arme la capture.

// UNE ANIMATION. Rend faux si le presse-papier n'a pas voulu.
//
// 🔴 SOUS WINDOWS, ELLE PASSE PAR UN FICHIER PORTEUR, et il n'y a pas d'autre
// voie. Le presse-papier de ce système ne transporte une IMAGE que sous forme de
// bitmap, qui n'a qu'une seule image — l'animation y mourrait. Le seul format
// qu'un salon de discussion recolle animé est la LISTE DE FICHIERS, celle qu'un
// gestionnaire de fichiers dépose : il faut donc un chemin à lui montrer.
//
// ⚠ CE PORTEUR VA DANS LE DOSSIER TEMPORAIRE DU SYSTÈME, jamais dans le jeu. Ce
// n'est pas une capture qu'on range, c'est le contenu du presse-papier qui a
// besoin d'une adresse ; le système en dispose quand il fait le ménage.
//
// ⚠ ET IL DOIT SURVIVRE AU COLLAGE : on ne l'efface pas derrière soi, sans quoi
// le presse-papier montrerait un chemin qui ne mène nulle part.
[[nodiscard]] bool PutAnimationOnClipboard(std::span<const std::uint8_t> gif);

// UNE IMAGE FIXE. Rend faux si le presse-papier n'a pas voulu.
//
// ⚠ AUCUN FICHIER ICI, MÊME SOUS WINDOWS : une image sans animation entre
// telle quelle dans le presse-papier, et s'y colle partout.
[[nodiscard]] bool PutPictureOnClipboard(const ro::formats::Image& picture);

}  // namespace moonclient
