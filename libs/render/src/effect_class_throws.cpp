// LES OBJETS LANCÉS : flacons et potions de l'alchimiste (298, 299, 539, 541,
// 980), pierre (308), lames et pièce (600, 613..616), spore (914), kunai et
// pièces d'Oboro et de Kagerou (981..983) et sphères libérées (989). Une icône
// du sac vole en cloche du lanceur à sa cible en tournant sur elle-même, trois
// copies pâlissantes à sa suite, puis s'éteint arrivée.
//
// ⚠ PLUSIEURS GROUPES POUR CERTAINS : huit kunai, dix pièces, autant d'orbes que
// de sphères. Un groupe est une tête et ses copies.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// UNE TÊTE ET TROIS COPIES. La première copie a cent cinquante d'alpha de moins
// que la tête, chacune des suivantes vingt-cinq de moins que la précédente.
constexpr std::int32_t kParticlesPerGroup = 4;
constexpr std::int32_t kFirstTrailAlphaFall = 150;
constexpr std::int32_t kNextTrailAlphaFall = 25;

constexpr std::int32_t kFullAlpha = 255;
// VINGT-CINQ PAS POUR TOUT LE VOL, quelle que soit la distance.
constexpr float kStepsAcrossAnyThrow = 25.0F;
// LA TÊTE TOURNE DE QUARANTE-CINQ DEGRÉS PAR TIC ; les copies prennent l'angle
// de celle qui les précède.
constexpr std::int32_t kSpinStepDegrees = 45;
// LE VOL DURE TANT QUE L'ÂGE NE DÉPASSE PAS D'UN VINGTIÈME LE NOMBRE DE PAS ;
// ensuite chaque objet perd vingt d'alpha par tic.
constexpr float kFlightEndsAfterRatio = 1.05F;
constexpr std::int32_t kAlphaFallAfterFlight = 20;
// LE COMPTE DES PAS S'ARRÊTE À CENT, ET N'EST ALORS PAS RETENU : le vol est
// tenu pour fini dès le premier tic.
constexpr std::int32_t kStepCountCeiling = 100;
// SANS ACTEUR LIÉ, la cible reste où le constructeur l'a posée : à l'origine du
// monde.
constexpr glm::vec3 kUnboundTarget{0.0F, 0.0F, 0.0F};
// CEUX QUI SE VISENT EUX-MÊMES sans acteur lié visent huit unités à l'ouest de
// leur place.
constexpr float kSelfAimWestward = 8.0F;

// LA FAÇON DONT UN OBJET SE TEINT.
enum class Tinting : std::uint8_t {
  // Une couleur fixe, mélangée par son alpha.
  kFixed,
  // La couleur que le paramètre de l'instance choisit : voir `PotionColor`.
  kByParam,
  // L'orbe de l'élément des sphères du porteur, en addition.
  kByCharmElement,
};

// D'OÙ VIENT LE NOMBRE DE GROUPES.
enum class GroupCount : std::uint8_t {
  kOne,
  kEightKunai,
  kTenGoldPieces,
  // Autant que de sphères sur le porteur, chacune d'une seule particule.
  kOwnerCharms,
};

// CE QUE CHAQUE IDENTIFIANT DONNE À SON OBJET.
struct ThrowLook {
  std::uint32_t effect_id = 0;
  std::string_view texture;
  Tinting tinting = Tinting::kFixed;
  std::array<std::int32_t, 3> color = {255, 255, 255};
  // LE SOMMET DE LA CLOCHE, au-dessus de la droite qui va du lanceur à la cible.
  float arc_height = 0.0F;
  // LE TIC DE L'EFFET OÙ L'OBJET COMMENCE À BOUGER. Avant, rien ne se peint ; et
  // la cloche se compte quand même depuis la naissance, si bien qu'un objet qui
  // part tard retombe avant d'avoir couvert toute la distance.
  std::int32_t start_tick = 0;
  float scale = 0.0F;
  // LE PAS DU VOL, ou zéro pour un pas tiré de la distance — vingt-cinq pas
  // pour tout le vol. Un pas fixe fait voler loin plus longtemps.
  float fixed_step = 0.0F;
  // L'ANGLE PROPRE DE DÉPART, en degrés.
  float start_spin_degrees = 0.0F;
  std::int32_t life_ticks = 0;
  // DE COMBIEN L'OBJET PART AU-DESSUS DES PIEDS DU LANCEUR.
  float height_above = 0.0F;
  GroupCount groups = GroupCount::kOne;
  // LES GROUPES SE RÉPARTISSENT SUR LE TOUR, le cap de chacun décalé d'autant.
  bool spreads_groups = false;
  // SANS ACTEUR LIÉ, il vise sa propre place, huit unités à l'ouest.
  bool aims_self_when_unbound = false;
};

constexpr float kHighArc = 30.0F;
constexpr float kStoneArc = 10.0F;
constexpr float kLowArc = 1.0F;
constexpr float kFlatArc = 0.0F;
constexpr std::int32_t kFireBottleStartTick = 5;
constexpr std::int32_t kAcidBottleStartTick = 10;
constexpr float kBottleScale = 1.5F;
constexpr float kStoneScale = 1.0F;
constexpr float kBladeScale = 2.0F;
constexpr float kGoldScale = 2.5F;
constexpr float kKunaiScale = 3.0F;
constexpr float kStoneStep = 2.0F;
constexpr float kBladeStep = 3.0F;
constexpr float kKunaiStep = 1.5F;
constexpr float kGoldStep = 2.0F;
constexpr float kTiltedSpinDegrees = 330.0F;
constexpr std::int32_t kBottleLifeTicks = 300;
constexpr std::int32_t kBladeLifeTicks = 200;
constexpr float kAboveFeet = 8.0F;
constexpr float kOrbAboveFeet = 15.0F;
// LES LAMES ET LA PIÈCE sont roses, les pièces d'or jaunes.
constexpr std::array<std::int32_t, 3> kBladeColor = {255, 200, 255};
constexpr std::array<std::int32_t, 3> kGoldColor = {255, 200, 0};
constexpr std::array<std::int32_t, 3> kWhite = {255, 255, 255};
constexpr std::string_view kPoisonKunai = "유저인터페이스\\item\\쿠나이_독.bmp";

// LE FLACON D'ACIDE D'ACID TERROR (298) est vert pâle ; les deux potions (299,
// 541) et la spore (914) prennent leur teinte de leur paramètre. Les deux
// flacons d'Acid Demonstration — de feu (539), puis d'acide (980) — sont blancs,
// rasent le sol et partent l'un après l'autre. La pierre (308) vole en cloche
// basse à pas fixe ; le couteau empoisonné (600), le shuriken (613), les deux
// kunai (614, 615) et la pièce (616) partent penchés, à pas fixe plus long.
// Les huit kunai de 981 partent en étoile, ceux de 983 en un seul groupe — tous
// deux avec l'image du kunai empoisonné, qui remplace celle du kunai de feu
// posée d'abord pour 983 ; dix pièces d'or (982) ; les orbes des sphères (989)
// volent à plat.
constexpr std::array<ThrowLook, 16> kThrowLooks = {{
    {298,
     "유저인터페이스\\item\\빈병.bmp",
     Tinting::kFixed,
     {135, 255, 135},
     kHighArc,
     0,
     kBottleScale,
     0.0F,
     0.0F,
     kBottleLifeTicks,
     kAboveFeet},
    {299,
     "유저인터페이스\\item\\빈포션병.bmp",
     Tinting::kByParam,
     {},
     kHighArc,
     0,
     kBottleScale,
     0.0F,
     0.0F,
     kBottleLifeTicks,
     kAboveFeet},
    {308, "유저인터페이스\\item\\돌.bmp", Tinting::kFixed, kWhite, kStoneArc, 0, kStoneScale,
     kStoneStep, 0.0F, kBottleLifeTicks, kAboveFeet},
    {539, "유저인터페이스\\item\\화염병.bmp", Tinting::kFixed, kWhite, kLowArc,
     kFireBottleStartTick, kBottleScale, 0.0F, 0.0F, kBottleLifeTicks, kAboveFeet},
    {541,
     "유저인터페이스\\item\\버서크포션.bmp",
     Tinting::kByParam,
     {},
     kHighArc,
     0,
     kBottleScale,
     0.0F,
     0.0F,
     kBottleLifeTicks,
     kAboveFeet},
    {600, "유저인터페이스\\item\\베넘나이프.bmp", Tinting::kFixed, kBladeColor, kLowArc, 0,
     kBladeScale, kBladeStep, kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet},
    {613, "유저인터페이스\\item\\수리검.bmp", Tinting::kFixed, kBladeColor, kLowArc, 0, kBladeScale,
     kBladeStep, kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet},
    {614, kPoisonKunai, Tinting::kFixed, kBladeColor, kLowArc, 0, kBladeScale, kBladeStep,
     kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet},
    {615, "유저인터페이스\\item\\풍마_열화.bmp", Tinting::kFixed, kBladeColor, kLowArc, 0,
     kBladeScale, kBladeStep, kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet},
    {616, "effect\\coin_a.bmp", Tinting::kFixed, kBladeColor, kLowArc, 0, kBladeScale, kBladeStep,
     kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet},
    {914,
     "유저인터페이스\\item\\폭탄버섯포자.bmp",
     Tinting::kByParam,
     {},
     kHighArc,
     0,
     kBottleScale,
     0.0F,
     0.0F,
     kBottleLifeTicks,
     kAboveFeet},
    {980, "유저인터페이스\\item\\염산병.bmp", Tinting::kFixed, kWhite, kLowArc,
     kAcidBottleStartTick, kBottleScale, 0.0F, 0.0F, kBottleLifeTicks, kAboveFeet},
    {981, kPoisonKunai, Tinting::kFixed, kBladeColor, kLowArc, 0, kKunaiScale, kKunaiStep,
     kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet, GroupCount::kEightKunai, true, true},
    {982, "effect\\coin_a.bmp", Tinting::kFixed, kGoldColor, kStoneArc, 0, kGoldScale, kGoldStep,
     kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet, GroupCount::kTenGoldPieces},
    {983, kPoisonKunai, Tinting::kFixed, kBladeColor, kLowArc, 0, kKunaiScale, kKunaiStep,
     kTiltedSpinDegrees, kBladeLifeTicks, kAboveFeet, GroupCount::kOne, false, true},
    {989, "", Tinting::kByCharmElement, kWhite, kFlatArc, 0, kBladeScale, kKunaiStep,
     kTiltedSpinDegrees, kBladeLifeTicks, kOrbAboveFeet, GroupCount::kOwnerCharms},
}};

constexpr std::int32_t kEightKunai = 8;
constexpr std::int32_t kTenGoldPieces = 10;

// L'ORBE D'UN ÉLÉMENT DE SPHÈRES : un l'eau, deux la terre, trois le feu, quatre
// le vent. Vide pour tout autre élément, que le client ne teint d'aucune image.
[[nodiscard]] std::string_view OrbOfCharm(std::int32_t element) {
  switch (element) {
    case 1:
      return "effect\\WaterOrb.bmp";
    case 2:
      return "effect\\StoneOrb.bmp";
    case 3:
      return "effect\\FireOrb.bmp";
    case 4:
      return "effect\\LightningOrb.bmp";
    default:
      return {};
  }
}

// LES TEINTES D'UNE POTION, par le paramètre de l'instance. La sixième garde le
// mélange additif ; toutes les autres se mélangent par leur alpha, y compris
// celles qui ne posent aucune teinte.
constexpr std::int32_t kAdditiveTint = 6;
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> PotionColor(std::int32_t tint) {
  switch (tint) {
    case 1:
      return std::array<std::int32_t, 3>{255, 135, 135};
    case 2:
      return std::array<std::int32_t, 3>{255, 205, 135};
    case 3:
      return std::array<std::int32_t, 3>{255, 255, 135};
    case 4:
    case kAdditiveTint:
      return std::array<std::int32_t, 3>{255, 255, 255};
    case 5:
      return std::array<std::int32_t, 3>{135, 135, 255};
    default:
      return std::nullopt;
  }
}
// SANS PARAMÈTRE, LA POTION PREND LA PREMIÈRE TEINTE.
constexpr std::int32_t kUnsetParam = -1;
constexpr std::int32_t kDefaultTint = 1;

// LE CAP BRUT, en degrés, de la direction (dx, dz) : l'arc cosinus de dx sur la
// longueur, retourné quand dz est négatif. Zéro sans longueur.
[[nodiscard]] float RawHeadingDegrees(float dx, float dz) {
  const float length = std::sqrt((dx * dx) + (dz * dz));
  if (!(length > 0.0F)) {
    return 0.0F;
  }
  const float angle = std::acos(dx / length) / kClientPi * kHalfTurnDegrees;
  return dz >= 0.0F ? angle : kFullTurnDegrees - angle;
}

// LE CAP D'UN GROUPE : le brut tronqué, plus le décalage du groupe, ramené sur
// seize bits puis dans le tour.
[[nodiscard]] std::int32_t GroupHeadingDegrees(float raw, std::int32_t offset) {
  constexpr std::int32_t kDegreesPerTurn = 360;
  return static_cast<std::int16_t>(static_cast<std::int32_t>(raw) + offset) % kDegreesPerTurn;
}

class ThrowItem final : public EffectClassInstance {
 public:
  explicit ThrowItem(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    look_ = &kThrowLooks[0];
    for (const ThrowLook& look : kThrowLooks) {
      if (look.effect_id == effect_id_) {
        look_ = &look;
      }
    }
    duration_ticks_ = look_->life_ticks;
    // LA CIBLE EST CELLE DE LA NAISSANCE : l'objet ne la suit pas si elle bouge.
    glm::vec3 target = birth.bound_position.value_or(kUnboundTarget);
    if (!birth.bound_position.has_value() && look_->aims_self_when_unbound) {
      target = position_ - glm::vec3{kSelfAimWestward, 0.0F, 0.0F};
    }
    const float dx = target.x - position_.x;
    const float dz = target.z - position_.z;
    rise_ = target.y - position_.y;
    distance_ = std::sqrt((dz * dz) + (dx * dx));
    const float raw_heading = RawHeadingDegrees(dx, dz);
    step_ = look_->fixed_step > 0.0F ? look_->fixed_step : distance_ / kStepsAcrossAnyThrow;
    steps_ = StepCount(step_, distance_);

    Particle model;
    model.spin_degrees = look_->start_spin_degrees;
    model.quad.alpha = kFullAlpha;
    model.quad.texture = look_->texture;
    model.position = position_ - glm::vec3{0.0F, look_->height_above, 0.0F};
    switch (look_->tinting) {
      case Tinting::kFixed:
        model.quad.color = look_->color;
        model.quad.blend_destination = kBlendInverseSourceAlpha;
        break;
      case Tinting::kByParam: {
        const std::int32_t tint = level_ == kUnsetParam ? kDefaultTint : level_;
        if (tint != kAdditiveTint) {
          model.quad.blend_destination = kBlendInverseSourceAlpha;
        }
        if (const auto color = PotionColor(tint); color.has_value()) {
          model.quad.color = *color;
        }
        break;
      }
      case Tinting::kByCharmElement:
        model.quad.texture = OrbOfCharm(birth.owner_charm_element);
        break;
    }

    std::int32_t groups = 1;
    std::int32_t per_group = kParticlesPerGroup;
    switch (look_->groups) {
      case GroupCount::kOne:
        break;
      case GroupCount::kEightKunai:
        groups = kEightKunai;
        break;
      case GroupCount::kTenGoldPieces:
        groups = kTenGoldPieces;
        break;
      case GroupCount::kOwnerCharms:
        groups = birth.owner_charm_count;
        per_group = 1;
        break;
    }
    // LE TOUR SE PARTAGE ENTRE LES GROUPES, par une division entière.
    constexpr std::int32_t kDegreesPerTurn = 360;
    const std::int32_t spread = groups > 0 ? kDegreesPerTurn / groups : 0;
    for (std::int32_t group = 0; group < groups; ++group) {
      model.heading_degrees =
          GroupHeadingDegrees(raw_heading, look_->spreads_groups ? group * spread : 0);
      for (std::int32_t rank = 0; rank < per_group; ++rank) {
        model.rank = rank;
        particles_.push_back(model);
      }
    }
  }

  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present && age_ < duration_ticks_;
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    if (!started_) {
      return;
    }
    for (const Particle& particle : particles_) {
      // UN OBJET ÉTEINT NE SE VOIT PAS : son alpha nul le rend transparent. Une
      // orbe sans élément n'a pas d'image.
      if (particle.quad.alpha <= 0 || particle.quad.texture.empty()) {
        continue;
      }
      QuadParticle quad = particle.quad;
      quad.matrix = NativeScale(look_->scale, look_->scale, 1.0F);
      MultiplyRotationX(quad.matrix, Radians(view.pitch_degrees));
      MultiplyRotationY(quad.matrix, Radians(particle.spin_degrees));
      MultiplyTranslation(quad.matrix, particle.position - glm::vec3{0.0F, particle.lift, 0.0F});
      SubmitQuad(quad, QuadUv{}, true, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    if (age_ < look_->start_tick) {
      return;
    }
    started_ = true;
    // CE QUE CHAQUE OBJET LAISSE À CELUI QUI LE SUIT, dans l'ordre de la file.
    glm::vec3 carried_position{0.0F};
    float carried_spin = 0.0F;
    std::int32_t carried_alpha = 0;
    // ⚠ UN VOL SANS PAS EST UNE DIVISION PAR ZÉRO chez le client : le rapport
    // n'est alors jamais sous le seuil, et l'objet pâlit sur place.
    const float ratio = steps_ > 0 ? static_cast<float>(age_) / static_cast<float>(steps_)
                                   : std::numeric_limits<float>::infinity();
    for (Particle& particle : particles_) {
      if (particle.rank == 0) {
        particle.spin_degrees =
            static_cast<float>(static_cast<std::int32_t>(particle.spin_degrees + kSpinStepDegrees) %
                               static_cast<std::int32_t>(kFullTurnDegrees));
      } else {
        carried_alpha -= particle.rank == 1 ? kFirstTrailAlphaFall : kNextTrailAlphaFall;
        particle.quad.alpha = carried_alpha;
        particle.position = carried_position;
        particle.spin_degrees = carried_spin;
      }
      particle.lift = 0.0F;
      if (ratio <= kFlightEndsAfterRatio) {
        particle.position.x += step_ * EffectClassCosTable(particle.heading_degrees);
        particle.position.y += (step_ / distance_) * rise_;
        particle.position.z += step_ * EffectClassSinTable(particle.heading_degrees);
        particle.lift = EffectClassSinTable(-static_cast<std::int32_t>(ratio * -kHalfTurnDegrees)) *
                        look_->arc_height;
      } else {
        particle.quad.alpha = std::max(particle.quad.alpha - kAlphaFallAfterFlight, 0);
      }
      carried_position = particle.position;
      carried_spin = particle.spin_degrees;
      carried_alpha = particle.quad.alpha;
    }
  }

 private:
  struct Particle {
    QuadParticle quad;
    glm::vec3 position{0.0F};
    float spin_degrees = 0.0F;
    float lift = 0.0F;
    std::int32_t heading_degrees = 0;
    // SA PLACE DANS SON GROUPE : zéro pour la tête.
    std::int32_t rank = 0;
  };

  [[nodiscard]] static std::int32_t StepCount(float step, float distance) {
    std::int32_t count = 0;
    while (static_cast<float>(count) * step <= distance) {
      if (++count >= kStepCountCeiling) {
        return 0;
      }
    }
    return count;
  }

  const ThrowLook* look_ = nullptr;
  bool started_ = false;
  std::vector<Particle> particles_;
  float distance_ = 0.0F;
  float rise_ = 0.0F;
  float step_ = 0.0F;
  std::int32_t steps_ = 0;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeThrowItem(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<ThrowItem>(birth);
}

}  // namespace ro::render::effect_class
