#include "ro/render/sprite_vignette.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ro/render/sprite_canvas.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// La moitié, pour les centrages. Nommée parce qu'elle sert des deux côtés d'un
// cadrage où toute autre valeur serait un décentrage.
constexpr float kHalf = 0.5F;

// Combien d'images porte l'animation du corps. Au moins une : une action sans
// image ne se mesure pas, mais elle ne doit pas non plus faire une boucle vide
// qui rendrait une enveloppe jamais touchée.
[[nodiscard]] std::size_t FrameCountOf(const ActorOutfit& outfit, std::size_t animation) {
  const float interval_ms = outfit.FrameIntervalMs(animation);
  if (interval_ms <= 0.0F) {
    return 1;
  }
  const float duration_ms = outfit.AnimationDurationMs(animation);
  const auto count = static_cast<std::size_t>(std::lround(duration_ms / interval_ms));
  return std::max<std::size_t>(count, 1);
}

// Fait entrer une pose composée dans l'enveloppe en cours.
//
// ⚠ LE CORPS SE RECONNAÎT PAR LA PROVENANCE DE SES CALQUES, et non par un rang
// dans la liste des quads : l'ordre de DESSIN est recalculé d'après la
// direction, si bien que le bouclier passe devant le corps de dos.
void GrowOver(const ComposedActor& composed, const formats::SprSprite* body_sprite,
              OutfitSpan& span, bool& grown_all, bool& grown_body) {
  for (std::size_t index = 0; index < composed.quads.size(); ++index) {
    GrowBounds(span.all, grown_all, composed.quads[index]);
    if (index < composed.sources.size() && composed.sources[index].sprite == body_sprite) {
      GrowBounds(span.body, grown_body, composed.quads[index]);
    }
  }
}

}  // namespace

OutfitSpan MeasureOutfitSpan(ActorOutfit& outfit, std::size_t animation) {
  OutfitSpan span;
  if (outfit.pieces().empty()) {
    return span;
  }
  // LE CORPS EST LA PREMIÈRE PIÈCE : c'est l'ordre des dépendances d'ancrage,
  // et c'est par son sprite qu'on reconnaît ses calques parmi les autres.
  const formats::SprSprite* body_sprite = outfit.pieces().front().sprite;

  bool grown_all = false;
  bool grown_body = false;
  const std::size_t frames = FrameCountOf(outfit, animation);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    for (std::size_t direction = 0; direction < kDirectionCount; ++direction) {
      // ⚠ TENUE SUR UNE IMAGE, temps écoulé nul : on mesure une borne, pas un
      // instant. Les coiffes qui vivent sur leur propre horloge n'y montrent
      // donc que leur première image de sous-groupe — une approximation
      // assumée, la même que celle de la référence, et qui ne peut que
      // resserrer l'enveloppe d'un costume animé.
      const ComposedActor& composed = outfit.Composed(animation, direction, 0.0F, false, frame);
      GrowOver(composed, body_sprite, span, grown_all, grown_body);
    }
  }
  // SANS CORPS, RIEN À CADRER : les pieds et la stature en viennent tous les
  // deux, et une enveloppe sans eux ne saurait ni où poser ni à quelle taille.
  span.measured = grown_all && grown_body;
  return span;
}

OutfitSpan MeasurePoseSpan(const ActorOutfit& outfit, const ComposedActor& composed) {
  OutfitSpan span;
  if (outfit.pieces().empty()) {
    return span;
  }
  bool grown_all = false;
  bool grown_body = false;
  GrowOver(composed, outfit.pieces().front().sprite, span, grown_all, grown_body);
  span.measured = grown_all && grown_body;
  return span;
}

OutfitSpan MeasureDirectionSpan(ActorOutfit& outfit, std::size_t animation, std::size_t direction) {
  OutfitSpan span;
  if (outfit.pieces().empty()) {
    return span;
  }
  const formats::SprSprite* body_sprite = outfit.pieces().front().sprite;
  bool grown_all = false;
  bool grown_body = false;
  const std::size_t frames = FrameCountOf(outfit, animation);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const ComposedActor& composed = outfit.Composed(animation, direction, 0.0F, false, frame);
    GrowOver(composed, body_sprite, span, grown_all, grown_body);
  }
  // ⚠ L'ENVELOPPE ENTIÈRE SUFFIT : le cadrage centré ne pose pas de pieds, et
  // ne lit donc rien du corps.
  span.measured = grown_all;
  return span;
}

namespace {

// LA BOÎTE CENTRÉE : l'enveloppe entière au milieu de la boîte, réduite pour y
// tenir, jamais agrandie. Les tailles du .act sont celles du jeu, et la taille
// d'un monstre dit son gabarit.
[[nodiscard]] VignetteFraming FrameCentered(const SpriteBounds& all, float width, float height) {
  VignetteFraming framing;
  const float span_x = all.max.x - all.min.x;
  const float span_y = all.max.y - all.min.y;
  if (span_x <= 0.0F || span_y <= 0.0F) {
    return framing;
  }
  constexpr float kNoUpscale = 1.0F;
  framing.scale = std::min({width / span_x, height / span_y, kNoUpscale});
  const glm::vec2 center = (all.min + all.max) * kHalf;
  framing.origin = glm::vec2(width * kHalf, height * kHalf) - center * framing.scale;
  return framing;
}

}  // namespace

VignetteFraming FrameVignette(const OutfitSpan& span, const VignetteBox& box) {
  VignetteFraming framing;
  if (!span.measured || box.width <= 0 || box.height <= 0) {
    return framing;
  }
  const auto width = static_cast<float>(box.width);
  const auto height = static_cast<float>(box.height);
  switch (box.fit) {
    case VignetteFit::kCenteredShrinkOnly:
      return FrameCentered(span.all, width, height);
    case VignetteFit::kStanding:
      break;
  }

  // Le centre de l'ENVELOPPE : il ne bouge pas d'une direction à l'autre,
  // puisqu'elle les couvre toutes. Le recaler sur le corps coûterait la moitié
  // du cadre — la demi-largeur retenue étant la plus grande des deux, une
  // coiffe latérale ferait réserver autant de vide de l'autre côté.
  const float center_x = (span.all.min.x + span.all.max.x) * kHalf;
  const float half_width = std::max(span.all.max.x - center_x, center_x - span.all.min.x);
  // LES PIEDS SONT CEUX DU CORPS. Une cape longue ou une coiffe qui descend
  // plus bas que les bottes enfoncerait sinon le personnage sous le cadre.
  const float feet = span.body.max.y;
  const float span_y = feet - span.all.min.y;

  // ÉCHELLE IMPOSÉE : on ne cadre plus, on POSE. Le personnage est centré sur
  // son CORPS et ses pieds touchent le bas ; tout ce que l'enveloppe a de plus
  // sort de la boîte. Voir `VignetteBox::scale`.
  //
  // ⚠ AUCUNE DES DEUX GARDES CI-DESSOUS NE S'APPLIQUE ICI : une enveloppe
  // large comme un cheveu (`half_width` nul) ou un personnage qui ne monte pas
  // au-dessus de ses pieds n'empêchent pas de le POSER — ils empêchaient de le
  // cadrer, ce qui est une division.
  if (box.scale > 0.0F) {
    const float body_center_x = (span.body.min.x + span.body.max.x) * kHalf;
    framing.scale = box.scale;
    framing.origin =
        glm::vec2(width * kHalf - body_center_x * box.scale, height - feet * box.scale);
    return framing;
  }

  if (half_width <= 0.0F || span_y <= 0.0F) {
    return framing;
  }

  float scale = std::min(height / span_y, width * kHalf / half_width);
  const float body_height = span.body.max.y - span.body.min.y;
  if (box.min_body_height > 0.0F && body_height > 0.0F) {
    scale = std::max(scale, box.min_body_height / body_height);
  }
  if (scale <= 0.0F) {
    return framing;
  }

  framing.scale = scale;
  framing.origin = glm::vec2(width * kHalf - center_x * scale, height - feet * scale);
  return framing;
}

formats::Image DrawVignette(const ComposedActor& actor, const VignetteBox& box,
                            const VignetteFraming& framing) {
  if (!framing.usable() || box.width <= 0 || box.height <= 0 || actor.quads.empty()) {
    return {};
  }
  // LA BOÎTE, RAMENÉE EN PIXELS DE SPRITE. Les quads ne sont donc pas mis à
  // l'échelle : ils se dessinent tels qu'ils sont, et c'est la carte qui
  // agrandira l'image entière. ⚠ AU MOINS UN PIXEL DE CÔTÉ : une boîte plus
  // petite que l'échelle donnerait un canevas nul, donc une vignette vide là
  // où l'on attend une silhouette minuscule.
  //
  // 🔴 SAUF QUAND LE CANEVAS DÉPASSERAIT `kVignetteLargestCanvasPx` : une
  // grande boîte sous une petite échelle demanderait des dizaines de milliers
  // de pixels de côté. L'échelle du CANEVAS a alors un plancher, et les quads
  // rétrécissent d'autant : la silhouette tombe au même endroit de la boîte,
  // seulement rendue plus grossièrement.
  const auto box_width = static_cast<float>(box.width);
  const auto box_height = static_cast<float>(box.height);
  const float canvas_scale =
      std::max(framing.scale, std::max(box_width, box_height) / kVignetteLargestCanvasPx);
  const float shrink = framing.scale / canvas_scale;
  const auto width = std::clamp(static_cast<std::int32_t>(std::lround(box_width / canvas_scale)), 1,
                                static_cast<std::int32_t>(kVignetteLargestCanvasPx));
  const auto height = std::clamp(static_cast<std::int32_t>(std::lround(box_height / canvas_scale)),
                                 1, static_cast<std::int32_t>(kVignetteLargestCanvasPx));
  std::vector<SpriteQuad> quads = actor.quads;
  if (shrink < 1.0F) {
    for (SpriteQuad& quad : quads) {
      for (glm::vec2& corner : quad.corners) {
        corner *= shrink;
      }
    }
  }
  SpriteCanvas canvas(width, height);
  canvas.Draw(quads, ResolveImages(actor), framing.origin / canvas_scale);
  return canvas.image();
}

}  // namespace ro::render
