#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LE CIEL D'UNE CARTE : la couleur peinte AU-DELÀ de ses bords, et le ciel
// animé qui s'y joue.
//
// Certaines cartes flottent — gonryun est une île dans les airs — et l'on voit
// alors le ciel autour d'elles. Le client le peint avec une couleur propre à
// chaque carte, que rien d'autre ne donne : ni le RSW, ni le GND, ni le GAT
// n'en parlent.
//
// La table est `data\yaml_files\map_sky.yml`, que `scripts/yaml_tables/map_sky.py`
// tire de la table de l'installation, indexée par le nom de la carte SANS
// extension :
//
//     version: 1
//     maps:
//       gonryun:
//         background: [102, 153, 204]
//         background_fog: true
//         star: true
//         styles: [1]
//         clouds: [{count: 400, …}]
//
// Une carte que la table ne cite pas s'ouvre sur du NOIR : voir
// `kUnlistedMapClearColor`.
inline constexpr std::string_view kMapSkyPath = "data\\yaml_files\\map_sky.yml";

// LE CIEL ANIMÉ D'UNE CARTE : les nuages d'un champ, la brume d'une ville
// industrielle, un champ d'étoiles la nuit.
//
// ⚠ CE N'EST PAS UN SYSTÈME À PART : chez le client, chacun de ces ciels est un
// EFFET ordinaire de son moteur d'effets, désigné par un identifiant. La table
// ne donne qu'un numéro de style ; le client le traduit par un switch que l'on
// transcrit ici. Notre client rejoue donc le ciel par le même chemin que les
// effets de compétence, sans une ligne de rendu de plus.

// LE STYLE DU CHAMP D'ÉTOILES, celui que `star` allume. Le client ne lui donne
// pas de numéro dans la table : il appelle le lanceur avec NEUF en dur.
inline constexpr std::uint8_t kStarSkyStyle = 9;

// D'un style de ciel à l'effet qui le porte, ou vide si aucun ne le porte.
//
// ⚠ LES TROUS SONT RÉELS : six, douze, treize et quatorze n'ont pas d'entrée
// dans le switch du client. Douze à quatorze existent bien comme STYLES —
// l'effet 1115 les sème tous les quatre à la fois — mais aucune carte ne peut
// les demander seuls.
[[nodiscard]] std::optional<std::uint32_t> SkyEffectIdForStyle(std::uint8_t style);

// CE QUE LA TABLE DU CIEL DIT D'UNE CARTE.
struct MapSkyLook {
  // La couleur peinte AU-DELÀ des bords, en composantes de zéro à un. Voir
  // ci-dessus.
  std::optional<std::array<float, 3>> background;
  // Les identifiants d'EFFET à jouer : le champ d'étoiles d'abord, puis les
  // styles dans l'ordre où la carte les déclare. Vide pour une carte sans ciel
  // animé, qui est le cas le plus courant.
  std::vector<std::uint32_t> effects;
  // Combien de nuages PARAMÉTRÉS (`clouds`) cette carte porte.
  //
  // ⚠ ILS NE SONT PAS PORTÉS, et le compte est là pour que leur absence se lise
  // au lieu de se deviner : le client les traite par un second chemin qui
  // bâtit des nuages réglés champ par champ au lieu de jouer un effet tout
  // fait. La table garde tous leurs champs.
  std::size_t parameterised_clouds = 0;
  // LE FOND SE MÊLE-T-IL AU BROUILLARD ? Vrai seulement quand la table le dit
  // en toutes lettres ; rien n'en change tant que la carte n'a pas de
  // brouillard allumé. Voir `FoggedClearColor`.
  bool background_fog = false;
};

// La table entière.
class MapSkyTable {
 public:
  // Le texte de `map_sky.yml`. Refuse un texte qui n'est pas du YAML, dont
  // `version` n'est pas 1, sans table `maps`, ou dont une carte porte un champ
  // inconnu ou mal formé : une couleur qui n'a pas ses trois composantes de 0
  // à 255, un style hors d'un octet.
  [[nodiscard]] static core::Result<MapSkyTable> Parse(std::string_view yaml_text);

  // Ce que la table dit d'une carte, par son nom sans extension. Une fiche
  // VIDE quand la carte n'y figure pas — c'est le cas normal, huit cartes sur
  // dix n'ont pas de ciel.
  [[nodiscard]] MapSkyLook LookOf(std::string_view map_name) const;

  [[nodiscard]] std::size_t map_count() const noexcept { return by_map_.size(); }

 private:
  std::unordered_map<std::string, MapSkyLook> by_map_;
};

// CE QUE LE CLIENT PEINT AU-DELÀ DES BORDS D'UNE CARTE QUE LA TABLE NE CITE
// PAS : du noir opaque. Il ne se voit pas autour d'une carte close, et il fond
// ce que certaines cartes d'intérieur posent hors du champ — des cubes noirs,
// qui découperaient leurs formes dans tout autre fond.
inline constexpr std::array<float, 4> kUnlistedMapClearColor = {0.0F, 0.0F, 0.0F, 1.0F};

// La couleur qui efface l'écran sous une carte : celle que la table déclare,
// rendue opaque, ou `kUnlistedMapClearColor`.
[[nodiscard]] constexpr std::array<float, 4> MapClearColorOf(
    const std::optional<std::array<float, 3>>& declared) noexcept {
  if (!declared.has_value()) {
    return kUnlistedMapClearColor;
  }
  return {(*declared)[0], (*declared)[1], (*declared)[2], 1.0F};
}

// Ce que la table dit d'une carte, en allant la chercher dans le client. Une
// table absente ou illisible rend une fiche vide, et le journal le dit : une
// carte doit rester affichable sans ciel.
[[nodiscard]] MapSkyLook LoadMapSky(vfs::Vfs& vfs, std::string_view map_name);

}  // namespace ro::render
