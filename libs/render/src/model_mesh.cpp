#include "ro/render/model_mesh.h"

#include <algorithm>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

#include "ro/render/second_family_pose.h"

namespace ro::render {
namespace {

using formats::RsmMesh;
using formats::RsmModel;

// À partir de cette version, la SECONDE famille : chaque nœud se pose par sa
// matrice du fichier et ses images-clés, et l'objet ne se recentre plus.
constexpr std::int32_t kVersionSecondFamily = 0x0202;

// L'image où le natif pose tout objet à son chargement.
constexpr std::int32_t kRestFrame = 0;

glm::vec3 ToVec3(const std::array<float, 3>& value) { return {value[0], value[1], value[2]}; }

// La matrice 3 × 3 du nœud, promue en 4 × 4 — TRANSPOSÉE au passage.
//
// Le fichier la range pour un produit en vecteur-LIGNE (`v · M`), convention de
// la référence. glm multiplie en vecteur-COLONNE (`M · v`), et le même effet y
// demande la transposée. La recopier telle quelle donne, pour une rotation, la
// rotation INVERSE : les modèles sortent retournés, et rien dans les comptes de
// sommets ou l'emprise ne le signale.
glm::mat4 NodeTransform(const RsmMesh& mesh) {
  glm::mat4 matrix{1.0F};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      matrix[column][row] = mesh.transform[static_cast<std::size_t>(column * 3 + row)];
    }
  }
  return matrix;
}

// Transformation du maillage dans son nœud : matrice 3 × 3, PUIS décalage local.
//
// L'ordre décide de tout. La 3 × 3 porte l'orientation et souvent une échelle ;
// appliquer le décalage avant elle le fait tourner et grandir avec le maillage,
// et le nœud part d'autant plus loin que son échelle est grande. C'est ce qui
// suspendait le feuillage des palmiers loin au-dessus de leur tronc.
//
// La référence l'écrit « TransformationMatrix * Translate(LocalPosition) » en
// vecteur-LIGNE, où le facteur de gauche s'applique en premier. glm multiplie
// en vecteur-COLONNE, où c'est celui de DROITE : les deux facteurs s'y écrivent
// donc dans l'ordre inverse.
glm::mat4 MeshTransform(const RsmMesh& mesh) {
  return glm::translate(glm::mat4{1.0F}, ToVec3(mesh.local_position)) * NodeTransform(mesh);
}

// Normale d'une face, dans l'ordre de lecture de ses sommets.
//
// C'est celui de la référence, et il n'y a rien à inverser ici : ce qui décide
// du côté éclairé est la DIRECTION de la lumière, dont le signe de longitude
// était faux. Retourner la normale pour compenser aurait déplacé la faute sans
// la corriger, et cassé l'accord avec les ombres cuites de la carte.
std::array<float, 3> FaceNormal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
  const glm::vec3 normal = glm::cross(b - a, c - a);
  const float length = glm::length(normal);
  if (length <= 0.0F) {
    // Face dégénérée : une normale nulle rendrait l'éclairage indéfini.
    return {0.0F, 1.0F, 0.0F};
  }
  const glm::vec3 unit = normal / length;
  return {unit.x, unit.y, unit.z};
}

// Normales par SOMMET, moyennées entre les faces qui se touchent.
//
// Un modèle éclairé face par face montre ses facettes : chaque triangle prend
// une teinte franche, et un toit en pente sort deux fois plus sombre que son
// voisin pour trois degrés d'écart. C'est ce qui rendait le décor de gonryun
// terne et charbonneux là où le client le montre lumineux.
//
// Le fichier dit lui-même comment lisser. Chaque face porte un GROUPE DE
// LISSAGE : deux faces qui partagent un sommet ET un groupe appartiennent à la
// même surface, et leurs normales s'additionnent en ce sommet. Deux faces de
// groupes différents restent franches — c'est ainsi qu'une arête vive reste
// vive.
//
// La clé est donc la paire (groupe, sommet), pas le sommet seul. Accumuler par
// sommet seul arrondirait les arêtes que le modeleur a voulues nettes.
std::vector<glm::vec3> SmoothNormals(const RsmMesh& mesh,
                                     const std::vector<glm::vec3>& face_normals) {
  std::map<std::pair<std::int32_t, std::uint16_t>, glm::vec3> sums;
  for (std::size_t index = 0; index < mesh.faces.size(); ++index) {
    const formats::RsmFace& face = mesh.faces[index];
    for (const std::uint16_t vertex : face.vertex_ids) {
      for (const std::int32_t group : face.smooth_group) {
        sums[{group, vertex}] += face_normals[index];
      }
    }
  }

  // Une normale par (face, coin) : deux faces de groupes différents donnent au
  // même sommet deux normales distinctes, et il n'y a donc pas de table par
  // sommet à rendre.
  std::vector<glm::vec3> smoothed(mesh.faces.size() * 3, glm::vec3{0.0F, 1.0F, 0.0F});
  for (std::size_t index = 0; index < mesh.faces.size(); ++index) {
    const formats::RsmFace& face = mesh.faces[index];
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const auto found = sums.find({face.smooth_group[0], face.vertex_ids[corner]});
      const glm::vec3 sum = found == sums.end() ? face_normals[index] : found->second;
      const float length = glm::length(sum);
      smoothed[index * 3 + corner] = length > 0.0F ? sum / length : face_normals[index];
    }
  }
  return smoothed;
}

// Où finit un sommet émis : posé dans le repère du modèle, ou laissé dans celui
// de son nœud pour une pose composée au dessin.
enum class Emitted { kPosed, kInNode };

// La clé d'un lot : le nom de sa texture, et — dans un gréement — l'emplacement
// de texture du NŒUD, celui qu'une animation de texture désigne. Les deux
// restent séparés quand deux emplacements portent le même nom : l'un peut
// glisser, l'autre non. -1 quand on regroupe par nom seul.
using SurfaceKey = std::pair<std::string, std::int32_t>;

// Les faces d'un lot : trois sommets chacune, et si elle se voit des deux côtés.
struct SortedFaces {
  std::vector<ModelVertex> vertices;
  std::vector<std::uint8_t> two_sided;
};
using SortedVertices = std::map<SurfaceKey, SortedFaces>;

// Une suite de faces dessinée d'un tenant, rangée dans son lot : son premier
// sommet y est relatif au début du lot.
struct PendingPrim {
  SurfaceKey key;
  std::uint32_t first_vertex = 0;
  std::uint32_t vertex_count = 0;
};

// Faute d'animation de texture, rien à cuire dans les coordonnées.
constexpr std::span<const formats::RsmTextureAnimation> kNoTextureMotion{};

// Ajoute les faces d'un maillage, groupées par texture.
//
// `local` place le maillage dans son nœud, `pose` place le nœud dans le modèle
// à l'image zéro. Les normales se calculent TOUJOURS sur la pose complète :
// c'est l'éclairage de l'image zéro, même pour un sommet laissé dans son nœud.
//
// `baked_motion` porte les animations de texture du nœud : un sommet posé y
// prend ses coordonnées de l'image zéro. Un sommet laissé dans son nœud garde
// les siennes, que le dessin déplace.
//
// Le type d'ombrage vient du modèle : 2 demande des normales lissées, tout le
// reste se contente de celle de la face.
//
// `prims` reçoit les suites de faces CONSÉCUTIVES du nœud qui portent le même
// emplacement de texture : c'est l'unité que le client dessine et trie quand le
// modèle se voile. Une suite ne franchit jamais la limite du nœud.
void EmitMesh(const RsmMesh& mesh, const RsmModel& model, const glm::mat4& local,
              const glm::mat4& pose, Emitted emitted,
              std::span<const formats::RsmTextureAnimation> baked_motion,
              SortedVertices& by_texture, std::vector<PendingPrim>& prims) {
  const std::vector<std::string>& model_textures = model.textures();
  const std::int32_t shade_type = model.shade_type();
  const bool second_family = model.version() >= kVersionSecondFamily;
  const glm::mat4 posed = pose * local;
  const glm::mat4& output = emitted == Emitted::kPosed ? posed : local;
  // Les positions transformées et la normale de chaque face, calculées d'abord :
  // le lissage a besoin de TOUTES les faces avant de pouvoir en moyenner une.
  std::vector<std::array<glm::vec3, 3>> corners(mesh.faces.size());
  std::vector<std::array<glm::vec3, 3>> outputs(mesh.faces.size());
  std::vector<glm::vec3> face_normals(mesh.faces.size(), glm::vec3{0.0F, 1.0F, 0.0F});
  for (std::size_t index = 0; index < mesh.faces.size(); ++index) {
    const formats::RsmFace& face = mesh.faces[index];
    bool valid = true;
    for (const std::uint16_t vertex : face.vertex_ids) {
      valid = valid && vertex < mesh.vertices.size();
    }
    if (!valid) {
      continue;
    }
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const glm::vec4 vertex{ToVec3(mesh.vertices[face.vertex_ids[corner]]), 1.0F};
      corners[index][corner] = glm::vec3(posed * vertex);
      outputs[index][corner] = glm::vec3(output * vertex);
    }
    const std::array<float, 3> normal =
        FaceNormal(corners[index][0], corners[index][1], corners[index][2]);
    face_normals[index] = glm::vec3(normal[0], normal[1], normal[2]);
  }

  constexpr std::int32_t kShadeSmooth = 2;
  const std::vector<glm::vec3> smoothed =
      shade_type == kShadeSmooth ? SmoothNormals(mesh, face_normals) : std::vector<glm::vec3>{};

  std::size_t face_index = 0;
  bool run_open = false;
  std::int32_t run_texture = 0;
  for (const formats::RsmFace& face : mesh.faces) {
    const std::size_t index = face_index++;
    // Un index hors table décrirait un triangle inexistant : on l'ignore
    // plutôt que de lire à côté.
    bool valid = true;
    for (int corner = 0; corner < 3; ++corner) {
      if (face.vertex_ids[static_cast<std::size_t>(corner)] >= mesh.vertices.size() ||
          face.texture_vertex_ids[static_cast<std::size_t>(corner)] >=
              mesh.texture_vertices.size()) {
        valid = false;
      }
    }
    if (!valid) {
      continue;
    }

    // La texture est nommée par le maillage à partir de la 2.3, par le modèle
    // avant. On résout ici pour que le reste du moteur n'ait qu'une table.
    std::string texture;
    const auto slot = static_cast<std::size_t>(face.texture_id);
    if (slot < mesh.textures.size()) {
      texture = mesh.textures[slot];
    } else if (slot < mesh.texture_indices.size()) {
      const auto global = static_cast<std::size_t>(mesh.texture_indices[slot]);
      if (global < model_textures.size()) {
        texture = model_textures[global];
      }
    }

    const bool in_node = emitted == Emitted::kInNode;
    const std::int32_t node_texture = face.texture_id;
    const SurfaceKey key{texture, in_node ? node_texture : -1};
    SortedFaces& faces = by_texture[key];
    std::vector<ModelVertex>& out = faces.vertices;
    constexpr std::uint32_t kVerticesPerFace = 3;
    if (run_open && node_texture == run_texture) {
      prims.back().vertex_count += kVerticesPerFace;
    } else {
      prims.push_back(PendingPrim{key, static_cast<std::uint32_t>(out.size()), kVerticesPerFace});
      run_open = true;
      run_texture = node_texture;
    }
    // DOUBLE, LA FACE DE LA PREMIÈRE FAMILLE L'EST POUR TOUT MOT NON NUL, celle
    // de la seconde pour le seul 1.
    const bool two_sided = second_family ? face.two_sided_word == 1 : face.two_sided_word != 0;
    faces.two_sided.push_back(two_sided ? 1 : 0);
    const TextureMotion motion =
        in_node || baked_motion.empty()
            ? TextureMotion()
            : TextureMotionAt(baked_motion, node_texture, static_cast<float>(kRestFrame));
    for (std::size_t corner = 0; corner < 3; ++corner) {
      const auto& texel = mesh.texture_vertices[face.texture_vertex_ids[corner]];
      const std::array<float, 2> uv = MoveTextureCoordinate(motion, {texel.u, texel.v});
      const glm::vec3& position = outputs[index][corner];
      const glm::vec3 normal =
          smoothed.empty() ? face_normals[index] : smoothed[index * 3 + corner];
      out.push_back(
          ModelVertex{{position.x, position.y, position.z}, uv, {normal.x, normal.y, normal.z}});
    }
  }
}

// Les lots d'un tri par texture, mis bout à bout dans `vertices`, dans la part
// d'un nœud. Les index de texture sont ceux de `textures`, où chaque nom doit
// déjà figurer.
ModelRigPart AppendPart(std::size_t node, const SortedVertices& by_texture,
                        std::span<const PendingPrim> prims,
                        const std::vector<std::string>& textures, ModelRig& rig) {
  ModelRigPart part;
  part.node = node;
  // Où chaque lot commence dans les sommets du gréement, et son rang.
  std::map<SurfaceKey, std::pair<std::uint32_t, std::uint32_t>> lots;
  for (const auto& [key, emitted] : by_texture) {
    const auto found = std::ranges::find(textures, key.first);
    const auto start = static_cast<std::uint32_t>(rig.vertices.size());
    lots[key] = {start, static_cast<std::uint32_t>(part.surfaces.size())};
    part.surfaces.push_back(ModelSurface{static_cast<std::int32_t>(found - textures.begin()), start,
                                         static_cast<std::uint32_t>(emitted.vertices.size()),
                                         false});
    part.node_textures.push_back(key.second);
    rig.vertices.insert(rig.vertices.end(), emitted.vertices.begin(), emitted.vertices.end());
    rig.two_sided.insert(rig.two_sided.end(), emitted.two_sided.begin(), emitted.two_sided.end());
  }
  for (const PendingPrim& prim : prims) {
    const auto& [start, lot] = lots.at(prim.key);
    part.prims.push_back(ModelPrim{.texture_index = part.surfaces[lot].texture_index,
                                   .first_vertex = start + prim.first_vertex,
                                   .vertex_count = prim.vertex_count,
                                   .lot = lot});
  }
  return part;
}

}  // namespace

std::array<float, 3> ModelMesh::center() const noexcept {
  return {(bounds_min[0] + bounds_max[0]) * 0.5F, (bounds_min[1] + bounds_max[1]) * 0.5F,
          (bounds_min[2] + bounds_max[2]) * 0.5F};
}

ModelMesh BuildModelMesh(const RsmModel& model) {
  SortedVertices sorted;
  std::vector<PendingPrim> prims;

  if (model.version() >= kVersionSecondFamily) {
    // Les sommets sont dans le repère de leur nœud, et la pose de l'image zéro
    // les en sort — y compris celle des nœuds sans image-clé, qui vaut la
    // matrice du fichier.
    const std::vector<SecondFamilyNode> nodes = SecondFamilyNodes(model);
    const std::vector<glm::mat4> poses = PoseSecondFamily(nodes, static_cast<float>(kRestFrame));
    for (std::size_t index = 0; index < nodes.size(); ++index) {
      const RsmMesh& mesh = model.meshes()[nodes[index].mesh_index];
      EmitMesh(mesh, model, glm::mat4{1.0F}, poses[index], Emitted::kPosed,
               nodes[index].texture_animations, sorted, prims);
    }
  } else {
    // Chaque nœud accumule la pose de ses parents ; le maillage y ajoute la
    // sienne, qui ne se transmet PAS aux enfants.
    const std::vector<FirstFamilyNode> nodes = FirstFamilyNodes(model);
    const std::vector<glm::mat4> poses = PoseFirstFamily(nodes, kRestFrame);
    for (std::size_t index = 0; index < nodes.size(); ++index) {
      const RsmMesh& mesh = model.meshes()[nodes[index].mesh_index];
      EmitMesh(mesh, model, MeshTransform(mesh), poses[index], Emitted::kPosed, kNoTextureMotion,
               sorted, prims);
    }
  }
  // Par nom seul : c'est ce qui partage une texture téléversée entre nœuds.
  // ⚠ UN LOT PAR NOM, déjà : un sommet posé n'a pas d'emplacement de nœud dans
  // sa clé, si bien que le lot garde ses rangs une fois fusionné.
  std::map<std::string, SortedFaces> by_texture;
  for (auto& [key, faces] : sorted) {
    SortedFaces& merged = by_texture[key.first];
    merged.vertices.insert(merged.vertices.end(), faces.vertices.begin(), faces.vertices.end());
    merged.two_sided.insert(merged.two_sided.end(), faces.two_sided.begin(), faces.two_sided.end());
  }

  ModelMesh mesh;
  std::size_t total = 0;
  for (const auto& [texture, faces] : by_texture) {
    total += faces.vertices.size();
  }
  mesh.vertices.reserve(total);
  mesh.textures.reserve(by_texture.size());
  mesh.surfaces.reserve(by_texture.size());

  // Où chaque lot commence, et le rang de sa texture.
  std::map<std::string, std::pair<std::uint32_t, std::int32_t>> starts;
  for (const auto& [texture, faces] : by_texture) {
    const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto rank = static_cast<std::int32_t>(mesh.textures.size());
    starts[texture] = {start, rank};
    mesh.surfaces.push_back(
        ModelSurface{rank, start, static_cast<std::uint32_t>(faces.vertices.size()), false});
    mesh.textures.push_back(texture);
    mesh.vertices.insert(mesh.vertices.end(), faces.vertices.begin(), faces.vertices.end());
    mesh.two_sided.insert(mesh.two_sided.end(), faces.two_sided.begin(), faces.two_sided.end());
  }
  mesh.prims.reserve(prims.size());
  for (const PendingPrim& prim : prims) {
    const auto& [start, rank] = starts.at(prim.key.first);
    mesh.prims.push_back(ModelPrim{.texture_index = rank,
                                   .first_vertex = start + prim.first_vertex,
                                   .vertex_count = prim.vertex_count});
  }

  constexpr float kFloatMax = std::numeric_limits<float>::max();
  mesh.bounds_min = {kFloatMax, kFloatMax, kFloatMax};
  mesh.bounds_max = {-kFloatMax, -kFloatMax, -kFloatMax};
  for (const ModelVertex& vertex : mesh.vertices) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
      mesh.bounds_min[axis] = std::min(mesh.bounds_min[axis], vertex.position[axis]);
      mesh.bounds_max[axis] = std::max(mesh.bounds_max[axis], vertex.position[axis]);
    }
  }
  if (mesh.vertices.empty()) {
    mesh.bounds_min = {0.0F, 0.0F, 0.0F};
    mesh.bounds_max = {0.0F, 0.0F, 0.0F};
  }
  return mesh;
}

ModelRig BuildModelRig(const RsmModel& model, const ModelMesh& mesh) {
  ModelRig rig;
  const bool second_family = model.version() >= kVersionSecondFamily;
  std::vector<FirstFamilyNode> first_nodes;
  std::vector<SecondFamilyNode> second_nodes;
  std::vector<glm::mat4> poses;
  if (second_family) {
    second_nodes = SecondFamilyNodes(model);
    if (!SecondFamilyMoves(second_nodes)) {
      return rig;
    }
    poses = PoseSecondFamily(second_nodes, static_cast<float>(kRestFrame));
  } else {
    first_nodes = FirstFamilyNodes(model);
    if (!FirstFamilyMoves(first_nodes)) {
      return rig;
    }
    poses = PoseFirstFamily(first_nodes, kRestFrame);
  }
  for (std::size_t index = 0; index < poses.size(); ++index) {
    const std::size_t mesh_index =
        second_family ? second_nodes[index].mesh_index : first_nodes[index].mesh_index;
    const RsmMesh& node_mesh = model.meshes()[mesh_index];
    // Un tri par NŒUD : chaque part se dessine sous la pose de son nœud. Les
    // sommets de la seconde famille sont déjà dans le repère du nœud.
    const glm::mat4 local = second_family ? glm::mat4{1.0F} : MeshTransform(node_mesh);
    SortedVertices by_texture;
    std::vector<PendingPrim> prims;
    EmitMesh(node_mesh, model, local, poses[index], Emitted::kInNode, kNoTextureMotion, by_texture,
             prims);
    if (by_texture.empty()) {
      continue;
    }
    rig.parts.push_back(AppendPart(index, by_texture, prims, mesh.textures, rig));
  }
  rig.first_nodes = std::move(first_nodes);
  rig.second_nodes = std::move(second_nodes);
  rig.animation_length = model.animation_length();
  rig.frame_rate = model.frame_rate();
  return rig;
}

std::vector<glm::mat4> PoseRig(const ModelRig& rig, float frame) {
  // La première famille compte en images entières : la conversion est exacte,
  // ses longueurs restant bien en deçà de ce qu'un flottant compte sans perte.
  return rig.second_family() ? PoseSecondFamily(rig.second_nodes, frame)
                             : PoseFirstFamily(rig.first_nodes, static_cast<std::int32_t>(frame));
}

std::vector<TextureMotion> RigTextureMotions(const ModelRig& rig, float frame) {
  std::vector<TextureMotion> motions;
  for (const ModelRigPart& part : rig.parts) {
    for (const std::int32_t node_texture : part.node_textures) {
      motions.push_back(
          rig.second_family()
              ? TextureMotionAt(rig.second_nodes[part.node].texture_animations, node_texture, frame)
              : TextureMotion());
    }
  }
  return motions;
}

bool AddInstance(ModelInstances& instances, const glm::mat4& instance,
                 const formats::RswModel& placement) {
  // La seconde famille compte en millisecondes : aucune vitesse admise par le
  // .rsw n'y fige un objet, contrairement au cran tronqué de la première.
  const bool requested =
      placement.animation_type == kModelPlayOnce || placement.animation_type == kModelPlayLooped;
  const bool played =
      !instances.rig.empty() &&
      (instances.rig.second_family()
           ? requested
           : !FirstFamilyStaysStill(placement.animation_type, placement.animation_speed,
                                    instances.rig.animation_length));
  if (!played) {
    instances.transforms.push_back(instance);
    return false;
  }
  const auto group =
      std::ranges::find_if(instances.animated, [&placement](const ModelPlaybackGroup& candidate) {
        return candidate.animation_type == placement.animation_type &&
               candidate.animation_speed == placement.animation_speed;
      });
  if (group == instances.animated.end()) {
    instances.animated.push_back({placement.animation_type, placement.animation_speed, {instance}});
  } else {
    group->transforms.push_back(instance);
  }
  return true;
}

bool IsInstanceOnMap(const glm::mat4& instance, std::int32_t gnd_width, std::int32_t gnd_height) {
  // Le facteur ramène les unités monde en cellules GAT : dix unités par cube,
  // deux cellules par cube, soit un cinquième.
  constexpr float kWorldToCell = 0.2F;
  constexpr float kCellsPerCube = 2.0F;

  const float x = instance[3][0] * kWorldToCell;
  const float z = instance[3][2] * kWorldToCell;
  return x >= 0.0F && x <= static_cast<float>(gnd_width) * kCellsPerCube && z >= 0.0F &&
         z <= static_cast<float>(gnd_height) * kCellsPerCube;
}

glm::mat4 ModelInstanceMatrix(const formats::RswModel& placement, std::int32_t gnd_width,
                              std::int32_t gnd_height, const ModelMesh& mesh,
                              std::int32_t rsm_version) {
  // Demi-plateau : la position d'un objet est relative au CENTRE de la carte.
  constexpr float kHalfCube = 5.0F;
  // Retrait constant en Z, repris de la référence.
  constexpr float kDepthBias = 10.0F;

  const glm::vec3 world{
      kHalfCube * static_cast<float>(gnd_width) + placement.position[0], -placement.position[1],
      -kDepthBias - kHalfCube * static_cast<float>(gnd_height) + placement.position[2]};

  // Produit écrit à l'ENDROIT, contrairement à la référence : la dernière
  // transformation appliquée s'écrit à gauche. La toute première appliquée est
  // donc le recentrage du modèle, tout à droite.
  glm::mat4 matrix = glm::scale(glm::mat4{1.0F}, glm::vec3{1.0F, 1.0F, -1.0F});
  matrix = glm::translate(matrix, world);
  matrix = glm::rotate(matrix, -glm::radians(placement.rotation[2]), glm::vec3{0.0F, 0.0F, 1.0F});
  matrix = glm::rotate(matrix, -glm::radians(placement.rotation[0]), glm::vec3{1.0F, 0.0F, 0.0F});
  matrix = glm::rotate(matrix, glm::radians(placement.rotation[1]), glm::vec3{0.0F, 1.0F, 0.0F});
  matrix =
      glm::scale(matrix, glm::vec3{placement.scale[0], -placement.scale[1], placement.scale[2]});

  if (rsm_version < kVersionSecondFamily) {
    // Recentrage en X et Z, et pose par la BASE en Y.
    //
    // Le décalage vertical vaut -bounds_max, pas +bounds_min, parce qu'il
    // s'applique AVANT l'inversion de l'axe Y juste au-dessus. Prendre l'autre
    // borne soulève chaque bâtiment de toute sa hauteur : ils flottent au-dessus
    // du sol, ce qui ne se voit qu'à l'image.
    const std::array<float, 3> center = mesh.center();
    matrix = glm::translate(matrix, glm::vec3{-center[0], -mesh.bounds_max[1], -center[2]});
  } else {
    matrix = glm::scale(matrix, glm::vec3{1.0F, -1.0F, 1.0F});
  }
  return matrix;
}

bool IsSecondFamily(std::int32_t rsm_version) { return rsm_version >= kVersionSecondFamily; }

bool InstanceMirrored(const glm::mat4& instance, std::int32_t rsm_version) {
  const float determinant = glm::determinant(glm::mat3(instance));
  return rsm_version < kVersionSecondFamily ? determinant < 0.0F : determinant > 0.0F;
}

bool ShowsCounterClockwise(const glm::mat4& instance, std::int32_t rsm_version) {
  return IsSecondFamily(rsm_version) == InstanceMirrored(instance, rsm_version);
}

}  // namespace ro::render
