#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/render/scene_fog.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LE BROUILLARD QU'UNE CARTE DÉCLARE.
//
// Ni le RSW ni le GND n'en parlent : le client le lit dans un fichier texte à
// part, une entrée par carte. Une carte qui n'y figure pas n'en a pas.
inline constexpr std::string_view kFogTablePath = "data\\fogparametertable.txt";

struct MapFog {
  // Où il commence et où il couvre tout, en FRACTIONS de la profondeur que la
  // vue du client embrasse. Voir `SceneFogOf`.
  float near_fraction = 0.0F;
  float far_fraction = 0.0F;
  // 0xAARRGGBB. L'alpha ne teint rien : il dose seulement le mélange du fond,
  // sur les cartes dont le ciel le demande. Voir `FoggedClearColor`.
  std::uint32_t color_argb = 0;
  // Lue, et sans effet : le client la transmet à un brouillard LINÉAIRE, que la
  // densité ne concerne pas.
  float density = 0.0F;
};

// UNE ENTRÉE DE LA TABLE : la clé, puis ses quatre valeurs dans l'ordre du
// fichier — début, fin, couleur, densité.
struct FogTableRow {
  std::string map_key;
  std::array<std::string, 4> values;
};

// DÉCOUPE LA TABLE COMME LE CLIENT.
//
// Chaque `#` ferme un mot, et un saut de ligne jette ce qui le précède sans
// `#` : c'est ce qui fait tomber les commentaires. Le premier mot est la clé,
// et la quatrième valeur ferme l'entrée. Un mot vide à la place d'une clé ne
// compte pas : le mot suivant la prend.
[[nodiscard]] std::vector<FogTableRow> ParseFogTable(std::string_view text);

// LA COULEUR D'UNE ENTRÉE, lue comme le client : les deux premiers caractères
// sont sautés — le « 0x » —, et chaque caractère suivant pèse quatre bits de
// plus que celui qui le suit. Un caractère qui n'est pas hexadécimal compte
// pour zéro, mais garde sa place.
[[nodiscard]] std::uint32_t FogColorOf(std::string_view text);

// LA CLÉ D'UNE CARTE : son nom de fichier RSW, extension comprise.
//
// ⚠ UNE CARTE D'INSTANCE PERD SON NUMÉRO : les trois caractères qui précèdent
// de quatre le DERNIER `@` sautent, dès que ce `@` est au moins au rang quatre
// — « 0011@exds » se cherche comme « 1@exds ».
[[nodiscard]] std::string FogTableKeyOf(std::string_view map_name);

// Le brouillard de `map_name`, ou rien quand la table ne le cite pas. La clé se
// compare À LA LETTRE, casse comprise.
[[nodiscard]] std::optional<MapFog> FogOfMap(std::span<const FogTableRow> rows,
                                             std::string_view map_name);

// Lit la table dans l'installation, puis y cherche la carte. Une table absente
// ou vide laisse toutes les cartes sans brouillard.
[[nodiscard]] std::optional<MapFog> LoadMapFog(vfs::Vfs& vfs, std::string_view map_name);

// LE BROUILLARD QUE LES RENDERERS PEIGNENT.
//
// Le client ramène la profondeur de chaque sommet à sa plage de vue — dix
// unités devant l'œil, mille cinq cents au plus loin — et y place les deux
// fractions. Quand elles sont égales, il ne soustrait pas le début : le
// brouillard court alors sur toute la plage.
[[nodiscard]] SceneFog SceneFogOf(const MapFog& fog);

// L'EFFACEMENT D'UNE CARTE DONT LE CIEL SE MÊLE AU BROUILLARD : le fond tiré
// vers la couleur du brouillard par son alpha, canal par canal, tronqué à
// l'octet comme le client le fait.
[[nodiscard]] std::array<float, 4> FoggedClearColor(const std::array<float, 4>& clear,
                                                    const MapFog& fog);

}  // namespace ro::render
