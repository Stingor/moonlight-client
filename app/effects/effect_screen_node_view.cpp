#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_dark_veil.h"
#include "ro/render/effect_grid_plane.h"
#include "ro/render/effect_hit_cross_seed.h"
#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_screen_burst.h"
#include "ro/render/effect_screen_burst_seed.h"
#include "ro/render/effect_screen_node.h"
#include "ro/render/effect_screen_quad.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/render/effect_split_beam.h"

#include "effects/effect_node_view.h"

// LES NŒUDS D'ÉCRAN : ce qui bat et ce qui se montre, pour la QUATRIÈME voie.
//
// Troisième moitié de `EffectNodesInPlay`, et la coupure suit le sens plutôt
// que le plafond de lignes : les sommets d'un nœud d'écran sortent DÉJÀ
// TRANSFORMÉS, en pixels (ADR 0022). Les ranger avec les quads du monde ferait
// chercher une position du monde là où le nœud porte des pixels.
//
// ⚠ CES NŒUDS BATTENT SÉPARÉMENT DE LA TROUPE, et ce n'est pas une commodité :
// ils ont leur propre reste de millisecondes, leur propre tassement des morts
// et leur propre générateur. Les faire battre ensemble donnerait à l'un le
// rythme de l'autre.
namespace moonclient {
namespace {

// COMBIEN DE MISES À JOUR un nœud semé à ce tic recevra avant que son instance
// ne l'emporte. Voir `PooledEffectNode::instance_beats_left`.
[[nodiscard]] std::int32_t InstanceBeatsLeftOf(std::uint32_t effect_id, std::int32_t tick) {
  if (!ro::render::EffectInstanceLifeIsKnown(effect_id)) {
    return -1;
  }
  return std::max(0, ro::render::EffectInstanceLifeFor(effect_id) - 1 - tick);
}

using TextureStore = std::map<std::string, ro::formats::Image, std::less<>>;

// LE CŒUR DU VOILE, ÉTIRÉ POUR REJOINDRE LE NOIR : dérivé une fois de l'image
// lue, et rangé à côté d'elle. Voir `ro::render::DarkVeilCoreJoinedToFrame`.
// Sans l'image lue, rien à dériver : la fin de la table.
[[nodiscard]] TextureStore::iterator JoinedDarkVeilCore(TextureStore& textures,
                                                        const std::string& core_path) {
  const std::string joined_path = core_path + std::string(ro::render::kDarkVeilJoinedCoreSuffix);
  if (const auto found = textures.find(joined_path); found != textures.end()) {
    return found;
  }
  const auto read = textures.find(core_path);
  if (read == textures.end()) {
    return textures.end();
  }
  return textures.emplace(joined_path, ro::render::DarkVeilCoreJoinedToFrame(read->second)).first;
}

}  // namespace

// ── CE QUI SÈME LA VOIE D'ÉCRAN ─────────────────────────────────────────────
//
// ⚠ CES DEUX-LÀ VIVAIENT AVEC LES SEMIS DU MONDE, et ils n'y partageaient
// rien : ni une troupe — les nœuds d'écran ont la leur —, ni un générateur,
// ni un en-tête. Les rendre à leur voie a suivi le sens autant que le plafond
// de lignes.

bool EffectNodesInPlay::PlayScreenBursts(std::uint32_t effect_id, glm::vec3 ground_position,
                                         std::int32_t tick, std::uint32_t owner_actor_id) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(screen_nodes_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectScreenBurstPattern& pattern :
       ro::render::EffectScreenBurstPatternsFor(effect_id)) {
    // ⚠ LE TIC EST L'ÂGE DE L'EFFET, pas celui du nœud, et deux bâtisseurs
    // n'en sèment pas au premier : Signum Crucis attend le trentième. Tout
    // semer au tic zéro le ferait paraître une demi-seconde trop tôt.
    if (pattern.sow_tick != tick) {
      continue;
    }
    const std::string path(pattern.texture);
    if (TextureFor(path).empty()) {
      continue;
    }
    for (const ro::render::EffectScreenNode& node :
         ro::render::SowEffectScreenBurstNodes(pattern, generator)) {
      screen_nodes_.push_back(ScreenNodeInPlay{node,
                                               {path},
                                               ground_position,
                                               false,
                                               InstanceBeatsLeftOf(effect_id, tick),
                                               ro::render::EffectInstanceBeatsEveryFrame(effect_id),
                                               owner_actor_id,
                                               sowing_hat_});
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayLensFlares(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::uint32_t owner_id) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(screen_nodes_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectLensFlarePattern& pattern :
       ro::render::EffectLensFlarePatternsFor(effect_id)) {
    // ⚠ LES DEUX IMAGES SONT EXIGÉES. Elles se distribuent sur les rangs : une
    // seule manquante laisserait un reflet sur deux invisible, ce qui se voit
    // comme une gerbe deux fois trop maigre plutôt que comme une absence.
    const std::string pair(pattern.even_texture);
    const std::string impaire(pattern.odd_texture);
    if (TextureFor(pair).empty() || TextureFor(impaire).empty()) {
      continue;
    }
    for (const ro::render::EffectLensFlareNode& reflet :
         ro::render::SowEffectLensFlareNodes(pattern, generator)) {
      // ⚠ SAUF QUAND LE PATRON DIT QUE LA GERBE RESTE : voir
      // `EffectLensFlarePattern::follows_bearer`.
      screen_nodes_.push_back(ScreenNodeInPlay{reflet.node,
                                               {std::string(reflet.texture)},
                                               ground_position,
                                               false,
                                               InstanceBeatsLeftOf(effect_id, 0),
                                               ro::render::EffectInstanceBeatsEveryFrame(effect_id),
                                               pattern.follows_bearer ? owner_id : 0U,
                                               sowing_hat_});
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayHitCrosses(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::uint32_t owner_id) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(screen_nodes_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectHitCrossPattern& pattern :
       ro::render::EffectHitCrossPatternsFor(effect_id)) {
    const std::string path(pattern.texture);
    if (TextureFor(path).empty()) {
      continue;
    }
    // ⚠ LES DEUX BRANCHES D'UN COUP : leur angle de départ est TIRÉ EN COMMUN, et
    // c'est lui qui les rend perpendiculaires. Semer une branche par appel les
    // ferait tirer chacune le sien.
    for (const ro::render::EffectScreenNode& branche :
         ro::render::SowEffectHitCrossNodes(pattern, generator)) {
      screen_nodes_.push_back(ScreenNodeInPlay{branche,
                                               {path},
                                               ground_position,
                                               false,
                                               InstanceBeatsLeftOf(effect_id, 0),
                                               ro::render::EffectInstanceBeatsEveryFrame(effect_id),
                                               owner_id,
                                               sowing_hat_});
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySplitBeams(std::uint32_t effect_id, glm::vec3 ground_position,
                                       std::uint32_t owner_id) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(screen_nodes_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSplitBeamPattern& pattern :
       ro::render::EffectSplitBeamPatternsFor(effect_id)) {
    const std::string path(pattern.texture);
    if (TextureFor(path).empty()) {
      continue;
    }
    // ⚠ L'ÉTOILE ENTIÈRE D'UN COUP : le rang d'un éclat décide de son angle de
    // base ET de sa taille — un sur trois est long —, si bien qu'un semis qui
    // rendrait un nœud à la fois perdrait les deux.
    for (const ro::render::EffectScreenNode& shard :
         ro::render::SowEffectSplitBeamNodes(pattern, generator)) {
      screen_nodes_.push_back(ScreenNodeInPlay{shard,
                                               {path},
                                               ground_position,
                                               false,
                                               InstanceBeatsLeftOf(effect_id, 0),
                                               ro::render::EffectInstanceBeatsEveryFrame(effect_id),
                                               owner_id,
                                               sowing_hat_});
    }
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayScreenNodes(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t tick, std::uint32_t owner_id) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(screen_nodes_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectScreenPattern& pattern :
       ro::render::EffectScreenPatternsFor(effect_id)) {
    // ⚠ LE TIC EST L'ÂGE DE L'EFFET, et la moitié de ces bâtisseurs sème à
    // RÉPÉTITION : la poussière d'EF_ENDURE naît à chacun des cinquante premiers
    // tics, le sable d'EF_SAND un tic sur deux. N'en semer qu'un au tic zéro
    // montrait un grain là où le client fait une gerbe.
    if (tick > pattern.birth_last_tick || pattern.birth_period <= 0 ||
        tick % pattern.birth_period != 0) {
      continue;
    }
    std::vector<std::string> paths = ro::render::EffectScreenTexturePaths(pattern);
    // ⚠ TOUTES LES IMAGES SONT EXIGEES, pas seulement la premiere : un jeu
    // incomplet ferait clignoter le noeud sur les images qui manquent.
    const bool all = !paths.empty() && std::ranges::none_of(paths, [this](const std::string& path) {
      return TextureFor(path).empty();
    });
    if (!all) {
      continue;
    }
    ro::render::EffectScreenNode node = ro::render::SowEffectScreenNode(pattern, generator);
    // LA DEMI-TAILLE LUE SUR L'IMAGE, quand le patron le dit : voir
    // `EffectScreenPattern::half_size_per_texture_pixel`.
    if (pattern.half_size_per_texture_pixel > 0.0F) {
      const ro::formats::Image& image = TextureFor(paths.front());
      node.half_width = static_cast<float>(image.width) * pattern.half_size_per_texture_pixel;
      node.half_height = static_cast<float>(image.height) * pattern.half_size_per_texture_pixel;
    }
    screen_nodes_.push_back(ScreenNodeInPlay{
        node, std::move(paths), ground_position, ro::render::EffectBeatsEveryFrame(effect_id),
        InstanceBeatsLeftOf(effect_id, tick), ro::render::EffectInstanceBeatsEveryFrame(effect_id),
        owner_id, sowing_hat_});
    sown = true;
  }
  return sown;
}

void EffectNodesInPlay::ScreenPass(bool single_frame, std::uint32_t& generator) {
  // UNE IMAGE SANS TIC DU fait battre les noeuds qui battent a chaque image, et
  // eux seuls. Voir `EffectBeatsEveryFrame`.
  {
    // Les morts se retirent en TASSANT plutot qu'en effacant un a un, comme
    // dans la troupe : une grappe qui meurt ferait glisser tout ce qui suit.
    std::size_t gardes = 0;
    for (std::size_t index = 0; index < screen_nodes_.size(); ++index) {
      // L'INSTANCE QUI L'A SEMÉ VIEILLIT, et l'emporte en mourant. Une image
      // sans tic ne fait vieillir que celles qui battent à chaque image.
      std::int32_t& remainder = screen_nodes_[index].instance_beats_left;
      if (remainder >= 0 && (!single_frame || screen_nodes_[index].instance_beats_every_frame) &&
          --remainder < 0) {
        continue;
      }
      const bool bat = !single_frame || screen_nodes_[index].beats_every_frame;
      // CE QUI RESTE À L'INSTANCE, TEL QUE LE NŒUD LE LIT : un passage de plus
      // que ce qu'il recevra encore de mises à jour, son âge ayant déjà crû.
      // Voir `EffectScreenNode::owner_remaining_ticks`.
      screen_nodes_[index].node.owner_remaining_ticks = remainder >= 0 ? remainder + 1 : 0;
      if (bat && !ro::render::AdvanceEffectScreenNode(screen_nodes_[index].node, generator)) {
        continue;
      }
      if (gardes != index) {
        screen_nodes_[gardes] = std::move(screen_nodes_[index]);
      }
      ++gardes;
    }
    screen_nodes_.resize(gardes);
  }
}

std::span<const ro::render::DrawnScreenQuads> EffectNodesInPlay::ScreenQuads(
    const ro::render::Camera& camera, float viewport_width, float viewport_height, bool dark_veil) {
  screen_quads_.clear();
  screen_quad_lots_.clear();
  if (viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return screen_quads_;
  }
  // LE VOILE DE TÉNÈBRES, SEUL DANS SA PASSE : ce client le peint à l'écran,
  // centré sur la place d'écran de son porteur, par-dessus tout ce qui
  // appartient au monde. Un lot par image — le cœur, puis le cadre.
  //
  // ⚠ ET LA BRUME DU POISON AVEC LUI, pour la même raison : voir
  // `ro::render::ComposeScreenGridPlaneQuads`. Une image, un lot.
  for (const ro::render::PooledEffectNode& pooled : pool_.nodes()) {
    if (!dark_veil) {
      break;
    }
    if (!pool_.IsShown(pooled)) {
      continue;
    }
    // CE QUE LE NŒUD COMPOSE SE RETIENT pour le lissage : voir `EffectNodeSteps`.
    const EffectNodeSteps::ScreenOutputs outputs_of_node(steps_, *this, pooled);
    const std::optional<glm::vec2> bearer = ro::render::EffectScreenPositionOf(
        camera, pooled.ground_position, viewport_width, viewport_height);
    if (pooled.shape == ro::render::PooledNodeShape::kGridPlane) {
      std::vector<ro::render::EffectScreenQuad> plane_quads;
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectScreenQuad> tiles =
            ro::render::ComposeScreenGridPlaneQuads(track, bearer, viewport_width, viewport_height);
        plane_quads.insert(plane_quads.end(), tiles.begin(), tiles.end());
      }
      const auto texture = pooled.resource_paths.empty()
                               ? textures_.end()
                               : textures_.find(pooled.resource_paths.front());
      if (plane_quads.empty() || texture == textures_.end() || texture->second.empty()) {
        continue;
      }
      screen_quad_lots_.push_back(std::move(plane_quads));
      screen_quads_.push_back(ro::render::DrawnScreenQuads{{}, &texture->first, &texture->second});
      continue;
    }
    if (pooled.shape != ro::render::PooledNodeShape::kDarkVeil) {
      continue;
    }
    std::vector<std::vector<ro::render::EffectScreenQuad>> per_image(pooled.resource_paths.size());
    for (const ro::render::EffectTrack& track : pooled.tracks) {
      for (const ro::render::EffectScreenQuad& quad :
           ro::render::ComposeScreenDarkVeilQuads(track, bearer, viewport_width, viewport_height)) {
        if (quad.texture < per_image.size()) {
          per_image[quad.texture].push_back(quad);
        }
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      const auto texture = image == ro::render::kDarkVeilCoreTexture
                               ? JoinedDarkVeilCore(textures_, pooled.resource_paths[image])
                               : textures_.find(pooled.resource_paths[image]);
      if (per_image[image].empty() || texture == textures_.end() || texture->second.empty()) {
        continue;
      }
      screen_quad_lots_.push_back(std::move(per_image[image]));
      screen_quads_.push_back(ro::render::DrawnScreenQuads{{}, &texture->first, &texture->second});
    }
  }
  for (const ScreenNodeInPlay& live : screen_nodes_) {
    if (dark_veil) {
      break;
    }
    // LA PORTE DE DESSIN, sur le porteur : voir `EffectDrawGate`.
    if (!draw_gate_.Draws(live.owner_actor_id, live.from_hat)) {
      continue;
    }
    // ⚠ ET UN NŒUD SEMÉ À LA NAISSANCE DE SON EFFET, AVANT TOUT PASSAGE, n'existe
    // pas encore chez le client — voir `ro::render::EffectNodePool::IsShown`.
    if (live.node.age_ticks == 0 && !live.instance_beats_every_frame) {
      continue;
    }
    const EffectNodeSteps::ScreenOutputs outputs_of_node(steps_, *this, live);
    // ⚠ L'INDEX DE VUE CHOISIT L'IMAGE, et il TOURNE. Il deborde quand le noeud
    // vit plus longtemps que son jeu d'images ; le ramener dans ses bornes est
    // plus sur que de compter sur l'horloge.
    const std::size_t count = live.texture_paths.size();
    if (count == 0) {
      continue;
    }
    const std::size_t rang = static_cast<std::size_t>(std::max(live.node.frame, 0)) % count;
    const auto texture = textures_.find(live.texture_paths[rang]);
    if (texture == textures_.end() || texture->second.empty()) {
      continue;
    }
    // ⚠ LA POSITION DU PORTEUR SE PROJETTE À CHAQUE IMAGE, et un porteur DERRIÈRE
    // LA CAMÉRA n'en a aucune : la fonction rend alors un vide, et le nœud
    // s'écarte. Voir `EffectScreenPositionOf`.
    const std::optional<glm::vec2> bearer = ro::render::EffectScreenPositionOf(
        camera, live.owner_position, viewport_width, viewport_height);
    if (!bearer.has_value()) {
      continue;
    }
    // LA FORME CHOISIT LA FIGURE, comme dans la troupe du monde.
    std::vector<ro::render::EffectScreenQuad> lot;
    switch (live.node.shape) {
      case ro::render::EffectScreenShape::kQuad:
        lot = ro::render::ComposeScreenQuad(
            ro::render::EffectScreenQuadRequestOf(live.node, *bearer));
        break;
      case ro::render::EffectScreenShape::kRing:
        lot = ro::render::ComposeScreenRingQuads(live.node, *bearer);
        break;
      case ro::render::EffectScreenShape::kWedge:
        // ⚠ UN SEUL TRIANGLE, et c'est pourquoi il ne rend pas un lot : le
        // client en soumet un par nœud, et ce sont les VINGT nœuds d'un semis
        // qui font la gerbe.
        lot.push_back(ro::render::ComposeScreenWedgeQuad(live.node, *bearer));
        break;
      case ro::render::EffectScreenShape::kSplitBeam:
        // ⚠ DEUX TRIANGLES PAR NŒUD, et ils partagent leur arête : le client les
        // soumet l'un après l'autre. N'en rendre qu'un donnerait une demi-pointe.
        lot = ro::render::ComposeScreenSplitBeamQuads(live.node, *bearer);
        break;
    }
    if (lot.empty()) {
      continue;
    }
    screen_quad_lots_.push_back(std::move(lot));
    screen_quads_.push_back(ro::render::DrawnScreenQuads{{}, &texture->first, &texture->second});
  }
  // LE LISSAGE MÊLE avant que les lots ne se recollent : voir `EffectNodeSteps`.
  steps_.SmoothScreen(*this, camera, viewport_width, viewport_height, dark_veil);
  // Les span se recollent APRÈS que la liste a fini de grandir : les prendre au
  // fil des ajouts les laisserait pendre à la première réallocation.
  for (std::size_t index = 0; index < screen_quads_.size(); ++index) {
    screen_quads_[index].quads = screen_quad_lots_[index];
  }
  return screen_quads_;
}

}  // namespace moonclient
