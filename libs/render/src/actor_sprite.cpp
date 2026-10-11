#include "ro/render/actor_sprite.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>

#include "ro/core/ascii.h"

namespace ro::render {
namespace {

// L'action que joue une pièce, et l'image dans cette action.
//
// ⚠ TOUTES les pièces partagent l'action DU CORPS. Le jeu ne lit qu'une action
// et qu'une image pour ses huit parties. Leur faire jouer leur propre pose de
// repos paraît plus logique — une décoration vivrait pour elle-même — mais
// c'est faux : l'action de repos d'un costume peut être tout autre chose que
// son animation en pose assise.
//
// Deux replis, et ils ne sont pas symétriques :
//
//   * l'ACTION se replie MODULO le nombre d'actions de la pièce. Une coiffe en
//     porte souvent moins que le corps, et la replier vaut mieux que l'omettre.
//   * l'IMAGE, elle, retombe à ZÉRO si la pièce n'en a pas autant. Ce n'est pas
//     un modulo : la pièce reprendrait sinon son animation depuis un point
//     arbitraire du cycle, et se désynchroniserait du corps.
//
// L'animation ALTERNATIVE d'une coiffe se choisit à part, en amont : voir
// AlternateFrame, et le paramètre attachment_frames de ComposeActor.
std::pair<std::size_t, std::size_t> PoseOf(const formats::ActAnimation& animation,
                                           std::size_t action_index, std::size_t frame_index) {
  const std::size_t action_count = animation.actions().size();
  if (action_count == 0) {
    return {0, 0};
  }
  const std::size_t action = action_index % action_count;
  const std::size_t frame_count = animation.actions()[action].frames.size();
  return {action, frame_index < frame_count ? frame_index : 0};
}

// L'action que joue le sprite d'une arme tenue seule quand le corps en joue
// une à deux armes : l'attaque 3 devient l'attaque 2, direction gardée.
std::size_t SingleWeaponAction(std::size_t action_index) {
  constexpr std::size_t kFirstAttack3 = kAnimationAttack3 * kDirectionCount;
  constexpr std::size_t kFirstAttack2 = kAnimationAttack2 * kDirectionCount;
  if (action_index < kFirstAttack3 || action_index >= kFirstAttack3 + kDirectionCount) {
    return action_index;
  }
  return kFirstAttack2 + (action_index - kFirstAttack3);
}

// La cadence d'une pièce dont le .act n'en déclare aucune, en unités RO.
constexpr std::int32_t kDefaultAlternateCadence = 4;

bool IsHeadgear(ActorPart part) {
  return part == ActorPart::kHeadgearLow || part == ActorPart::kHeadgearMiddle ||
         part == ActorPart::kHeadgearTop;
}

// Une pièce dont la géométrie est résolue, en attente de son rang de dessin.
struct PlacedPiece {
  const ActorPiece* piece = nullptr;
  std::vector<SpriteQuad> quads;
};

// Le fichier son que cette image déclenche, ou rien.
//
// ⚠ Une entrée de la liste qui ne finit pas par « .wav » n'est PAS un son :
// les fichiers en portent qui valent « atk » ou un chiffre, laissés là par
// l'outil qui les a produits. Les jouer chercherait des fichiers qui n'existent
// pas, à chaque image.
std::string_view SoundOfFrame(const formats::ActAnimation& animation, std::size_t action_index,
                              std::size_t frame_index) {
  const std::span<const formats::ActAction> actions = animation.actions();
  if (action_index >= actions.size()) {
    return {};
  }
  const std::vector<formats::ActFrame>& frames = actions[action_index].frames;
  if (frame_index >= frames.size()) {
    return {};
  }
  const std::int32_t index = frames[frame_index].sound_index;
  if (index < 0 || static_cast<std::size_t>(index) >= animation.sound_files().size()) {
    return {};
  }
  const std::string& name = animation.sound_files()[static_cast<std::size_t>(index)];
  constexpr std::string_view kWaveExtension = ".wav";
  if (name.size() <= kWaveExtension.size() ||
      core::AsciiLower(name.substr(name.size() - kWaveExtension.size())) != kWaveExtension) {
    return {};
  }
  return name;
}

}  // namespace

std::optional<ActorPieceRequest> ParseActorPieceRequest(std::string_view text,
                                                        std::size_t headgears_already_read) {
  const std::size_t separator = text.find('=');
  if (separator == std::string_view::npos || separator + 1 >= text.size()) {
    return std::nullopt;
  }
  const std::string_view role = text.substr(0, separator);
  ActorPieceRequest request;
  request.base_path = std::string(text.substr(separator + 1));

  if (role == "corps") {
    request.part = ActorPart::kBody;
  } else if (role == "tete" || role == "tête") {
    request.part = ActorPart::kHead;
  } else if (role == "coiffe") {
    // Les trois rangs du client, dans l'ordre d'arrivée. Au-delà, les pièces
    // surnuméraires reprennent le rang du haut plutôt que de sortir de
    // l'échelle des parties.
    constexpr std::array<ActorPart, 3> kHeadgearRanks = {
        ActorPart::kHeadgearLow, ActorPart::kHeadgearMiddle, ActorPart::kHeadgearTop};
    request.part = kHeadgearRanks[std::min(headgears_already_read, kHeadgearRanks.size() - 1)];
  } else if (role == "cape") {
    request.part = ActorPart::kGarment;
  } else if (role == "arme") {
    request.part = ActorPart::kWeapon;
  } else if (role == "trainee" || role == "traînée") {
    request.part = ActorPart::kWeaponTrail;
  } else if (role == "bouclier") {
    request.part = ActorPart::kShield;
  } else {
    return std::nullopt;
  }
  return request;
}

core::Result<std::vector<ActorPieceRequest>> ParseActorRequests(
    std::span<const std::string_view> arguments) {
  std::vector<ActorPieceRequest> requests;
  std::size_t headgears = 0;

  for (const std::string_view argument : arguments) {
    std::optional<ActorPieceRequest> request = ParseActorPieceRequest(argument, headgears);
    if (!request.has_value()) {
      return core::Fail(core::ErrorCode::kCorruptData,
                        "pièce d'acteur incomprise : " + std::string(argument) +
                            " (rôles : corps, tete, coiffe, cape, arme, trainee, bouclier)");
    }
    if (IsHeadgear(request->part)) {
      ++headgears;
    }
    requests.push_back(std::move(*request));
  }

  const auto body = std::ranges::find_if(
      requests, [](const ActorPieceRequest& piece) { return piece.part == ActorPart::kBody; });
  if (body == requests.end()) {
    return core::Fail(core::ErrorCode::kCorruptData, "aucun corps donné (corps=<chemin>)");
  }
  std::rotate(requests.begin(), body, body + 1);

  const auto head = std::ranges::find_if(
      requests, [](const ActorPieceRequest& piece) { return piece.part == ActorPart::kHead; });
  if (head != requests.end()) {
    std::rotate(requests.begin() + 1, head, head + 1);
  }
  return requests;
}

core::Result<std::vector<LoadedActorSprite>> LoadActorSprites(
    vfs::Vfs& vfs, std::span<const ActorPieceRequest> requests) {
  std::vector<LoadedActorSprite> loaded;
  loaded.reserve(requests.size());
  for (const ActorPieceRequest& request : requests) {
    const std::string_view base = request.base_path;
    // Read, et non ReadRaw : ces chemins viennent d'un être humain ou d'une
    // configuration, donc de l'UTF-8.
    auto spr_bytes = vfs.Read(std::string(base) + ".spr");
    if (!spr_bytes) {
      return std::unexpected(spr_bytes.error());
    }
    auto act_bytes = vfs.Read(std::string(base) + ".act");
    if (!act_bytes) {
      return std::unexpected(act_bytes.error());
    }
    auto sprite = formats::SprSprite::Parse(*spr_bytes);
    if (!sprite) {
      return std::unexpected(sprite.error());
    }
    auto animation = formats::ActAnimation::Parse(*act_bytes);
    if (!animation) {
      return std::unexpected(animation.error());
    }
    loaded.push_back(LoadedActorSprite{std::move(*sprite), std::move(*animation)});
  }
  return loaded;
}

ActorPiece DescribeActorPiece(const LoadedActorSprite& loaded, ActorPart part,
                              const formats::Palette* palette) {
  ActorPiece piece;
  piece.sprite = &loaded.sprite;
  piece.animation = &loaded.animation;
  piece.palette = palette;
  piece.part = part;
  piece.is_head = part == ActorPart::kHead;
  piece.attach = IsHeadgear(part) ? AttachTo::kHead : AttachTo::kBody;
  return piece;
}

std::vector<ActorPiece> DescribeActorPieces(std::span<LoadedActorSprite> loaded,
                                            std::span<const ActorPieceRequest> requests) {
  std::vector<ActorPiece> pieces;
  pieces.reserve(loaded.size());
  for (std::size_t index = 0; index < loaded.size() && index < requests.size(); ++index) {
    pieces.push_back(DescribeActorPiece(loaded[index], requests[index].part, nullptr));
  }
  return pieces;
}

bool HasImage(const SpriteImageKey& key) {
  // Les mêmes gardes que ResolveImage, dans le même ordre, mais SANS décoder :
  // la question se pose une fois par calque et par image, et appliquer une
  // palette pour n'en lire que la taille se paierait à chaque tic.
  if (key.sprite == nullptr || key.image_index < 0) {
    return false;
  }
  const auto index = static_cast<std::size_t>(key.image_index);
  if (key.section == 1) {
    return index < key.sprite->bgra_frames().size() && !key.sprite->bgra_frames()[index].empty();
  }
  if (index >= key.sprite->indexed_frames().size()) {
    return false;
  }
  if (key.palette == nullptr && !key.sprite->palette().has_value()) {
    return false;
  }
  const formats::SprIndexedFrame& frame = key.sprite->indexed_frames()[index];
  return frame.width > 0 && frame.height > 0 && !frame.indices.empty();
}

formats::Image WithClientAlphaQuartet(formats::Image image) {
  // `alpha / 16 * 17` est exactement ce que la carte rend d'un quartet :
  // 255 reste 255, 240 remonte à 255, 16 devient 17, et 15 tombe à ZÉRO.
  constexpr std::size_t kChannelsPerPixel = 4;
  constexpr std::size_t kAlphaChannel = 3;
  constexpr std::uint8_t kAlphaSteps = 16;
  constexpr std::uint8_t kAlphaStepValue = 17;
  for (std::size_t pixel = 0; pixel + kChannelsPerPixel <= image.pixels.size();
       pixel += kChannelsPerPixel) {
    std::uint8_t& alpha = image.pixels[pixel + kAlphaChannel];
    alpha = static_cast<std::uint8_t>(alpha / kAlphaSteps * kAlphaStepValue);
  }
  return image;
}

formats::Image ResolveImage(const SpriteImageKey& key) {
  if (key.sprite == nullptr || key.image_index < 0) {
    return {};
  }
  const auto index = static_cast<std::size_t>(key.image_index);

  if (key.section == 1) {
    if (index >= key.sprite->bgra_frames().size()) {
      return {};
    }
    // Une image BGRA porte ses couleurs en propre : aucune palette ne s'y
    // applique, pas même une palette externe. Son ALPHA, lui, se ramène aux
    // quatre bits du client — voir `WithClientAlphaQuartet`.
    return WithClientAlphaQuartet(key.sprite->bgra_frames()[index]);
  }

  if (index >= key.sprite->indexed_frames().size()) {
    return {};
  }
  const formats::Palette* palette = key.palette;
  if (palette == nullptr) {
    if (!key.sprite->palette().has_value()) {
      return {};
    }
    palette = &key.sprite->palette().value();
  }
  return formats::ApplyPalette(key.sprite->indexed_frames()[index], *palette);
}

std::vector<formats::Image> ResolveImages(const ComposedActor& actor) {
  std::vector<formats::Image> images;
  images.reserve(actor.sources.size());
  for (const SpriteImageKey& key : actor.sources) {
    images.push_back(ResolveImage(key));
  }
  return images;
}

std::optional<std::uint8_t> PaletteIndexAt(const ComposedActor& actor, glm::vec2 at) {
  // ⚠ DU DERNIER AU PREMIER : les quads se peignent dans l'ordre, donc le
  // dernier est CELUI DU DESSUS — et c'est lui que le regard désigne.
  for (std::size_t rank = actor.quads.size(); rank-- > 0;) {
    if (rank >= actor.sources.size()) {
      continue;
    }
    const SpriteQuad& quad = actor.quads[rank];
    const SpriteImageKey& source = actor.sources[rank];
    // 🔴 SECTION INDEXÉE UNIQUEMENT : une image BGRA porte ses couleurs par
    // pixel et n'a AUCUN index. Il n'y a rien à y désigner.
    if (source.sprite == nullptr || quad.section != 0 || quad.image_index < 0) {
      continue;
    }
    const auto image = static_cast<std::size_t>(quad.image_index);
    if (image >= source.sprite->indexed_frames().size()) {
      continue;
    }
    const formats::SprIndexedFrame& frame = source.sprite->indexed_frames()[image];
    if (frame.width <= 0 || frame.height <= 0 || frame.indices.empty()) {
      continue;
    }

    // ⚠ PAR LES ARÊTES DU QUAD, et non par ses bornes : un calque peut porter
    // une ROTATION, et un rectangle englobant répondrait alors pour des points
    // que le calque ne couvre pas. Les coins sont rangés haut-gauche,
    // haut-droit, bas-droit, bas-gauche : deux vecteurs suffisent.
    const glm::vec2 origin = quad.corners[0];
    const glm::vec2 across = quad.corners[1] - origin;
    const glm::vec2 down = quad.corners[3] - origin;
    const float determinant = across.x * down.y - across.y * down.x;
    if (determinant == 0.0F) {
      continue;
    }
    const glm::vec2 local = at - origin;
    float u = (local.x * down.y - local.y * down.x) / determinant;
    const float v = (across.x * local.y - across.y * local.x) / determinant;
    if (u < 0.0F || u >= 1.0F || v < 0.0F || v >= 1.0F) {
      continue;
    }
    // Le miroir échange les abscisses de TEXTURE, il ne retourne pas le quad.
    if (quad.mirrored) {
      u = 1.0F - u;
    }
    const auto column = std::clamp(static_cast<std::int32_t>(u * static_cast<float>(frame.width)),
                                   0, frame.width - 1);
    const auto row = std::clamp(static_cast<std::int32_t>(v * static_cast<float>(frame.height)), 0,
                                frame.height - 1);
    const std::uint8_t index =
        frame.indices[static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.width) +
                      static_cast<std::size_t>(column)];
    // L'index zéro est le transparent : le calque du dessous répondra.
    if (index == 0) {
      continue;
    }
    return index;
  }
  return std::nullopt;
}

bool HasOwnAnimation(ActorPart part) noexcept {
  return IsHeadgear(part) || part == ActorPart::kGarment;
}

std::optional<std::size_t> AlternateFrame(const formats::ActAnimation& reference,
                                          const formats::ActAnimation& piece,
                                          std::size_t action_index, std::size_t frame_index,
                                          float elapsed_ms) {
  if (reference.actions().empty() || piece.actions().empty()) {
    return std::nullopt;
  }
  const formats::ActAction& reference_action =
      reference.actions()[action_index % reference.actions().size()];
  const formats::ActAction& piece_action = piece.actions()[action_index % piece.actions().size()];
  const std::size_t reference_frames = reference_action.frames.size();
  const std::size_t piece_frames = piece_action.frames.size();
  // La règle demande un MULTIPLE STRICT : à nombre d'images égal ou moindre, la
  // pièce suit l'acteur comme n'importe quelle autre.
  if (reference_frames == 0 || piece_frames <= reference_frames ||
      piece_frames % reference_frames != 0) {
    return std::nullopt;
  }
  const std::size_t multiple = piece_frames / reference_frames;
  // La cadence est celle de la PIÈCE, tronquée à l'entier comme le fait le
  // client ; nulle, elle vaut quatre.
  const auto declared = static_cast<std::int32_t>(piece_action.speed_units);
  const std::int32_t cadence = declared > 0 ? declared : kDefaultAlternateCadence;
  const float ticks = std::max(0.0F, elapsed_ms) / formats::kMillisecondsPerSpeedUnit;
  const auto step = static_cast<std::size_t>(ticks / static_cast<float>(cadence));
  return frame_index * multiple + step % multiple;
}

namespace {

// LE COIN D'UN CALQUE, tel que le client le garde en mémoire : le fichier en
// donne le CENTRE, son chargeur retranche la demi-image arrondie au-dessus,
// grandie de l'échelle du calque, puis divise par cette échelle et tronque.
float LayerCorner(float center, std::int32_t extent, float layer_scale) {
  const std::int32_t half_up = extent / 2 + extent % 2;
  if (layer_scale == 0.0F) {
    return center;
  }
  return std::trunc((center - static_cast<float>(half_up) * layer_scale) / layer_scale);
}

}  // namespace

std::optional<glm::vec2> HeadPlace(const formats::ActAnimation& body, std::size_t body_action,
                                   std::size_t body_frame, const formats::SprSprite& head_sprite,
                                   const formats::ActAnimation& head, std::size_t head_action,
                                   std::size_t head_frame) {
  const std::span<const formats::ActAction> actions = head.actions();
  if (head_action >= actions.size() || head_frame >= actions[head_action].frames.size()) {
    return std::nullopt;
  }
  const std::vector<formats::ActLayer>& layers = actions[head_action].frames[head_frame].layers;
  if (layers.size() <= kHeadLayerRank || layers[kHeadLayerRank].frame_index < 0) {
    return std::nullopt;
  }
  const formats::ActLayer& layer = layers[kHeadLayerRank];
  const auto image = static_cast<std::size_t>(layer.frame_index);
  // À partir de la 2.5, le chargeur prend les dimensions que le fichier DÉCLARE ;
  // avant, celles de l'image du `.spr`.
  std::int32_t width = 0;
  std::int32_t height = 0;
  if (layer.declared_width.has_value() && layer.declared_height.has_value()) {
    width = *layer.declared_width;
    height = *layer.declared_height;
  } else if (layer.section == 0 && image < head_sprite.indexed_frames().size()) {
    width = head_sprite.indexed_frames()[image].width;
    height = head_sprite.indexed_frames()[image].height;
  } else if (layer.section != 0 && image < head_sprite.bgra_frames().size()) {
    width = head_sprite.bgra_frames()[image].width;
    height = head_sprite.bgra_frames()[image].height;
  }
  return glm::vec2(LayerCorner(layer.offset_x, width, layer.scale_x),
                   LayerCorner(layer.offset_y, height, layer.scale_y)) +
         AttachOffset(FrameAnchor(body, body_action, body_frame),
                      FrameAnchor(head, head_action, head_frame));
}

ComposedActor ComposeActor(const ActorPiece& body, std::span<const ActorPiece> attachments,
                           std::size_t action_index, std::size_t frame_index, bool garment_behind,
                           std::span<const std::size_t> attachment_frames) {
  ComposedActor composed;
  if (body.sprite == nullptr || body.animation == nullptr) {
    return composed;
  }

  // PREMIÈRE PASSE : les ancrages, dans l'ordre où l'appelant a donné les
  // pièces. Cet ordre porte les DÉPENDANCES — une coiffe a besoin du décalage
  // de la tête. Le séparer de l'ordre de DESSIN évite qu'un changement de
  // direction, qui réordonne le dessin, ne casse silencieusement l'ancrage.
  const auto [body_action, body_frame] = PoseOf(*body.animation, action_index, frame_index);
  const auto body_anchor = FrameAnchor(*body.animation, body_action, body_frame);
  composed.action_index = body_action;
  composed.frame_index = body_frame;
  composed.sound = SoundOfFrame(*body.animation, body_action, body_frame);

  std::vector<PlacedPiece> placed;
  placed.reserve(attachments.size() + 1);
  placed.push_back(
      PlacedPiece{&body, BuildFrameQuads(*body.sprite, *body.animation, body_action, body_frame)});

  // Le décalage de la TÊTE, et l'ancre par laquelle elle porte ce qui s'y
  // accroche. Les deux vont ensemble : une coiffe reprend le premier, puis se
  // pose sur la seconde.
  glm::vec2 head_offset{0.0F, 0.0F};
  std::optional<formats::ActAnchor> head_anchor;
  bool head_seen = false;

  for (std::size_t index = 0; index < attachments.size(); ++index) {
    const ActorPiece& piece = attachments[index];
    if (piece.sprite == nullptr || piece.animation == nullptr) {
      continue;
    }
    const std::size_t wanted =
        index < attachment_frames.size() ? attachment_frames[index] : frame_index;
    const auto [action, frame] = PoseOf(
        *piece.animation,
        piece.single_weapon_sprite ? SingleWeaponAction(action_index) : action_index, wanted);
    std::vector<SpriteQuad> quads = BuildFrameQuads(*piece.sprite, *piece.animation, action, frame);
    if (quads.empty()) {
      continue;
    }

    // L'ancrage suit une CHAÎNE — corps, puis tête, puis coiffe : chaque pièce
    // se pose sur l'ancre de celle qui la PORTE, et hérite de son décalage.
    //
    // ⚠ Une coiffe s'accroche à l'ancre de la TÊTE, pas à celle du corps.
    // Reprendre celle du corps ajoute une seconde fois l'écart tête-corps, et
    // le chapeau part de travers.
    const std::optional<formats::ActAnchor> anchor = FrameAnchor(*piece.animation, action, frame);
    const bool on_head = piece.attach == AttachTo::kHead && head_seen;
    const glm::vec2 offset = on_head ? head_offset + AttachOffset(head_anchor, anchor)
                                     : AttachOffset(body_anchor, anchor);

    if (piece.is_head) {
      head_offset = offset;
      head_anchor = anchor;
      head_seen = true;
      composed.head_place = HeadPlace(*body.animation, body_action, body_frame, *piece.sprite,
                                      *piece.animation, action, frame);
    }

    for (SpriteQuad& quad : quads) {
      for (glm::vec2& corner : quad.corners) {
        corner += offset;
      }
    }
    placed.push_back(PlacedPiece{&piece, std::move(quads)});
  }

  // SECONDE PASSE : l'ordre de dessin, de l'arrière vers l'avant, qui dépend
  // de la direction. Voir actor_layers.h pour la règle du jeu.
  std::vector<ActorPart> present;
  present.reserve(placed.size());
  bool off_hand_weapon = false;
  for (const PlacedPiece& entry : placed) {
    present.push_back(entry.piece->part);
    off_hand_weapon = off_hand_weapon || (entry.piece->part == ActorPart::kWeaponTrail &&
                                          entry.piece->single_weapon_sprite);
  }
  const std::size_t direction = action_index % kDirectionCount;

  for (const ActorPart part : PartDrawOrder(present, direction, garment_behind, off_hand_weapon)) {
    // Plusieurs pièces peuvent porter la même partie — un joueur ne cumule pas
    // deux coiffes du même rang, mais rien n'empêche l'appelant d'en donner
    // deux ; elles se dessinent alors dans l'ordre où elles ont été fournies.
    for (PlacedPiece& entry : placed) {
      if (entry.piece->part != part || entry.quads.empty()) {
        continue;
      }
      for (const SpriteQuad& quad : entry.quads) {
        composed.sources.push_back(SpriteImageKey{entry.piece->sprite, entry.piece->palette,
                                                  quad.section, quad.image_index,
                                                  entry.piece->palette_serial});
        composed.quads.push_back(quad);
        composed.parts.push_back(part);
      }
      entry.quads.clear();
    }
  }
  return composed;
}

ActorPart PartOfQuad(const ComposedActor& composed, std::size_t index) noexcept {
  return index < composed.parts.size() ? composed.parts[index] : ActorPart::kBody;
}

void LayerDrawsOf(const ComposedActor& composed, ActorPartSet shown_parts,
                  ActorPartSet additive_parts, bool doubles_weapon, std::vector<LayerDraw>& out) {
  out.clear();
  for (std::size_t index = 0; index < composed.quads.size(); ++index) {
    const ActorPart part = PartOfQuad(composed, index);
    const ActorPartSet bit = PartBit(part);
    if ((shown_parts & bit) == 0) {
      continue;
    }
    out.push_back(LayerDraw{index, (additive_parts & bit) != 0, part});
    if (doubles_weapon && part == ActorPart::kWeapon) {
      out.push_back(LayerDraw{index, true, part});
    }
  }
}

}  // namespace ro::render
