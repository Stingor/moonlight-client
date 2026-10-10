#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LES EFFETS D'UNE CARTE ──────────────────────────────────────────────────
//
// 🔴 LE NUMÉRO D'UN OBJET D'EFFET DU RSW EST UN IDENTIFIANT D'EFFET, et c'est le
// moteur d'effets qui le joue. Le client parcourt les objets dans l'ordre du
// fichier et, pour tout numéro que le gestionnaire d'effets connaît, fait
// naître l'effet à la PLACE de l'objet — sans lui passer ni sa vitesse ni ses
// quatre paramètres. Il charge ENSUITE la table `effecttool\<carte>.lub`, qui
// règle les effets déjà créés.
//
// Trois de ces classes posent le décor des cartes — la fumée de cheminée,
// l'émetteur et l'anneau de vent :
inline constexpr std::int32_t kMapChimneySmokeEffect = 44;
inline constexpr std::int32_t kMapEmitterEffect = 974;
inline constexpr std::int32_t kMapWindEffect = 2343;
// Et quatre autres familles que la même table sait régler. Le client les suit
// comme les deux premières, mais leur réglage n'est pas porté.
inline constexpr std::int32_t kMapMagicFloorEffect = 1025;
inline constexpr std::int32_t kMapEvilsPawEffect = 1039;
inline constexpr std::int32_t kMapAnimatedEmitterEffect = 1073;
inline constexpr std::int32_t kMapEz2StrEffect = 1412;

// Ce numéro désigne-t-il l'une des SEPT classes que le gestionnaire suit ?
//
// 🔴 C'EST LA SEULE FRONTIÈRE QUE LE CLIENT TRACE : un objet dont le numéro est
// suivi devient une instance de sa classe, TOUT LE RESTE devient un semeur.
// Il n'y a pas de troisième cas, et notamment aucun numéro
// écarté.
[[nodiscard]] constexpr bool IsMapEffectClassNumber(std::int32_t number) noexcept {
  return number == kMapChimneySmokeEffect || number == kMapEmitterEffect ||
         number == kMapWindEffect || number == kMapMagicFloorEffect ||
         number == kMapEvilsPawEffect || number == kMapAnimatedEmitterEffect ||
         number == kMapEz2StrEffect;
}

// LE RÉGLAGE D'UN ÉMETTEUR, tel que la table le pose.
//
// 🔴 L'ENTRÉE DE CLÉ k RÈGLE LE k-IÈME OBJET 974 DU RSW, et rien d'autre. Une
// entrée ne s'applique donc jamais à tous les points de la carte — et le champ `pos` de
// la table n'est PAS lu : sa ligne est en commentaire dans le lecteur Lua du
// client. La place est celle de l'objet.
//
// Les valeurs par défaut sont celles du client : c'est ce que garde un objet 974 que la table ne
// règle pas.
//
// ⚠ TOUT EST DANS LE REPÈRE DU CLIENT, dont l'ordonnée DESCEND, et en unités
// PAR SECONDE : le client multiplie chaque grandeur par un pas fixe d'un
// soixantième.
struct MapEmitterSetting {
  std::int32_t max_count = 40;
  // Les deux bornes de la vitesse de départ, tirée axe par axe.
  glm::vec3 first_direction{-2.0F, 2.0F, -2.0F};
  glm::vec3 second_direction{2.0F, -1.0F, 2.0F};
  glm::vec3 gravity{0.7F, -2.0F, 1.0F};
  // La demi-boîte où naît une particule autour du point d'émission.
  glm::vec3 radius{-1.0F, 2.0F, -1.0F};
  // Rouge, vert, bleu, alpha. Bornés à 255 par le HAUT seulement, comme le
  // client ; l'alpha ne sert qu'aux particules éternelles.
  std::array<std::int32_t, 4> color{255, 255, 255, 255};
  // Particules par SECONDE, entre ces deux bornes.
  std::array<float, 2> rate{20.0F, 100.0F};
  std::array<float, 2> size{3.0F, 8.0F};
  // En SECONDES.
  std::array<float, 2> life{5.0F, 10.0F};
  std::array<float, 2> scale{1.0F, 1.0F};
  float speed = 1.0F;
  // Relatif au dossier des textures, tel que la table l'écrit : octets CP949.
  std::string texture = "smoke2.bmp";
  // Les facteurs de mélange, dans la numérotation de Direct3D.
  std::int32_t blend_source = 5;
  std::int32_t blend_destination = 2;
  // `zenable` : faux, la particule se peint par-dessus tout.
  bool depth_tested = true;
  // `billboard_off` : vrai, la particule ne se tourne plus vers l'œil et prend
  // les trois angles de `rotation_degrees`.
  bool flat = false;
  glm::vec3 rotation_degrees{0.0F, 0.0F, 0.0F};
  // `eternity` : ne s'éteint pas tant que le joueur est proche.
  bool eternal = false;
};

// LE RÉGLAGE D'UN ANNEAU DE VENT, tel que la table le pose. Défauts du client.
struct MapWindSetting {
  // 🔴 CETTE TABLE-CI DONNE LA PLACE, contrairement à celle des émetteurs : son
  // lecteur Lua transmet `pos`, qui remplace celle de l'objet.
  std::optional<glm::vec3> position;
  // Combien d'anneaux, de vingt et un segments chacun.
  std::uint16_t rings = 4;
  // 🔴 LA COULEUR DE LA TABLE SE LIT (A, R, V, B) : sa première composante est
  // l'alpha qu'un anneau atteint en respirant, les trois autres sa teinte.
  std::uint8_t alpha_peak = 200;
  std::array<std::uint8_t, 3> rgb{255, 100, 100};
  // `radius` de la table : le rayon du bord intérieur.
  float radius = 12.0F;
  // `thickness` : de combien le bord extérieur s'écarte, en rayon ET en hauteur.
  float thickness = 0.5F;
  // `height` : l'étagement des anneaux, du premier au dernier.
  float height = 50.0F;
  // `speed` : des DEGRÉS par pas.
  float speed = 5.0F;
  // `fullAngle` : l'arc que couvrent les vingt et un segments, en degrés.
  std::uint16_t full_angle = 200;
  // `rotateVector` : la bascule autour de l'abscisse, puis de la profondeur.
  float tilt_x_degrees = 0.0F;
  float tilt_z_degrees = 0.0F;
  std::int32_t blend_source = 5;
  std::int32_t blend_destination = 7;
  std::string texture = "effect\\cloud11.tga";
};

// La table d'une carte, rangée par CLÉ.
struct MapEffectTable {
  // Vrai quand le fichier existe et se lit.
  bool present = false;
  // `_<carte>_effect_version`, zéro quand la globale manque. Sous deux, seul
  // l'ancien lecteur passe, qui ne connaît que les émetteurs ; au-delà de deux,
  // le client ne lit RIEN.
  std::uint32_t version = 0;
  std::map<std::int32_t, MapEmitterSetting> emitters;
  std::map<std::int32_t, MapWindSetting> winds;
  // LES ENTRÉES QUE LE LECTEUR DU CLIENT REFUSERAIT : un champ obligatoire
  // absent. Le client lève alors une erreur Lua qui interrompt TOUTE la lecture,
  // dans un ordre de parcours que nous ne reproduisons pas ; nous gardons les
  // champs lus avant la faute et passons aux entrées suivantes. Compter ces
  // entrées dit si l'écart peut se voir — aucune table de l'installation n'en a.
  std::size_t malformed_emitters = 0;
  std::size_t malformed_winds = 0;
  // Les autres familles que la table déclare, avec leur nombre d'entrées.
  std::map<std::string, std::size_t> other_tables;
};

// Où le client range la table d'une carte.
[[nodiscard]] std::string MapEffectToolPath(std::string_view map_name);

// Le nom d'une globale de la table : `_<carte>_<famille>`, privé de son premier
// `@` — celle de `1@adv` est `_1adv_emitterInfo`.
[[nodiscard]] std::string MapEffectToolGlobal(std::string_view map_name, std::string_view family);

// Lit la table dans les globales déjà décodées.
[[nodiscard]] MapEffectTable ReadMapEffectTable(const formats::LuaTable& globals,
                                                std::string_view map_name);

// Les globales du fichier de la carte, ou rien quand il n'existe pas.
//
// ⚠ `ReadLuaFile` ET NON `ReadLuaChunk` : ces tables sont livrées tantôt
// compilées, tantôt en clair.
[[nodiscard]] std::optional<formats::LuaTable> LoadMapEffectToolGlobals(vfs::Vfs& vfs,
                                                                        std::string_view map_name);

}  // namespace ro::render
