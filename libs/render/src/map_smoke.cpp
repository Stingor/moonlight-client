#include "ro/render/map_smoke.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/effect_node.h"
#include "ro/render/map_emitter.h"

namespace ro::render {
namespace {

// UNE BOUFFÉE RENAÎT quand son âge atteint ce nombre de pas.
constexpr std::int32_t kPuffCycleSteps = 50;
// Passé cet âge, elle s'efface de `kPuffFadeStep` par pas.
constexpr std::int32_t kPuffFadeAfterSteps = 34;
constexpr std::int32_t kPuffFadeStep = 15;
constexpr std::int32_t kPuffOpaque = 255;
// Ce que la renaissance pose.
constexpr float kPuffSize = 1.5F;
constexpr float kPuffRollStepDegrees = 0.75F;
// La montée par pas : (rand() % 3 + 3) / 10, donc de 0,3 à 0,5.
constexpr std::uint32_t kRiseSpread = 3;
constexpr std::int32_t kRiseFloor = 3;
constexpr double kRiseDivisor = 10.0;
// La pente : 75 + rand() % 30 degrés autour de l'abscisse, puis rand() % 360
// autour de la verticale.
constexpr std::uint32_t kTiltSpread = 30;
constexpr std::int32_t kTiltFloorDegrees = 75;
constexpr std::uint32_t kTurnSpread = 360;
// La bouffée se peint NEUF UNITÉS au-dessus de son écart : sa matrice est une
// translation de (x, y − 9, z), et l'ordonnée du client descend.
constexpr float kPuffLift = 9.0F;

constexpr double kDegreesPerHalfTurn = 180.0;
constexpr float kDegreesPerHalfTurnFloat = 180.0F;
// Le pi du client, à six décimales.
constexpr double kClientPi = 3.141592;
constexpr float kClientPiFloat = 3.141592F;
constexpr float kHalf = 0.5F;

// LE QUAD DE LA PARTICULE : ±1,
// à une unité de profondeur, dans l'ordre de la bande du client.
constexpr std::array<glm::vec3, 4> kUnitQuad = {
    glm::vec3{-1.0F, 1.0F, 1.0F}, glm::vec3{1.0F, 1.0F, 1.0F}, glm::vec3{-1.0F, -1.0F, 1.0F},
    glm::vec3{1.0F, -1.0F, 1.0F}};
// ⚠ ICI L'IMAGE EST À L'ENDROIT, contrairement aux particules d'un émetteur :
// le coin d'ordonnée +1 — le bas — reçoit la DERNIÈRE ligne de l'image.
constexpr std::array<glm::vec2, 4> kUnitQuadUv = {glm::vec2{0.0F, 1.0F}, glm::vec2{1.0F, 1.0F},
                                                  glm::vec2{0.0F, 0.0F}, glm::vec2{1.0F, 0.0F}};
// Notre renderer prend ses coins en tour.
constexpr std::array<std::size_t, 4> kStripToCycle = {0, 1, 3, 2};

// Le mélange d'une particule-sprite qui n'est pas du type cinq : ordinaire.
constexpr std::int32_t kSmokeBlendSource = 5;
constexpr std::int32_t kSmokeBlendDestination = 6;
constexpr std::uint8_t kWhite = 255;

// Les degrés d'une renaissance vers les radians, comme chez le client :
// le quotient repasse par un flottant.
[[nodiscard]] float RadiansOf(float degrees) {
  const auto fraction = static_cast<float>(static_cast<double>(degrees) / kDegreesPerHalfTurn);
  return static_cast<float>(static_cast<double>(fraction) * kClientPi);
}

// Les degrés du dessin, calculés en simple précision comme chez le client.
[[nodiscard]] float DrawnRadiansOf(float degrees) {
  return degrees / kDegreesPerHalfTurnFloat * kClientPiFloat;
}

}  // namespace

std::optional<ChimneySmokeLook> LoadChimneySmoke(vfs::Vfs& vfs) {
  auto act_bytes = vfs.Read(std::string(kChimneySmokeSprite) + ".act");
  auto spr_bytes = vfs.Read(std::string(kChimneySmokeSprite) + ".spr");
  if (!act_bytes || !spr_bytes) {
    return std::nullopt;
  }
  auto act = formats::ActAnimation::Parse(*act_bytes);
  auto spr = formats::SprSprite::Parse(*spr_bytes);
  if (!act || !spr || act->actions().empty() || act->actions().front().frames.empty() ||
      act->actions().front().frames.front().layers.empty()) {
    return std::nullopt;
  }
  // ⚠ UN SEUL CALQUE ET UNE SEULE IMAGE : le client dessine tous les
  // calques de l'image courante et fait avancer l'image à chaque dessin, mais
  // ce sprite n'a qu'une action, une image et un calque.
  ChimneySmokeLook look;
  look.layer = act->actions().front().frames.front().layers.front();
  if (look.layer.frame_index < 0) {
    return std::nullopt;
  }
  const auto index = static_cast<std::size_t>(look.layer.frame_index);
  if (look.layer.section == 1) {
    if (index >= spr->bgra_frames().size()) {
      return std::nullopt;
    }
    // ⚠ L'ALPHA AUX QUATRE BITS DU CLIENT, comme pour tout sprite BGRA : voir
    // `WithClientAlphaQuartet`.
    look.image = WithClientAlphaQuartet(spr->bgra_frames()[index]);
  } else {
    if (!spr->palette().has_value() || index >= spr->indexed_frames().size()) {
      return std::nullopt;
    }
    look.image = formats::ApplyPalette(spr->indexed_frames()[index], *spr->palette());
  }
  return look;
}

const std::string& ChimneySmokeTextureKey() {
  static const std::string key = std::string(kChimneySmokeSprite) + ".spr";
  return key;
}

MapChimney::MapChimney(glm::vec3 native_position, std::uint32_t owner_id)
    : position_(native_position), owner_id_(owner_id) {}

void MapChimney::Tick(glm::vec3 native_player, std::uint32_t& draws, float reach_units) {
  ticked_ = true;
  hidden_ = reach_units <= glm::distance(native_player, position_);
  if (hidden_) {
    return;
  }
  for (Puff& puff : puffs_) {
    if (puff.age_steps >= kPuffCycleSteps) {
      // LA RENAISSANCE, dans l'ordre des tirages du client : la montée, la
      // pente, le cap, puis le nombre de bouffées peintes.
      puff.dressed = true;
      puff.size = kPuffSize;
      puff.roll_step_degrees = kPuffRollStepDegrees;
      const auto spread = static_cast<std::int32_t>(NextEffectNodeDraw(draws) % kRiseSpread);
      puff.age_steps = 0;
      puff.offset = {0.0F, 0.0F, 0.0F};
      puff.alpha = kPuffOpaque;
      puff.rise = static_cast<float>(static_cast<double>(spread + kRiseFloor) / kRiseDivisor);
      const auto tilt = static_cast<std::int32_t>(NextEffectNodeDraw(draws) % kTiltSpread);
      puff.direction = NativeRotationX(RadiansOf(static_cast<float>(tilt + kTiltFloorDegrees)));
      const auto turn = static_cast<std::int32_t>(NextEffectNodeDraw(draws) % kTurnSpread);
      MultiplyRotationY(puff.direction, RadiansOf(static_cast<float>(turn)));
      // ⚠ TIRÉ À CHAQUE RENAISSANCE, et c'est la dernière qui l'emporte.
      drawn_count_ = static_cast<std::int32_t>(
          EffectToolRandomBetween(draws, 1.0F, static_cast<float>(puffs_.size())));
    } else {
      puff.offset += Transform({0.0F, 0.0F, puff.rise}, puff.direction);
      puff.roll_degrees += puff.roll_step_degrees;
      if (puff.age_steps > kPuffFadeAfterSteps) {
        puff.alpha = std::max(0, puff.alpha - kPuffFadeStep);
      }
    }
    ++puff.age_steps;
  }
}

void MapChimney::AppendQuads(const MapEffectFrame& frame, const BillboardBasis& basis,
                             const ChimneySmokeLook& look,
                             std::vector<EffectWorldQuad>& quads) const {
  if (!drawn() || look.image.empty()) {
    return;
  }
  // 🔴 LE DÉCALAGE D'UN CALQUE EN MÉMOIRE N'EST PAS CELUI DU FICHIER : le
  // chargeur du client en a DÉJÀ RETRANCHÉ LA DEMI-IMAGE — sur le calque de
  // `fireball`, le fichier porte (0 ; +16) pour une image de 64 sur 64, et la
  // mémoire (−32 ; −16). Voir la même règle dans `MakeSpriteFrameQuads`.
  //
  // Le dessin de la particule rajoute ensuite cette demi-image :
  // `T(x + demi-largeur, y + demi-hauteur)` sur un quad de ±1 étiré de la
  // demi-taille. Les deux se compensent, donc L'IMAGE EST CENTRÉE SUR SON
  // ANCRE — et c'est ce qui fait que le roulis la tourne SUR PLACE au lieu de
  // la promener sur un cercle large comme elle.
  //
  // Un pixel vaut un septième d'unité (l'échelle d'écran vaut
  // `k · profondeur / 7`, et le quotient par la profondeur annule la profondeur).
  const float half_width = static_cast<float>(look.image.width) * kHalf;
  const float half_height = static_cast<float>(look.image.height) * kHalf;
  const float anchor_x = look.layer.offset_x - static_cast<float>((look.image.width + 1) / 2);
  const float anchor_y = look.layer.offset_y - static_cast<float>((look.image.height + 1) / 2);
  const float unit_x = look.layer.scale_x / kSpritePixelsPerWorldUnit;
  const float unit_y = look.layer.scale_y / kSpritePixelsPerWorldUnit;
  NativeMatrix layer = NativeScale(half_width * unit_x, half_height * unit_y, 1.0F);
  MultiplyRotationZ(layer, DrawnRadiansOf(static_cast<float>(look.layer.rotation_degrees)));
  MultiplyTranslation(layer,
                      {(anchor_x + half_width) * unit_x, (anchor_y + half_height) * unit_y, 0.0F});

  for (std::size_t rank = 0; rank < puffs_.size(); ++rank) {
    const Puff& puff = puffs_[rank];
    if (static_cast<std::int32_t>(rank) >= drawn_count_ || !puff.dressed) {
      continue;
    }
    NativeMatrix spin = NativeScale(puff.size, puff.size, 1.0F);
    MultiplyRotationZ(spin, DrawnRadiansOf(puff.roll_degrees));
    const glm::vec3 pivot =
        frame.ToWorld(position_ + puff.offset + glm::vec3{0.0F, -kPuffLift, 0.0F});

    EffectWorldQuad quad;
    for (std::size_t corner = 0; corner < kStripToCycle.size(); ++corner) {
      const std::size_t strip = kStripToCycle[corner];
      const glm::vec3 local = Transform(Transform(kUnitQuad[strip], layer), spin);
      // Face à l'œil comme un sprite : l'ordonnée du client descend à l'écran,
      // sa profondeur s'éloigne de l'œil.
      quad.corners[corner] =
          pivot + basis.right * local.x - basis.up * local.y + basis.forward * local.z;
      glm::vec2 uv = kUnitQuadUv[strip];
      if (look.layer.mirrored) {
        uv.x = 1.0F - uv.x;
      }
      quad.texture_uv[corner] = uv;
    }
    quad.tint = {kWhite, kWhite, kWhite,
                 static_cast<std::uint8_t>(std::clamp(puff.alpha, 0, kPuffOpaque))};
    quad.blend_source = kSmokeBlendSource;
    quad.blend_destination = kSmokeBlendDestination;
    quads.push_back(quad);
  }
}

}  // namespace ro::render
