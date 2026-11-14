#include "ro/render/damage_mark.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/sprite_billboard.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeDamageMark;
using ro::render::DamageMark;
using ro::render::DamageMarkAction;
using ro::render::DamageMarkLifeMs;
using ro::render::kMarkOnOther;
using ro::render::kMarkOnPlayer;
using ro::render::kMarkOnStar;
using ro::render::kMarkTickMs;
using ro::render::kMissLifeTicks;
using ro::render::kMissUnitsPerTick;
using ro::render::kSpritePixelsPerWorldUnit;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

// Le fichier du jeu porte cinq actions ; celle du « Miss » est la première et
// celle de la bulle « LUCKY!! » la quatrième. Deux longueurs différentes
// suffisent à montrer qu'on lit bien l'une ou l'autre.
constexpr std::size_t kActionCount = 5;
constexpr std::size_t kMissImages = 20;
constexpr std::size_t kLuckyImages = 8;
constexpr std::int32_t kMarkWidth = 12;
constexpr std::int32_t kMarkHeight = 10;
// Ce que le .act déclare comme cadence, en unités RO.
constexpr float kMarkSpeedUnits = 4.0F;

std::size_t ImagesOfAction(std::size_t action) {
  if (action == DamageMarkAction(DamageMark::kMiss)) {
    return kMissImages;
  }
  return action == DamageMarkAction(DamageMark::kLucky) ? kLuckyImages : 1;
}

SprSprite MarkSprite() {
  SprBuildOptions options;
  for (std::size_t image = 0; image < kMissImages; ++image) {
    options.indexed.push_back(SprIndexedFrameSpec{
        kMarkWidth, kMarkHeight,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kMarkWidth * kMarkHeight), 1)});
  }
  options.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 255, .blue = 255}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

ActAnimation MarkAnimation() {
  ActBuildOptions options;
  for (std::size_t index = 0; index < kActionCount; ++index) {
    ActActionSpec action;
    for (std::size_t image = 0; image < ImagesOfAction(index); ++image) {
      ActLayerSpec layer;
      layer.frame_index = static_cast<std::int32_t>(image);
      action.frames.push_back(ActFrameSpec{.layers = {layer}});
    }
    options.actions.push_back(action);
    options.speeds.push_back(kMarkSpeedUnits);
  }
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

}  // namespace

TEST_CASE("Marque de coup : chaque mot nomme son action", "[render][mark]") {
  // Le fichier porte les deux dans le même sprite, et rien ne les distingue
  // que leur numéro d'action : le « Miss » en tête, la bulle en quatrième.
  CHECK(DamageMarkAction(DamageMark::kMiss) == 0);
  CHECK(DamageMarkAction(DamageMark::kLucky) == 3);
}

TEST_CASE("Marque de coup : le « Miss » ne s'anime pas, il monte et pâlit", "[render][mark]") {
  // Le client le range dans un genre qui ne fait jamais avancer l'image : le
  // mot part de sa place, monte et pâlit, puis s'éteint. Jouer les neuf images
  // de son action le ferait clignoter au lieu de le faire monter.
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  const float life_ms = DamageMarkLifeMs(animation, DamageMark::kMiss);
  CHECK(life_ms == Catch::Approx(kMissLifeTicks * kMarkTickMs));

  const ro::render::ComposedActor born =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F);
  REQUIRE(born.quads.size() == 1);
  REQUIRE(born.sources.size() == 1);
  CHECK(born.sources[0].image_index == 0);

  // Au milieu de sa vie, la MÊME image — mais plus haut et plus pâle. La
  // montée se lit en unités du monde, sept pixels chacune : quarante tics à
  // cinquante-quatre centièmes d'unité.
  const ro::render::ComposedActor middle =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, life_ms * 0.5F);
  REQUIRE(middle.sources.size() == 1);
  CHECK(middle.sources[0].image_index == 0);
  CHECK(born.quads[0].corners[0].y - middle.quads[0].corners[0].y ==
        Catch::Approx(kMissUnitsPerTick * 40.0F * kSpritePixelsPerWorldUnit));
  CHECK(middle.quads[0].tint[3] < born.quads[0].tint[3]);

  // Passé son terme, plus rien.
  CHECK(
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, life_ms).quads.empty());
  CHECK(ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, life_ms * 2.0F)
            .quads.empty());
  CHECK(ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, -1.0F).quads.empty());
}

TEST_CASE("Marque de coup : la bulle de l'esquive JOUE son animation", "[render][mark]") {
  // Elle n'est PAS du genre du « Miss » : le client la range dans le
  // dix-septième, qui fait avancer l'image et meurt au bout d'une passe, sans
  // montée ni pâlissement. Sa durée vient donc du fichier, et non des
  // quatre-vingts tics du mot.
  //
  // 🔴 LA GELER LA RÉDUIT À UN POINT : la première image de la bulle du jeu est
  // large de six pixels et ne porte aucun texte — le mot « LUCKY!! » n'entre
  // qu'à la troisième. Deux assertions valent ici pour ce que l'œil voit : que
  // l'image du milieu ne soit pas la première, et que la dernière soit atteinte.
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  const float miss_ms = DamageMarkLifeMs(animation, DamageMark::kMiss);
  const float lucky_ms = DamageMarkLifeMs(animation, DamageMark::kLucky);
  CHECK(miss_ms == Catch::Approx(kMissLifeTicks * kMarkTickMs));
  CHECK(lucky_ms == Catch::Approx(static_cast<float>(kLuckyImages) *
                                  animation.actions()[3].frame_interval_ms()));
  CHECK(lucky_ms < miss_ms);

  const ro::render::ComposedActor born =
      ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther, 0.0F);
  const ro::render::ComposedActor middle =
      ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther, lucky_ms * 0.5F);
  const ro::render::ComposedActor last = ComposeDamageMark(sprite, animation, DamageMark::kLucky,
                                                           kMarkOnOther, lucky_ms - kMarkTickMs);
  REQUIRE(born.quads.size() == 1);
  REQUIRE(middle.quads.size() == 1);
  REQUIRE(last.quads.size() == 1);
  CHECK(born.sources[0].image_index == 0);
  CHECK(middle.sources[0].image_index == static_cast<std::int32_t>(kLuckyImages / 2));
  CHECK(last.sources[0].image_index == static_cast<std::int32_t>(kLuckyImages - 1));

  // Elle ne bouge pas et ne pâlit pas : le genre dix-sept ne touche ni la
  // position ni la couleur.
  CHECK(middle.quads[0].corners[0].y == Catch::Approx(born.quads[0].corners[0].y));
  CHECK(middle.quads[0].tint[3] == born.quads[0].tint[3]);
  CHECK(ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther, lucky_ms)
            .quads.empty());

  // L'étoile, elle, tourne : sa durée est celle de SON genre, soixante-dix
  // tics, et non celle de son fichier.
  const float star_ms = DamageMarkLifeMs(animation, DamageMark::kCritical);
  CHECK(star_ms == Catch::Approx(ro::render::kStarLifeTicks * kMarkTickMs));
  CHECK(star_ms != Catch::Approx(static_cast<float>(animation.actions()[2].frames.size()) *
                                 animation.actions()[2].frame_interval_ms()));
}

TEST_CASE("Marque de coup : l'étoile tourne d'une demie par tic et se fige", "[render][mark]") {
  // Le genre quinze fait avancer l'image à MI-VITESSE du tic — deux fois plus
  // vite que ce que le fichier déclare —, puis cesse de la calculer une fois la
  // dernière atteinte : l'étoile reste alors sur place en pâlissant, jusqu'au
  // terme de la cloche.
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  const auto frame_at = [&](float ticks) {
    const ro::render::ComposedActor composed = ComposeDamageMark(
        sprite, animation, DamageMark::kCritical, kMarkOnStar, ticks * kMarkTickMs);
    REQUIRE(composed.sources.size() == 1);
    return composed.sources[0].image_index;
  };
  // L'action de l'étoile ne porte qu'une image dans ce fichier d'essai : elle
  // est donc atteinte d'emblée, et ne bouge plus.
  const auto last = static_cast<std::int32_t>(animation.actions()[2].frames.size() - 1);
  CHECK(frame_at(0.0F) == 0);
  CHECK(frame_at(2.0F) == std::min(last, 1));
  CHECK(frame_at(60.0F) == last);

  // Elle naît à l'échelle trois et rétrécit jusqu'à un, deux fois plus
  // lentement que le nombre : son plancher est atteint au quatorzième tic.
  const auto width_at = [&](float ticks) {
    const ro::render::ComposedActor composed = ComposeDamageMark(
        sprite, animation, DamageMark::kCritical, kMarkOnStar, ticks * kMarkTickMs);
    REQUIRE(composed.quads.size() == 1);
    return composed.quads[0].corners[1].x - composed.quads[0].corners[0].x;
  };
  const float born = width_at(0.0F);
  const float floor_ratio = ro::render::kStarFinalScale / ro::render::kStarInitialScale;
  CHECK(
      width_at(5.0F) ==
      Catch::Approx(born * (ro::render::kStarInitialScale - 5.0F * ro::render::kStarShrinkPerTick) /
                    ro::render::kStarInitialScale));
  CHECK(width_at(40.0F) == Catch::Approx(born * floor_ratio));
  CHECK(width_at(60.0F) == Catch::Approx(born * floor_ratio));

  // Et elle pâlit depuis le PLEIN, du début à la fin de sa vie.
  const ro::render::ComposedActor born_star =
      ComposeDamageMark(sprite, animation, DamageMark::kCritical, kMarkOnStar, 0.0F);
  const ro::render::ComposedActor late_star =
      ComposeDamageMark(sprite, animation, DamageMark::kCritical, kMarkOnStar, 60.0F * kMarkTickMs);
  REQUIRE(born_star.quads.size() == 1);
  REQUIRE(late_star.quads.size() == 1);
  CHECK(born_star.quads[0].tint[3] == 255);
  CHECK(late_star.quads[0].tint[3] < born_star.quads[0].tint[3]);

  // La teinte de son genre, qui l'assombrit d'un tiers.
  CHECK(born_star.quads[0].tint[0] == kMarkOnStar[0]);
  CHECK(born_star.quads[0].tint[1] == kMarkOnStar[1]);
  CHECK(born_star.quads[0].tint[2] == kMarkOnStar[2]);
}

TEST_CASE(
    "Marque de coup : l'étoile d'un coup de pied reste sur place, bleu pâle, à l'échelle deux",
    "[render][mark]") {
  // Le genre vingt-cinq : la même image que le critique, mais REMISE à la
  // première à chaque tic, une taille qui tombe de trois à deux dès le premier
  // tic, et une teinte reposée à chaque tic — bleu pâle, deux cent dix
  // d'opacité, qu'elle perd de huit points par tic passé le quatre-vingt-dixième.
  using ro::render::kMarkOnRisingStar;
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  CHECK(DamageMarkAction(DamageMark::kRisingStar) == DamageMarkAction(DamageMark::kCritical));
  CHECK(DamageMarkLifeMs(animation, DamageMark::kRisingStar) ==
        Catch::Approx(ro::render::kRisingStarLifeTicks * kMarkTickMs));

  const auto at = [&](float ticks) {
    const ro::render::ComposedActor composed = ComposeDamageMark(
        sprite, animation, DamageMark::kRisingStar, kMarkOnOther, ticks * kMarkTickMs);
    REQUIRE(composed.quads.size() == 1);
    REQUIRE(composed.sources.size() == 1);
    return composed;
  };
  const ro::render::ComposedActor born = at(0.0F);
  const ro::render::ComposedActor early = at(1.0F);
  const ro::render::ComposedActor late = at(100.0F);
  CHECK(born.sources[0].image_index == 0);
  CHECK(late.sources[0].image_index == 0);

  // La naissance : l'échelle trois, le gris des étoiles, à pleine opacité.
  const auto width = [](const ro::render::ComposedActor& composed) {
    return composed.quads[0].corners[1].x - composed.quads[0].corners[0].x;
  };
  CHECK(width(early) == Catch::Approx(width(born) * ro::render::kRisingStarScale /
                                      ro::render::kRisingStarBirthScale));
  CHECK(width(late) == Catch::Approx(width(early)));
  CHECK(born.quads[0].tint[0] == kMarkOnStar[0]);
  CHECK(born.quads[0].tint[3] == 255);

  // Puis le bleu pâle, quoi que demande l'appelant, à deux cent dix.
  CHECK(early.quads[0].tint[0] == kMarkOnRisingStar[0]);
  CHECK(early.quads[0].tint[1] == kMarkOnRisingStar[1]);
  CHECK(early.quads[0].tint[2] == kMarkOnRisingStar[2]);
  CHECK(early.quads[0].tint[3] == 210);
  CHECK(at(90.0F).quads[0].tint[3] == 210);
  CHECK(late.quads[0].tint[3] == 210 - 80);

  // Elle ne monte pas d'elle-même : c'est le nombre qui la porte.
  CHECK(late.quads[0].corners[0].y == Catch::Approx(early.quads[0].corners[0].y));
  CHECK(ComposeDamageMark(sprite, animation, DamageMark::kRisingStar, kMarkOnOther,
                          ro::render::kRisingStarLifeTicks * kMarkTickMs)
            .quads.empty());
}

TEST_CASE("Marque de coup : elle prend la couleur de sa cible", "[render][mark]") {
  // La règle des nombres, à l'identique : blanc sur un monstre ou un PNJ,
  // rouge sur un joueur.
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();

  const ro::render::ComposedActor on_monster =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F);
  REQUIRE(on_monster.quads.size() == 1);
  CHECK(on_monster.quads[0].tint[0] == 255);
  CHECK(on_monster.quads[0].tint[1] == 255);
  CHECK(on_monster.quads[0].tint[2] == 255);
  // Le « Miss » naît à l'opacité des sprites-enfants du client, deux cent
  // cinquante, et non à pleine opacité : lui seul pâlit.
  CHECK(on_monster.quads[0].tint[3] == 250);

  // La bulle, qui s'ANIME, ne pâlit pas : elle s'éteint avec son animation, et
  // reste donc pleine jusqu'au bout. L'étoile pâlit, mais depuis le PLEIN.
  const ro::render::ComposedActor lucky =
      ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther, 0.0F);
  REQUIRE(lucky.quads.size() == 1);
  CHECK(lucky.quads[0].tint[3] == 255);

  const ro::render::ComposedActor star =
      ComposeDamageMark(sprite, animation, DamageMark::kCritical, kMarkOnOther, 0.0F);
  REQUIRE(star.quads.size() == 1);
  CHECK(star.quads[0].tint[3] == 255);

  const ro::render::ComposedActor on_player =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnPlayer, 0.0F);
  REQUIRE(on_player.quads.size() == 1);
  CHECK(on_player.quads[0].tint[0] == 255);
  CHECK(on_player.quads[0].tint[1] == 0);
  CHECK(on_player.quads[0].tint[2] == 0);
}

TEST_CASE("Marque de coup : une action absente ne dure pas", "[render][mark]") {
  // Une installation dont le sprite est illisible, ou une peau de dégâts qui
  // ne porterait pas toutes les actions, ne doit pas faire vivre une marque
  // immortelle : sans action, la marque n'a pas de durée.
  ActBuildOptions options;
  options.actions = {};
  auto animation = ActAnimation::Parse(BuildAct(options));
  if (animation.has_value()) {
    CHECK(DamageMarkLifeMs(*animation, DamageMark::kMiss) == Catch::Approx(0.0F));
    CHECK(DamageMarkLifeMs(*animation, DamageMark::kLucky) == Catch::Approx(0.0F));
  }
}

TEST_CASE("Marque de coup : l'étoile d'impact est la troisième action", "[render][mark]") {
  // Elle ne remplace pas le nombre, elle l'accompagne — et rien ne la distingue
  // des autres marques que son numéro d'action dans le même fichier.
  CHECK(DamageMarkAction(DamageMark::kCritical) == 2);
  CHECK(DamageMarkAction(DamageMark::kCritical) != DamageMarkAction(DamageMark::kMiss));
  CHECK(DamageMarkAction(DamageMark::kCritical) != DamageMarkAction(DamageMark::kLucky));
}

TEST_CASE("Marque de coup : elle suit les curseurs des nombres", "[render][mark]") {
  using ro::render::NumberLook;
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  const float miss_life_ms = DamageMarkLifeMs(animation, DamageMark::kMiss);

  // L'étirement étire la vie du mot et celle de la bulle, et ralentit la
  // montée : à la vie du client, doublée, le « Miss » n'en est qu'à sa moitié.
  const ro::render::MarkPace slow{.stretch = 2.0F};
  CHECK(DamageMarkLifeMs(animation, DamageMark::kMiss, 2.0F) == Catch::Approx(miss_life_ms * 2.0F));
  CHECK(DamageMarkLifeMs(animation, DamageMark::kLucky, 2.0F) ==
        Catch::Approx(DamageMarkLifeMs(animation, DamageMark::kLucky) * 2.0F));
  const ro::render::ComposedActor half = ComposeDamageMark(
      sprite, animation, DamageMark::kMiss, kMarkOnOther, miss_life_ms, NumberLook{}, slow);
  REQUIRE(half.quads.size() == 1);
  CHECK(half.quads[0].tint[3] == 130);
  NumberLook look;

  // L'échelle grandit, la hauteur de départ relève, l'opacité voile.
  const ro::render::ComposedActor client =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F);
  REQUIRE(client.quads.size() == 1);
  look = NumberLook{};
  look.scale = 2.0F;
  const ro::render::ComposedActor bigger =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F, look);
  CHECK(bigger.quads[0].corners[1].x - bigger.quads[0].corners[0].x ==
        Catch::Approx((client.quads[0].corners[1].x - client.quads[0].corners[0].x) * 2.0F));
  look = NumberLook{};
  look.lift_pixels = 10.0F;
  const ro::render::ComposedActor lifted =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F, look);
  CHECK(lifted.quads[0].corners[0].y == Catch::Approx(client.quads[0].corners[0].y - 10.0F));
  look = NumberLook{};
  look.opacity = 0.5F;
  CHECK(ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F, look)
            .quads[0]
            .tint[3] == 125);
  // La bulle naît pleine : la moitié d'une opacité pleine, et non celle de deux
  // cent cinquante.
  CHECK(ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther, 0.0F, look)
            .quads[0]
            .tint[3] == 127);

  // La bulle ne monte pas d'elle-même ; ce qui l'étire est sa vie, et une
  // bulle deux fois plus longue en est à la moitié de ses images à mi-vie.
  const ro::render::ComposedActor lucky_born =
      ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther, 0.0F);
  const ro::render::ComposedActor lucky_mid =
      ComposeDamageMark(sprite, animation, DamageMark::kLucky, kMarkOnOther,
                        DamageMarkLifeMs(animation, DamageMark::kLucky) * 0.5F);
  CHECK(lucky_mid.quads[0].corners[0].y == Catch::Approx(lucky_born.quads[0].corners[0].y));

  const ro::render::ComposedActor slow_mid = ComposeDamageMark(
      sprite, animation, DamageMark::kLucky, kMarkOnOther,
      DamageMarkLifeMs(animation, DamageMark::kLucky, slow.stretch) * 0.5F, NumberLook{}, slow);
  REQUIRE(slow_mid.sources.size() == 1);
  CHECK(slow_mid.sources[0].image_index == static_cast<std::int32_t>(kLuckyImages / 2));
}

TEST_CASE("Marque de coup : elle rejoint le point d'arrivée comme un nombre", "[render][mark]") {
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  const float miss_life_ms = DamageMarkLifeMs(animation, DamageMark::kMiss);
  ro::render::NumberLook look;
  look.arrival_x_pixels = 20.0F;

  // Le départ ne bouge pas ; à mi-vie, la moitié du chemin est faite — sans
  // poignée, il va à vitesse égale.
  const ro::render::ComposedActor client_born =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F);
  const ro::render::ComposedActor born =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F, look);
  const ro::render::ComposedActor midway = ComposeDamageMark(
      sprite, animation, DamageMark::kMiss, kMarkOnOther, miss_life_ms * 0.5F, look);
  REQUIRE(client_born.quads.size() == 1);
  REQUIRE(born.quads.size() == 1);
  REQUIRE(midway.quads.size() == 1);
  CHECK(born.quads[0].corners[0].x == Catch::Approx(client_born.quads[0].corners[0].x));
  CHECK(midway.quads[0].corners[0].x ==
        Catch::Approx(client_born.quads[0].corners[0].x + 20.0F * 0.5F));
}

TEST_CASE("Marque de coup : elle part du point de départ et naît à sa taille de naissance",
          "[render][mark]") {
  const SprSprite sprite = MarkSprite();
  const ActAnimation animation = MarkAnimation();
  const ro::render::ComposedActor client =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F);
  REQUIRE(client.quads.size() == 1);
  const float client_width = client.quads[0].corners[1].x - client.quads[0].corners[0].x;

  ro::render::NumberLook look;
  look.start_x_pixels = 12.0F;
  const ro::render::ComposedActor shifted =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F, look);
  REQUIRE(shifted.quads.size() == 1);
  CHECK(shifted.quads[0].corners[0].x == Catch::Approx(client.quads[0].corners[0].x + 12.0F));

  // Une naissance deux fois celle du client : la marque naît deux fois plus
  // large, et reprend sa taille une fois le retour fait.
  const ro::render::MarkPace grown{.birth_ratio = 2.0F, .birth_settle_ms = 400.0F};
  const ro::render::ComposedActor born = ComposeDamageMark(
      sprite, animation, DamageMark::kMiss, kMarkOnOther, 0.0F, ro::render::NumberLook{}, grown);
  const ro::render::ComposedActor settled =
      ComposeDamageMark(sprite, animation, DamageMark::kMiss, kMarkOnOther, grown.birth_settle_ms,
                        ro::render::NumberLook{}, grown);
  REQUIRE(born.quads.size() == 1);
  REQUIRE(settled.quads.size() == 1);
  CHECK(born.quads[0].corners[1].x - born.quads[0].corners[0].x ==
        Catch::Approx(client_width * 2.0F));
  CHECK(settled.quads[0].corners[1].x - settled.quads[0].corners[0].x ==
        Catch::Approx(client_width));
}
