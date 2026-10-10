#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/render/effect_anchored_skirt.h"
#include "ro/render/effect_arc_ribbon.h"
#include "ro/render/effect_cycling_skirt.h"
#include "ro/render/effect_gliding_slash.h"
#include "ro/render/effect_job_aura.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_scrolling_band.h"
#include "ro/render/effect_sphere_shell.h"
#include "ro/render/effect_spiral_fan.h"
#include "ro/render/effect_thunder_freeze.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_typed_ribbon.h"

#include "effects/effect_node_view.h"

// LES NŒUDS QUI NE TIENNENT PAS DANS UN SEUL LOT.
//
// ⚠ CE FICHIER EXISTE AU PLAFOND DE HUIT CENTS LIGNES, et la coupure suit une
// frontière réelle : toutes les figures qui vivent ici partagent une propriété
// que les vingt-cinq autres n'ont pas — leurs quads ne prennent pas tous la
// MÊME image. Le renderer paie un lot par texture, et c'est le client qui en
// décide ainsi.
//
// Elles y arrivent pour trois raisons distinctes :
//
//   - l'ÉTINCELLE change de texture piste par piste, le temps de deux tics ;
//   - la CASCADE, le RUBAN TYPÉ et la COQUILLE peignent des passes
//     qui ne portent pas la même image ;
//   - la TAILLADE fait DÉFILER la sienne sur quatre, et deux pistes retardées
//     différemment n'en sont pas au même cran ;
//   - l'AURA tire une image par PARTICULE, parmi cinq éclairs ;
//   - les deux JUPES font défiler la leur PISTE PAR PISTE, au compteur de
//     chacune.
//
// La composition ordinaire, elle, prend UNE image par nœud et la donne à tous
// ses quads : voir `effect_node_compose.cpp`.

namespace moonclient {
namespace {

// LES DEUX MARQUES QUE LE CARRÉ TEINTÉ LIT, et que la fenêtre doit prendre sur
// la PREMIÈRE piste — voir `effect_tinted_quad.h`.
constexpr std::size_t kMarkOfQuadDrawing = 2;
constexpr std::size_t kMarkOfQuadSubDrawing = 3;

// ⚠ ET LA PREMIÈRE MARQUE EST UN INDEX DE TEXTURE, piste par piste. C'est ainsi
// qu'une étincelle ÉCLATE : elle change de texture, le temps de deux tics.
constexpr std::size_t kMarkOfSparkTexture = 0;

// L'ÉVENTAIL TOURNÉ VERS LA CAMÉRA lit la sienne au même endroit — mais une fois
// pour toutes : ses trois traînées naissent chacune avec son image et n'en
// changent plus.
constexpr std::size_t kMarkOfFanTexture = 0;

}  // namespace

bool EffectNodesInPlay::ComposeNodeInSeveralLots(const ro::render::PooledEffectNode& pooled,
                                                 std::size_t& missing_texture,
                                                 std::size_t& missing_quad) {
  // ⚠ L'ÉTINCELLE NE TIENT PAS DANS UN SEUL LOT, et c'est la seule figure dans
  // ce cas : ses quatre pistes ne partagent pas toujours leur texture, puisque
  // chacune éclate à son tour. Un lot par texture, donc — deux au plus, et le
  // client en paie autant.
  if (pooled.shape == ro::render::PooledNodeShape::kRisingSparks) {
    const std::uint8_t mark = pooled.tracks.front().mark[kMarkOfQuadDrawing];
    const std::uint8_t sub_mark = pooled.tracks.front().mark[kMarkOfQuadSubDrawing];
    const std::int16_t tour = pooled.tracks.front().style;
    const std::int16_t tilt = pooled.tracks[1].style;
    for (std::size_t image = 0; image < pooled.resource_paths.size(); ++image) {
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      std::vector<ro::render::EffectWorldQuad> quads;
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        // ⚠ UNE MARQUE HORS DU TABLEAU RETOMBE SUR LA PREMIÈRE IMAGE, et ne
        // fait pas disparaître la piste. Le semis n'autorise l'éclat qu'aux
        // nœuds à deux textures, mais une piste perdue en silence serait la
        // pire des façons de l'apprendre.
        const std::size_t own = track.mark[kMarkOfSparkTexture] < pooled.resource_paths.size()
                                    ? track.mark[kMarkOfSparkTexture]
                                    : 0;
        if (own != image) {
          continue;
        }
        if (const std::optional<ro::render::EffectWorldQuad> world_quad =
                ro::render::ComposeTintedRotatedQuad(track, mark, sub_mark, tour, tilt);
            world_quad.has_value()) {
          quads.push_back(*world_quad);
        }
      }
      if (quads.empty()) {
        ++missing_quad;
        continue;
      }
      ring_quads_.push_back(std::move(quads));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position});
    }
    return true;
  }
  // ⚠ L'ÉVENTAIL TOURNÉ VERS LA CAMÉRA NON PLUS, et pour une troisième raison :
  // ses trois traînées naissent chacune avec SON image et la gardent. Le client
  // en charge trois par nœud et les distribue par la marque zéro.
  if (pooled.shape == ro::render::PooledNodeShape::kCameraFan) {
    const std::int16_t tour = pooled.tracks.front().style;
    const std::int16_t tilt = pooled.tracks[1].style;
    for (std::size_t image = 0; image < pooled.resource_paths.size(); ++image) {
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      std::vector<ro::render::EffectWorldQuad> quads;
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        // ⚠ UNE MARQUE HORS DU TABLEAU RETOMBE SUR LA PREMIÈRE IMAGE : une
        // traînée perdue en silence serait la pire des façons de l'apprendre.
        const std::size_t own = track.mark[kMarkOfFanTexture] < pooled.resource_paths.size()
                                    ? track.mark[kMarkOfFanTexture]
                                    : 0;
        if (own != image) {
          continue;
        }
        for (const ro::render::EffectWorldQuad& quad :
             ro::render::ComposeSpiralFanQuads(track, tour, tilt, pooled.ground_position)) {
          quads.push_back(quad);
        }
      }
      if (quads.empty()) {
        ++missing_quad;
        continue;
      }
      ring_quads_.push_back(std::move(quads));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position});
    }
    return true;
  }
  // ⚠ LA CASCADE NON PLUS NE TIENT PAS DANS UN SEUL LOT, et pour une autre
  // raison que l'étincelle : ses cinq segments prennent leur image dans un jeu
  // de TROIS qui tourne, et deux segments voisins n'ont donc pas la même. Un
  // lot par image, comme le client en paie autant.
  // ⚠ LE RUBAN TYPÉ NON PLUS : ses deux passes ne partagent pas leur image
  // quand le type ralentit la seconde, et le client paie bien deux lots.
  if (pooled.shape == ro::render::PooledNodeShape::kTypedRibbon) {
    std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
    for (const ro::render::EffectTrack& track : pooled.tracks) {
      for (const ro::render::EffectWorldQuad& quad : ro::render::ComposeTypedRibbonQuads(
               track, pooled.ground_position, pooled.resource_paths.size())) {
        const std::size_t own = quad.texture < per_image.size() ? quad.texture : std::size_t{0};
        if (!per_image.empty()) {
          per_image[own].push_back(quad);
        }
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      if (per_image[image].empty()) {
        continue;
      }
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      ring_quads_.push_back(std::move(per_image[image]));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position});
    }
    return true;
  }
  // ⚠ LE VOILE NE SORT PAS D'ICI : ce client le peint à l'écran, par la
  // quatrième voie, qui connaît la taille de la fenêtre. Voir `ScreenQuads` et
  // `ro::render::ComposeScreenDarkVeilQuads`. Il compte pourtant comme un lot
  // composé : une troupe qui ne porte qu'un voile ne se tait pas.
  if (pooled.shape == ro::render::PooledNodeShape::kDarkVeil ||
      pooled.shape == ro::render::PooledNodeShape::kGridPlane) {
    ++lots_composes_;
    return true;
  }
  // ⚠ LA COQUILLE SPHÉRIQUE NON PLUS NE TIENT PAS DANS UN SEUL LOT : ses deux
  // coquilles intérieures prennent la première image, l'extérieure la seconde,
  // et le client paie bien deux lots. Les rassembler peindrait quinze
  // panneaux avec l'image des trente autres.
  // ⚠ LA TAILLADE NON PLUS NE TIENT PAS DANS UN SEUL LOT quand elle défile :
  // son index d'image tourne sur QUATRE, et deux pistes retardées
  // différemment n'en sont pas au même cran. Un lot par image, donc — un
  // seul quand le bâtisseur n'en a chargé qu'une.
  if (pooled.shape == ro::render::PooledNodeShape::kGlidingSlash ||
      pooled.shape == ro::render::PooledNodeShape::kAimedGlidingSlash) {
    std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
    for (const ro::render::EffectTrack& track : pooled.tracks) {
      const std::optional<ro::render::EffectWorldQuad> world_quad =
          ro::render::ComposeGlidingSlashQuad(track);
      if (!world_quad.has_value()) {
        continue;
      }
      const std::size_t own =
          world_quad->texture < per_image.size() ? world_quad->texture : std::size_t{0};
      if (!per_image.empty()) {
        per_image[own].push_back(*world_quad);
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      if (per_image[image].empty()) {
        continue;
      }
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      ring_quads_.push_back(std::move(per_image[image]));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position});
    }
    return true;
  }
  if (pooled.shape == ro::render::PooledNodeShape::kSphereShell) {
    std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
    // ⚠ UNE SEULE PISTE SE DESSINE, mais on les parcourt toutes les quatre :
    // le semis n'en allume qu'une, et une piste éteinte ne rend rien.
    for (const ro::render::EffectTrack& track : pooled.tracks) {
      for (const ro::render::EffectWorldQuad& quad :
           ro::render::ComposeSphereShellQuads(track, pooled.ground_position)) {
        const std::size_t own = quad.texture < per_image.size() ? quad.texture : std::size_t{0};
        if (!per_image.empty()) {
          per_image[own].push_back(quad);
        }
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      if (per_image[image].empty()) {
        continue;
      }
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      ring_quads_.push_back(std::move(per_image[image]));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position});
    }
    return true;
  }
  // ⚠ LE CŒUR QUI ERRE NON PLUS NE TIENT PAS DANS UN SEUL LOT : ses deux carrés
  // prennent `thunder_center.bmp` (image zéro), ses éclats `freeze_a.bmp`
  // (image un).
  if (pooled.shape == ro::render::PooledNodeShape::kThunderFreeze) {
    std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
    for (const ro::render::EffectWorldQuad& quad :
         ro::render::ComposeThunderFreezeQuads(pooled.tracks, pooled.ground_position)) {
      if (quad.texture < per_image.size()) {
        per_image[quad.texture].push_back(quad);
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      if (per_image[image].empty()) {
        continue;
      }
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      ring_quads_.push_back(std::move(per_image[image]));
      // Le mot de rendu du nœud (5) : le mélange propre de chaque quad.
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position, pooled.node.render_word});
    }
    return true;
  }
  // ⚠ LE TOURBILLON NON PLUS : il FAIT DÉFILER huit images, et la marque zéro de
  // sa piste dit laquelle peindre. La figure est celle de l'arc de ruban ; les
  // deux angles se relisent dans les pistes, où son mouvement les a rangés.
  if (pooled.shape == ro::render::PooledNodeShape::kTetraVortex) {
    const std::int16_t tour = pooled.tracks.front().style;
    const std::int16_t tilt = pooled.tracks[1].style;
    std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
    for (const ro::render::EffectTrack& track : pooled.tracks) {
      for (const ro::render::EffectWorldQuad& quad :
           ro::render::ComposeArcRibbonQuads(track, tour, tilt, pooled.ground_position)) {
        if (quad.texture < per_image.size()) {
          per_image[quad.texture].push_back(quad);
        }
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      if (per_image[image].empty()) {
        continue;
      }
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      ring_quads_.push_back(std::move(per_image[image]));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position, pooled.node.render_word});
    }
    return true;
  }
  if (pooled.shape == ro::render::PooledNodeShape::kScrollingBand) {
    std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
    for (const ro::render::EffectTrack& track : pooled.tracks) {
      for (const ro::render::EffectWorldQuad& quad :
           ro::render::ComposeScrollingBandQuads(track, pooled.ground_position)) {
        // ⚠ UN INDEX HORS DU JEU RETOMBE SUR LA PREMIÈRE IMAGE plutôt que de
        // perdre le quad : une bande qui s'éclaircit d'un cinquième serait la
        // pire des façons d'apprendre que le jeu a changé de taille.
        const std::size_t own = quad.texture < per_image.size() ? quad.texture : std::size_t{0};
        if (!per_image.empty()) {
          per_image[own].push_back(quad);
        }
      }
    }
    for (std::size_t image = 0; image < per_image.size(); ++image) {
      if (per_image[image].empty()) {
        continue;
      }
      const auto texture = textures_.find(pooled.resource_paths[image]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++missing_texture;
        continue;
      }
      ring_quads_.push_back(std::move(per_image[image]));
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position});
    }
    return true;
  }
  // 🔴 L'AURA ET LES DEUX JUPES NON PLUS : chaque quad porte son image. L'aura
  // la tire par particule parmi cinq éclairs, dessinés chacun sous une autre
  // inclinaison ; les jupes la font défiler au compteur de chaque piste, et le
  // bâtisseur échelonne ces compteurs. Peintes d'une seule image par nœud,
  // l'aura ne montrait qu'un éclair, droit ou en miroir, et toutes les pistes
  // d'une jupe en étaient au même cran.
  std::vector<ro::render::EffectWorldQuad> quads;
  const std::int16_t tour = pooled.tracks[0].style;
  const std::int16_t tilt = pooled.tracks[1].style;
  switch (pooled.shape) {
    case ro::render::PooledNodeShape::kPlainAura:
      quads =
          ro::render::ComposePlainAuraNodeQuads(pooled.tracks, tour, tilt, pooled.ground_position);
      break;
    case ro::render::PooledNodeShape::kLayeredAura:
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> layers =
            ro::render::ComposeLayeredAuraQuads(track, tour, tilt, pooled.ground_position);
        quads.insert(quads.end(), layers.begin(), layers.end());
      }
      break;
    case ro::render::PooledNodeShape::kCyclingSkirt:
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> skirt = ro::render::ComposeCyclingSkirtQuads(
            track, pooled.trail_variant, pooled.resource_paths.size(), pooled.ground_position);
        quads.insert(quads.end(), skirt.begin(), skirt.end());
      }
      break;
    case ro::render::PooledNodeShape::kAnchoredSkirt: {
      // Un décalage global pris dans la PREMIÈRE piste — jamais dans celle
      // qu'on dessine.
      const glm::vec2 drift = ro::render::EffectAnchoredSkirtDrift(pooled.tracks);
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> bands =
            ro::render::ComposeAnchoredSkirtQuads(track, drift, pooled.resource_paths.size(),
                                                  pooled.ground_position);
        quads.insert(quads.end(), bands.begin(), bands.end());
      }
      break;
    }
    default:
      return false;
  }
  if (quads.empty()) {
    ++missing_quad;
    return true;
  }
  std::vector<std::vector<ro::render::EffectWorldQuad>> per_image(pooled.resource_paths.size());
  for (const ro::render::EffectWorldQuad& quad : quads) {
    if (quad.texture < per_image.size()) {
      per_image[quad.texture].push_back(quad);
    }
  }
  for (std::size_t image = 0; image < per_image.size(); ++image) {
    if (per_image[image].empty()) {
      continue;
    }
    const auto texture = textures_.find(pooled.resource_paths[image]);
    if (texture == textures_.end() || texture->second.empty()) {
      ++missing_texture;
      continue;
    }
    ring_quads_.push_back(std::move(per_image[image]));
    world_quads_.push_back(ro::render::DrawnWorldQuads{
        {}, &texture->first, &texture->second, pooled.ground_position, pooled.node.render_word});
  }
  return true;
}

}  // namespace moonclient
