// LES CONVIVES DU BANQUET : de la ligne de liste au pantin téléversé.

#include "shell/banquet_dolls.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_vignette.h"
#include "ro/sim/actor.h"

#include "game/connected_session.h"
#include "world/actor_view.h"

namespace moonclient {
namespace {

// LA BOÎTE DE LA SONDE, en pixels. Sa valeur n'a aucune importance en soi : on
// ne garde que le RAPPORT entre elle et l'échelle qu'elle obtient. Mille parce
// qu'un nombre rond se relit, et que l'arrondi y est négligeable.
constexpr std::int32_t kProbeHeightPixels = 1000;

// LE PERSONNAGE DE RÉFÉRENCE : un novice masculin, nu, sans coiffe ni cape.
//
// ⚠ TOUT À ZÉRO SAUF LE SEXE. Ce n'est pas « un personnage moyen » mais un
// ÉTALON : ce qu'il faut, c'est qu'il soit toujours le même, et que rien de ce
// qu'on lui mettrait ne change sa stature.
[[nodiscard]] ro::render::ActorLook ReferenceLook() {
  ro::render::ActorLook look;
  look.player = true;
  look.sex = 1;
  return look;
}

}  // namespace

float BanquetDolls::StandardSpanUnits(int animation) {
  // ⚠ UNE ACTION HORS DE LA TABLE SE MESURE COMME L'ATTENTE, comme la
  // composition la jouerait.
  const int measured = animation >= 0 && animation < ro::ui::kSeatAnimationTypes
                           ? animation
                           : ro::ui::kSeatAnimStand;
  float& known = standard_span_units_[static_cast<std::size_t>(measured)];
  if (known > 0.0F) {
    return known;
  }
  ro::render::ActorOutfit* reference = wardrobe_->OutfitFor(ReferenceLook());
  if (reference == nullptr) {
    return 0.0F;
  }
  const ro::render::OutfitSpan span =
      ro::render::MeasureOutfitSpan(*reference, static_cast<std::size_t>(measured));
  // 🔴 AUX PROPORTIONS D'UN SIÈGE, et non dans un cadre libre : c'est l'échelle
  // qu'aurait choisie l'ajustement pour ce novice-là, CONTRAINTE DE LARGEUR
  // COMPRISE. Sans elle, une pose assise — plus large que haute — mesurerait
  // une hauteur qui n'a jamais été celle du rendu, et toute la scène
  // grossirait d'un tiers.
  const ro::render::VignetteFraming probe = ro::render::FrameVignette(
      span,
      ro::render::VignetteBox{.width = static_cast<std::int32_t>(
                                  static_cast<float>(kProbeHeightPixels) * ro::ui::kSeatBoxAspect),
                              .height = kProbeHeightPixels});
  if (!probe.usable()) {
    return 0.0F;
  }
  known = static_cast<float>(kProbeHeightPixels) / probe.scale;
  return known;
}

BanquetDolls::~BanquetDolls() { dressing_.Sweep(*wardrobe_, ro::sim::StyleBook{}); }

void BanquetDolls::UseStyles(const std::map<std::uint32_t, ro::sim::StyleVariants>& kept) {
  for (const auto& [character_id, variants] : kept) {
    styles_.BeginBatchFor(character_id);
    for (const auto& [body_key, recipe] : variants.by_body) {
      styles_.Remember(character_id, body_key, recipe, body_key == variants.fallback_body);
    }
  }
}

BanquetDolls::Seated BanquetDolls::Seat(int seat, const ro::proto::CharacterInfo& character,
                                        std::uint8_t account_sex,
                                        const ro::ui::CharacterSeat& place,
                                        const ro::ui::SeatFrame& frame, std::uint32_t elapsed_ms) {
  if (seat < 0 || seat >= ro::ui::kSeatCount || frame.doll_height <= 0.0F) {
    return {};
  }
  const float standard_units = StandardSpanUnits(place.animation);
  if (standard_units <= 0.0F) {
    return {};
  }

  // ⚠ L'APPARENCE PASSE PAR LES DEUX MÊMES PORTES QUE L'ENTRÉE EN JEU —
  // `AppearanceOf` puis `LookOf` —, et c'est ce qui garantit qu'un personnage
  // se ressemble d'un écran à l'autre. Les recopier ici les ferait diverger sur
  // un champ qu'on aurait oublié : l'ordre des trois accessoires, ou le sexe
  // « celui du compte ».
  ro::sim::Actor seated;
  seated.kind = ro::sim::ActorKind::kPlayer;
  seated.look = AppearanceOf(character, account_sex);

  auto& puppet = puppets_[static_cast<std::size_t>(seat)];
  if (!puppet.has_value()) {
    puppet.emplace(*wardrobe_);
  }

  ro::render::ActorVignette::Pose pose;
  pose.look = LookOf(seated);
  // 🔴 ET LES COULEURS QUE LE SERVEUR LUI DONNE EN JEU, qu'il ne donne pas ici :
  // celles que le jeu a gardées. Sans elles, chaque convive paraîtrait dans
  // ses couleurs natives. Le même registre que la carte, pour le même rendu.
  dressing_.Dress(static_cast<std::uint32_t>(character.GID), pose.look, *wardrobe_, styles_);
  // ⚠ LA POSE EST CELLE DU SIÈGE, que le mode « Personnaliser » règle : son
  // action, son orientation, sa tête et son image.
  pose.animation = static_cast<std::size_t>(std::max(place.animation, 0));
  pose.direction = static_cast<std::size_t>(
      (place.direction % ro::ui::kSeatDirections + ro::ui::kSeatDirections) %
      ro::ui::kSeatDirections);
  // ⚠ L'HORLOGE COURT TOUJOURS, et c'est l'IMAGE TENUE qui fige le corps :
  // sauf demande, il tient sa pose, et ce qu'il porte continue de vivre — un
  // masque dont les couleurs changent, une mâchoire qui mordille. C'est ce que
  // fait le jeu d'un personnage immobile.
  pose.animated = true;
  const ro::render::ActorOutfit* outfit = wardrobe_->OutfitFor(pose.look);
  const int frames = outfit != nullptr ? static_cast<int>(outfit->FrameCount(pose.animation)) : 0;
  const int held = ro::ui::SeatHeldFrame(place, frames);
  if (held >= 0) {
    pose.held_frame = static_cast<std::size_t>(held);
  }
  pose.elapsed_ms = static_cast<float>(elapsed_ms);
  pose.box.width = static_cast<std::int32_t>(frame.doll_width);
  pose.box.height = static_cast<std::int32_t>(frame.doll_height);
  // 🔴 L'ÉCHELLE COMMUNE, et c'est tout l'objet de cette classe. Ajustée à sa
  // boîte, chaque vignette sortirait à SA taille : un grand chapeau ou une arme
  // d'hast rétréciraient leur porteur de moitié à côté de son voisin nu, et
  // vingt-cinq convives feraient vingt-cinq échelles.
  pose.box.scale = frame.standard_height / standard_units;

  const ro::render::IconTexture& drawn = puppet->Get(pose);
  if (!drawn.usable()) {
    return {};
  }
  // ⚠ LA TAILLE RENDUE EST CELLE DE LA BOÎTE, et non celle de la texture : la
  // vignette est composée en pixels de SPRITE, et c'est la carte qui l'étire.
  return Seated{.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                           .width = frame.doll_width,
                                           .height = frame.doll_height},
                .frames = frames};
}

}  // namespace moonclient
