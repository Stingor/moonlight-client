#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/render/scene_fog.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LE BROUILLARD QU'UNE CARTE DÉCLARE.
//
// Ni le RSW ni le GND n'en parlent : il vient d'une table à part, une entrée
// par carte, que `scripts/yaml_tables/install_text_tables.py` tire de
// `data\fogparametertable.txt` de l'installation :
//
//     version: 1
//     maps:
//       1@exds:
//         near: 0.23
//         far: 0.9
//         color: [68, 153, 204, 255]
//         density: 0.3
//
// La couleur s'écrit alpha, rouge, vert, bleu. Une carte qui n'y figure pas
// n'a pas de brouillard.
inline constexpr std::string_view kFogTablePath = "data\\yaml_files\\map_fog.yml";

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

// LA CLÉ D'UNE CARTE : son nom de fichier RSW, extension comprise.
//
// ⚠ UNE CARTE D'INSTANCE PERD SON NUMÉRO : les trois caractères qui précèdent
// de quatre le DERNIER `@` sautent, dès que ce `@` est au moins au rang quatre
// — « 0011@exds » se cherche comme « 1@exds ».
[[nodiscard]] std::string FogTableKeyOf(std::string_view map_name);

// La table entière.
class MapFogTable {
 public:
  // Le texte de `map_fog.yml`. Refuse un texte qui n'est pas du YAML, dont
  // `version` n'est pas 1, sans table `maps`, ou dont une carte n'a pas ses
  // quatre champs bien formés : une couleur sans ses quatre octets, un nom qui
  // ne s'écrit pas en CP949.
  [[nodiscard]] static core::Result<MapFogTable> Parse(std::string_view yaml_text);

  // Le brouillard de `map_name`, ou rien quand la table ne le cite pas. La clé se
  // compare À LA LETTRE, casse comprise.
  [[nodiscard]] std::optional<MapFog> FogOf(std::string_view map_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_key_.size(); }

  // Toutes les entrées, par clé de carte — extension comprise, en CP949.
  [[nodiscard]] const std::unordered_map<std::string, MapFog>& entries() const noexcept {
    return by_key_;
  }

 private:
  // Par clé de carte, extension comprise, en CP949.
  std::unordered_map<std::string, MapFog> by_key_;
};

// Lit la table dans l'installation, puis y cherche la carte. Une table absente
// laisse toutes les cartes sans brouillard ; illisible aussi, et le journal le
// dit.
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
