#include "ro/render/sprite_frame.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

namespace ro::render {
namespace {

// Une teinte entièrement nulle rendrait le calque invisible. La plupart des
// .act laissent ce champ à blanc opaque, quelques-uns le laissent à zéro : on
// lit alors zéro comme « pas de teinte », faute de quoi ces sprites-là
// disparaissent.
constexpr std::array<std::uint8_t, 4> kNoTint = {255, 255, 255, 255};

bool IsFullyZero(const std::array<std::uint8_t, 4>& tint) {
  return tint[0] == 0 && tint[1] == 0 && tint[2] == 0 && tint[3] == 0;
}

// Dimensions de l'image que désigne un calque, ou rien si l'index sort de sa
// section. Les dimensions déclarées par le .act ne font pas foi : un fichier
// réédité garde parfois d'anciennes tailles, et la seule source vraie est le
// .spr.
std::optional<glm::ivec2> ImageSize(const formats::SprSprite& sprite, std::int32_t section,
                                    std::int32_t image_index) {
  if (image_index < 0) {
    return std::nullopt;
  }
  const auto index = static_cast<std::size_t>(image_index);
  if (section == 0) {
    if (index >= sprite.indexed_frames().size()) {
      return std::nullopt;
    }
    const auto& frame = sprite.indexed_frames()[index];
    return glm::ivec2{frame.width, frame.height};
  }
  if (index >= sprite.bgra_frames().size()) {
    return std::nullopt;
  }
  const auto& frame = sprite.bgra_frames()[index];
  return glm::ivec2{frame.width, frame.height};
}

}  // namespace

std::vector<SpriteQuad> BuildFrameQuads(const formats::SprSprite& sprite,
                                        const formats::ActAnimation& animation,
                                        std::size_t action_index, std::size_t frame_index,
                                        bool apply_rotation) {
  std::vector<SpriteQuad> quads;
  if (action_index >= animation.actions().size()) {
    return quads;
  }
  const formats::ActAction& action = animation.actions()[action_index];
  if (frame_index >= action.frames.size()) {
    return quads;
  }
  const formats::ActFrame& frame = action.frames[frame_index];

  quads.reserve(frame.layers.size());
  for (const formats::ActLayer& layer : frame.layers) {
    const std::optional<glm::ivec2> size = ImageSize(sprite, layer.section, layer.frame_index);
    if (!size.has_value()) {
      // Calque vide, ou index hors de sa section : le fichier en contient
      // réellement, et les écarter décalerait tous les calques suivants.
      continue;
    }

    SpriteQuad quad;
    quad.image_index = layer.frame_index;
    quad.section = layer.section;
    quad.mirrored = layer.mirrored;
    quad.tint = IsFullyZero(layer.tint) ? kNoTint : layer.tint;

    // La construction suit exactement celle du jeu, dans cet ordre :
    //
    //   1. image CENTRÉE sur l'origine, par une division ENTIÈRE ;
    //   2. échelle du calque, autour de l'origine — le centrage est donc mis à
    //      l'échelle lui aussi ;
    //   3. rotation éventuelle, dont le centre se décale d'un demi-pixel sur
    //      les dimensions IMPAIRES, sans quoi un calque de largeur impaire
    //      dérive d'un demi-pixel à chaque image ;
    //   4. translation par le décalage du calque.
    //
    // ⚠ Le décalage du point 4 n'est PAS mis à l'échelle. Le multiplier par
    // l'échelle du calque décroche les calques réduits : sur un sprite qui
    // monte et descend, un calque à l'échelle 0,3 ne suivrait que 30 % du
    // mouvement et resterait en arrière. Deux sources indépendantes le
    // confirment — la référence, et le fait que le jeu pose la position à
    // « acteur + décalage du calque ».
    //
    // 🔴 LE CENTRAGE DU POINT 1 EST CELUI DU CLIENT, ET IL SE FAIT AU CHARGEMENT.
    // Le client pose le quad de `décalage` à `décalage + taille`, c'est-à-dire
    // par son COIN — pour un acteur comme pour une particule d'effet. Ce n'est
    // pas une convention différente de la nôtre : son CHARGEUR a déjà retranché
    // la demi-image. Le calque de `fireball`, dont le fichier porte (0 ; +16)
    // pour une image de 64 sur 64, se pose donc en (−32 ; −16).
    const float width = static_cast<float>(size->x);
    const float height = static_cast<float>(size->y);
    const auto left = static_cast<float>(-((size->x + 1) / 2));
    const auto top = static_cast<float>(-((size->y + 1) / 2));

    std::array<float, 4> corner_x = {left, left + width, left + width, left};
    std::array<float, 4> corner_y = {top, top, top + height, top + height};
    for (std::size_t corner = 0; corner < 4; ++corner) {
      corner_x[corner] *= layer.scale_x;
      corner_y[corner] *= layer.scale_y;
    }

    if (apply_rotation && layer.rotation_degrees != 0) {
      const float radians =
          static_cast<float>(-layer.rotation_degrees) * std::numbers::pi_v<float> / 180.0F;
      const float cosine = std::cos(radians);
      const float sine = std::sin(radians);
      const float center_x = (size->x % 2) != 0 ? -0.5F * layer.scale_x : 0.0F;
      const float center_y = (size->y % 2) != 0 ? -0.5F * layer.scale_y : 0.0F;
      for (std::size_t corner = 0; corner < 4; ++corner) {
        const float dx = corner_x[corner] - center_x;
        const float dy = corner_y[corner] - center_y;
        corner_x[corner] = center_x + dx * cosine + dy * sine;
        corner_y[corner] = center_y - dx * sine + dy * cosine;
      }
    }

    for (std::size_t corner = 0; corner < 4; ++corner) {
      quad.corners[corner] = {layer.offset_x + corner_x[corner], layer.offset_y + corner_y[corner]};
    }
    quads.push_back(quad);
  }
  return quads;
}

std::optional<formats::ActAnchor> FrameAnchor(const formats::ActAnimation& animation,
                                              std::size_t action_index, std::size_t frame_index) {
  if (action_index >= animation.actions().size()) {
    return std::nullopt;
  }
  const formats::ActAction& action = animation.actions()[action_index];
  // Premier groupe d'actions, celui des poses au repos : l'ancre de référence
  // est celle de l'image 0. Prendre celle de l'image courante ferait vibrer
  // les pièces rapportées pendant l'animation d'attente.
  const std::size_t anchor_frame = action_index < kDirectionCount ? 0 : frame_index;
  if (anchor_frame >= action.frames.size()) {
    return std::nullopt;
  }
  const formats::ActFrame& frame = action.frames[anchor_frame];
  if (frame.anchors.empty()) {
    return std::nullopt;
  }
  return frame.anchors[0];
}

std::size_t SpriteDirection(std::size_t compass_direction, float camera_yaw_degrees) noexcept {
  // Le nord de la boussole est la cinquième direction du .act.
  constexpr auto kSpriteNorth = static_cast<std::int64_t>(kDirectionCount / 2);
  constexpr auto kDirections = static_cast<std::int64_t>(kDirectionCount);
  constexpr float kDegreesPerDirection = 360.0F / static_cast<float>(kDirectionCount);

  // Le client arrondit au huitième de tour le plus proche : il ajoute
  // vingt-deux degrés et demi avant de diviser par quarante-cinq.
  const auto turned =
      static_cast<std::int64_t>(std::llround(camera_yaw_degrees / kDegreesPerDirection));
  const auto compass = static_cast<std::int64_t>(compass_direction % kDirectionCount);
  const std::int64_t direction = (kSpriteNorth - compass + turned) % kDirections;
  return static_cast<std::size_t>((direction + kDirections) % kDirections);
}

std::size_t CompassOfSpriteDirection(std::size_t sprite_direction) noexcept {
  // Le nord de la boussole est la cinquième direction du .act, et les deux
  // tournent en sens contraires : la même soustraction les relie dans les deux
  // sens.
  constexpr std::size_t kSpriteNorth = kDirectionCount / 2;
  return (kSpriteNorth + kDirectionCount - sprite_direction % kDirectionCount) % kDirectionCount;
}

std::size_t SpriteDirectionOfDegrees(float client_body_degrees, float camera_yaw_degrees,
                                     bool rounded) noexcept {
  constexpr float kFullTurnDegrees = 360.0F;
  constexpr float kDegreesPerDirection = kFullTurnDegrees / static_cast<float>(kDirectionCount);
  constexpr float kHalfDirectionDegrees = kDegreesPerDirection / 2.0F;
  // L'angle de face entre en NÉGATIF et celui de la caméra en positif : la
  // caméra du client tourne à l'envers de la nôtre, et c'est cette différence
  // qui rend `SpriteDirection` juste sous une vue qui pivote.
  float relative = std::fmod(camera_yaw_degrees - client_body_degrees, kFullTurnDegrees);
  if (relative < 0.0F) {
    relative += kFullTurnDegrees;
  }
  // Un angle négatif infime remonté au tour retombe SUR le tour entier.
  if (relative >= kFullTurnDegrees) {
    relative -= kFullTurnDegrees;
  }
  if (!rounded) {
    return static_cast<std::size_t>(relative / kDegreesPerDirection) % kDirectionCount;
  }
  if (relative < kHalfDirectionDegrees || relative > kFullTurnDegrees - kHalfDirectionDegrees) {
    return 0;
  }
  return static_cast<std::size_t>((relative + kHalfDirectionDegrees) / kDegreesPerDirection) %
         kDirectionCount;
}

bool ScreenDirectionRounds(std::uint32_t job_id, bool player_layout,
                           std::size_t animation) noexcept {
  // Les mercenaires et les homoncules évolués, tels que le client borne cette
  // famille — dès le DEUXIÈME mercenaire, et jusqu'au dernier homoncule.
  constexpr std::uint32_t kFirstRoundingCompanionJob = 6018;
  constexpr std::uint32_t kLastRoundingCompanionJob = 6052;
  // Les classes de PNJ : après le portail, avant le premier monstre.
  constexpr std::uint32_t kFirstNpcJob = 46;
  constexpr std::uint32_t kLastNpcJob = 999;
  const bool resting_row =
      animation == kAnimationIdle || animation == kAnimationWalk || animation == kAnimationSit;
  if (player_layout) {
    return resting_row;
  }
  if (job_id >= kFirstRoundingCompanionJob && job_id <= kLastRoundingCompanionJob) {
    return resting_row;
  }
  if (job_id >= kFirstNpcJob && job_id <= kLastNpcJob) {
    return animation == kAnimationIdle;
  }
  return false;
}

std::size_t AnimationFor(Motion motion, SpriteLayout layout) noexcept {
  const bool player = layout == SpriteLayout::kPlayer;
  switch (motion) {
    case Motion::kIdle:
      return kAnimationIdle;
    case Motion::kWalk:
      return kAnimationWalk;
    case Motion::kSit:
      return player ? kAnimationSit : kCreatureAnimationAttack;
    case Motion::kPickup:
      return player ? kAnimationPickup : kCreatureAnimationAttack;
    // ⚠ UNE CRÉATURE RESTE À L'ATTENTE : ses cinq rangs n'en portent pas de
    // garde, et lui prêter celui de l'attaque la ferait frapper à vide entre
    // deux coups. Le client ne l'y envoie pas non plus — il ne pose cette pose
    // que par la table des treize rangs.
    case Motion::kReady:
      return player ? kAnimationReady : kAnimationIdle;
    case Motion::kAttack:
      return player ? kAnimationAttack2 : kCreatureAnimationAttack;
    case Motion::kCast:
      return player ? kAnimationCast : kAnimationIdle;
    case Motion::kHurt:
      return player ? kAnimationHurt : kCreatureAnimationHurt;
    case Motion::kDie:
      return player ? kAnimationDie : kCreatureAnimationDie;
  }
  return kAnimationIdle;
}

std::size_t WalkFrameAt(float travelled_world_units, float frame_interval_ms,
                        std::size_t frame_count) noexcept {
  if (frame_count == 0 || frame_interval_ms <= 0.0F || travelled_world_units <= 0.0F) {
    return 0;
  }
  // Le délai du client se compte en TICS, pas en millisecondes.
  const float ticks = frame_interval_ms / formats::kMillisecondsPerSpeedUnit;
  const auto reached =
      static_cast<std::size_t>((travelled_world_units * kWalkFramesPerWorldUnit) / ticks);
  return reached % frame_count;
}

std::size_t GlideHeldWalkFrame(std::size_t frame_count) noexcept {
  constexpr std::size_t kHeldFrame = 7;
  return frame_count == 0 ? 0 : std::min(kHeldFrame, frame_count - 1);
}

bool MotionLoops(Motion motion) noexcept {
  // 🔴 L'INCANTATION NE BOUCLE PAS, ET L'EMPLOI NON PLUS. Qui incante TIENT la
  // première image de son rang d'incantation, sans la faire défiler, jusqu'à la
  // fin de sa barre ou à sa prochaine motion ; qui emploie une compétence joue
  // son geste UNE fois, depuis sa première image, puis revient au repos. Faire
  // boucler ce rang montre ses six images là où le jeu n'en montre qu'une.
  //
  // L'attente, la marche, l'assise et la garde tournent — l'attente et l'assise
  // d'un joueur tiennent en fait l'image de son regard, et c'est l'appelant qui
  // la pose ; celle d'une créature défile.
  return motion == Motion::kIdle || motion == Motion::kWalk || motion == Motion::kSit ||
         motion == Motion::kReady;
}

std::size_t FrameAtTimeOnce(const formats::ActAction& action, float elapsed_ms) {
  if (action.frames.empty()) {
    return 0;
  }
  const float interval_ms = action.frame_interval_ms();
  if (!(interval_ms > 0.0F) || !(elapsed_ms > 0.0F)) {
    return 0;
  }
  const auto elapsed_frames = static_cast<std::size_t>(elapsed_ms / interval_ms);
  return elapsed_frames < action.frames.size() ? elapsed_frames : action.frames.size() - 1;
}

std::size_t AttackSoundFrame(const formats::ActAnimation& animation, std::size_t action_index) {
  if (action_index >= animation.actions().size()) {
    return 0;
  }
  const formats::ActAction& action = animation.actions()[action_index];
  const std::vector<std::string>& sounds = animation.sound_files();
  for (std::size_t frame = 0; frame < action.frames.size(); ++frame) {
    const std::int32_t sound_index = action.frames[frame].sound_index;
    if (sound_index >= 0 && static_cast<std::size_t>(sound_index) < sounds.size() &&
        sounds[static_cast<std::size_t>(sound_index)] == kAttackSoundMarker) {
      return frame;
    }
  }
  // Sans marqueur, l'avant-dernière image ; une action d'une image n'a que
  // celle-là.
  return action.frames.size() > 1 ? action.frames.size() - 2 : 0;
}

std::size_t FrameAtTime(const formats::ActAction& action, float elapsed_ms) {
  if (action.frames.empty()) {
    return 0;
  }
  const float interval_ms = action.frame_interval_ms();
  // Une cadence nulle ou négative figerait l'animation sur une division par
  // zéro : quelques fichiers portent une cadence aberrante, et mieux vaut la
  // première image qu'un index indéfini.
  if (!(interval_ms > 0.0F) || !(elapsed_ms > 0.0F)) {
    return 0;
  }
  const auto elapsed_frames = static_cast<std::size_t>(elapsed_ms / interval_ms);
  return elapsed_frames % action.frames.size();
}

glm::vec2 AttachOffset(const std::optional<formats::ActAnchor>& parent,
                       const std::optional<formats::ActAnchor>& piece) {
  if (!parent.has_value() || !piece.has_value()) {
    return {0.0F, 0.0F};
  }
  // Les attributs doivent coïncider : ils identifient le point d'attache, et
  // sans ce test on accroche des pièces qui n'ont rien à voir l'une avec
  // l'autre.
  if (parent->attribute != piece->attribute) {
    return {0.0F, 0.0F};
  }
  return {static_cast<float>(parent->x - piece->x), static_cast<float>(parent->y - piece->y)};
}

}  // namespace ro::render
