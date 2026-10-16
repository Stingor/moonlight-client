#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Fichier .rsw : la « scène » d'une carte — quels fichiers .gnd et .gat
// l'accompagnent, l'eau, la lumière, le décor et les effets.
//
// L'eau est le seul champ dont dépend la parité de franchissement avec le
// serveur. La lumière, le sol, la liste d'objets et le quadtree servent au
// dessin.
//
// Référence : GRF/FileFormats/RswFormat/ de GRFEditor, et
// grfio_read_rsw_water_level() dans src/common/grfio.cpp de rAthena.

// Sentinelle « cette carte n'a pas d'eau » côté serveur (RSW_NO_WATER, dans
// src/common/grfio.hpp). Elle ne traverse pas notre API — on rend un
// std::optional vide — mais elle est conservée ici parce qu'elle documente la
// plage de valeurs qu'un niveau d'eau ne peut pas prendre.
inline constexpr std::int32_t kRswNoWater = 1000000;

struct RswSceneHeader {
  std::uint8_t major_version = 0;
  std::uint8_t minor_version = 0;
  std::int32_t build_number = 0;
  std::uint8_t render_flag = 1;

  std::string ini_file;
  std::string ground_file;    // le .gnd associé
  std::string altitude_file;  // le .gat associé
  std::string source_file;

  // Version comparable, sous la forme 0xMMmm. On compare des ENTIERS et non un
  // flottant « majeure.mineure » : 1.10 serait alors jugé antérieur à 1.3.
  [[nodiscard]] std::int32_t version() const noexcept {
    return (static_cast<std::int32_t>(major_version) << 8) |
           static_cast<std::int32_t>(minor_version);
  }
};

struct RswWater {
  // Fausse à partir de la version 2.6, où la section a quitté cet emplacement.
  bool present = false;
  float level = 0.0F;
  std::int32_t type = 0;
  float wave_height = 1.0F;
  float wave_speed = 2.0F;
  float wave_pitch = 50.0F;
  std::int32_t texture_cycling = 3;
};

// Éclairage global de la carte, juste après la section eau.
//
// C'est ce qui donne à chaque carte sa teinte : sans lui, un rendu correct par
// ailleurs sort avec un voile qui n'existe pas dans le client.
struct RswLight {
  bool present = false;
  // Direction du soleil, en degrés. La longitude tourne autour de l'axe
  // vertical, la latitude l'incline.
  std::int32_t longitude = 45;
  std::int32_t latitude = 45;
  std::array<float, 3> diffuse{1.0F, 1.0F, 1.0F};
  std::array<float, 3> ambient{1.0F, 1.0F, 1.0F};
  // Présente à partir de la version 1.7. GRFEditor la note comme inutilisée.
  float shadow_intensity = 0.5F;
};

// Emprise du sol, juste après la lumière.
//
// GRFEditor note que ces valeurs sont « complètement fausses » dans les
// fichiers réels — probablement des restes de mémoire d'un champ abandonné. On
// les lit quand même : c'est leur PLACE qui compte, pas leur contenu, et les
// sauter décalerait la liste d'objets qui suit.
struct RswGround {
  bool present = false;
  std::int32_t top = -500;
  std::int32_t bottom = 500;
  std::int32_t left = -500;
  std::int32_t right = 500;
};

// Un modèle 3D posé sur la carte : quel .rsm, et où.
struct RswModel {
  std::string name;
  std::int32_t animation_type = 0;
  float animation_speed = 1.0F;
  std::int32_t block_type = 0;
  // Chemin du .rsm, relatif à data\model\. Octets CP949 bruts, comme tous les
  // noms qu'un format RO cite : à passer à Vfs::ReadRaw, pas à Vfs::Read.
  std::string model_name;
  std::string node_name;
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};

// UN ÉMETTEUR POSÉ SUR LA CARTE : d'où sort une fumée de cheminée, une vapeur,
// une gerbe d'étincelles.
//
// ⚠ SON NUMÉRO N'EST PAS UN IDENTIFIANT D'EFFET. Les 152 objets de prontera
// portent tous le 44, et aucun effet de ce numéro ne dessine quoi que ce soit :
// c'est un RENVOI vers la table d'émetteurs de la carte,
// `data\yaml_files\map_effects\<carte>.yml` — laquelle porte la texture, la
// cadence, la gravité et la durée de vie des particules.
struct RswEffect {
  // Octets CP949 bruts, comme tout nom qu'un format RO cite.
  std::string name;
  std::array<float, 3> position{};
  std::int32_t number = 0;
  // Tous les combien l'émetteur lâche une bouffée.
  float emit_speed = 0.0F;
  std::array<float, 4> param{};
};

// Tous les combien de secondes une source de son se rejoue quand le fichier ne
// le dit pas : les versions antérieures à 2.0 n'écrivent pas ce champ, et le
// client y pose quatre.
inline constexpr float kRswDefaultSoundCycleSeconds = 4.0F;

// UNE SOURCE DE SON POSÉE SUR LA CARTE : l'eau, le vent, les animaux, la foule.
//
// ⚠ LA LARGEUR ET LA HAUTEUR SONT DES ENTIERS, et le client calcule avec eux en
// entiers — jusqu'à une division qui tronque. Les lire en réels changerait le
// point d'où le son part.
struct RswSound {
  // Octets CP949 bruts, comme tout nom qu'un format RO cite.
  std::string name;
  // Le fichier `.wav`, relatif au dossier des sons, en octets CP949 bruts.
  std::string wave_name;
  std::array<float, 3> position{};
  float volume = 0.0F;
  // L'emprise de la source, un rectangle centré sur sa place, en unités.
  std::int32_t width = 0;
  std::int32_t height = 0;
  // Jusqu'où elle s'entend, en unités.
  float range = 0.0F;
  float cycle_seconds = kRswDefaultSoundCycleSeconds;
};

// LE QUADTREE DU FICHIER : 1 365 nœuds, un arbre complet de quatre à six
// niveaux, rangés en profondeur d'abord — un nœud, puis ses quatre enfants
// l'un après l'autre avec leur descendance. Il suit la liste d'objets à partir
// de la version 2.1, et seulement en 2.x.
//
// ⚠ DANS LE REPÈRE DU CLIENT, tel que le fichier l'écrit : l'ordonnée y descend
// et la profondeur y croît avec la rangée.
inline constexpr std::size_t kRswQuadTreeNodeCount = 1365;

struct RswQuadTreeNode {
  // L'ordre du fichier : le coin haut, le coin bas, la demi-taille, le centre.
  std::array<float, 3> max{};
  std::array<float, 3> min{};
  std::array<float, 3> half_size{};
  std::array<float, 3> center{};
};

// Décompte des objets que l'on traverse sans les exploiter encore.
//
// Ils sont lus pour rester aligné — sauter un son décalerait tous les modèles
// suivants — et compter ce que l'on ignore dit tout de suite ce qui manquera
// quand on s'y attellera.
struct RswObjectCounts {
  std::size_t lights = 0;
  std::size_t sounds = 0;
  std::size_t effects = 0;
  std::size_t unknown = 0;
};

class RswScene {
 public:
  [[nodiscard]] static core::Result<RswScene> Parse(std::span<const std::uint8_t> data);

  [[nodiscard]] const RswSceneHeader& header() const noexcept { return header_; }
  [[nodiscard]] const RswWater& water() const noexcept { return water_; }
  [[nodiscard]] const RswLight& light() const noexcept { return light_; }
  [[nodiscard]] const RswGround& ground() const noexcept { return ground_; }
  [[nodiscard]] const std::vector<RswModel>& models() const noexcept { return models_; }
  [[nodiscard]] const std::vector<RswEffect>& effects() const noexcept { return effects_; }
  // Déplace l'objet d'effet de ce rang, dans le repère du fichier. Un rang hors
  // de la liste ne déplace rien.
  void MoveEffect(std::size_t effect_index, const std::array<float, 3>& position) noexcept;
  // Repose le modèle de ce rang : position, rotation et échelle, dans le repère
  // du fichier. Un rang hors de la liste ne repose rien.
  void PlaceModel(std::size_t model_index, const std::array<float, 3>& position,
                  const std::array<float, 3>& rotation, const std::array<float, 3>& scale) noexcept;
  [[nodiscard]] const std::vector<RswSound>& sounds() const noexcept { return sounds_; }
  [[nodiscard]] const RswObjectCounts& object_counts() const noexcept { return object_counts_; }
  // Vide quand le fichier n'en porte pas, ou quand la liste d'objets n'a pas pu
  // se lire jusqu'au bout : il la suit, et on ne saurait plus où il commence.
  [[nodiscard]] const std::vector<RswQuadTreeNode>& quad_tree() const noexcept {
    return quad_tree_;
  }

  // Le niveau d'eau retenu pour la dérivation des cellules serveur, ou vide si
  // la carte est sans eau de ce point de vue.
  //
  // Le niveau est rendu BRUT, sans troncature : c'est DeriveServerCells qui
  // décide de la règle de comparaison (voir WaterLevelRule et docs/adr/0004).
  //
  // La plage de versions 1.4 à 2.5 reproduit grfio_read_rsw_water_level() :
  // en dehors, rAthena refuse le fichier et traite la carte comme sans eau,
  // même si elle en a une. Reproduire ce refus est le but — c'est la grille du
  // serveur qui fait autorité, pas la nôtre.
  [[nodiscard]] std::optional<float> ServerWaterLevel() const noexcept;

 private:
  RswScene() = default;

  RswSceneHeader header_;
  RswWater water_;
  RswLight light_;
  RswGround ground_;
  std::vector<RswModel> models_;
  std::vector<RswEffect> effects_;
  std::vector<RswSound> sounds_;
  RswObjectCounts object_counts_;
  std::vector<RswQuadTreeNode> quad_tree_;
};

}  // namespace ro::formats
