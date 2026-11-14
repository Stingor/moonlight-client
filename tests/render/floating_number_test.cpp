#include "ro/render/floating_number.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/damage_mark.h"
#include "ro/render/sprite_billboard.h"

#include "support/damage_sprites.h"
#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeCriticalNumber;
using ro::render::ComposedActor;
using ro::render::ComposeFloatingNumber;
using ro::render::DamageMark;
using ro::render::DamageMarkLifeMs;
using ro::render::DigitSprite;
using ro::render::FloatingNumber;
using ro::render::FloatingNumberExpired;
using ro::render::kDigitPitchPixels;
using ro::render::kLargestShownValue;
using ro::render::kNumberTickMs;
using ro::render::NumberStyle;
using ro::render::ShownHitCount;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::CenterX;
using ro::test_support::DigitsAnimation;
using ro::test_support::DigitsSprite;
using ro::test_support::kDigitWidth;
using ro::test_support::NearlyEqual;
using ro::test_support::Rise;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::Width;

namespace {

// Les hauteurs du client se lisent en unités du monde, sept pixels chacune :
// vingt-sept pour la montante, quatre pour la jaillissante, douze pour le soin.
constexpr float kUnit = ro::render::kSpritePixelsPerWorldUnit;
constexpr float kRisingStart = 27.0F * kUnit;
constexpr float kBurstingStart = 4.0F * kUnit;
constexpr float kHealingStart = 12.0F * kUnit;
// Le rebond de quatre tiers d'unité, qui rabaisse la cloche dès le tic suivant
// sa naissance.
constexpr float kBounce = 4.0F / 3.0F * kUnit;

}  // namespace

TEST_CASE("Nombre flottant : les chiffres se lisent de gauche à droite, centrés",
          "[render][number]") {
  // Le client détache les chiffres unités d'abord et pose le rang i à
  // 4 × (n − 1 − 2i) : le plus fort à gauche, le nombre centré sur la cible.
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const FloatingNumber number{.value = 1234, .style = NumberStyle::kBursting};

  // Tard dans sa vie, l'allure jaillissante est à l'échelle un et deux dixièmes
  // — une valeur connue, qui laisse lire l'écart.
  const ComposedActor frame =
      ComposeFloatingNumber(DigitSprite{&sprite, &animation}, number, 40.0F * kNumberTickMs);
  REQUIRE(frame.quads.size() == 4);
  REQUIRE(frame.sources.size() == 4);
  CHECK(frame.sources[0].image_index == 1);
  CHECK(frame.sources[1].image_index == 2);
  CHECK(frame.sources[2].image_index == 3);
  CHECK(frame.sources[3].image_index == 4);

  constexpr float kScale = 1.2F;
  for (std::size_t rank = 1; rank < 4; ++rank) {
    CHECK(NearlyEqual(CenterX(frame.quads[rank]) - CenterX(frame.quads[rank - 1]),
                      kDigitPitchPixels * kScale));
  }
  float sum = 0.0F;
  for (const auto& quad : frame.quads) {
    sum += CenterX(quad);
  }
  CHECK(NearlyEqual(sum, 0.0F, 1e-2F));
  CHECK(NearlyEqual(Width(frame.quads[0]), static_cast<float>(kDigitWidth) * kScale));
}

TEST_CASE("Nombre flottant : l'allure montante naît petite, grossit, monte et pâlit",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 7, .color = {255, 255, 255}, .style = NumberStyle::kRising};

  const ComposedActor born = ComposeFloatingNumber(digits, number, 0.0F);
  REQUIRE(born.quads.size() == 1);
  CHECK(NearlyEqual(Width(born.quads[0]), static_cast<float>(kDigitWidth) * 0.5F));
  CHECK(born.quads[0].tint[3] == 250);
  CHECK(NearlyEqual(Rise(born.quads[0]), kRisingStart));

  // Cent tics : l'échelle est à trois depuis longtemps, et le nombre a monté
  // d'un dixième d'unité par tic. Son fondu a commencé au quatre-vingt-dixième,
  // et va d'un pas régulier jusqu'à zéro au cent vingtième : il en a fait le
  // tiers. ⚠ LE CLIENT PERD HUIT POINTS PAR TIC, soit cent soixante-dix ici, et
  // s'éteint à dix : c'est l'écart qu'on accepte pour en faire un réglage.
  const ComposedActor old = ComposeFloatingNumber(digits, number, 100.0F * kNumberTickMs);
  REQUIRE(old.quads.size() == 1);
  CHECK(NearlyEqual(Width(old.quads[0]), static_cast<float>(kDigitWidth) * 3.0F));
  CHECK(old.quads[0].tint[3] == 166);
  CHECK(NearlyEqual(Rise(old.quads[0]), kRisingStart + 10.0F * kUnit));

  CHECK_FALSE(FloatingNumberExpired(NumberStyle::kRising, 120.0F * kNumberTickMs));
  CHECK(FloatingNumberExpired(NumberStyle::kRising, 121.0F * kNumberTickMs));
  CHECK(ComposeFloatingNumber(digits, number, 121.0F * kNumberTickMs).quads.empty());
}

TEST_CASE("Nombre flottant : l'allure jaillissante naît grande, rétrécit et retombe",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 42, .color = {255, 0, 0}, .style = NumberStyle::kBursting};

  const ComposedActor born = ComposeFloatingNumber(digits, number, 0.0F);
  REQUIRE(born.quads.size() == 2);
  CHECK(NearlyEqual(Width(born.quads[0]), static_cast<float>(kDigitWidth) * 5.0F));
  CHECK(born.quads[0].tint[0] == 255);
  CHECK(born.quads[0].tint[1] == 0);
  // ⚠ LE REBOND Y EST DÉJÀ : le client ne le retire qu'à la deuxième image, et
  // une image ne vaut pas un réglage à part.
  CHECK(NearlyEqual(Rise(born.quads[0]), kBurstingStart - kBounce));

  // Au sommet de la cloche, trentième tic : trente unités de plus, moins le
  // rebond, l'échelle déjà au plancher, l'opacité entamée des trois septièmes
  // — d'un pas régulier jusqu'à zéro au soixante-dixième tic. Le client, qui
  // perd trois virgule quatre points par tic, en est à cent quarante-huit.
  const ComposedActor peak = ComposeFloatingNumber(digits, number, 30.0F * kNumberTickMs);
  REQUIRE(peak.quads.size() == 2);
  CHECK(NearlyEqual(Rise(peak.quads[0]), kBurstingStart + 30.0F * kUnit - kBounce, 1e-2F));
  CHECK(NearlyEqual(Width(peak.quads[0]), static_cast<float>(kDigitWidth) * 1.2F));
  CHECK(peak.quads[0].tint[3] == 142);

  // Redescendu, il passe SOUS les pieds, comme chez le client, qui ne le cache
  // pas : quatre, plus 2t − t²/30 au soixante-neuvième tic, moins le rebond.
  CHECK(NearlyEqual(Rise(ComposeFloatingNumber(digits, number, 69.0F * kNumberTickMs).quads[0]),
                    kBurstingStart + (138.0F - 69.0F * 69.0F / 30.0F) * kUnit - kBounce, 1e-2F));
  CHECK(FloatingNumberExpired(NumberStyle::kBursting, 71.0F * kNumberTickMs));
}

TEST_CASE("Nombre flottant : la montée déplace le nombre sans l'étirer", "[render][number]") {
  // La hauteur s'ajoute APRÈS l'échelle. Multipliée avant, elle grandirait avec
  // le nombre : à l'échelle trois, l'allure montante partirait trois fois trop
  // haut et monterait trois fois trop vite.
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 3, .style = NumberStyle::kRising};

  const ComposedActor grown = ComposeFloatingNumber(digits, number, 50.0F * kNumberTickMs);
  REQUIRE(grown.quads.size() == 1);
  // L'échelle est au plafond depuis longtemps ; la hauteur, elle, suit ses
  // cinquante tics à un dixième d'unité.
  CHECK(NearlyEqual(Width(grown.quads[0]), static_cast<float>(kDigitWidth) * 3.0F));
  CHECK(NearlyEqual(Rise(grown.quads[0]), kRisingStart + 5.0F * kUnit));

  // Le nombre reste centré sur sa cible en abscisse : seule l'ordonnée bouge.
  CHECK(NearlyEqual(CenterX(grown.quads[0]), 0.0F));
}

TEST_CASE("Nombre flottant : zéro se montre, et un million se borne", "[render][number]") {
  // Le client borne à 999 999 quand le réglage des marqueurs K et M est éteint.
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};

  const ComposedActor zero = ComposeFloatingNumber(digits, FloatingNumber{.value = 0}, 0.0F);
  REQUIRE(zero.quads.size() == 1);
  CHECK(zero.sources[0].image_index == 0);

  const ComposedActor huge =
      ComposeFloatingNumber(digits, FloatingNumber{.value = kLargestShownValue + 1}, 0.0F);
  REQUIRE(huge.quads.size() == 6);
  for (const auto& source : huge.sources) {
    CHECK(source.image_index == 9);
  }
}

TEST_CASE("Nombre flottant : sans sprite ou avant sa naissance, rien", "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  CHECK(ComposeFloatingNumber(DigitSprite{}, FloatingNumber{.value = 5}, 0.0F).quads.empty());
  CHECK(ComposeFloatingNumber(DigitSprite{&sprite, &animation}, FloatingNumber{.value = 5}, -1.0F)
            .quads.empty());
}

TEST_CASE("Nombre flottant : un coup multiple se montre coup par coup", "[render][number]") {
  // Le serveur n'envoie qu'un montant, le TOTAL, et le nombre de coups qui
  // l'ont fait. Le compte ne vaut que pour les sortes de coup qui l'annoncent :
  // ailleurs, le champ porte autre chose, et en tirer des nombres en ferait
  // naître au hasard.
  using ro::render::kCriticalMultipleBlows;
  using ro::render::kLargestHitCount;
  using ro::render::kMultipleBlows;
  using ro::render::kMultipleBlowsEndured;

  CHECK(ShownHitCount(kMultipleBlows, 3) == 3);
  CHECK(ShownHitCount(kMultipleBlowsEndured, 7) == 7);
  CHECK(ShownHitCount(kCriticalMultipleBlows, 2) == 2);

  // Un coup ordinaire, un critique simple : un seul nombre, quoi que le champ
  // annonce.
  CHECK(ShownHitCount(0, 5) == 1);
  CHECK(ShownHitCount(10, 5) == 1);

  // Un compte absurde ne fait pas naître trente-six mille nombres.
  CHECK(ShownHitCount(kMultipleBlows, 0) == 1);
  CHECK(ShownHitCount(kMultipleBlows, kLargestHitCount) == kLargestHitCount);
  CHECK(ShownHitCount(kMultipleBlows, kLargestHitCount + 1) == 1);
}

TEST_CASE("Nombre : deux compétences sans dégâts seulement en portent un", "[render][number]") {
  using ro::render::kHealNumberColor;
  using ro::render::kHealSkill;
  using ro::render::kSpRecoveryNumberColor;
  using ro::render::kSpRecoverySkill;
  using ro::render::SupportNumberColor;

  // Le répartiteur du client traite des centaines de compétences sans dégâts
  // et deux seulement y posent un nombre : le soin, en vert, et la
  // récupération de points de magie, en bleu.
  REQUIRE(SupportNumberColor(kHealSkill).has_value());
  CHECK(*SupportNumberColor(kHealSkill) == kHealNumberColor);
  REQUIRE(SupportNumberColor(kSpRecoverySkill).has_value());
  CHECK(*SupportNumberColor(kSpRecoverySkill) == kSpRecoveryNumberColor);

  // Le vert et le bleu, et non l'inverse : les intervertir passerait un essai
  // qui ne comparerait que la présence.
  CHECK(kHealNumberColor[1] == 255);
  CHECK(kSpRecoveryNumberColor[2] == 255);

  // Tout le reste n'en a pas — y compris les voisines immédiates.
  CHECK_FALSE(SupportNumberColor(0).has_value());
  CHECK_FALSE(SupportNumberColor(kHealSkill - 1).has_value());
  CHECK_FALSE(SupportNumberColor(kHealSkill + 1).has_value());
  CHECK_FALSE(SupportNumberColor(kSpRecoverySkill + 1).has_value());
  CHECK_FALSE(SupportNumberColor(60000).has_value());
}

TEST_CASE("Nombre : le soin et le critique ont leur propre allure", "[render][number]") {
  using ro::render::FloatingNumberExpired;
  using ro::render::kNumberTickMs;
  using ro::render::NumberStyle;

  // Les deux cloches vivent soixante-dix tics, le montant et le soin cent
  // vingt : confondre les durées laisserait un nombre à l'écran ou l'en
  // ôterait trop tôt.
  CHECK(FloatingNumberExpired(NumberStyle::kCriticalBurst, 71.0F * kNumberTickMs));
  CHECK_FALSE(FloatingNumberExpired(NumberStyle::kCriticalBurst, 69.0F * kNumberTickMs));
  CHECK_FALSE(FloatingNumberExpired(NumberStyle::kHealing, 100.0F * kNumberTickMs));
  CHECK(FloatingNumberExpired(NumberStyle::kHealing, 121.0F * kNumberTickMs));

  // Le soin naît à DOUZE unités au-dessus des pieds — le message de position
  // retire douze, sans les quinze de la montante — et monte de dix-huit
  // centièmes d'unité par tic.
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber heal{.value = 7, .style = NumberStyle::kHealing};
  CHECK(NearlyEqual(Rise(ComposeFloatingNumber(digits, heal, 0.0F).quads[0]), kHealingStart));
  CHECK(NearlyEqual(Rise(ComposeFloatingNumber(digits, heal, 50.0F * kNumberTickMs).quads[0]),
                    kHealingStart + 9.0F * kUnit));
}

TEST_CASE("Nombre flottant : la cloche dérive de côté selon l'angle de la cible",
          "[render][number]") {
  using ro::render::BurstDriftClientDiagonal;
  using ro::render::BurstDriftUnitsAt;
  using ro::render::IsBurst;
  using Diagonal = std::array<float, 2>;

  // Trois unités dès la naissance, puis un tiers d'unité par tic.
  CHECK(BurstDriftUnitsAt(0.0F) == Catch::Approx(3.0F));
  CHECK(BurstDriftUnitsAt(30.0F) == Catch::Approx(13.0F));
  CHECK(BurstDriftUnitsAt(70.0F) == Catch::Approx(3.0F + 70.0F / 3.0F));

  // La diagonale par quadrant de l'angle du client, dans SON repère.
  CHECK(BurstDriftClientDiagonal(0.0F) == Diagonal{-0.8F, 0.8F});
  CHECK(BurstDriftClientDiagonal(89.0F) == Diagonal{-0.8F, 0.8F});
  CHECK(BurstDriftClientDiagonal(90.0F) == Diagonal{-0.8F, -0.8F});
  CHECK(BurstDriftClientDiagonal(180.0F) == Diagonal{0.8F, -0.8F});
  CHECK(BurstDriftClientDiagonal(270.0F) == Diagonal{0.8F, 0.8F});
  CHECK(BurstDriftClientDiagonal(359.0F) == Diagonal{0.8F, 0.8F});
  // 🔴 L'ANGLE NE SE RAMÈNE PAS AU TOUR : au-delà de deux cent soixante-dix, et
  // en deçà de zéro, tout prend (+x, +z). Le client tient quatre cent cinquante
  // degrés pour une cible que le serveur tourne vers l'est.
  CHECK(BurstDriftClientDiagonal(450.0F) == Diagonal{0.8F, 0.8F});
  CHECK(BurstDriftClientDiagonal(360.0F) == Diagonal{0.8F, 0.8F});
  CHECK(BurstDriftClientDiagonal(-90.0F) == Diagonal{0.8F, 0.8F});
  // Et l'angle est TRONQUÉ en entier : un rien sous zéro compte pour zéro.
  CHECK(BurstDriftClientDiagonal(-0.5F) == Diagonal{-0.8F, 0.8F});

  // Seules les deux cloches dérivent.
  CHECK(IsBurst(NumberStyle::kBursting));
  CHECK(IsBurst(NumberStyle::kCriticalBurst));
  CHECK_FALSE(IsBurst(NumberStyle::kRising));
  CHECK_FALSE(IsBurst(NumberStyle::kHealing));
}

TEST_CASE("Nombre : dix effets seulement font monter un nombre", "[render][number]") {
  // Le client joue l'effet de tous les numéros et n'en montre la valeur que
  // pour dix : deux qui nomment
  // ce qu'ils comptent, huit qui ne nomment qu'une couleur.
  using ro::render::EffectNumberColor;

  // La vie rendue au fil du temps prend le vert du soin, la magie le bleu de la
  // récupération : les mêmes que les compétences correspondantes.
  REQUIRE(EffectNumberColor(ro::render::kHpOverTimeEffect).has_value());
  CHECK(*EffectNumberColor(ro::render::kHpOverTimeEffect) == ro::render::kHealNumberColor);
  REQUIRE(EffectNumberColor(ro::render::kSpOverTimeEffect).has_value());
  CHECK(*EffectNumberColor(ro::render::kSpOverTimeEffect) == ro::render::kSpRecoveryNumberColor);

  // Les huit couleurs, dans l'ordre de e_effect_type : vert, bleu, rouge,
  // violet, noir, blanc, jaune, rose.
  const std::array<std::array<std::uint8_t, 3>, 8> attendues = {{
      {0, 255, 0},
      {0, 0, 255},
      {255, 0, 0},
      {255, 0, 255},
      {0, 0, 0},
      {255, 255, 255},
      {255, 255, 0},
      {255, 85, 177},
  }};
  for (std::uint32_t effect_rank = 0; effect_rank < attendues.size(); ++effect_rank) {
    const auto number_color =
        EffectNumberColor(ro::render::kFirstColoredNumberEffect + effect_rank);
    REQUIRE(number_color.has_value());
    CHECK(*number_color == attendues[effect_rank]);
  }

  // De part et d'autre de la plage, et ailleurs : rien. Le nombre ne paraît
  // pas, mais l'effet, lui, se joue quand même.
  CHECK_FALSE(EffectNumberColor(ro::render::kFirstColoredNumberEffect - 1).has_value());
  CHECK_FALSE(EffectNumberColor(ro::render::kLastColoredNumberEffect + 1).has_value());
  CHECK_FALSE(EffectNumberColor(0).has_value());
  CHECK_FALSE(EffectNumberColor(330).has_value());
  CHECK_FALSE(EffectNumberColor(333).has_value());
}

TEST_CASE("Nombre flottant : abrégé, un million s'écrit en milliers et K", "[render][number]") {
  using ro::render::kMillionsGlyph;
  using ro::render::kThousandsGlyph;
  using ro::render::NumberGlyphs;
  using Glyphs = std::vector<std::uint8_t>;

  // Sans le réglage, tout se borne à 999 999.
  CHECK(NumberGlyphs(1'234'567, false) == Glyphs{9, 9, 9, 9, 9, 9});
  CHECK(NumberGlyphs(1'234'567, false) == NumberGlyphs(kLargestShownValue, false));

  // Avec : d'un million à cent millions, en milliers suivis de K. Le marqueur
  // est détaché le premier, donc il prend le rang des unités et se dessine à
  // droite : « 1234K ».
  CHECK(NumberGlyphs(1'234'567, true) == Glyphs{kThousandsGlyph, 4, 3, 2, 1});
  CHECK(NumberGlyphs(1'000'000, true) == Glyphs{kThousandsGlyph, 0, 0, 0, 1});
  CHECK(NumberGlyphs(999'999, true) == Glyphs{9, 9, 9, 9, 9, 9});
  // Au-delà, en millions suivis de M : « 123M ».
  CHECK(NumberGlyphs(123'456'789, true) == Glyphs{kMillionsGlyph, 3, 2, 1});
  CHECK(NumberGlyphs(100'000'000, true) == Glyphs{kMillionsGlyph, 0, 0, 1});
  CHECK(NumberGlyphs(0, true) == Glyphs{0});

  // Et la composition pose bien le K à DROITE des chiffres.
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  ro::render::NumberLook look;
  look.abbreviate = true;
  const ComposedActor frame =
      ComposeFloatingNumber(DigitSprite{&sprite, &animation},
                            FloatingNumber{.value = 1'234'567, .style = NumberStyle::kBursting},
                            40.0F * kNumberTickMs, look);
  REQUIRE(frame.quads.size() == 5);
  CHECK(frame.sources.back().image_index == kThousandsGlyph);
  CHECK(frame.sources.front().image_index == 1);
  CHECK(CenterX(frame.quads.back()) > CenterX(frame.quads[3]));
}

TEST_CASE("Nombre flottant : les facteurs du réglage multiplient sans changer les constantes",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 7, .style = NumberStyle::kRising};

  // La taille : l'échelle de naissance, un demi, doublée.
  ro::render::NumberLook look;
  look.scale = 2.0F;
  const ComposedActor bigger = ComposeFloatingNumber(digits, number, 0.0F, look);
  REQUIRE(bigger.quads.size() == 1);
  CHECK(NearlyEqual(Width(bigger.quads[0]), static_cast<float>(kDigitWidth) * 0.5F * 2.0F));

  // Une vie doublée : le nombre vit deux fois plus longtemps, et parcourt son
  // chemin deux fois plus lentement — à cent tics, il n'en est qu'à cinq
  // unités. Son fondu, lui, commence à l'heure dite.
  look = ro::render::ClientLookOf(NumberStyle::kRising);
  look.life->life_ms *= 2.0F;
  CHECK_FALSE(FloatingNumberExpired(NumberStyle::kRising, 240.0F * kNumberTickMs, look));
  CHECK(FloatingNumberExpired(NumberStyle::kRising, 241.0F * kNumberTickMs, look));
  const ComposedActor slower = ComposeFloatingNumber(digits, number, 100.0F * kNumberTickMs, look);
  REQUIRE(slower.quads.size() == 1);
  CHECK(NearlyEqual(Rise(slower.quads[0]), kRisingStart + 5.0F * kUnit));
  CHECK(slower.quads[0].tint[3] == 233);

  // La hauteur de départ s'ajoute telle quelle.
  look = ro::render::NumberLook{};
  look.lift_pixels = 10.0F;
  CHECK(NearlyEqual(Rise(ComposeFloatingNumber(digits, number, 0.0F, look).quads[0]),
                    kRisingStart + 10.0F));

  // L'opacité voile, en facteur sur celle de la naissance.
  look = ro::render::NumberLook{};
  look.opacity = 0.5F;
  CHECK(ComposeFloatingNumber(digits, number, 0.0F, look).quads[0].tint[3] == 125);

  // L'écart entre chiffres, à l'échelle du moment.
  look = ro::render::NumberLook{};
  look.digit_pitch_pixels = 16.0F;
  const ComposedActor spaced =
      ComposeFloatingNumber(digits, FloatingNumber{.value = 12, .style = NumberStyle::kBursting},
                            40.0F * kNumberTickMs, look);
  REQUIRE(spaced.quads.size() == 2);
  CHECK(NearlyEqual(CenterX(spaced.quads[1]) - CenterX(spaced.quads[0]), 16.0F * 1.2F));
}

TEST_CASE("Nombre flottant : l'opacité propre du nombre s'ajoute à celle du réglage",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber half{.value = 7, .opacity = 0.5F, .style = NumberStyle::kRising};
  CHECK(ComposeFloatingNumber(digits, half, 0.0F).quads[0].tint[3] == 125);
  ro::render::NumberLook look;
  look.opacity = 0.5F;
  CHECK(ComposeFloatingNumber(digits, half, 0.0F, look).quads[0].tint[3] == 62);
}

TEST_CASE("Nombre flottant : la cloche du client est une trajectoire exacte", "[render][number]") {
  // La parabole du client — 2t − t²/30 unités, t en tics — est une courbe de
  // Bézier dans le temps : sans réglage, le nombre passe là où le client le
  // passe, au sommet comme sous les pieds.
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 42, .style = NumberStyle::kBursting};
  const ComposedActor peak = ComposeFloatingNumber(digits, number, 30.0F * kNumberTickMs);
  REQUIRE(peak.quads.size() == 2);
  CHECK(NearlyEqual(Rise(peak.quads[0]), kBurstingStart + 30.0F * kUnit - kBounce, 1e-2F));
  const ComposedActor fallen = ComposeFloatingNumber(digits, number, 69.0F * kNumberTickMs);
  CHECK(NearlyEqual(Rise(fallen.quads[0]),
                    kBurstingStart + (138.0F - 69.0F * 69.0F / 30.0F) * kUnit - kBounce, 1e-2F));

  // Son chemin triplé monte trois fois plus loin ; la naissance reste celle du
  // client.
  ro::render::NumberLook look = ro::render::ClientLookOf(NumberStyle::kBursting);
  look.arrival_y_pixels *= 3.0F;
  look.bend_one_y_pixels *= 3.0F;
  look.bend_two_y_pixels *= 3.0F;
  const ComposedActor tripled = ComposeFloatingNumber(digits, number, 30.0F * kNumberTickMs, look);
  CHECK(NearlyEqual(Rise(tripled.quads[0]), kBurstingStart + 90.0F * kUnit - kBounce, 1e-2F));
}

TEST_CASE("Nombre flottant : sa vie le grandit, puis l'efface d'un pas régulier",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 7, .style = NumberStyle::kHealing};
  ro::render::NumberLook look;
  look.life = ro::render::NumberLife{.fade_start_ms = 1000.0F, .life_ms = 2000.0F};
  // Opaque jusqu'au début du fondu, à moitié au milieu, éteint au terme.
  CHECK(ComposeFloatingNumber(digits, number, 900.0F, look).quads[0].tint[3] == 250);
  CHECK(ComposeFloatingNumber(digits, number, 1500.0F, look).quads[0].tint[3] == 125);
  CHECK(ComposeFloatingNumber(digits, number, 2000.0F, look).quads[0].tint[3] == 0);
  CHECK(ComposeFloatingNumber(digits, number, 2001.0F, look).quads.empty());
}

TEST_CASE("Nombre flottant : il rejoint son point d'arrivée à vitesse égale", "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 7, .style = NumberStyle::kRising};
  ro::render::NumberLook look;
  look.arrival_x_pixels = 20.0F;
  look.arrival_y_pixels = 12.0F;

  // Le DÉPART ne bouge pas : à la naissance, le nombre est là où le client le
  // fait naître. ⚠ CE RÉGLAGE N'A QUE CE CHEMIN : la montée du client n'y est
  // pas.
  const ComposedActor born = ComposeFloatingNumber(digits, number, 0.0F, look);
  REQUIRE(born.quads.size() == 1);
  CHECK(NearlyEqual(CenterX(born.quads[0]), 0.0F));
  CHECK(NearlyEqual(Rise(born.quads[0]), kRisingStart));

  // À mi-vie, la moitié du chemin est faite : sans poignée, il va droit et à
  // vitesse égale.
  const ComposedActor midway = ComposeFloatingNumber(digits, number, 60.0F * kNumberTickMs, look);
  REQUIRE(midway.quads.size() == 1);
  CHECK(NearlyEqual(CenterX(midway.quads[0]), 20.0F * 0.5F));
  CHECK(NearlyEqual(Rise(midway.quads[0]), kRisingStart + 12.0F * 0.5F));

  // À l'extinction, il y est.
  const ComposedActor arrived = ComposeFloatingNumber(digits, number, 120.0F * kNumberTickMs, look);
  REQUIRE(arrived.quads.size() == 1);
  CHECK(NearlyEqual(CenterX(arrived.quads[0]), 20.0F));
  CHECK(NearlyEqual(Rise(arrived.quads[0]), kRisingStart + 12.0F));

  // Le chemin est APRÈS l'échelle : à l'échelle trois, vingt pixels restent
  // vingt pixels.
  CHECK(NearlyEqual(Width(arrived.quads[0]), static_cast<float>(kDigitWidth) * 3.0F));
}

TEST_CASE("Nombre flottant : les poignées bombent le chemin, et il retombe à l'arrivée",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 7, .style = NumberStyle::kRising};
  // DEUX POIGNÉES ÉGALES ET VERTICALES FONT UN ARC : à quatre tiers de la
  // flèche, la courbe est la parabole de flèche vingt.
  constexpr float kArcHandle = 20.0F * 4.0F / 3.0F;
  ro::render::NumberLook look;
  look.bend_one_y_pixels = kArcHandle;
  look.bend_two_y_pixels = kArcHandle;

  // Sans arrivée, l'arc seul fait sauter le nombre sur place : à mi-vie, il est
  // au sommet de sa flèche, et retombe à son départ.
  CHECK(
      NearlyEqual(Rise(ComposeFloatingNumber(digits, number, 0.0F, look).quads[0]), kRisingStart));
  CHECK(
      NearlyEqual(Rise(ComposeFloatingNumber(digits, number, 60.0F * kNumberTickMs, look).quads[0]),
                  kRisingStart + 20.0F));
  CHECK(NearlyEqual(
      Rise(ComposeFloatingNumber(digits, number, 120.0F * kNumberTickMs, look).quads[0]),
      kRisingStart));

  // Négatif, il bombe vers le bas.
  look.bend_one_y_pixels = -kArcHandle;
  look.bend_two_y_pixels = -kArcHandle;
  CHECK(
      NearlyEqual(Rise(ComposeFloatingNumber(digits, number, 60.0F * kNumberTickMs, look).quads[0]),
                  kRisingStart - 20.0F));

  // DEUX POIGNÉES OPPOSÉES FONT UN S : le nombre passe d'un côté puis de
  // l'autre, et revient sur place au bout du chemin.
  look = ro::render::NumberLook{};
  look.bend_one_x_pixels = 30.0F;
  look.bend_two_x_pixels = -30.0F;
  CHECK(ro::render::ArrivalDriftAt(0.1F, look)[0] > 0.0F);
  CHECK(ro::render::ArrivalDriftAt(0.6F, look)[0] < 0.0F);
  CHECK(ro::render::ArrivalDriftAt(1.0F, look)[0] == Catch::Approx(0.0F).margin(1e-4F));

  // Sans arrivée ni poignée, le nombre ne bouge pas.
  const std::array<float, 2> still = ro::render::ArrivalDriftAt(0.5F, ro::render::NumberLook{});
  CHECK(still[0] == 0.0F);
  CHECK(still[1] == 0.0F);
}

TEST_CASE("Nombre flottant : le départ se décale à l'horizontale, et le chemin le suit",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{.value = 7, .style = NumberStyle::kRising};
  ro::render::NumberLook look;
  look.start_x_pixels = -15.0F;
  look.arrival_x_pixels = 20.0F;

  // Le nombre naît à quinze pixels à gauche, à sa hauteur de toujours…
  const ComposedActor born = ComposeFloatingNumber(digits, number, 0.0F, look);
  REQUIRE(born.quads.size() == 1);
  CHECK(NearlyEqual(CenterX(born.quads[0]), -15.0F));
  CHECK(NearlyEqual(Rise(born.quads[0]), kRisingStart));
  // … et son point d'arrivée se compte depuis ce départ-là.
  const ComposedActor arrived = ComposeFloatingNumber(digits, number, 120.0F * kNumberTickMs, look);
  REQUIRE(arrived.quads.size() == 1);
  CHECK(NearlyEqual(CenterX(arrived.quads[0]), -15.0F + 20.0F));
}

TEST_CASE("Nombre flottant : il naît à sa taille de naissance, et rejoint sa taille finale",
          "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber burst{.value = 7, .style = NumberStyle::kBursting};
  const auto width_at = [&](const FloatingNumber& number, float elapsed_ms,
                            const ro::render::NumberLook& look) {
    return Width(ComposeFloatingNumber(digits, number, elapsed_ms, look).quads[0]) /
           static_cast<float>(kDigitWidth);
  };

  // LE CLIENT : cinq fois la taille, un point de moins toutes les cent
  // millisecondes, et la cloche s'arrête à un point vingt.
  ro::render::NumberLook look = ro::render::ClientLookOf(NumberStyle::kBursting);
  CHECK(NearlyEqual(width_at(burst, 0.0F, look), 5.0F));
  CHECK(NearlyEqual(width_at(burst, 200.0F, look), 3.0F));
  CHECK(NearlyEqual(width_at(burst, 600.0F, look), 1.2F));

  // Deux fois et demie sa taille finale, ramené en une seconde : à mi-temps,
  // une fois trois quarts ; puis la taille finale de la cloche.
  look.life->birth_scale = 2.5F;
  look.life->birth_settle_ms = 1000.0F;
  CHECK(NearlyEqual(width_at(burst, 0.0F, look), 1.2F * 2.5F));
  CHECK(NearlyEqual(width_at(burst, 500.0F, look), 1.2F * 1.75F));
  CHECK(NearlyEqual(width_at(burst, 1500.0F, look), 1.2F));
  // Sans temps de retour, il naît à sa taille finale.
  look.life->birth_settle_ms = 0.0F;
  CHECK(NearlyEqual(width_at(burst, 0.0F, look), 1.2F));

  // LE NOMBRE QUI MONTE naît au sixième de sa taille finale, trois ; né à sa
  // taille finale, il ne grandit plus.
  const FloatingNumber rising{.value = 7, .style = NumberStyle::kRising};
  look = ro::render::ClientLookOf(NumberStyle::kRising);
  CHECK(NearlyEqual(width_at(rising, 0.0F, look), 0.5F));
  look.life->birth_scale = 1.0F;
  CHECK(NearlyEqual(width_at(rising, 0.0F, look), 3.0F));
}

TEST_CASE("Nombre flottant : écrit en police, il garde la pose du sprite", "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const DigitSprite digits{&sprite, &animation};
  const FloatingNumber number{
      .value = 1234, .color = {255, 128, 0}, .opacity = 0.5F, .style = NumberStyle::kBursting};
  ro::render::NumberLook look;
  look.start_x_pixels = 6.0F;
  look.lift_pixels = 9.0F;
  constexpr float kElapsedMs = 40.0F * kNumberTickMs;

  const std::optional<ro::render::NumberText> text =
      ro::render::ComposeFloatingNumberText(number, kElapsedMs, look);
  const ComposedActor frame = ComposeFloatingNumber(digits, number, kElapsedMs, look);
  const std::optional<ro::render::GlyphBox> box = ro::render::DigitGlyphBox(digits);
  REQUIRE(text.has_value());
  REQUIRE(box.has_value());
  REQUIRE(frame.quads.size() == 4);
  CHECK(text->glyphs == "1234");
  CHECK(text->tint == frame.quads[0].tint);
  // Chaque glyphe tombe au milieu du chiffre qu'il remplace, à la même taille.
  for (std::size_t rank = 0; rank < 4; ++rank) {
    const ro::render::SpriteQuad& quad = frame.quads[rank];
    const float slot = text->pitch_pixels * text->scale * (static_cast<float>(rank) - 1.5F);
    CHECK(NearlyEqual(CenterX(quad), text->landing_pixels.x + slot));
    CHECK(NearlyEqual((quad.corners[0].y + quad.corners[2].y) * 0.5F,
                      text->landing_pixels.y + box->middle_y_pixels * text->scale));
    CHECK(NearlyEqual(quad.corners[2].y - quad.corners[0].y, box->height_pixels * text->scale));
  }

  // Abrégé, la lettre suit les chiffres ; éteint, il n'y a plus rien à écrire.
  look.abbreviate = true;
  const FloatingNumber million{.value = 1'000'000, .style = NumberStyle::kRising};
  CHECK(ro::render::ComposeFloatingNumberText(million, 0.0F, look)->glyphs == "1000K");
  CHECK_FALSE(
      ro::render::ComposeFloatingNumberText(number, 71.0F * kNumberTickMs, look).has_value());
  CHECK_FALSE(ro::render::ComposeFloatingNumberText(number, -1.0F, look).has_value());
}

TEST_CASE("Nombre flottant : la boîte d'un chiffre est celle du zéro", "[render][number]") {
  const SprSprite sprite = DigitsSprite();
  const ActAnimation animation = DigitsAnimation();
  const std::optional<ro::render::GlyphBox> box =
      ro::render::DigitGlyphBox(DigitSprite{&sprite, &animation});
  REQUIRE(box.has_value());
  CHECK(NearlyEqual(box->height_pixels, static_cast<float>(ro::test_support::kDigitHeight)));
  CHECK_FALSE(ro::render::DigitGlyphBox(DigitSprite{}).has_value());
}

TEST_CASE("Nombre flottant : l'étoile seule est celle du nombre critique", "[render][number]") {
  const SprSprite digits_sprite = DigitsSprite();
  const ActAnimation digits_animation = DigitsAnimation();
  const SprSprite star_sprite = ro::test_support::StarSprite();
  const ActAnimation star_animation = ro::test_support::StarAnimation();
  const FloatingNumber number{.value = 42, .style = NumberStyle::kCriticalBurst};
  ro::render::NumberLook look;
  look.start_x_pixels = 5.0F;
  constexpr float kElapsedMs = 10.0F * kNumberTickMs;

  const ComposedActor whole =
      ComposeCriticalNumber(DigitSprite{&digits_sprite, &digits_animation}, number, star_sprite,
                            star_animation, kElapsedMs, look);
  const ComposedActor star = ro::render::ComposeNumberStar(number, star_sprite, star_animation,
                                                           DamageMark::kCritical, kElapsedMs, look);
  // L'étoile, et elle seule : les deux chiffres manquent, et ce qui reste est
  // le fond du nombre entier, à la même place.
  REQUIRE_FALSE(star.quads.empty());
  REQUIRE(whole.quads.size() == star.quads.size() + 2);
  for (std::size_t index = 0; index < star.quads.size(); ++index) {
    CHECK(star.quads[index].corners == whole.quads[index].corners);
  }
}
