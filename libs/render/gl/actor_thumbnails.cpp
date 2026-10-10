#include "ro/render/actor_thumbnails.h"

#include <tuple>
#include <utility>

#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// LA POSE D'UNE VIGNETTE : debout, de face, première image.
//
// ⚠ FIGÉE, ET C'EST TOUT L'INTÉRÊT : une grille dont les quatre-vingts cases
// s'animeraient recomposerait quatre-vingts textures par image, et l'on ne
// choisit pas une coiffure à ce qu'elle fait en marchant.
constexpr std::size_t kFacingUs = 0;
constexpr float kFrozen = 0.0F;
constexpr std::size_t kFirstFrame = 0;

}  // namespace

bool ActorThumbnails::Before::operator()(const Cell& left, const Cell& right) const noexcept {
  if (left.subject != right.subject) {
    return left.subject < right.subject;
  }
  const ActorLookOrder before;
  if (before(left.look, right.look)) {
    return true;
  }
  if (before(right.look, left.look)) {
    return false;
  }
  return std::tie(left.palette, left.box.width, left.box.height) <
         std::tie(right.palette, right.box.width, right.box.height);
}

const IconTexture& ActorThumbnails::Get(const Cell& cell) {
  if (const auto found = drawn_.find(cell); found != drawn_.end()) {
    return found->second;
  }
  if (wardrobe_ == nullptr || cell.box.width <= 0 || cell.box.height <= 0) {
    return nothing_;
  }
  // ⚠ LE BUDGET SE CONSULTE AVANT DE COMPOSER, ET NE SE PRÉLÈVE QU'ENSUITE :
  // une case déjà connue ne coûte rien et ne doit rien prendre, sinon une
  // grille pleine consommerait son budget sans rien produire et les cases
  // manquantes n'arriveraient jamais.
  if (budget_ <= 0) {
    return nothing_;
  }
  --budget_;

  ActorOutfit* outfit = cell.subject == Subject::kHead
                            ? wardrobe_->HeadOnly(cell.look)
                            : wardrobe_->BodySwatch(cell.look, cell.palette);
  if (outfit == nullptr) {
    // 🔴 L'ABSENCE SE RETIENT, comme une icône d'objet manquante : sans cela on
    // rouvrirait le même fichier absent soixante fois par seconde, et le
    // budget n'y suffirait plus pour les cases qui existent.
    return drawn_.emplace(cell, IconTexture{}).first->second;
  }

  const ComposedActor& composed =
      outfit->Composed(kAnimationIdle, kFacingUs, kFrozen, /*loop=*/true, kFirstFrame);
  // 🔴 CADRÉE SUR LA POSE DESSINÉE, ET NON SUR L'ENVELOPPE DES HUIT
  // DIRECTIONS. L'enveloppe existe pour un pantin qui TOURNE : ce qui rentre
  // doit rentrer partout. Une case ne tourne pas — la mesurer ainsi la
  // rapetisse de tout ce que la direction la plus encombrante occupe, et deux
  // coiffures voisines s'y montrent à deux tailles différentes.
  const OutfitSpan span = MeasurePoseSpan(*outfit, composed);
  const VignetteFraming framing = FrameVignette(span, cell.box);
  const formats::Image image = DrawVignette(composed, cell.box, framing);
  if (image.empty()) {
    return drawn_.emplace(cell, IconTexture{}).first->second;
  }
  // ⚠ COMME UN SPRITE : composée en pixels de sprite, l'interface l'étire sur
  // sa case. Même raison que la vignette d'acteur — voir `actor_vignette.cpp`.
  auto texture = GlTexture::Upload(image, GlTexture::Filtering::kSprite);
  if (!texture) {
    return drawn_.emplace(cell, IconTexture{}).first->second;
  }
  IconTexture made;
  made.texture = std::move(*texture);
  made.width_px = image.width;
  made.height_px = image.height;
  return drawn_.emplace(cell, std::move(made)).first->second;
}

void ActorThumbnails::Clear() { drawn_.clear(); }

}  // namespace ro::render
