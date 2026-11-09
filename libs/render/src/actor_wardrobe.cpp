#include "ro/render/actor_wardrobe.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_scale.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/vanish_fade.h"

namespace ro::render {
namespace {

constexpr std::string_view kSpriteExtension = ".spr";
constexpr std::string_view kAnimationExtension = ".act";

// Le nombre d'animations d'un .act : ses actions vont par huit, une par
// direction. Zéro sans corps ; un pour un .act qui n'en porte pas huit — un
// objet au sol n'a qu'une action, vue de partout pareil.
std::size_t AnimationCount(std::span<const ActorPiece> pieces) {
  if (pieces.empty() || pieces.front().animation == nullptr) {
    return 0;
  }
  const std::size_t actions = pieces.front().animation->actions().size();
  if (actions == 0) {
    return 0;
  }
  return std::max<std::size_t>(1, actions / kDirectionCount);
}

// Grandit ou rétrécit un acteur composé autour de son origine — tous ses quads
// d'un coup, décalages d'ancrage compris, comme le client le fait à l'écran.
void ScaleQuads(ComposedActor& composed, float scale) {
  if (scale == 1.0F) {
    return;
  }
  for (SpriteQuad& quad : composed.quads) {
    for (glm::vec2& corner : quad.corners) {
      corner *= scale;
    }
  }
}

// L'ombre au sol, composée une fois pour toutes.
//
// Elle ne suit ni l'action ni l'image : le client en dessine toujours la
// première image de la première action de shadow.act, grandie de l'échelle de
// la classe.
ComposedActor ComposeShadow(const ActorPiece& shadow, float scale) {
  ComposedActor composed;
  if (shadow.sprite == nullptr || shadow.animation == nullptr || scale <= 0.0F) {
    return composed;
  }
  composed.quads = BuildFrameQuads(*shadow.sprite, *shadow.animation, 0, 0);
  for (const SpriteQuad& quad : composed.quads) {
    composed.sources.push_back(
        SpriteImageKey{shadow.sprite, shadow.palette, quad.section, quad.image_index});
  }
  ScaleQuads(composed, scale);
  return composed;
}

// L'index d'action d'une animation vue dans une direction, borné à ce que le
// .act porte.
std::size_t BoundedActionIndex(const formats::ActAnimation& animation, std::size_t played,
                               std::size_t direction) {
  return std::min(ActionIndex(played, direction), animation.actions().size() - 1);
}

}  // namespace

ActorOutfit::ActorOutfit(std::vector<ActorPiece> pieces, GarmentPlacement garment,
                         ActorPiece shadow, float shadow_scale, float body_scale)
    : pieces_(std::move(pieces)),
      garment_(std::move(garment)),
      // L'ombre porte les DEUX échelles, comme le client les multiplie
      // l'une par l'autre : l'échelle du sprite de l'acteur, puis celle de sa
      // classe. Un bébé aux trois quarts
      // a donc une ombre aux trois quarts. Le PARAMÈTRE sert ici, et non le
      // membre : les membres s'initialisent dans l'ordre où ils sont déclarés,
      // et body_scale_ vient après shadow_.
      shadow_(ComposeShadow(shadow, shadow_scale * body_scale)),
      body_scale_(body_scale),
      composed_(AnimationCount(pieces_)) {
  // La TÊTE sert de référence à l'animation propre des coiffes, et non le
  // corps : c'est elle que le client compare.
  for (const ActorPiece& piece : pieces_) {
    if (piece.is_head) {
      head_animation_ = piece.animation;
      break;
    }
  }
}

const ComposedActor& ActorOutfit::Composed(std::size_t animation, std::size_t direction,
                                           float elapsed_ms, bool loop,
                                           std::optional<std::size_t> held_frame) {
  if (composed_.empty()) {
    return nothing_;
  }
  const std::size_t facing = direction % kDirectionCount;
  const std::size_t played = animation < composed_.size() ? animation : kAnimationIdle;
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  const formats::ActAction& action =
      animation_of_body.actions()[BoundedActionIndex(animation_of_body, played, 0)];
  const std::size_t frame = held_frame.value_or(loop ? FrameAtTime(action, elapsed_ms)
                                                     : FrameAtTimeOnce(action, elapsed_ms));
  const std::span<const ActorPiece> attachments = std::span<const ActorPiece>(pieces_).subspan(1);

  // Les coiffes et la cape peuvent porter leur PROPRE animation, qui court
  // pendant que l'acteur tient sa pose : voir AlternateFrame. Elles se
  // calculent pour la direction zéro, qui sert de témoin au cache — les huit
  // actions d'un même rang portent le même nombre d'images.
  std::vector<std::size_t>& attachment_frames = attachment_frames_;
  attachment_frames.assign(attachments.size(), frame);
  if (head_animation_ != nullptr) {
    const std::size_t witness = BoundedActionIndex(animation_of_body, played, 0);
    for (std::size_t index = 0; index < attachments.size(); ++index) {
      const ActorPiece& piece = attachments[index];
      if (piece.animation == nullptr || !HasOwnAnimation(piece.part)) {
        continue;
      }
      const std::optional<std::size_t> alternate =
          AlternateFrame(*head_animation_, *piece.animation, witness, frame, elapsed_ms);
      if (alternate.has_value()) {
        attachment_frames[index] = *alternate;
      }
    }
  }

  auto found = composed_[played].find(
      FrameKeyOrder::Probe(frame, std::span<const std::size_t>(attachment_frames)));
  if (found == composed_[played].end()) {
    std::array<ComposedActor, kDirectionCount> directions;
    for (std::size_t each = 0; each < kDirectionCount; ++each) {
      const std::size_t action_index = BoundedActionIndex(animation_of_body, played, each);
      directions[each] = ComposeActor(pieces_.front(), attachments, action_index, frame,
                                      garment_.DrawsBehind(action_index, frame), attachment_frames);
      ScaleQuads(directions[each], body_scale_);
    }
    found =
        composed_[played].emplace(FrameKey(frame, attachment_frames), std::move(directions)).first;
  }
  return found->second[facing];
}

float ActorOutfit::AnimationDurationMs(std::size_t animation) const {
  if (animation >= composed_.size()) {
    return 0.0F;
  }
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  const formats::ActAction& action =
      animation_of_body.actions()[BoundedActionIndex(animation_of_body, animation, 0)];
  return static_cast<float>(action.frames.size()) * action.frame_interval_ms();
}

float ActorOutfit::FrameIntervalMs(std::size_t animation) const {
  if (animation >= composed_.size()) {
    return 0.0F;
  }
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  return animation_of_body.actions()[BoundedActionIndex(animation_of_body, animation, 0)]
      .frame_interval_ms();
}

std::size_t ActorOutfit::FrameCount(std::size_t animation) const {
  if (composed_.empty()) {
    return 0;
  }
  const std::size_t played = animation < composed_.size() ? animation : kAnimationIdle;
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  return animation_of_body.actions()[BoundedActionIndex(animation_of_body, played, 0)]
      .frames.size();
}

std::size_t ActorOutfit::BodyActionFrameCount(std::size_t animation, std::size_t direction) const {
  if (pieces_.empty() || pieces_.front().animation == nullptr) {
    return 1;
  }
  const std::vector<formats::ActAction>& actions = pieces_.front().animation->actions();
  const std::size_t action = ActionIndex(animation, direction);
  if (action >= actions.size()) {
    return 1;
  }
  return std::max<std::size_t>(actions[action].frames.size(), 1);
}

float ActorOutfit::BodyActionDelayUnits(std::size_t animation, std::size_t direction) const {
  if (pieces_.empty() || pieces_.front().animation == nullptr) {
    return kUndeclaredActionDelayUnits;
  }
  const std::vector<formats::ActAction>& actions = pieces_.front().animation->actions();
  const std::size_t action = ActionIndex(animation, direction);
  return action < actions.size() ? actions[action].speed_units : kUndeclaredActionDelayUnits;
}

std::vector<AlternateClock> ActorOutfit::OwnClocks(std::size_t animation) const {
  std::vector<AlternateClock> clocks;
  if (composed_.empty() || head_animation_ == nullptr) {
    return clocks;
  }
  // ⚠ LE MÊME TÉMOIN QUE `Composed` : l'action de la direction zéro, sur le
  // rang réellement joué.
  const std::size_t played = animation < composed_.size() ? animation : kAnimationIdle;
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  const std::size_t witness = BoundedActionIndex(animation_of_body, played, 0);
  for (const ActorPiece& piece : std::span<const ActorPiece>(pieces_).subspan(1)) {
    if (piece.animation == nullptr || !HasOwnAnimation(piece.part)) {
      continue;
    }
    if (const std::optional<AlternateClock> clock =
            AlternateClockOf(*head_animation_, *piece.animation, witness);
        clock.has_value()) {
      clocks.push_back(*clock);
    }
  }
  return clocks;
}

std::size_t ActorOutfit::AttackSoundFrame(std::size_t animation) const {
  if (composed_.empty()) {
    return 0;
  }
  const std::size_t played = animation < composed_.size() ? animation : kAnimationIdle;
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  return ro::render::AttackSoundFrame(animation_of_body,
                                      BoundedActionIndex(animation_of_body, played, 0));
}

std::optional<glm::vec2> ActorOutfit::HeadRestingPlace(const ActorOutfit& foot) const {
  const auto head = std::ranges::find_if(pieces_, [](const ActorPiece& piece) {
    return piece.is_head && piece.sprite != nullptr && piece.animation != nullptr;
  });
  if (head == pieces_.end() || foot.pieces_.empty() || foot.pieces_.front().animation == nullptr) {
    return std::nullopt;
  }
  return HeadPlace(*foot.pieces_.front().animation, 0, 0, *head->sprite, *head->animation, 0, 0);
}

std::optional<float> ActorOutfit::BodyFirstLayerY() const {
  if (pieces_.empty() || pieces_.front().animation == nullptr) {
    return std::nullopt;
  }
  const std::vector<formats::ActAction>& actions = pieces_.front().animation->actions();
  if (actions.empty() || actions.front().frames.empty() ||
      actions.front().frames.front().layers.empty()) {
    return std::nullopt;
  }
  const formats::ActLayer& layer = actions.front().frames.front().layers.front();
  // ⚠ LE CHARGEUR DU CLIENT RETRANCHE LA DEMI-IMAGE : c'est cette ordonnée-là
  // qu'il lit, et non celle du fichier. Un calque vide n'a pas d'image.
  float height = 0.0F;
  if (const formats::SprSprite* sprite = pieces_.front().sprite;
      sprite != nullptr && layer.frame_index >= 0) {
    const auto index = static_cast<std::size_t>(layer.frame_index);
    if (layer.section == 0 && index < sprite->indexed_frames().size()) {
      height = static_cast<float>(sprite->indexed_frames()[index].height);
    } else if (layer.section != 0 && index < sprite->bgra_frames().size()) {
      height = static_cast<float>(sprite->bgra_frames()[index].height);
    }
  }
  return layer.offset_y - height * 0.5F;
}

bool ActorLookOrder::operator()(const ActorLook& left, const ActorLook& right) const noexcept {
  return std::tie(left.player, left.item, left.job, left.sex, left.hair, left.body_style,
                  left.headgears, left.weapon, left.shield, left.robe, left.cash_mount,
                  left.hair_palette, left.body_palette, left.body_palette_names_file, left.style,
                  left.orc_head, left.madogear_suit, left.pet_accessory, left.gm_outfit) <
         std::tie(right.player, right.item, right.job, right.sex, right.hair, right.body_style,
                  right.headgears, right.weapon, right.shield, right.robe, right.cash_mount,
                  right.hair_palette, right.body_palette, right.body_palette_names_file,
                  right.style, right.orc_head, right.madogear_suit, right.pet_accessory,
                  right.gm_outfit);
}

ActorWardrobe::ActorWardrobe(vfs::Vfs& vfs, SpriteCatalog catalog)
    : vfs_(vfs), catalog_(std::move(catalog)) {
  // ⚠ SON ABSENCE NE COUPE RIEN : aucune créature n'est alors grandie ni parée.
  if (auto sizes = vfs::MonsterSizeEffects::Load(vfs); sizes.has_value()) {
    core::LogInfo("tailles d'office : {} créature(s)", sizes->entries().size());
    monster_size_effects_ = std::move(*sizes);
  } else {
    core::LogInfo("tailles d'office : {} — aucune créature n'est grandie",
                  core::Describe(sizes.error()));
  }
}

ActorOutfit* ActorWardrobe::OutfitFor(const ActorLook& look) {
  auto found = outfits_.find(look);
  if (found == outfits_.end()) {
    const float scale = ShadowScaleFor(catalog_, look);
    ActorPiece shadow;
    if (scale > 0.0F) {
      // L'ombre manque sans un mot : elle n'est pas ce qui fait un acteur.
      if (const LoadedActorSprite* loaded =
              Sprite(std::string(kShadowSpritePath), std::string(kShadowSpritePath), false);
          loaded != nullptr) {
        shadow = DescribeActorPiece(*loaded, ActorPart::kBody, nullptr);
      }
    }
    // Un objet au sol n'est pas un acteur : son « métier » est un numéro
    // d'objet, et les cartes tombent dans les numéros des bébés.
    float body_scale = look.item ? 1.0F : BodyScaleFor(look.job);
    // LA TAILLE D'OFFICE d'une créature grandit son sprite, et son ombre avec.
    // Un joueur ne se dessine pas par ce chemin chez le client : elle ne le
    // touche pas.
    if (!look.item && !look.player && monster_size_effects_.has_value()) {
      body_scale *= monster_size_effects_->ScaleOf(look.job);
    }
    found = outfits_
                .emplace(look, ActorOutfit(Dress(look), GarmentPlacementFor(catalog_, look), shadow,
                                           scale, body_scale))
                .first;
  }
  return found->second.pieces().empty() ? nullptr : &found->second;
}

std::vector<ActorPiece> ActorWardrobe::Dress(const ActorLook& look) {
  std::vector<ActorPiece> pieces;
  // La peau se décide sur le corps, qui vient en premier : elle vaut ensuite
  // pour la tête.
  bool skin = false;
  for (const ActorPieceFiles& files : ActorFiles(catalog_, look)) {
    // Une pièce facultative — ce que les mains tiennent, la cape — manque sans
    // un mot, comme chez le client, qui la sonde.
    FirstFound found = FirstLoaded(files.candidates, files.animation_path, !files.optional);
    // UN .ACT DE REMPLACEMENT ABSENT NE COÛTE PAS LE CORPS : il reprend son
    // propre .act. L'accessoire d'un pet disparaît, pas le pet.
    if (found.loaded == nullptr && files.part == ActorPart::kBody &&
        !files.animation_path.empty()) {
      found = FirstLoaded(files.candidates, {}, true);
    }
    if (found.loaded == nullptr) {
      // Sans corps, rien ne s'ancre : l'acteur ne se montre pas. Une pièce
      // rapportée, elle, s'omet seule.
      if (files.part == ActorPart::kBody) {
        return {};
      }
      continue;
    }
    if (files.part == ActorPart::kBody) {
      skin = look.player && !look.gm_outfit && !IsDoramJob(look.job) &&
             SkinAdjustable(files.candidates[found.rank]);
    }
    const formats::Palette* official =
        files.palette_path.empty() ? nullptr : Palette(files.palette_path);
    const formats::Palette* palette = official;
    // UNE PIÈCE GARDE LES COULEURS QUE SA PALETTE OFFICIELLE NE DÉFINIT PAS, le
    // corps sous sa teinte de vêtement comme la tête sous sa couleur de
    // cheveux : l'une et l'autre laissent vides des index que certains sprites
    // emploient, et qui sortiraient noirs (`MergeServerPalette`).
    if (palette != nullptr && found.loaded->sprite.palette().has_value()) {
      palette = &actor_palettes_.Base(found.loaded->sprite, palette);
    }
    pieces.push_back(DescribeActorPiece(*found.loaded, files.part, palette));
    Tint(look, files.part, official, skin, *found.loaded, pieces.back());
  }
  SplitDualWield(look, pieces);
  return pieces;
}

void ActorWardrobe::SplitDualWield(const ActorLook& look, std::vector<ActorPiece>& pieces) {
  const std::optional<DualWieldFiles> dual = DualWieldSpritePaths(catalog_, look);
  if (!dual.has_value()) {
    return;
  }
  // Le premier candidat qui se charge, comme pour toute pièce tenue : une arme
  // absente de l'installation manque sans un mot.
  const LoadedActorSprite* off_hand = FirstLoaded(dual->off_hand, {}, false).loaded;
  const LoadedActorSprite* main_hand = FirstLoaded(dual->main_hand, {}, false).loaded;
  // DEUX MAINS SOUS LE MÊME SPRITE se dessineraient l'une sur l'autre, au même
  // ancrage : on n'en verrait qu'une. La pièce du couple, qui place chaque
  // main, vaut mieux, et reste.
  if (off_hand == nullptr || off_hand == main_hand) {
    return;
  }
  std::erase_if(pieces, [](const ActorPiece& piece) {
    return piece.part == ActorPart::kWeapon || piece.part == ActorPart::kWeaponTrail;
  });
  const auto held = [&pieces](const LoadedActorSprite& loaded, ActorPart part) {
    pieces.push_back(DescribeActorPiece(loaded, part, nullptr));
    pieces.back().single_weapon_sprite = true;
  };
  if (main_hand != nullptr) {
    held(*main_hand, ActorPart::kWeapon);
  }
  held(*off_hand, ActorPart::kWeaponTrail);
}

const LoadedActorSprite* ActorWardrobe::Sprite(const std::string& sprite_path,
                                               const std::string& animation_path,
                                               bool note_absence) {
  const SpritePaths paths(sprite_path, animation_path);
  if (const auto found = sprites_.find(paths); found != sprites_.end()) {
    return &found->second;
  }
  if (failed_sprites_.contains(paths)) {
    return nullptr;
  }
  auto sprite_bytes = vfs_.Read(sprite_path + std::string(kSpriteExtension));
  auto animation_bytes = vfs_.Read(animation_path + std::string(kAnimationExtension));
  auto sprite = sprite_bytes
                    ? formats::SprSprite::Parse(*sprite_bytes)
                    : core::Result<formats::SprSprite>(std::unexpected(sprite_bytes.error()));
  auto animation =
      animation_bytes
          ? formats::ActAnimation::Parse(*animation_bytes)
          : core::Result<formats::ActAnimation>(std::unexpected(animation_bytes.error()));
  if (!sprite || !animation) {
    failed_sprites_.insert(paths);
    if (note_absence) {
      missing_.push_back(sprite ? animation_path : sprite_path);
    }
    return nullptr;
  }
  const auto inserted =
      sprites_.emplace(paths, LoadedActorSprite{std::move(*sprite), std::move(*animation)});
  return &inserted.first->second;
}

ActorWardrobe::FirstFound ActorWardrobe::FirstLoaded(std::span<const std::string> candidates,
                                                     const std::string& animation_path,
                                                     bool note_absence) {
  for (std::size_t rank = 0; rank < candidates.size(); ++rank) {
    const std::string& sprite_path = candidates[rank];
    if (const LoadedActorSprite* loaded =
            Sprite(sprite_path, animation_path.empty() ? sprite_path : animation_path,
                   note_absence && rank + 1 == candidates.size())) {
      return FirstFound{loaded, rank};
    }
  }
  return {};
}

ActorWardrobe::LoadedBodySprite ActorWardrobe::LoadedBody(const ActorLook& look) {
  // Le corps n'a pas d'animation à part : un `.act` de remplacement est
  // l'affaire de `Dress`, et il ne change pas quel `.spr` se charge.
  std::vector<std::string> candidates = BodySpritePaths(catalog_, look);
  const FirstFound found = FirstLoaded(candidates, {}, false);
  if (found.loaded == nullptr) {
    return {};
  }
  return LoadedBodySprite{found.loaded, std::move(candidates[found.rank])};
}

ActorOutfit* ActorWardrobe::HeadOnly(const ActorLook& look) {
  // ⚠ RÉDUITE À CE QUI DÉCIDE DE LA TÊTE : sans cela, deux joueurs qui
  // essaient la même coupe avec des bottes différentes composeraient chacun la
  // leur, et la grille paierait quatre-vingts sprites par tenue croisée.
  ActorLook key;
  key.player = look.player;
  key.job = look.job;
  key.sex = look.sex;
  key.hair = look.hair;
  key.hair_palette = look.hair_palette;
  if (const auto found = heads_.find(key); found != heads_.end()) {
    return &found->second;
  }
  const std::string path = HeadSpritePath(key);
  // ⚠ L'ABSENCE NE SE NOTE PAS : une grille passe en revue quatre-vingts
  // numéros dont beaucoup n'ont aucun sprite, et les journaliser noierait les
  // manques qui comptent.
  const LoadedActorSprite* head = path.empty() ? nullptr : Sprite(path, path, false);
  if (head == nullptr) {
    return nullptr;
  }
  const std::string palette_path = HairPalettePath(key);
  const formats::Palette* palette = palette_path.empty() ? nullptr : Palette(palette_path);
  if (palette != nullptr && head->sprite.palette().has_value()) {
    palette = &sample_palettes_.Base(head->sprite, palette);
  }
  std::vector<ActorPiece> pieces;
  pieces.push_back(DescribeActorPiece(*head, ActorPart::kHead, palette));
  return &heads_.emplace(key, ActorOutfit(std::move(pieces))).first->second;
}

ActorOutfit* ActorWardrobe::BodySwatch(const ActorLook& look, std::int16_t palette) {
  const LoadedBodySprite body = LoadedBody(look);
  if (body.loaded == nullptr || !body.loaded->sprite.palette().has_value()) {
    return nullptr;
  }
  const std::pair<std::string, std::int16_t> key{body.path, palette};
  if (const auto found = swatches_.find(key); found != swatches_.end()) {
    return &found->second;
  }
  ActorLook official_look;
  official_look.job = look.job;
  official_look.body_palette = palette;
  // La vignette n montre body_<n>.pal : c'est ce que la recette choisira.
  official_look.body_palette_names_file = true;
  const std::string palette_path = BodyPalettePath(official_look);
  const formats::Palette* official = palette_path.empty() ? nullptr : Palette(palette_path);
  if (palette > 0 && official == nullptr) {
    // 🔴 UNE TEINTE ABSENTE NE SE MONTRE PAS NUE. Rendre le sprite sans elle
    // afficherait la couleur d'origine sous un numéro qui promet autre chose,
    // et le joueur choisirait une teinte qu'il ne recevra pas. Une case vide
    // dit la vérité.
    return nullptr;
  }
  std::vector<ActorPiece> pieces;
  pieces.push_back(DescribeActorPiece(*body.loaded, ActorPart::kBody,
                                      &sample_palettes_.Base(body.loaded->sprite, official)));
  return &swatches_.emplace(key, ActorOutfit(std::move(pieces))).first->second;
}

void ActorWardrobe::ForgetSamples() {
  // Les tenues D'ABORD : elles pointent dans les palettes.
  heads_.clear();
  swatches_.clear();
  sample_palettes_ = ActorPalettes();
}

const formats::Palette* ActorWardrobe::Palette(const std::string& path) {
  if (const auto found = palettes_.find(path); found != palettes_.end()) {
    return &found->second;
  }
  if (failed_palettes_.contains(path)) {
    return nullptr;
  }
  auto bytes = vfs_.Read(path);
  auto palette = bytes ? formats::DecodePalette(*bytes)
                       : core::Result<formats::Palette>(std::unexpected(bytes.error()));
  if (!palette) {
    failed_palettes_.insert(path);
    missing_.push_back(path);
    return nullptr;
  }
  return &palettes_.emplace(path, std::move(*palette)).first->second;
}

}  // namespace ro::render
