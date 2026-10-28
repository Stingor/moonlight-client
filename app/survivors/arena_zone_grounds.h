#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/world.h"

#include "effects/effect_view.h"
#include "survivors/arena_effect_plan.h"

namespace moonclient {

// UN SEMIS REFUSÉ — la troupe des nœuds pleine — SE RETENTE APRÈS CE DÉLAI, et
// non à chaque image : une grosse vague ne doit pas faire frapper huit piliers
// à la porte soixante fois par seconde.
inline constexpr std::uint32_t kZoneGroundRetryMs = 250;

// LES MAINS DU GARDIEN, ce qu'il demande au moteur d'effets sans le connaître :
// c'est ce qui le rend vérifiable sans GPU.
struct ArenaZoneGroundHands {
  // Vrai si l'effet au sol porte encore un nœud vivant sous son acteur.
  std::function<bool(const ArenaZoneGround&)> carries;
  // Pose l'acteur et sème l'effet au sol ; `first` vaut vrai tant qu'aucun
  // semis n'a pris, et lui seul s'annonce. Vrai si le semis a pris.
  std::function<bool(const ArenaZoneGround&, bool first)> sow;
  // Éteint l'effet au sol et retire l'acteur qui le porte.
  std::function<void(const ArenaZoneGround&)> put_out;
};

// LE GARDIEN DES EFFETS AU SOL DES ZONES : il allume celui d'une zone neuve, ne
// le resème que quand tout ce qu'il avait semé est mort de sa propre vie —
// un visuel permanent sous une unité ne se relance pas —, et l'éteint, avec
// l'acteur qui le porte, quand la zone sort du plan : expirée, au bout de ses
// coups, ou cachée par les réglages.
class ArenaZoneGroundKeeper {
 public:
  // UNE IMAGE : `planned` est le plan du moment (`PlanArenaZoneGrounds`).
  void Keep(std::span<const ArenaZoneGround> planned, std::uint32_t now_ms,
            const ArenaZoneGroundHands& hands);
  // Éteint ceux que `switches` ne montre plus, à la bascule même.
  void PutOutHidden(ArenaEffectSwitches switches, const ArenaZoneGroundHands& hands);
  // Éteint tout : « Rejouer ».
  void Forget(const ArenaZoneGroundHands& hands);

  // Les zones dont l'effet au sol est allumé.
  [[nodiscard]] std::size_t size() const noexcept { return lit_.size(); }
  // Les semis qui ont pris depuis le dernier `Forget`.
  [[nodiscard]] std::size_t sown_total() const noexcept { return sown_total_; }

 private:
  struct Lit {
    ArenaZoneGround ground;
    // Faux tant qu'aucun semis n'a pris.
    bool sown_once = false;
    // Avant cette heure, un semis refusé ne se retente pas.
    std::uint32_t retry_ms = 0;
  };
  void Sow(Lit& lit, std::uint32_t now_ms, const ArenaZoneGroundHands& hands);

  std::vector<Lit> lit_;
  std::size_t sown_total_ = 0;
};

// LES MAINS QUE L'ARÈNE PRÊTE AU GARDIEN, sur ses vraies pièces : `place` pose
// l'acteur du monde privé au point de la zone, `ground_of` dit le sol sous un
// point, et l'effet se sème à `now_ms` sous cet acteur, sans aura ni vie
// imposée — le patron du visuel décide. L'extinction arrête l'effet ET retire
// l'acteur de `world`. `effects` et `world` doivent survivre aux mains.
[[nodiscard]] ArenaZoneGroundHands ArenaZoneGroundHandsOn(
    EffectsInPlay& effects, ro::sim::World& world,
    std::function<void(const ArenaZoneGround&)> place,
    std::function<glm::vec3(ro::sim::FieldPoint)> ground_of, std::uint32_t now_ms);

}  // namespace moonclient
