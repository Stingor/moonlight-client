// Les styles de la garde-robe : la clé du corps, ce qu'il y a à régler sur une
// pièce, et ce qu'un style peint sur chacune.

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "ro/render/actor_wardrobe.h"

namespace ro::render {
namespace {

// Le calque que le profil 1 découpe pour une partie, ou rien pour une partie
// qui ne se teint pas comme un calque.
[[nodiscard]] std::optional<RampLayer> RampLayerOfPart(ActorPart part) noexcept {
  switch (part) {
    case ActorPart::kHead:
      return RampLayer::kHead;
    case ActorPart::kHeadgearLow:
    case ActorPart::kHeadgearMiddle:
    case ActorPart::kHeadgearTop:
      return RampLayer::kHeadgear;
    case ActorPart::kGarment:
      return RampLayer::kGarment;
    default:
      return std::nullopt;
  }
}

[[nodiscard]] SkinRace RaceOf(const ActorLook& look) {
  return IsDoramJob(look.job) ? SkinRace::kDoram : SkinRace::kHuman;
}

}  // namespace

std::uint32_t ActorWardrobe::BodyKeyFor(const ActorLook& look) {
  const LoadedBodySprite body = LoadedBody(look);
  if (body.loaded == nullptr) {
    return 0;
  }
  const auto [found, fresh] = body_keys_.try_emplace(body.loaded, 0U);
  if (fresh) {
    found->second = BodySpriteKey(body.path);
  }
  return found->second;
}

bool ActorWardrobe::SetStyle(std::uint32_t style, const PaletteRecipe& recipe) {
  // ⚠ REFUSÉ PLUTÔT QUE REMPLACÉ : voir l'en-tête. Ce qui est déjà dessiné
  // retient l'adresse de sa palette, et la réécrire sous le même pointeur
  // laisserait des textures d'hier jusqu'à la prochaine éviction.
  return actor_palettes_.SetRecipe(style, recipe);
}

bool ActorWardrobe::SetStyleSet(std::uint32_t style, StyleSet set) {
  return actor_palettes_.SetStyle(style, std::move(set));
}

bool ActorWardrobe::SetStylePalette(std::uint32_t style, formats::Palette palette) {
  return actor_palettes_.SetPalette(style, std::move(palette));
}

void ActorWardrobe::ForgetStyle(std::uint32_t style) {
  if (!actor_palettes_.Knows(style)) {
    return;
  }
  // Les tenues D'ABORD : elles pointent dans les palettes, et l'ordre inverse
  // laisserait une tenue vivante sur une palette détruite le temps d'une ligne.
  std::erase_if(outfits_, [style](const auto& entry) { return entry.first.style == style; });
  // Les têtes de groupe et d'amis aussi : celles d'un style pointent dans ses
  // palettes.
  std::erase_if(heads_, [style](const auto& entry) { return entry.first.style == style; });
  actor_palettes_.Forget(style);
}

ActorWardrobe::BodyRamps ActorWardrobe::BodyRampsFor(const ActorLook& look) {
  BodyRamps found;
  const LoadedBodySprite body = LoadedBody(look);
  if (body.loaded == nullptr || !body.loaded->sprite.palette().has_value()) {
    return found;
  }
  const std::string palette_path = BodyPalettePath(look);
  const formats::Palette* official = palette_path.empty() ? nullptr : Palette(palette_path);
  const ActorPalettes::Analysis& analysis = actor_palettes_.Analyze(body.loaded->sprite, official);
  found.base = *analysis.base;
  found.ramps = analysis.ramps;
  found.pixels_total = analysis.pixels_total;
  found.pixels_covered = analysis.pixels_covered;
  found.continuation = analysis.continuation;
  found.continuation_pixels = analysis.continuation_pixels;
  return found;
}

ActorWardrobe::BodyRamps ActorWardrobe::LayerRampsFor(const ActorLook& look, ActorPart part) {
  BodyRamps found;
  const std::optional<RampLayer> layer = RampLayerOfPart(part);
  if (!layer.has_value()) {
    return found;
  }
  for (const ActorPieceFiles& files : ActorFiles(catalog_, look)) {
    if (files.part != part) {
      continue;
    }
    const LoadedActorSprite* loaded =
        FirstLoaded(files.candidates, files.animation_path, false).loaded;
    if (loaded == nullptr || !loaded->sprite.palette().has_value()) {
      return found;
    }
    const formats::Palette* official =
        files.palette_path.empty() ? nullptr : Palette(files.palette_path);
    const ActorPalettes::Analysis& analysis =
        actor_palettes_.AnalyzeLayer(loaded->sprite, official, *layer, RaceOf(look));
    found.base = *analysis.base;
    found.ramps = analysis.ramps;
    found.pixels_total = analysis.pixels_total;
    found.pixels_covered = analysis.pixels_covered;
    return found;
  }
  return found;
}

void ActorWardrobe::Tint(const ActorLook& look, ActorPart part, const formats::Palette* official,
                         bool skin, const LoadedActorSprite& loaded, ActorPiece& piece) {
  if (look.style == 0) {
    return;
  }
  // 🔴 L'HABIT DU GM GARDE LES COULEURS DE SES .SPR, corps et tête : les index
  // d'une recette sont ceux du corps du métier, et sous l'habit la tête ne
  // charge plus sa couleur de cheveux, donc plus la base de sa teinture.
  const bool gm_outfit = look.player && look.gm_outfit;
  ActorPalettes::Piece wanted{.race = RaceOf(look), .skin = skin};
  switch (part) {
    case ActorPart::kBody:
      if (gm_outfit) {
        return;
      }
      wanted.layer = StyleLayer::kBody;
      // La clé du corps désigne sa suite ; la recette v7, elle, ne la lit pas.
      wanted.key = BodyKeyFor(look);
      break;
    case ActorPart::kHead:
      // La tête d'orc est un visage d'effet, sans palette de cheveux.
      if (gm_outfit || look.orc_head) {
        return;
      }
      wanted.layer = StyleLayer::kHead;
      wanted.key = HeadDyeKey(look.hair, look.hair_palette);
      break;
    case ActorPart::kHeadgearLow:
    case ActorPart::kHeadgearMiddle:
    case ActorPart::kHeadgearTop: {
      // Les coiffes se rangent de la basse à la haute, comme les parties.
      const auto rank =
          static_cast<std::size_t>(part) - static_cast<std::size_t>(ActorPart::kHeadgearLow);
      wanted.layer = StyleLayer::kHeadgear;
      wanted.key = look.headgears[rank];
      wanted.skin = false;
      break;
    }
    case ActorPart::kGarment:
      wanted.layer = StyleLayer::kGarment;
      wanted.key = look.robe;
      wanted.skin = false;
      break;
    default:
      return;
  }
  const ActorPalettes::StyledPalette* styled =
      actor_palettes_.StyledFor(look.style, loaded.sprite, official, wanted);
  if (styled == nullptr) {
    return;
  }
  if (loaded.sprite.palette().has_value()) {
    piece.palette = &styled->palette;
  }
  piece.palette_serial = styled->serial;
  piece.tint = styled->tint.has_value() ? &*styled->tint : nullptr;
}

}  // namespace ro::render
