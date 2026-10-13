#pragma once

#include <string_view>

namespace moonclient {

// LE DOSSIER, SOUS L'INSTALLATION, OÙ LE CLIENT RANGE CE QU'IL ENREGISTRE : celui
// des captures du jeu. La visionneuse d'images du chat y dépose une image, la
// fiche de personnage y propose le GIF de son pantin.
//
// ⚠ UN SEUL NOM POUR TOUS : deux dossiers voisins aux noms presque égaux
// laisseraient le joueur chercher son fichier dans le mauvais.
inline constexpr std::string_view kSavedPicturesFolder = "ScreenShot";

}  // namespace moonclient
