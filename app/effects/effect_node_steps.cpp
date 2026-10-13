#include "effects/effect_node_steps.h"

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/effect_spawn_request.h"
#include "ro/render/effect_step_blend.h"
#include "ro/render/sprite_renderer.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

// Mêle les cases de la grille de visée, case à case : couleur et coordonnées de
// texture. FAUX, et les cases courantes intactes, quand ce ne sont pas les
// mêmes cases.
bool BlendGroundCells(std::span<const ro::render::EffectGroundCellPaint> previous, float alpha,
                      std::span<ro::render::EffectGroundCellPaint> current) {
  if (previous.size() != current.size()) {
    return false;
  }
  for (std::size_t index = 0; index < current.size(); ++index) {
    if (previous[index].cell_offset_x != current[index].cell_offset_x ||
        previous[index].cell_offset_y != current[index].cell_offset_y) {
      return false;
    }
  }
  for (std::size_t index = 0; index < current.size(); ++index) {
    current[index].color =
        ro::render::BlendEffectTint(previous[index].color, current[index].color, alpha);
    for (std::size_t corner = 0; corner < current[index].texture_uv.size(); ++corner) {
      const glm::vec2 from = previous[index].texture_uv[corner];
      current[index].texture_uv[corner] = from + (current[index].texture_uv[corner] - from) * alpha;
    }
  }
  return true;
}

}  // namespace

std::int32_t EffectNodeSteps::Beat(EffectNodesInPlay& view, std::uint32_t now_ms,
                                   const ro::render::Camera& camera) {
  if (composing_previous_) {
    return 0;
  }
  spans_.clear();
  if (!enabled_ && has_previous_) {
    Forget();
  }
  if (enabled_) {
    IdentifyScreenNodes(view);
  }
  // Le PREMIER passage ne rattrape rien : `beat_ms_` n'est pas encore un
  // instant, et le prendre pour tel ferait vieillir la troupe de tout le temps
  // écoulé depuis le début de la partie.
  // ⚠ LES ANGLES AVANT LE MOUVEMENT. Le mouvement de la cascade les RANGE dans
  // les pistes, et son dessinateur les y relit : les poser après le laisserait
  // dessiner avec ceux de l'image précédente.
  view.pool_.FaceCamera(ro::render::EffectBillboardTurnDegrees(camera),
                        ro::render::EffectBillboardTiltDegrees(camera));
  const std::uint32_t elapsed_ms = view.beat_ms_ == 0 ? 0 : now_ms - view.beat_ms_;
  view.beat_ms_ = now_ms;
  // 🔴 UN SEUL COMPTE DE TICS, ET TIC PAR TIC : le client refait une passe
  // complète par tic dû — le bâtisseur de chaque instance sème, puis ses nœuds
  // sont mis à jour dans ce même passage. Semer tous les tics d'une image avant
  // d'avancer la troupe donnerait au dernier semé autant de mises à jour qu'au
  // premier, et deux comptes séparés feraient naître un nœud avant son passage.
  const std::int32_t ticks_advanced = view.pool_.TakeDueTicks(elapsed_ms);
  const bool single_frame = ticks_advanced == 0;
  std::uint32_t screen_generator = now_ms + 1U;
  for (std::int32_t pass = 0; pass < (single_frame ? 1 : ticks_advanced); ++pass) {
    // LA COPIE SE PREND AVANT LE DERNIER TIC, semis compris : ce qu'il sème
    // n'a pas d'état d'avant, et se montre tel quel.
    if (enabled_ && pass + 1 == ticks_advanced) {
      IdentifyScreenNodes(view);
      previous_pool_ = view.pool_;
      previous_screen_nodes_ = view.screen_nodes_;
      has_previous_ = true;
    }
    view.StepClocks(single_frame);
    view.pool_.Pass(single_frame);
    // ⚠ LES NŒUDS D'ECRAN BATTENT ICI, au meme passage que la troupe et par le
    // meme chemin : les avancer au moment de composer les figerait des qu'une
    // image est sautee, et les ferait courir deux fois si l'on compose deux fois.
    view.ScreenPass(single_frame, screen_generator);
  }
  view.pool_.FinishFrame();
  // LES EFFETS QUE LES NŒUDS DEMANDENT en se peignant, au plus une fois par
  // image et seulement après un tic — voir `TakeSpawnRequests`. Celles qu'on n'a
  // pas emportées tombent : une demande vaut pour l'image qui l'a faite.
  view.spawn_requests_.clear();
  if (ticks_advanced > 0) {
    for (const ro::render::PooledEffectNode& pooled : view.pool_.nodes()) {
      // ⚠ PAS SANS SON DESSIN : c'est en se peignant qu'un nœud demande.
      if (view.pool_.IsShown(pooled)) {
        ro::render::CollectEffectSpawnRequests(pooled, view.spawn_requests_);
      }
    }
  }
  alpha_ = ro::render::EffectStepAlpha(view.pool_.owed_ms());
  return ticks_advanced;
}

EffectNodeSteps::NodeOutputs::NodeOutputs(EffectNodeSteps& steps, const EffectNodesInPlay& view,
                                          const ro::render::PooledEffectNode& node)
    : steps_(steps.enabled_ ? &steps : nullptr), view_(&view) {
  if (steps_ != nullptr) {
    steps_->OpenNode(view, node);
  }
}

EffectNodeSteps::NodeOutputs::~NodeOutputs() {
  if (steps_ != nullptr) {
    steps_->CloseNode(*view_);
  }
}

void EffectNodeSteps::OpenNode(const EffectNodesInPlay& view,
                               const ro::render::PooledEffectNode& node) {
  NodeSpan span;
  span.serial = node.pool_serial;
  span.anchor_ground_position = node.anchor_ground_position;
  span.beats_every_frame = ro::render::EffectBeatsEveryFrame(node.sown_by_effect_id);
  span.first_lot = view.ring_quads_.size();
  span.first_pose = view.placements_.size();
  span.first_cells = view.ground_cell_lots_.size();
  Recording().push_back(span);
}

void EffectNodeSteps::CloseNode(const EffectNodesInPlay& view) {
  NodeSpan& span = Recording().back();
  span.end_lot = view.ring_quads_.size();
  span.end_pose = view.placements_.size();
  span.end_cells = view.ground_cell_lots_.size();
}

void EffectNodeSteps::Smooth(EffectNodesInPlay& view, std::uint32_t now_ms,
                             const ro::render::Camera& camera) {
  if (!enabled_ || composing_previous_ || !has_previous_) {
    return;
  }
  // LA COPIE SE COMPOSE PAR LE MÊME CHEMIN que la troupe, dans des listes à
  // part : c'est la seule façon d'avoir ses quads vus de la caméra de l'image.
  std::vector<ro::render::ComposedActor> composed;
  std::vector<EffectNodesInPlay::ComposedPlacement> placements;
  std::vector<ro::render::DrawnActor> drawn;
  std::vector<EffectNodesInPlay::SpriteNodeBatch> batches;
  std::vector<std::vector<ro::render::EffectWorldQuad>> ring_quads;
  std::vector<ro::render::DrawnWorldQuads> world_quads;
  std::vector<std::vector<ro::render::EffectGroundCellPaint>> cell_lots;
  std::vector<DrawnGroundCells> cells;
  const auto exchange_outputs = [&] {
    view.composed_.swap(composed);
    view.placements_.swap(placements);
    view.drawn_.swap(drawn);
    view.sprite_batches_.swap(batches);
    view.ring_quads_.swap(ring_quads);
    view.world_quads_.swap(world_quads);
    view.ground_cell_lots_.swap(cell_lots);
    view.ground_cells_.swap(cells);
    std::swap(view.pool_, previous_pool_);
  };
  // ⚠ LE DESSIN DE LA COPIE NE LAISSE RIEN : le grésillement tire le même aléa
  // que sans lissage, et les comptes de diagnostic ne comptent qu'une image.
  const std::uint32_t shimmer = view.shimmer_;
  const std::uint32_t silent_frames = view.silent_frames_;
  const std::size_t lots_composes = view.lots_composes_;
  exchange_outputs();
  previous_spans_.clear();
  composing_previous_ = true;
  static_cast<void>(view.Drawn(now_ms, camera));
  composing_previous_ = false;
  exchange_outputs();
  view.shimmer_ = shimmer;
  view.silent_frames_ = silent_frames;
  view.lots_composes_ = lots_composes;

  std::ranges::sort(previous_spans_, {}, &NodeSpan::serial);
  for (const NodeSpan& now : spans_) {
    const auto before =
        std::ranges::lower_bound(previous_spans_, now.serial, {}, &NodeSpan::serial);
    if (now.beats_every_frame || before == previous_spans_.end() || before->serial != now.serial) {
      continue;
    }
    const glm::vec3 shift = now.anchor_ground_position - before->anchor_ground_position;
    if (now.end_lot - now.first_lot == before->end_lot - before->first_lot) {
      for (std::size_t rank = 0; rank < now.end_lot - now.first_lot; ++rank) {
        ro::render::DrawnWorldQuads& lot = view.world_quads_[now.first_lot + rank];
        const ro::render::DrawnWorldQuads& old_lot = world_quads[before->first_lot + rank];
        if (lot.texture_path == old_lot.texture_path &&
            ro::render::BlendEffectWorldQuads(ring_quads[before->first_lot + rank], shift, alpha_,
                                              view.ring_quads_[now.first_lot + rank])) {
          lot.ground_position = ro::render::BlendEffectPoint(old_lot.ground_position, shift,
                                                             lot.ground_position, alpha_);
        }
      }
    }
    if (now.end_pose - now.first_pose == before->end_pose - before->first_pose) {
      for (std::size_t rank = 0; rank < now.end_pose - now.first_pose; ++rank) {
        EffectNodesInPlay::ComposedPlacement& pose = view.placements_[now.first_pose + rank];
        const EffectNodesInPlay::ComposedPlacement& old_pose =
            placements[before->first_pose + rank];
        pose.ground_position = ro::render::BlendEffectPoint(old_pose.ground_position, shift,
                                                            pose.ground_position, alpha_);
        pose.tint = ro::render::BlendEffectTint(old_pose.tint, pose.tint, alpha_);
        pose.scale = ro::render::MixEffectValue(old_pose.scale, pose.scale, alpha_);
        pose.grow_screen_pixels = ro::render::MixEffectValue(old_pose.grow_screen_pixels,
                                                             pose.grow_screen_pixels, alpha_);
      }
    }
    if (now.end_cells - now.first_cells == before->end_cells - before->first_cells) {
      for (std::size_t rank = 0; rank < now.end_cells - now.first_cells; ++rank) {
        DrawnGroundCells& lot = view.ground_cells_[now.first_cells + rank];
        const DrawnGroundCells& old_lot = cells[before->first_cells + rank];
        if (lot.texture_path == old_lot.texture_path &&
            BlendGroundCells(cell_lots[before->first_cells + rank], alpha_,
                             view.ground_cell_lots_[now.first_cells + rank])) {
          lot.ground_position = ro::render::BlendEffectPoint(old_lot.ground_position, shift,
                                                             lot.ground_position, alpha_);
        }
      }
    }
  }
}

void EffectNodeSteps::Forget() {
  previous_pool_ = ro::render::EffectNodePool{};
  previous_screen_nodes_.clear();
  previous_spans_.clear();
  spans_.clear();
  previous_screen_spans_.clear();
  screen_spans_.clear();
  has_previous_ = false;
}

}  // namespace moonclient
