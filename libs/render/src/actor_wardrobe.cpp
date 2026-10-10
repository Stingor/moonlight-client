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
  std::vector<std::size_t> attachment_frames(attachments.size(), frame);
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

  FrameKey key(frame, std::move(attachment_frames));
  auto found = composed_[played].find(key);
  if (found == composed_[played].end()) {
    std::array<ComposedActor, kDirectionCount> directions;
    for (std::size_t each = 0; each < kDirectionCount; ++each) {
      const std::size_t action_index = BoundedActionIndex(animation_of_body, played, each);
      directions[each] = ComposeActor(pieces_.front(), attachments, action_index, frame,
                                      garment_.DrawsBehind(action_index, frame), key.second);
      ScaleQuads(directions[each], body_scale_);
    }
    found = composed_[played].emplace(std::move(key), std::move(directions)).first;
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

std::size_t ActorOutfit::AttackSoundFrame(std::size_t animation) const {
  if (composed_.empty()) {
    return 0;
  }
  const std::size_t played = animation < composed_.size() ? animation : kAnimationIdle;
  const formats::ActAnimation& animation_of_body = *pieces_.front().animation;
  return ro::render::AttackSoundFrame(animation_of_body,
                                      BoundedActionIndex(animation_of_body, played, 0));
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
                  left.hair_palette, left.body_palette, left.style, left.orc_head,
                  left.madogear_suit, left.pet_accessory) <
         std::tie(right.player, right.item, right.job, right.sex, right.hair, right.body_style,
                  right.headgears, right.weapon, right.shield, right.robe, right.cash_mount,
                  right.hair_palette, right.body_palette, right.style, right.orc_head,
                  right.madogear_suit, right.pet_accessory);
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
  for (const ActorPieceFiles& files : ActorFiles(catalog_, look)) {
    const LoadedActorSprite* loaded = nullptr;
    for (std::size_t rank = 0; rank < files.candidates.size() && loaded == nullptr; ++rank) {
      const std::string& sprite_path = files.candidates[rank];
      const std::string& animation_path =
          files.animation_path.empty() ? sprite_path : files.animation_path;
      // Une pièce facultative — ce que les mains tiennent, la cape — manque
      // sans un mot, comme chez le client, qui la sonde.
      loaded = Sprite(sprite_path, animation_path,
                      !files.optional && rank + 1 == files.candidates.size());
    }
    // UN .ACT DE REMPLACEMENT ABSENT NE COÛTE PAS LE CORPS : il reprend son
    // propre .act. L'accessoire d'un pet disparaît, pas le pet.
    if (loaded == nullptr && files.part == ActorPart::kBody && !files.animation_path.empty()) {
      for (std::size_t rank = 0; rank < files.candidates.size() && loaded == nullptr; ++rank) {
        loaded = Sprite(files.candidates[rank], files.candidates[rank],
                        rank + 1 == files.candidates.size());
      }
    }
    if (loaded == nullptr) {
      // Sans corps, rien ne s'ancre : l'acteur ne se montre pas. Une pièce
      // rapportée, elle, s'omet seule.
      if (files.part == ActorPart::kBody) {
        return {};
      }
      continue;
    }
    const formats::Palette* palette =
        files.palette_path.empty() ? nullptr : Palette(files.palette_path);
    std::uint32_t palette_serial = 0;
    // LE STYLE NE REPEINT QUE LE CORPS : une recette est tirée de SA palette,
    // et les index n'ont aucune signification commune d'une pièce à l'autre.
    if (files.part == ActorPart::kBody && look.style != 0) {
      if (const StyledPalette* styled = StylePalette(look, *loaded, palette)) {
        palette = &styled->palette;
        palette_serial = styled->serial;
      }
    }
    pieces.push_back(DescribeActorPiece(*loaded, files.part, palette));
    pieces.back().palette_serial = palette_serial;
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
  const auto first_loaded = [this](const std::vector<std::string>& candidates) {
    const LoadedActorSprite* loaded = nullptr;
    for (std::size_t rank = 0; rank < candidates.size() && loaded == nullptr; ++rank) {
      loaded = Sprite(candidates[rank], candidates[rank], false);
    }
    return loaded;
  };
  const LoadedActorSprite* off_hand = first_loaded(dual->off_hand);
  const LoadedActorSprite* main_hand = first_loaded(dual->main_hand);
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

std::uint32_t ActorWardrobe::BodyKeyFor(const ActorLook& look) {
  // ⚠ LA MÊME RÈGLE QUE `Dress` : le premier candidat qui se charge gagne, et
  // le corps n'a pas d'animation à part. Les deux boucles doivent rester
  // d'accord, sans quoi la clé désignerait un autre corps que celui qu'on
  // dessine.
  for (const std::string& path : BodySpritePaths(catalog_, look)) {
    if (Sprite(path, path, false) != nullptr) {
      return BodySpriteKey(path);
    }
  }
  return 0;
}

bool ActorWardrobe::SetStyle(std::uint32_t style, const PaletteRecipe& recipe) {
  // ⚠ NI UN IDENTIFIANT TENU PAR UNE PALETTE TOUTE FAITE : elles passent avant
  // les recettes, et l'acteur porterait celle d'un autre.
  if (style == 0 || raw_styles_.contains(style)) {
    return false;
  }
  // ⚠ REFUSÉ PLUTÔT QUE REMPLACÉ : voir l'en-tête. Ce qui est déjà dessiné
  // retient l'adresse de sa palette, et la réécrire sous le même pointeur
  // laisserait des textures d'hier jusqu'à la prochaine éviction.
  return styles_.emplace(style, recipe).second;
}

bool ActorWardrobe::SetStylePalette(std::uint32_t style, formats::Palette palette) {
  if (style == 0 || styles_.contains(style)) {
    return false;
  }
  return raw_styles_.emplace(style, StyledPalette{std::move(palette), ++last_palette_serial_})
      .second;
}

void ActorWardrobe::ForgetStyle(std::uint32_t style) {
  // ⚠ LES DEUX TABLES, SANS COURT-CIRCUIT : un `||` s'arrêterait à la première.
  const bool recipe_known = styles_.erase(style) != 0;
  const bool palette_known = raw_styles_.erase(style) != 0;
  if (!recipe_known && !palette_known) {
    return;
  }
  // Les tenues D'ABORD : elles pointent dans les palettes, et l'ordre inverse
  // laisserait une tenue vivante sur une palette détruite le temps d'une ligne.
  std::erase_if(outfits_, [style](const auto& entry) { return entry.first.style == style; });
  std::erase_if(style_palettes_, [style](const auto& entry) { return entry.first.style == style; });
}

ActorWardrobe::BodyRamps ActorWardrobe::BodyRampsFor(const ActorLook& look) {
  BodyRamps found;
  // ⚠ LA MÊME RÈGLE QUE `Dress` : le premier candidat qui se charge gagne.
  const LoadedActorSprite* body = nullptr;
  for (const std::string& path : BodySpritePaths(catalog_, look)) {
    body = Sprite(path, path, false);
    if (body != nullptr) {
      break;
    }
  }
  if (body == nullptr || !body->sprite.palette().has_value()) {
    return found;
  }
  const std::string palette_path = BodyPalettePath(look);
  const formats::Palette* official = palette_path.empty() ? nullptr : Palette(palette_path);
  found.base = MergeServerPalette(*body->sprite.palette(), official);
  const std::array<int, formats::kPaletteEntryCount> usage = CountIndexUsage(body->sprite);
  found.ramps = DetectRamps(found.base, usage);
  for (const int used : usage) {
    found.pixels_total += used;
  }
  for (const PaletteRamp& ramp : found.ramps) {
    const std::size_t end = std::min<std::size_t>(
        static_cast<std::size_t>(ramp.start) + ramp.length, formats::kPaletteEntryCount);
    for (std::size_t index = ramp.start; index < end; ++index) {
      found.pixels_covered += usage[index];
    }
  }
  return found;
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
  std::vector<ActorPiece> pieces;
  pieces.push_back(DescribeActorPiece(*head, ActorPart::kHead, palette));
  return &heads_.emplace(key, ActorOutfit(std::move(pieces))).first->second;
}

ActorOutfit* ActorWardrobe::BodySwatch(const ActorLook& look, std::int16_t palette) {
  // ⚠ LA MÊME RÈGLE QUE `Dress` : le premier candidat qui se charge gagne.
  const LoadedActorSprite* body = nullptr;
  std::string path;
  for (const std::string& candidate : BodySpritePaths(catalog_, look)) {
    body = Sprite(candidate, candidate, false);
    if (body != nullptr) {
      path = candidate;
      break;
    }
  }
  if (body == nullptr || !body->sprite.palette().has_value()) {
    return nullptr;
  }
  const std::pair<std::string, std::int16_t> key{path, palette};
  if (const auto found = swatches_.find(key); found != swatches_.end()) {
    return &found->second;
  }
  ActorLook official_look;
  official_look.job = look.job;
  official_look.body_palette = palette;
  const std::string palette_path = BodyPalettePath(official_look);
  const formats::Palette* official = palette_path.empty() ? nullptr : Palette(palette_path);
  if (palette > 0 && official == nullptr) {
    // 🔴 UNE TEINTE ABSENTE NE SE MONTRE PAS NUE. Rendre le sprite sans elle
    // afficherait la couleur d'origine sous un numéro qui promet autre chose,
    // et le joueur choisirait une teinte qu'il ne recevra pas. Une case vide
    // dit la vérité.
    return nullptr;
  }
  const formats::Palette& merged =
      swatch_palettes_.emplace(key, MergeServerPalette(*body->sprite.palette(), official))
          .first->second;
  std::vector<ActorPiece> pieces;
  pieces.push_back(DescribeActorPiece(*body, ActorPart::kBody, &merged));
  return &swatches_.emplace(key, ActorOutfit(std::move(pieces))).first->second;
}

const ActorWardrobe::StyledPalette* ActorWardrobe::StylePalette(const ActorLook& look,
                                                                const LoadedActorSprite& body,
                                                                const formats::Palette* official) {
  // ⚠ LES PALETTES TOUTES FAITES D'ABORD : l'appelant a déjà fusionné et peint,
  // et rien de ce qui suit n'aurait de sens sur elles.
  if (const auto raw = raw_styles_.find(look.style); raw != raw_styles_.end()) {
    return &raw->second;
  }
  if (const auto found = style_palettes_.find(look); found != style_palettes_.end()) {
    return &found->second;
  }
  const auto recipe = styles_.find(look.style);
  // Un style qu'on ne connaît pas laisse l'apparence NATIVE. Cela arrive pour
  // de bon : le lot du serveur peut décrire un joueur dont le corps n'est pas
  // encore chargé, et la recette arrive alors avant ce qu'elle repeint.
  if (recipe == styles_.end() || !body.sprite.palette().has_value()) {
    return nullptr;
  }
  // ⚠ LA DÉTECTION SE FAIT SUR LA BASE FUSIONNÉE, jamais sur le sprite nu :
  // c'est l'ordre de la référence, et les frontières des rampes ne sont pas les
  // mêmes des deux côtés de la fusion. Voir l'ADR 0050.
  const formats::Palette base = MergeServerPalette(*body.sprite.palette(), official);
  const std::array<int, formats::kPaletteEntryCount> usage = CountIndexUsage(body.sprite);
  const std::vector<PaletteRamp> ramps = DetectRamps(base, usage);
  return &style_palettes_
              .emplace(look, StyledPalette{ApplyRecipe(base, ramps, recipe->second),
                                           ++last_palette_serial_})
              .first->second;
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
