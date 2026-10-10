#include "world/entity_sample.h"

#include <algorithm>
#include <array>
#include <utility>

#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_vignette.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// Les représentants : leur classe, telle que le serveur la numérote, et
// l'objet posé au sol.
constexpr std::uint32_t kPoringJob = 1002;
constexpr std::uint32_t kBaphometJob = 1039;
// L'ONGLET « PNJ » PORTE AUSSI LES FAMILIERS, LES HOMONCULES ET LES
// MERCENAIRES : son représentant tourne entre les quatre, chacun quelques
// secondes, pour qu'on règle ses barres en les voyant tous.
struct Companion {
  std::uint32_t job = 0;
  const char* name = "";
};
constexpr std::array<Companion, kOtherSampleCount> kOtherSamples = {{
    {.job = 117, .name = "Kafra Employee"},
    {.job = 1063, .name = "Lunatic"},
    {.job = 6001, .name = "Lif"},
    {.job = 6017, .name = "Archer Mercenary"},
}};
constexpr std::uint32_t kOtherSampleMs = 3'000;

[[nodiscard]] const Companion& OtherSampleAt(std::uint32_t now_ms) {
  return kOtherSamples[(now_ms / kOtherSampleMs) % kOtherSamples.size()];
}
constexpr std::uint32_t kRedPotionItem = 501;
// Le repli d'une échelle qu'on n'a pas su mesurer : un pixel pour un pixel.
constexpr float kOneToOne = 1.0F;

// LES MOTS DE L'ÉTIQUETTE ET LE SORT INCANTÉ, plausibles pour l'espèce. ⚠ DES NOMS DE JEU, qui ne
// se traduisent pas ; la parenthèse d'un monstre a la forme que ce serveur lui
// donne — son niveau et sa vie.
void NameSample(const ro::ui::EntitySampleShown& drawn, const ro::sim::World& world,
                std::uint32_t now_ms, ro::ui::EntitySample& sample) {
  using ro::render::NamePlateKind;
  const NamePlateKind kind = drawn.name.value_or(NamePlateKind::kMonster);
  switch (kind) {
    case NamePlateKind::kOwn:
    case NamePlateKind::kPlayer:
      sample.name = world.player_name();
      sample.cast_skill = "Storm Gust";
      if (const ro::sim::Actor* player = world.player();
          player != nullptr && player->card.has_value()) {
        sample.title = player->card->title;
        sample.aside = player->card->aside;
        sample.banner = player->card->banner;
        sample.badge = player->card->badge;
      }
      break;
    // @showmobinfo MET LA RACE SUR LA SECONDE LIGNE, et l'élément entre
    // crochets à sa suite.
    case NamePlateKind::kMonster:
      sample.name = "Poring";
      sample.cast_skill = "Water Ball";
      sample.aside = "Lv. 1 | HP: 100%";
      sample.banner = "Plant";
      sample.badge = "Water 1";
      break;
    case NamePlateKind::kBoss:
      sample.name = "Baphomet";
      sample.cast_skill = "Earthquake";
      sample.aside = "Lv. 81 | HP: 100%";
      sample.banner = "Demon";
      sample.badge = "Dark 3";
      break;
    case NamePlateKind::kNonPlayer:
      sample.name = OtherSampleAt(now_ms).name;
      sample.cast_skill = "Heal";
      break;
    case NamePlateKind::kGroundItem:
      sample.name = "Red Potion";
      sample.aside = "5";
      break;
  }
}

}  // namespace

ro::render::ActorLook SampleLookOf(const ro::ui::EntitySampleShown& drawn,
                                   const ro::sim::World& world, std::uint32_t now_ms) {
  using ro::render::NamePlateKind;
  using ro::ui::EntityBarKind;
  ro::render::ActorLook look;
  if (drawn.name == NamePlateKind::kGroundItem) {
    look.item = true;
    look.job = kRedPotionItem;
    return look;
  }
  const EntityBarKind kind = drawn.bars.value_or(EntityBarKind::kOther);
  switch (kind) {
    case EntityBarKind::kOwn:
    case EntityBarKind::kParty:
    case EntityBarKind::kPlayer:
      if (const ro::sim::Actor* player = world.player(); player != nullptr) {
        look = LookOf(*player);
      } else {
        look.player = true;
      }
      break;
    case EntityBarKind::kMonster:
      look.job = kPoringJob;
      break;
    case EntityBarKind::kBoss:
      look.job = kBaphometJob;
      break;
    case EntityBarKind::kOther:
      look.job = OtherSampleAt(now_ms).job;
      break;
  }
  return look;
}

ro::ui::EntitySample EntitySampler::Compose(ro::render::ActorWardrobe* wardrobe,
                                            ActorStyles* dressing,
                                            const ro::ui::EntitySampleShown& drawn,
                                            const ro::sim::World& world, float sprite_scale,
                                            float anchor_scale, std::uint32_t now_ms) {
  ro::ui::EntitySample sample;
  // 🔴 COMME SUR LA CARTE, AU ZOOM COURANT : le sprite à l'échelle que la
  // caméra lui donne à l'écran, les écarts à l'échelle d'ancrage du jeu, et
  // tout le reste — barres, texte, zone cliquable — en pixels d'écran. Les deux
  // échelles suivent le recul au même pas sans valoir la même chose : un cadre
  // qui en prenait une pour l'autre posait barres et zone ailleurs qu'en jeu.
  sample.anchor_scale = anchor_scale;
  NameSample(drawn, world, now_ms, sample);
  if (wardrobe == nullptr) {
    return sample;
  }
  if (drawn.bars == ro::ui::EntityBarKind::kOther) {
    sample.reserved_stature = OtherStature(*wardrobe) * sprite_scale;
  }
  ro::render::ActorVignette::Pose pose;
  pose.look = SampleLookOf(drawn, world, now_ms);
  // 🔴 SES COULEURS À LUI, celles que le serveur a envoyées : le joueur se
  // reconnaît, et un personnage aux teintes d'origine dans ce cadre passerait
  // pour une recette perdue. ⚠ LE REGISTRE DE LA CARTE, et non un à nous :
  // deux registres donneraient deux identifiants pour une recette.
  if (dressing != nullptr && pose.look.player && world.player() != nullptr) {
    dressing->Dress(world.player_id(), pose.look, *wardrobe, world.styles());
  }
  // LE REGARD DROIT : au repos, les images d'un joueur sont les orientations de
  // sa tête, et les dérouler la fait tourner sans fin. Voir `VignetteHeldFrame`.
  if (pose.look.player && world.player() != nullptr) {
    pose.held_frame = VignetteHeldFrame(*world.player(), ro::render::kAnimationIdle);
  }
  // Et le mercenaire humain, figé au repos comme le client le tient.
  if (ro::render::IsHumanMercenaryClass(pose.look.job)) {
    pose.held_frame = std::size_t{0};
  }
  pose.animation = ro::render::kAnimationIdle;
  pose.animated = true;
  pose.elapsed_ms = static_cast<float>(now_ms);
  pose.box.width = kSampleBoxPixels;
  pose.box.height = kSampleBoxPixels;
  // ⚠ L'ÉCHELLE IMPOSÉE : ajustée à sa boîte, la vignette grandirait un Poring
  // à la taille d'un Baphomet.
  pose.box.scale = sprite_scale > 0.0F ? sprite_scale : kOneToOne;
  return Draw(*wardrobe, pose, std::move(sample));
}

float EntitySampler::OtherStature(ro::render::ActorWardrobe& wardrobe) {
  if (wardrobe_ != &wardrobe) {
    other_tops_ = {};
  }
  float tallest = 0.0F;
  for (std::size_t rank = 0; rank < kOtherSamples.size(); ++rank) {
    if (!other_tops_[rank].has_value()) {
      ro::render::ActorLook look;
      look.job = kOtherSamples[rank].job;
      ro::render::ActorOutfit* outfit = wardrobe.OutfitFor(look);
      // LE HAUT DE TOUT CE QU'IL PORTE, coiffes comprises, au-dessus de ses
      // pieds — l'origine du sprite.
      other_tops_[rank] =
          outfit != nullptr
              ? -ro::render::MeasureOutfitSpan(*outfit, ro::render::kAnimationIdle).all.min.y
              : 0.0F;
    }
    tallest = std::max(tallest, *other_tops_[rank]);
  }
  return tallest;
}

ro::ui::EntitySample EntitySampler::Draw(ro::render::ActorWardrobe& wardrobe,
                                         const ro::render::ActorVignette::Pose& pose,
                                         ro::ui::EntitySample sample) {
  if (wardrobe_ != &wardrobe || !vignette_.has_value()) {
    vignette_.emplace(wardrobe);
    wardrobe_ = &wardrobe;
  }
  const ro::render::IconTexture& drawn_texture = vignette_->Get(pose);
  const ro::render::VignetteFraming& framing = vignette_->framing();
  if (!drawn_texture.usable() || !framing.usable()) {
    return sample;
  }
  const ro::render::OutfitSpan& span = vignette_->span();
  const auto box = static_cast<float>(kSampleBoxPixels);
  sample.sprite = ro::ui::SkinImage{
      .id = static_cast<ImTextureID>(drawn_texture.texture.name()), .width = box, .height = box};
  // LES PIEDS SONT L'ORIGINE DU SPRITE, sa position au sol : c'est d'elle que
  // le jeu projette le point sous lequel une barre descend.
  sample.feet = ImVec2(framing.origin.x, framing.origin.y);
  sample.head_y = framing.origin.y + span.body.min.y * framing.scale;
  // LA BOÎTE DE LA ZONE CLIQUABLE : tout ce qui est peint, tête comprise,
  // comme l'emprise du jeu — et non le corps seul.
  const ro::render::SpriteBounds& drawn = vignette_->drawn_bounds();
  sample.body_min = ImVec2(framing.origin.x + drawn.min.x * framing.scale,
                           framing.origin.y + drawn.min.y * framing.scale);
  sample.body_max = ImVec2(framing.origin.x + drawn.max.x * framing.scale,
                           framing.origin.y + drawn.max.y * framing.scale);
  return sample;
}

}  // namespace moonclient
