#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "ro/vfs/map_display_names.h"

namespace moonclient {

// ── LE LIEN D'UN LIEU : `<NAVIL>carte + x + y</NAVIL>` ──────────────────────
//
// La balise du CLIENT D'ORIGINE, celle du bouton de partage de la navigation :
// elle circule déjà entre joueurs, et chacun la rend cliquable de son côté.
//
// 🔴 LE CORPS NE PORTE AUCUN SÉPARATEUR. Les quatre DERNIERS caractères sont les
// coordonnées — deux pour `x`, deux pour `y`, en base 62 et POIDS FAIBLE
// D'ABORD —, et tout ce qui précède est le nom interne de la carte, lequel
// contient lui-même des caractères base 62. La découpe se fait donc par la FIN,
// la seule qui ne soit pas ambiguë.
//
// ⚠ LE NOM AFFICHÉ N'EST PAS DANS LA BALISE : chacun le résout dans SA langue.
//
// ⚠ (0, 0) DÉSIGNE LA CARTE ENTIÈRE, sans case précise.

inline constexpr std::string_view kPlaceLinkOpen = "<NAVIL>";
inline constexpr std::string_view kPlaceLinkClose = "</NAVIL>";

// L'alphabet de la base 62, dans l'ordre des chiffres.
inline constexpr std::string_view kPlaceLinkDigits =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
inline constexpr int kPlaceLinkRadix = 62;
// Deux caractères par coordonnée : au-delà de 62 × 62 - 1, la balise en
// demanderait trois, et une coordonnée de carte n'y arrive jamais.
inline constexpr int kPlaceLinkCoordinateDigits = 2;
inline constexpr int kPlaceLinkCoordinateMax = kPlaceLinkRadix * kPlaceLinkRadix - 1;
inline constexpr std::size_t kPlaceLinkTailLength = 2 * kPlaceLinkCoordinateDigits;

struct PlaceLink {
  // Le nom INTERNE de la carte, sans extension : « prontera ».
  std::string map;
  // La case, ou zéro pour la carte entière.
  int x = 0;
  int y = 0;
};

// La balise. VIDE sans carte, ou quand le nom porte un chevron : il couperait la
// balise en deux à la relecture. Une coordonnée hors bornes se rabat sur elles.
[[nodiscard]] std::string BuildPlaceLink(const PlaceLink& link);

// Relit une balise qui COMMENCE à `text[0]`. Faux si ce n'en est pas une, sans
// nom de carte, ou si la queue n'est pas du base 62.
//
// ⚠ `after` REÇOIT CE QUI SUIT LA FERMANTE, et la fin du texte quand elle
// manque : une ligne coupée par le serveur ne doit pas faire lire au-delà.
[[nodiscard]] bool ParsePlaceLink(std::string_view text, PlaceLink* out, std::size_t* after);

// Où commence la prochaine balise à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindPlaceLink(std::string_view text, std::size_t from);

// Le libellé cliquable : « [Prontera 150,180] », sans coordonnées pour la carte
// entière. `shown_name` est le nom affiché de la carte chez le lecteur ; vide, le
// nom interne le remplace.
[[nodiscard]] std::string PlaceLinkLabel(const PlaceLink& link, std::string_view shown_name);

// Le nom affiché de la carte selon la table de l'installation, vide quand elle
// est absente ou ne connaît pas cette carte.
[[nodiscard]] std::string PlaceShownName(const ro::vfs::MapDisplayNames* names,
                                         std::string_view map);

// Le libellé tel que le fil le rend : celui de `PlaceLinkLabel`, avec le nom
// affiché résolu chez le lecteur.
[[nodiscard]] std::string PlaceLinkLabel(const PlaceLink& link,
                                         const ro::vfs::MapDisplayNames* names);

// La ligne que la conversation écrit quand un guidage part : « Destination :
// [Prontera 150,180] ». Le trajet se lit sur la minimap et au sol, la
// destination reste écrite.
[[nodiscard]] std::string PlaceDestinationLine(const PlaceLink& link,
                                               const ro::vfs::MapDisplayNames* names);

}  // namespace moonclient
