#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// La couleur que le client peint AU-DELÀ des bords d'une carte.
//
// Certaines cartes flottent — gonryun est une île dans les airs — et l'on voit
// alors le ciel autour d'elles. Le client le peint avec une couleur propre à
// chaque carte, rangée dans une table Lua compilée que rien d'autre ne donne :
// ni le RSW, ni le GND, ni le GAT n'en parlent.
//
// Une carte que la table ne cite pas s'ouvre sur du NOIR : voir
// `kUnlistedMapClearColor`.

// Où le client range cette table, et sous quel nom.
//
// La clé d'une carte y est son nom de fichier RSW, extension comprise. Chercher
// le nom nu n'y trouverait rien.
inline constexpr std::string_view kMapSkyDataPath =
    "data\\luafiles514\\lua files\\mapskydata\\mapskydata.lub";
inline constexpr std::string_view kMapSkyDataGlobal = "MapSkyData";
inline constexpr std::string_view kBackgroundColorField = "BG_Color";
// Vrai quand le fond de la carte se mêle à son brouillard. Voir
// `MapSkyLook::background_fog`.
inline constexpr std::string_view kBackgroundFogField = "BG_Fog";

// LE CIEL ANIMÉ D'UNE CARTE : les nuages d'un champ, la brume d'une ville
// industrielle, un champ d'étoiles la nuit.
//
// ⚠ CE N'EST PAS UN SYSTÈME À PART : chez le client, chacun de ces ciels est un
// EFFET ordinaire de son moteur d'effets, désigné par un identifiant. La table
// Lua ne donne qu'un numéro de style ; le client le traduit par un switch que
// l'on transcrit ici. Notre client rejoue donc le ciel par le même chemin que les
// effets de compétence, sans une ligne de rendu de plus.
inline constexpr std::string_view kOldCloudEffectField = "Old_Cloud_Effect";
inline constexpr std::string_view kStarEffectField = "Star_Effect";
inline constexpr std::string_view kCloudEffectField = "Cloud_Effect";

// LE STYLE DU CHAMP D'ÉTOILES, celui que `Star_Effect` allume. Le client ne lui
// donne pas de numéro dans la table : il appelle le lanceur avec NEUF en dur.
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
  // La couleur peinte AU-DELÀ des bords. Voir ci-dessus.
  std::optional<std::array<float, 3>> background;
  // Les identifiants d'EFFET à jouer, dans l'ordre où la carte les déclare.
  // Vide pour une carte sans ciel animé, qui est le cas le plus courant.
  std::vector<std::uint32_t> effects;
  // Combien d'entrées de la table NOUVELLE (`Cloud_Effect`) cette carte porte.
  //
  // ⚠ ELLE N'EST PAS PORTÉE, et le compte est là pour que son absence se lise
  // au lieu de se deviner : le client la traite par un second chemin
  // qui bâtit des nuages
  // PARAMÉTRÉS au lieu de jouer un effet tout fait. Vingt et une cartes sur
  // quatre-vingt-cinq en ont.
  std::size_t parameterised_clouds = 0;
  // LE FOND SE MÊLE-T-IL AU BROUILLARD ? Vrai seulement quand la table le dit
  // en toutes lettres ; rien n'en change tant que la carte n'a pas de
  // brouillard allumé. Voir `FoggedClearColor`.
  bool background_fog = false;
};

// La couleur de fond d'une carte, en composantes de zéro à un.
//
// Rend nullopt quand la table est muette sur cette carte. C'est le cas NORMAL :
// la plupart des cartes sont closes, on n'en voit jamais le bord, et elles ne
// figurent pas dans la table.
//
// Prend les GLOBALES déjà lues plutôt qu'un fichier : la recherche est une
// transformation pure, elle s'éprouve donc sans monter de VFS ni écrire sur le
// disque.
[[nodiscard]] std::optional<std::array<float, 3>> SkyBackgroundOf(const formats::LuaTable& globals,
                                                                  std::string_view map_name);

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

// La même, en allant chercher la table dans le client. Rend nullopt de plus
// quand elle est absente ou illisible : une carte doit rester affichable sans
// son ciel.
[[nodiscard]] std::optional<std::array<float, 3>> LoadSkyBackground(vfs::Vfs& vfs,
                                                                    std::string_view map_name);

// Tout ce que la table dit d'une carte : le fond et le ciel animé.
//
// Rend une fiche VIDE quand la carte n'y figure pas — c'est le cas normal, huit
// cartes sur dix n'ont pas de ciel.
[[nodiscard]] MapSkyLook SkyLookOf(const formats::LuaTable& globals, std::string_view map_name);

// La même, en allant chercher la table dans le client. Une table absente ou
// illisible rend une fiche vide : une carte doit rester affichable sans ciel.
[[nodiscard]] MapSkyLook LoadMapSky(vfs::Vfs& vfs, std::string_view map_name);

}  // namespace ro::render
