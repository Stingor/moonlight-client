#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"
#include "ro/render/model_pose.h"
#include "ro/render/second_family_pose.h"

namespace ro::render {

// Maillage d'un modèle du décor, aplati en coordonnées locales au modèle.
//
// Comme pour le terrain, aucune dépendance à OpenGL : c'est une transformation
// de données pure, donc vérifiable par des assertions numériques. C'est là que
// se cachent les pièges d'orientation, et l'expérience du sol a montré qu'ils
// ne se voient pas autrement qu'en comparant deux images.

struct ModelVertex {
  std::array<float, 3> position{};
  std::array<float, 2> texture_uv{};
  std::array<float, 3> normal{};
};

// Un lot de dessin : les sommets partageant une texture sont contigus.
struct ModelSurface {
  // Index dans ModelMesh::textures.
  std::int32_t texture_index = 0;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
  bool two_sided = false;
};

// UNE SUITE DE FACES QUE LE CLIENT DESSINE D'UN TENANT : les faces CONSÉCUTIVES
// d'un nœud qui portent le même emplacement de texture. C'est l'unité qu'il
// trie quand le modèle se voile — voir `model_veil.h` — et ses sommets sont
// contigus : trois par face, dans l'ordre du fichier.
struct ModelPrim {
  std::int32_t texture_index = 0;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
  // Dans un gréement, le rang du lot de sa part où elle tombe : c'est lui qui
  // porte l'animation de texture. Zéro dans un maillage posé.
  std::uint32_t lot = 0;
};

struct ModelMesh {
  std::vector<ModelVertex> vertices;
  std::vector<ModelSurface> surfaces;
  // Chemins des textures, relatifs à data\texture\. Octets CP949 bruts.
  std::vector<std::string> textures;
  std::array<float, 3> bounds_min{};
  std::array<float, 3> bounds_max{};
  // UNE ENTRÉE PAR FACE, dans l'ordre des sommets : vraie pour une face que le
  // fichier déclare visible des deux côtés. Les autres ne se dessinent que
  // tournées vers l'œil.
  std::vector<std::uint8_t> two_sided;
  // Dans l'ordre des nœuds, puis des faces.
  std::vector<ModelPrim> prims;

  [[nodiscard]] bool empty() const noexcept { return vertices.empty(); }
  [[nodiscard]] std::array<float, 3> center() const noexcept;
};

// Aplatit l'arbre de nœuds en un seul maillage, transformations composées.
//
// Le RSM est une hiérarchie : chaque nœud porte sa transformation et celles de
// ses parents s'y ajoutent. On la parcourt une fois, à la pose de l'IMAGE ZÉRO :
// c'est celle où le natif pose tout objet à son chargement, et un objet qui ne
// s'anime pas la garde. Un nœud qui porte des images-clés y prend la première,
// et non sa pose au repos ; une texture animée y prend ses coordonnées de
// l'image zéro.
[[nodiscard]] ModelMesh BuildModelMesh(const formats::RsmModel& model);

// La part d'un nœud dans un modèle animé : ses lots de dessin.
struct ModelRigPart {
  // Index dans les nœuds du gréement, de l'une ou l'autre famille.
  std::size_t node = 0;
  // Lots dans ModelRig::vertices ; leurs textures s'indexent comme celles du
  // maillage statique du même modèle.
  std::vector<ModelSurface> surfaces;
  // Pour chaque lot, l'emplacement de texture du nœud qu'une animation de
  // texture désigne, ou -1.
  std::vector<std::int32_t> node_textures;
  // Les suites de faces du nœud, dans l'ordre du fichier ; leurs sommets sont
  // dans ModelRig::vertices.
  std::vector<ModelPrim> prims;
};

// Un modèle qui BOUGE : ses sommets restent dans le repère de leur nœud, et la
// pose se compose au dessin, image par image.
//
// Ses normales, elles, sont celles de l'image zéro, dans le repère du modèle :
// le natif n'éclaire jamais un nœud selon sa pose animée.
struct ModelRig {
  std::vector<ModelVertex> vertices;
  std::vector<ModelRigPart> parts;
  // Une seule des deux listes est remplie, selon la famille du fichier.
  std::vector<FirstFamilyNode> first_nodes;
  std::vector<SecondFamilyNode> second_nodes;
  std::int32_t animation_length = 0;
  // Images par seconde : la seconde famille seule s'en sert.
  float frame_rate = 0.0F;
  // Une entrée par face de `vertices` : voir ModelMesh::two_sided.
  std::vector<std::uint8_t> two_sided;

  [[nodiscard]] bool empty() const noexcept { return parts.empty(); }
  [[nodiscard]] bool second_family() const noexcept { return !second_nodes.empty(); }
};

// Le gréement d'un modèle dont un nœud bouge, ou un gréement vide. `mesh` est
// son maillage statique, dont il reprend les index de texture.
[[nodiscard]] ModelRig BuildModelRig(const formats::RsmModel& model, const ModelMesh& mesh);

// Les matrices des nœuds d'un gréement à une image, dans l'ordre de ses nœuds.
// La première famille compte en images entières.
[[nodiscard]] std::vector<glm::mat4> PoseRig(const ModelRig& rig, float frame);

// Les grandeurs de texture de chaque lot d'un gréement à une image : part par
// part, lot par lot. L'identité pour un lot dont la texture ne bouge pas.
[[nodiscard]] std::vector<TextureMotion> RigTextureMotions(const ModelRig& rig, float frame);

// Matrice de pose d'une instance sur la carte.
//
// ⚠ Conversion de convention. La référence écrit ses matrices en vecteur-LIGNE
// (`v * M`), où chaque appel PRÉFIXE la nouvelle transformation : l'ordre
// d'écriture y est donc l'inverse de l'ordre d'application. glm travaille en
// vecteur-COLONNE (`M * v`), et le produit s'écrit à l'endroit. Traduire sans
// inverser donne une matrice plausible et un décor éparpillé.
//
// `gnd_width` et `gnd_height` sont les dimensions de la grille de CUBES : la
// position d'un objet est relative au centre de la carte, pas à son coin.
[[nodiscard]] glm::mat4 ModelInstanceMatrix(const formats::RswModel& placement,
                                            std::int32_t gnd_width, std::int32_t gnd_height,
                                            const ModelMesh& mesh, std::int32_t rsm_version);

// La version d'un .rsm dit-elle la seconde famille ?
[[nodiscard]] bool IsSecondFamily(std::int32_t rsm_version);

// LA POSE EST-ELLE EN MIROIR — un nombre impair d'échelles négatives au .rsw ?
// Le client en déduit le côté qu'une face simple montre.
//
// ⚠ ELLE SE LIT SUR LA MATRICE, et le signe à lire dépend de la famille :
// `ModelInstanceMatrix` retourne l'axe Y du modèle une fois de plus pour la
// seconde, si bien qu'une pose ordinaire y a un déterminant NÉGATIF.
[[nodiscard]] bool InstanceMirrored(const glm::mat4& instance, std::int32_t rsm_version);

// LE CÔTÉ QU'UNE FACE SIMPLE MONTRE : le sens où ses sommets doivent tourner à
// l'écran pour qu'elle se dessine.
//
// D'une pose ordinaire de la première famille, le client dessine les faces qui
// tournent dans le sens contraire des aiguilles d'une montre ; de la seconde,
// les autres. Une pose en miroir retourne la règle.
[[nodiscard]] bool ShowsCounterClockwise(const glm::mat4& instance, std::int32_t rsm_version);

// Le côté qu'un sommet porte au dessin : celui de sa face.
enum class FaceSide : std::uint8_t {
  // Une face double : elle se dessine vue de face comme de dos.
  kBoth = 0,
  kCounterClockwise = 1,
  kClockwise = 2,
};

// L'instance tombe-t-elle sur le terrain ?
//
// Les fichiers contiennent réellement des objets posés hors de la carte —
// restes d'édition, decors coupés. Le client les ÉCARTE, et ne pas le faire les
// laisse flotter dans le vide autour du terrain.
[[nodiscard]] bool IsInstanceOnMap(const glm::mat4& instance, std::int32_t gnd_width,
                                   std::int32_t gnd_height);

// Des poses d'un même modèle animé qui JOUENT de la même façon : parties
// ensemble de l'image zéro, elles restent à la même image.
struct ModelPlaybackGroup {
  // Ce que le .rsw demande : kModelPlayOnce ou kModelPlayLooped.
  std::int32_t animation_type = 0;
  float animation_speed = 1.0F;
  std::vector<glm::mat4> transforms;
};

// Un modèle prêt à dessiner, et les poses où il apparaît.
//
// Une carte pose le même arbre des centaines de fois : on téléverse sa
// géométrie UNE fois et on la redessine à chaque pose. Prontera compte 1303
// instances pour 62 modèles distincts — un facteur vingt.
struct ModelInstances {
  ModelMesh mesh;
  // Une image décodée par entrée de ModelMesh::textures, dans le même ordre ;
  // nulle quand le fichier manque ou ne se décode pas.
  //
  // ⚠ PARTAGÉE, PAS RECOPIÉE : deux modèles qui citent la même texture portent
  // la même image. Une ville cite jusqu'à 43 % de ses textures deux fois ou
  // plus — morocc, 256 citations pour 146 fichiers.
  std::vector<std::shared_ptr<const formats::Image>> textures;
  // Les poses FIGÉES, dessinées d'un seul maillage.
  std::vector<glm::mat4> transforms;
  // Vide tant que le modèle n'a rien qui bouge.
  ModelRig rig;
  // Les poses qui s'animent, par façon de jouer. Leur matrice de pose est
  // celle des poses figées : seuls les nœuds bougent.
  std::vector<ModelPlaybackGroup> animated;
  // Version du .rsm d'origine. Elle décide du recentrage à la pose, et la
  // perdre en chemin enterre ou fait flotter tout le décor d'un même modèle.
  std::int32_t rsm_version = 0;
  // Comment le modèle veut être éclairé, tel que son fichier le déclare.
  //
  // Zéro veut dire « pas d'ombrage » : le modèle est peint à pleine lumière,
  // sans que l'orientation de ses faces n'entre en compte. Ses textures sont
  // déjà ombrées. Lui appliquer une incidence l'assombrit d'autant que sa
  // valeur d'ambiante est basse, et une ville entière vire au brun.
  std::int32_t shade_type = 0;
};

// Range une pose parmi les figées ou parmi les animées, et dit si elle s'anime.
//
// Elle s'anime si le .rsw le DEMANDE, si le modèle a un nœud qui tourne, ET si
// son cran n'est pas nul : une horloge de vitesse 0,01 ne bouge jamais chez le
// natif, et la dessiner nœud par nœud ne montrerait rien de plus.
bool AddInstance(ModelInstances& instances, const glm::mat4& instance,
                 const formats::RswModel& placement);

}  // namespace ro::render
