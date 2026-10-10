// LES PARTICULES QUI VIVENT AUTOUR D'UN PORTEUR : les orbes — sphères
// élémentaires (735..738), sphère du sort (228) et brume de Gensou (1005) — en
// orbite lente, et les pétales de Flower Leaf (1013), qui montent du sol en
// dérivant — tant que le porteur est là.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// Le tangage de la caméra que la classe lit, zéro sans vue.
float CameraPitch(const EffectClassTick& tick) {
  return tick.view != nullptr ? tick.view->pitch_degrees : 0.0F;
}

constexpr std::int32_t kDegreesPerTurn = 360;

// ── Les orbes : sphères élémentaires (735..738), sphère du sort (228) et
//    brume de Gensou (1005) ───────────────────────────────────────────────────
//
// DES GROUPES DE DEUX ORBES qui coïncident, répartis à parts égales sur le
// tour et tournant lentement autour du porteur ; la première orbe d'un groupe
// est d'un dixième plus large. Chacune balance sur un angle tiré, d'un pas
// tiré aussi, et gagne un cran d'alpha par tic pendant deux cents tics.
// Panneaux face à la caméra et retournés. La première orbe se mélange par son
// alpha, la seconde s'ajoute.
//
// Le NOMBRE de groupes est celui des sphères, que l'appelant donne ; seule la
// brume le pose elle-même, à dix. Chaque identifiant choisit son image, sa
// taille, sa vitesse, son orbite et sa hauteur.
constexpr std::uint32_t kGensouFogId = 1005;
constexpr std::uint32_t kSpellSphereId = 228;
constexpr std::int32_t kFogGroups = 10;
constexpr std::int32_t kOrbsPerGroup = 2;
constexpr std::int32_t kOrbFadeInUntilAge = 200;
constexpr float kOrbHeadingPerSpeed = 5.0F;
constexpr float kOrbBreathPerScale = 0.1F;
constexpr float kOrbWobble = 0.60000002F;
constexpr std::int32_t kOrbBobSpan = 3;
constexpr float kFullTurn = 360.0F;

// CE QUE CHAQUE IDENTIFIANT DONNE À SES ORBES.
struct OrbLook {
  std::string_view texture;
  float scale = 0.0F;
  float speed = 0.0F;
  float radius = 0.0F;
  float height = 0.0F;
  // Le pas du balancement est tiré ; la brume l'écrase à zéro.
  bool bobs = true;
  // La brume mélange TOUTES ses nappes par leur alpha, les autres la première
  // orbe seule.
  bool every_orb_blends_by_alpha = false;
};

// L'ORBE ORDINAIRE, celle de la sphère du sort, que les éléments
// réhabillent ensuite.
constexpr OrbLook kPlainOrb{"effect\\thunder_center.bmp", 0.80000001F, 0.15000001F, 8.0F, -12.0F};

[[nodiscard]] OrbLook OrbLookFor(std::uint32_t effect_id) {
  constexpr float kBigElement = 2.3F;
  constexpr float kWaterElement = 1.3F;
  OrbLook look = kPlainOrb;
  switch (effect_id) {
    case 735:
      look.texture = "effect\\FireOrb.bmp";
      look.scale = kBigElement;
      break;
    case 736:
      look.texture = "effect\\LightningOrb.bmp";
      look.scale = kBigElement;
      break;
    case 737:
      look.texture = "effect\\WaterOrb.bmp";
      look.scale = kWaterElement;
      break;
    case 738:
      look.texture = "effect\\StoneOrb.bmp";
      look.scale = kBigElement;
      break;
    case kGensouFogId:
      look = OrbLook{"effect\\fog1.tga", 6.0F, 0.050000001F, 4.0F, -8.0F, false, true};
      break;
    default:
      break;
  }
  return look;
}

// LA TEINTE D'UNE ORBE : la sphère du sort peint sa première orbe en bleu et
// la seconde en jaune ; les autres gardent l'image telle quelle.
[[nodiscard]] std::array<std::int32_t, 3> OrbColorFor(std::uint32_t effect_id, std::int32_t rank) {
  constexpr std::array<std::int32_t, 3> kBlue = {0, 0, 255};
  constexpr std::array<std::int32_t, 3> kYellow = {255, 255, 0};
  constexpr std::array<std::int32_t, 3> kUntinted = {255, 255, 255};
  if (effect_id != kSpellSphereId) {
    return kUntinted;
  }
  return rank == 0 ? kBlue : kYellow;
}

// Le nombre de groupes : dix pour la brume, dont le poseur écrit le compte
// lui-même ; celui des sphères pour les autres.
[[nodiscard]] std::int32_t OrbGroupsFor(std::uint32_t effect_id, std::int32_t level) {
  return effect_id == kGensouFogId ? kFogGroups : level;
}

class EnergyOrb final : public EffectClassInstance {
 public:
  explicit EnergyOrb(const EffectClassBirth& birth)
      : EffectClassInstance(birth), look_(OrbLookFor(effect_id_)) {
    const std::int32_t groups = OrbGroupsFor(effect_id_, level_);
    for (std::int32_t group = 0; group < groups; ++group) {
      const std::int32_t bob_angle = std::max(0, Rand() % kDegreesPerTurn);
      const std::int32_t bob_step = std::max(0, Rand() % kOrbBobSpan);
      for (std::int32_t rank = 0; rank < kOrbsPerGroup; ++rank) {
        Orb orb;
        orb.rank = rank;
        orb.heading = static_cast<float>(group * (kDegreesPerTurn / groups));
        orb.bob_angle = bob_angle;
        orb.bob_step = look_.bobs ? static_cast<float>(bob_step) : 0.0F;
        orb.quad.texture = look_.texture;
        orb.quad.alpha = 0;
        orb.quad.color = OrbColorFor(effect_id_, rank);
        if (rank == 0 || look_.every_orb_blends_by_alpha) {
          orb.quad.blend_destination = kBlendInverseSourceAlpha;
        }
        orbs_.push_back(orb);
      }
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Orb& orb : orbs_) {
      if (orb.quad.alpha > 0) {
        SubmitQuad(orb.quad, QuadUv{}, false, view, output);
      }
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (Orb& orb : orbs_) {
      orb.quad.position = position_;
      if (age_ <= kOrbFadeInUntilAge) {
        ++orb.quad.alpha;
      }
      orb.heading += look_.speed * kOrbHeadingPerSpeed;
      if (orb.heading > kFullTurn) {
        orb.heading -= kFullTurn;
      }
      if (orb.quad.alpha <= 0) {
        continue;
      }
      orb.bob_angle = static_cast<std::int32_t>(static_cast<float>(orb.bob_angle) + orb.bob_step) %
                      kDegreesPerTurn;
      const float wobble = EffectClassSinTable(orb.bob_angle);
      float breath = look_.scale * kOrbBreathPerScale * wobble;
      // La première orbe d'un groupe est d'un dixième plus large.
      if (kOrbsPerGroup > 1 && orb.rank == 0) {
        breath += look_.scale * kOrbBreathPerScale;
      }
      const float scale = look_.scale + breath;
      const auto heading = static_cast<std::int32_t>(orb.heading);
      const float reach = wobble * kOrbWobble + look_.radius;
      const glm::vec3 offset{reach * EffectClassCosTable(heading),
                             wobble * kOrbWobble * orb.bob_step + look_.height,
                             reach * EffectClassSinTable(heading)};
      orb.quad.matrix = NativeScale(scale, scale, scale);
      MultiplyRotationX(orb.quad.matrix, Radians(CameraPitch(tick)));
      MultiplyRotationX(orb.quad.matrix, kClientPi);
      MultiplyTranslation(orb.quad.matrix, offset);
    }
  }

 private:
  struct Orb {
    QuadParticle quad;
    std::int32_t rank = 0;
    // L'angle de l'orbite, en degrés, et celui du balancement, entier.
    float heading = 0.0F;
    std::int32_t bob_angle = 0;
    float bob_step = 0.0F;
  };
  OrbLook look_;
  std::vector<Orb> orbs_;
};

// ── Les pétales de Flower Leaf (1013) ────────────────────────────────────────
//
// SEIZE PÉTALES qui naissent sous le sol, à une profondeur tirée jusqu'à cent,
// et montent de quinze centièmes par tic. Sortis de terre, ils dérivent en
// suivant deux caps qui se dirigent chacun vers un cap visé — trois degrés par
// tic vers lui, deux en arrière —, un nouveau cap tiré dès qu'ils s'en
// approchent ; ils sont pleins jusqu'à vingt unités, s'effacent de quatre par
// tic au-delà, et renaissent sous terre passé trente. Panneaux additifs de
// cinquante-cinq centièmes, face à la caméra et retournés, peints deux fois.
constexpr std::string_view kPetalTexture = "effect\\cherry_circle.bmp";
constexpr std::int32_t kPetalRows = 4;
constexpr std::int32_t kPetalColumns = 4;
constexpr float kPetalScale = 0.55F;
constexpr float kPetalRise = 0.15000001F;
constexpr std::int32_t kPetalDepthSpan = 100;
constexpr float kPetalResetHeight = -30.0F;
constexpr float kPetalFadeHeight = -20.0F;
constexpr std::int32_t kPetalFullAlpha = 255;
constexpr std::int32_t kPetalFadeStep = 4;
constexpr std::int32_t kPetalTurnToward = 3;
constexpr std::int32_t kPetalTurnAway = -2;
constexpr std::int32_t kPetalNearGoal = 3;
constexpr std::int32_t kPetalLastDegree = 359;
constexpr std::int32_t kPetalPaintings = 2;

class CherryBlossom final : public EffectClassInstance {
 public:
  explicit CherryBlossom(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    for (std::int32_t row = 0; row < kPetalRows; ++row) {
      for (std::int32_t column = 0; column < kPetalColumns; ++column) {
        Petal petal;
        petal.quad.texture = kPetalTexture;
        petal.quad.alpha = 0;
        petal.spot = {0.0F, static_cast<float>(std::max(0, Rand() % kPetalDepthSpan)), 0.0F};
        for (float& heading : petal.heading) {
          heading = static_cast<float>(std::max(0, Rand() % kDegreesPerTurn));
        }
        for (float& goal : petal.goal) {
          goal = static_cast<float>(std::max(0, Rand() % kDegreesPerTurn));
        }
        petals_.push_back(petal);
      }
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Petal& petal : petals_) {
      if (petal.quad.alpha <= 0) {
        continue;
      }
      for (std::int32_t painting = 0; painting < kPetalPaintings; ++painting) {
        SubmitQuad(petal.quad, QuadUv{}, false, view, output);
      }
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (Petal& petal : petals_) {
      petal.quad.position = position_;
      if (petal.spot.y < 0.0F) {
        if (petal.spot.y >= kPetalResetHeight) {
          Drift(petal);
        } else {
          Rebury(petal);
        }
      }
      petal.spot.y -= kPetalRise;
      petal.quad.matrix = NativeScale(kPetalScale, kPetalScale, 1.0F);
      MultiplyRotationX(petal.quad.matrix, kClientPi);
      MultiplyRotationX(petal.quad.matrix, Radians(CameraPitch(tick)));
      MultiplyTranslation(petal.quad.matrix, petal.spot);
    }
  }

 private:
  struct Petal {
    QuadParticle quad;
    // La place du pétale autour du porteur, dans le repère du client.
    glm::vec3 spot{0.0F};
    // Les deux caps suivis et les deux caps visés, en degrés.
    std::array<float, 2> heading = {0.0F, 0.0F};
    std::array<float, 2> goal = {0.0F, 0.0F};
  };

  // Un cap se dirige vers celui qu'il vise ; arrivé à moins de trois degrés, il
  // en tire un nouveau — en deçà de sa position s'il visait plus bas, au-delà
  // sinon —, et le pétale glisse selon les deux sinus.
  void Drift(Petal& petal) {
    for (std::size_t axis = 0; axis < petal.heading.size(); ++axis) {
      const auto current = static_cast<std::int32_t>(petal.heading[axis]);
      const std::int32_t away = static_cast<std::int32_t>(petal.goal[axis]) - current;
      petal.heading[axis] =
          static_cast<float>(current + (away > 0 ? kPetalTurnToward : kPetalTurnAway));
      if (std::abs(away) < kPetalNearGoal) {
        const std::int32_t draw = current != 0 ? std::max(0, Rand() % current) : 0;
        petal.goal[axis] = static_cast<float>(away <= 0 ? kPetalLastDegree - draw : draw);
      }
    }
    petal.spot.x += kPetalRise * EffectClassSinTable(static_cast<std::int32_t>(petal.heading[0]));
    petal.spot.z += kPetalRise * EffectClassSinTable(static_cast<std::int32_t>(petal.heading[1]));
    if (petal.spot.y >= kPetalFadeHeight) {
      petal.quad.alpha = kPetalFullAlpha;
    } else {
      petal.quad.alpha = std::max(0, petal.quad.alpha - kPetalFadeStep);
    }
  }

  // Le pétale retourne sous terre, à une profondeur neuve, avec quatre caps
  // neufs et invisible.
  void Rebury(Petal& petal) {
    petal.spot = {0.0F, static_cast<float>(std::max(0, Rand() % kPetalDepthSpan)), 0.0F};
    for (float& heading : petal.heading) {
      heading = static_cast<float>(std::max(0, Rand() % kDegreesPerTurn));
    }
    for (float& goal : petal.goal) {
      goal = static_cast<float>(std::max(0, Rand() % kDegreesPerTurn));
    }
    petal.quad.alpha = 0;
  }

  std::vector<Petal> petals_;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeEnergyOrb(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<EnergyOrb>(birth);
}

std::unique_ptr<EffectClassInstance> MakeCherryBlossom(const EffectClassBirth& birth,
                                                       const EffectClassView& /*view*/) {
  return std::make_unique<CherryBlossom>(birth);
}

}  // namespace ro::render::effect_class
