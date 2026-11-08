#include "survivors/arena_decor_veil.h"

namespace moonclient {
namespace {

// LE RANG D'UNE CIBLE dans l'ordre du plafond ; rien pour un corps qui ne se
// regarde pas (un monstre commun, une mort).
[[nodiscard]] std::optional<std::size_t> TargetRankOf(const HordePose& pose,
                                                      const ro::sim::SurvivorsRules& rules) {
  switch (pose.kind) {
    case HordeBodyKind::kHero:
      return 0;
    case HordeBodyKind::kFoe: {
      const ro::sim::FoeRule* foe = ro::sim::FindFoe(rules, pose.look_id);
      if (foe == nullptr || foe->rank == ro::sim::FoeRank::kCommon) return std::nullopt;
      return 1;
    }
    case HordeBodyKind::kPickup:
      return 2;
    case HordeBodyKind::kPlant:
    case HordeBodyKind::kSummon:
      return 3;
    case HordeBodyKind::kGem:
      return 4;
    case HordeBodyKind::kStranger:
      return 1;
    case HordeBodyKind::kFalling:
      return std::nullopt;
  }
  return std::nullopt;
}

constexpr std::size_t kTargetRanks = 5;

// LE FONDU D'UNE FENÊTRE NEUVE : de quoi descendre du plein au plancher, une
// image de plus pour la marque qui tombe.
constexpr float kVeilSettleMs =
    ro::render::kVeilFrameMs *
    static_cast<float>((ro::render::kVeilOpaque - ro::render::kVeilFloor) / ro::render::kVeilStep +
                       1);

}  // namespace

void ArenaVeilTargets(std::span<const HordePose> poses, const ro::sim::SurvivorsRules& rules,
                      const std::function<glm::vec3(glm::vec2)>& ground_of,
                      std::vector<glm::vec3>& into) {
  into.clear();
  for (std::size_t wanted = 0; wanted < kTargetRanks; ++wanted) {
    for (const HordePose& pose : poses) {
      if (into.size() == kMostArenaVeilTargets) return;
      if (TargetRankOf(pose, rules) == wanted) into.push_back(ground_of(pose.cell));
    }
  }
}

void ArenaDecorVeil::Advance(const ArenaVeilFrame& frame) {
  if (frame.stage == nullptr || frame.camera == nullptr) return;
  const MapStage& stage = *frame.stage;
  const bool fresh = prepared_mount_ != frame.mount;
  if (fresh) {
    veils_.Prepare(stage.scene, stage.gat.has_value() ? &*stage.gat : nullptr);
    prepared_mount_ = frame.mount;
  }
  std::uint32_t elapsed_ms = 0;
  if (last_ms_.has_value() && frame.now_ms > *last_ms_) elapsed_ms = frame.now_ms - *last_ms_;
  last_ms_ = frame.now_ms;

  const ro::render::ClientFrame client = ro::render::ClientFrameOf(stage.scene.gnd);
  segments_.clear();
  for (const glm::vec3& target : frame.targets) {
    segments_.push_back(ro::render::PlayerSightSegment(*frame.camera, frame.aspect_ratio,
                                                       frame.width, frame.height, target, client));
  }
  veils_.Advance(fresh ? kVeilSettleMs : static_cast<float>(elapsed_ms), segments_,
                 ro::render::SightFrustumOf(*frame.camera, frame.aspect_ratio, client));
}

void ArenaDecorVeil::Forget() {
  prepared_mount_.reset();
  last_ms_.reset();
  veils_.Clear();
}

LitDecorVeil ArenaDecorVeil::decor() const noexcept {
  if (!prepared_mount_.has_value()) return {};
  return LitDecorVeil{.opacities = &veils_.opacities(), .veiled = veils_.veiled()};
}

}  // namespace moonclient
