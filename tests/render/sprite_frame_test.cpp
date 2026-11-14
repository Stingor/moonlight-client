#include "ro/render/sprite_frame.h"

#include <cmath>
#include <numbers>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ActionIndex;
using ro::render::AnimationFor;
using ro::render::AttachOffset;
using ro::render::BuildFrameQuads;
using ro::render::FrameAnchor;
using ro::render::kAnimationIdle;
using ro::render::Motion;
using ro::render::MotionLoops;
using ro::render::SpriteDirection;
using ro::render::SpriteLayout;
using ro::render::SpriteQuad;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Un .spr dont l'image 0 fait 4×6 : dimensions PAIRES et DIFFÉRENTES, pour que
// toute confusion entre les deux axes se voie.
SprSprite EvenSprite() {
  SprIndexedFrameSpec frame{.width = 4, .height = 6};
  frame.indices.assign(24, 1);
  SprBuildOptions options;
  options.indexed = {frame};
  options.palette = {SpriteColor{}, SpriteColor{.red = 255}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

ActAnimation AnimationOf(const ActBuildOptions& options) {
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

// Une action unique, portant un seul calque.
ActBuildOptions OneLayerAt(const ActLayerSpec& layer) {
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {ActFrameSpec{.layers = {layer}}}}};
  options.speeds = {6.0F};
  return options;
}

}  // namespace

TEST_CASE("ActionIndex : huit directions par animation", "[render][sprite]") {
  // Les actions sont rangées par groupes de huit. Se tromper de groupe affiche
  // la mauvaise animation, se tromper de rang affiche le mauvais profil.
  CHECK(ActionIndex(0, 0) == 0);
  CHECK(ActionIndex(0, 7) == 7);
  CHECK(ActionIndex(1, 0) == 8);
  CHECK(ActionIndex(3, 5) == 29);
  // Une direction hors bornes revient dans le groupe plutôt que d'en sortir.
  CHECK(ActionIndex(1, 8) == 8);
}

TEST_CASE("Quad : l'image est CENTRÉE sur l'origine du calque", "[render][sprite]") {
  // Le jeu translate de moins la moitié des dimensions, par division ENTIÈRE.
  // Poser le coin haut-gauche à l'origine décalerait chaque pièce d'une
  // demi-image — et toutes les tailles resteraient justes.
  const SprSprite sprite = EvenSprite();
  const ActAnimation animation = AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0}));

  const std::vector<SpriteQuad> quads = BuildFrameQuads(sprite, animation, 0, 0);
  REQUIRE(quads.size() == 1);
  // 4 de large, 6 de haut : le coin haut-gauche tombe en (-2, -3).
  CHECK(NearlyEqual(quads[0].corners[0].x, -2.0F));
  CHECK(NearlyEqual(quads[0].corners[0].y, -3.0F));
  CHECK(NearlyEqual(quads[0].corners[2].x, 2.0F));
  CHECK(NearlyEqual(quads[0].corners[2].y, 3.0F));
}

TEST_CASE("Quad : le décalage du calque n'est PAS mis à l'échelle", "[render][sprite]") {
  // Le piège documenté de l'implémentation de référence : multiplier le
  // décalage par l'échelle décroche les calques réduits, qui ne suivent alors
  // qu'une fraction du mouvement du sprite. Rien dans les tailles ne le montre.
  const SprSprite sprite = EvenSprite();
  ActLayerSpec layer{.offset_x = 100.0F, .offset_y = 50.0F, .frame_index = 0};
  layer.scale_x = 0.5F;
  layer.scale_y = 0.5F;

  const std::vector<SpriteQuad> quads =
      BuildFrameQuads(sprite, AnimationOf(OneLayerAt(layer)), 0, 0);
  REQUIRE(quads.size() == 1);

  // Le centre du quad reste EXACTEMENT sur le décalage déclaré.
  const float center_x = (quads[0].corners[0].x + quads[0].corners[2].x) * 0.5F;
  const float center_y = (quads[0].corners[0].y + quads[0].corners[2].y) * 0.5F;
  CHECK(NearlyEqual(center_x, 100.0F));
  CHECK(NearlyEqual(center_y, 50.0F));
  // Et l'image, elle, est bien réduite de moitié.
  CHECK(NearlyEqual(quads[0].corners[2].x - quads[0].corners[0].x, 2.0F));
}

TEST_CASE("Quad : le miroir n'est PAS une échelle négative", "[render][sprite]") {
  // La documentation d'un éditeur tiers dit « miroir = échelle X négative », ce
  // qui retournerait le calque autour de son origine et le déplacerait. Le jeu
  // se contente d'échanger les abscisses de texture : le quad ne bouge pas.
  const SprSprite sprite = EvenSprite();
  const ActAnimation plain = AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0}));
  const ActAnimation flipped =
      AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0, .mirrored = true}));

  const std::vector<SpriteQuad> a = BuildFrameQuads(sprite, plain, 0, 0);
  const std::vector<SpriteQuad> b = BuildFrameQuads(sprite, flipped, 0, 0);
  REQUIRE(a.size() == 1);
  REQUIRE(b.size() == 1);
  CHECK_FALSE(a[0].mirrored);
  CHECK(b[0].mirrored);
  // Géométrie identique : seule la lecture de la texture change.
  CHECK(NearlyEqual(a[0].corners[0].x, b[0].corners[0].x));
  CHECK(NearlyEqual(a[0].corners[2].x, b[0].corners[2].x));
}

TEST_CASE("Quad : la rotation est IGNORÉE pour un acteur", "[render][sprite]") {
  // Des costumes portent un angle par image. Le respecter est fidèle au
  // FICHIER et infidèle au JEU, qui construit un rectangle aligné aux axes —
  // sans quoi le chapeau pivote sur la tête.
  const SprSprite sprite = EvenSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0, .rotation_degrees = 90}));

  const std::vector<SpriteQuad> actor = BuildFrameQuads(sprite, animation, 0, 0);
  REQUIRE(actor.size() == 1);
  CHECK(NearlyEqual(actor[0].corners[0].x, -2.0F));
  CHECK(NearlyEqual(actor[0].corners[0].y, -3.0F));

  // Le chemin fidèle au format reste accessible pour les effets.
  const std::vector<SpriteQuad> rotated = BuildFrameQuads(sprite, animation, 0, 0, true);
  REQUIRE(rotated.size() == 1);
  CHECK_FALSE(NearlyEqual(rotated[0].corners[0].x, -2.0F));
}

TEST_CASE("Quad : une teinte entièrement nulle vaut « pas de teinte »", "[render][sprite]") {
  // Quelques .act laissent ce champ à zéro. Le prendre au mot rendrait ces
  // sprites entièrement invisibles, sans erreur ni trace.
  const SprSprite sprite = EvenSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0, .tint = {0, 0, 0, 0}}));

  const std::vector<SpriteQuad> quads = BuildFrameQuads(sprite, animation, 0, 0);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].tint == std::array<std::uint8_t, 4>{255, 255, 255, 255});
}

TEST_CASE("Quad : un calque sans image est écarté", "[render][sprite]") {
  const SprSprite sprite = EvenSprite();
  ActBuildOptions options;
  options.actions = {
      ActActionSpec{.frames = {ActFrameSpec{.layers = {ActLayerSpec{.frame_index = -1},
                                                       ActLayerSpec{.frame_index = 0},
                                                       ActLayerSpec{.frame_index = 99}}}}}};
  options.speeds = {6.0F};

  const std::vector<SpriteQuad> quads = BuildFrameQuads(sprite, AnimationOf(options), 0, 0);
  CHECK(quads.size() == 1);
}

TEST_CASE("Ancre : au repos, elle se lit sur l'IMAGE 0", "[render][sprite]") {
  // Sans cette règle, les pièces rapportées VIBRENT pendant l'animation
  // d'attente : elles suivraient une ancre qui bouge d'une image à l'autre
  // alors que le jeu, lui, garde celle de la première.
  ActFrameSpec first{.layers = {ActLayerSpec{.frame_index = 0}}};
  first.anchors = {ActAnchorSpec{.x = 10, .y = 20}};
  ActFrameSpec second{.layers = {ActLayerSpec{.frame_index = 0}}};
  second.anchors = {ActAnchorSpec{.x = 77, .y = 88}};

  ActBuildOptions options;
  // Action 0 : premier groupe, donc « au repos ». Action 8 : hors du groupe.
  options.actions.assign(9, ActActionSpec{.frames = {first, second}});
  options.speeds.assign(9, 6.0F);
  const ActAnimation animation = AnimationOf(options);

  // Action au repos, image 1 : l'ancre reste celle de l'image 0.
  const auto resting = FrameAnchor(animation, 0, 1);
  REQUIRE(resting.has_value());
  CHECK(resting->x == 10);

  // Action 8, image 1 : l'ancre suit bien l'image courante — c'est ce qui
  // empêche la tête de décrocher du cou pendant la marche et le combat.
  const auto moving = FrameAnchor(animation, 8, 1);
  REQUIRE(moving.has_value());
  CHECK(moving->x == 77);
}

TEST_CASE("Cadence : l'image suit le temps écoulé, et boucle", "[render][sprite]") {
  ActFrameSpec frame{.layers = {ActLayerSpec{.frame_index = 0}}};
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {frame, frame, frame, frame}}};
  options.speeds = {4.0F};  // 4 × 24 ms = 96 ms par image
  const ActAnimation animation = AnimationOf(options);
  const auto& action = animation.actions()[0];

  CHECK(ro::render::FrameAtTime(action, 0.0F) == 0);
  CHECK(ro::render::FrameAtTime(action, 95.0F) == 0);
  CHECK(ro::render::FrameAtTime(action, 96.0F) == 1);
  CHECK(ro::render::FrameAtTime(action, 350.0F) == 3);
  // Quatre images à 96 ms : le cycle reprend à 384 ms, et l'image 0 tient
  // jusqu'à 480 ms comme au premier tour.
  CHECK(ro::render::FrameAtTime(action, 384.0F) == 0);
  CHECK(ro::render::FrameAtTime(action, 450.0F) == 0);
  CHECK(ro::render::FrameAtTime(action, 480.0F) == 1);
}

TEST_CASE("Cadence : jouée une fois, l'action s'arrête sur sa dernière image", "[render][sprite]") {
  ActFrameSpec frame{.layers = {ActLayerSpec{.frame_index = 0}}};
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {frame, frame, frame, frame}}};
  options.speeds = {4.0F};
  const ActAnimation animation = AnimationOf(options);
  const auto& action = animation.actions()[0];

  CHECK(ro::render::FrameAtTimeOnce(action, 0.0F) == 0);
  CHECK(ro::render::FrameAtTimeOnce(action, 350.0F) == 3);
  // Passé le terme, la dernière image tient : un mort reste à terre.
  CHECK(ro::render::FrameAtTimeOnce(action, 384.0F) == 3);
  CHECK(ro::render::FrameAtTimeOnce(action, 10'000.0F) == 3);
  CHECK(ro::render::FrameAtTimeOnce(ro::formats::ActAction{}, 500.0F) == 0);
}

TEST_CASE("Motion : le rang d'animation suit la disposition du .act", "[render][sprite]") {
  using ro::render::AnimationFor;
  using ro::render::Motion;
  using ro::render::MotionLoops;
  using ro::render::SpriteLayout;
  // Treize rangs pour un joueur, cinq pour une créature — et une créature
  // qu'on assoit montre son attaque, comme chez le client.
  CHECK(AnimationFor(Motion::kIdle, SpriteLayout::kPlayer) == 0);
  CHECK(AnimationFor(Motion::kWalk, SpriteLayout::kPlayer) == 1);
  CHECK(AnimationFor(Motion::kSit, SpriteLayout::kPlayer) == 2);
  CHECK(AnimationFor(Motion::kPickup, SpriteLayout::kPlayer) == 3);
  CHECK(AnimationFor(Motion::kReady, SpriteLayout::kPlayer) == 4);
  CHECK(AnimationFor(Motion::kAttack, SpriteLayout::kPlayer) == 10);
  CHECK(AnimationFor(Motion::kHurt, SpriteLayout::kPlayer) == 6);
  CHECK(AnimationFor(Motion::kDie, SpriteLayout::kPlayer) == 8);
  CHECK(AnimationFor(Motion::kIdle, SpriteLayout::kCreature) == 0);
  CHECK(AnimationFor(Motion::kWalk, SpriteLayout::kCreature) == 1);
  CHECK(AnimationFor(Motion::kSit, SpriteLayout::kCreature) == 2);
  CHECK(AnimationFor(Motion::kPickup, SpriteLayout::kCreature) == 2);
  // ⚠ LA GARDE D'UNE CRÉATURE EST SON ATTENTE, et non son attaque : ses cinq
  // rangs n'en portent pas, et lui prêter l'attaque la ferait frapper à vide.
  CHECK(AnimationFor(Motion::kReady, SpriteLayout::kCreature) == 0);
  CHECK(AnimationFor(Motion::kAttack, SpriteLayout::kCreature) == 2);
  CHECK(AnimationFor(Motion::kHurt, SpriteLayout::kCreature) == 3);
  CHECK(AnimationFor(Motion::kDie, SpriteLayout::kCreature) == 4);
  // L'attente, la marche et l'assise tournent ; la blessure et la mort se
  // jouent une fois.
  CHECK(MotionLoops(Motion::kIdle));
  CHECK(MotionLoops(Motion::kWalk));
  CHECK(MotionLoops(Motion::kSit));
  // La GARDE tourne aussi : c'est une pose tenue, pas un geste.
  CHECK(MotionLoops(Motion::kReady));
  CHECK_FALSE(MotionLoops(Motion::kPickup));
  CHECK_FALSE(MotionLoops(Motion::kAttack));
  CHECK_FALSE(MotionLoops(Motion::kHurt));
  CHECK_FALSE(MotionLoops(Motion::kDie));
  // 🔴 ET L'INCANTATION NON PLUS : elle tient sa première image, et l'emploi
  // d'une compétence joue son geste une seule fois.
  CHECK_FALSE(MotionLoops(Motion::kCast));
}

TEST_CASE("Cadence : les cas dégénérés rendent la première image", "[render][sprite]") {
  // Une cadence nulle figerait l'animation sur une division par zéro, et un
  // temps négatif produirait un index négatif : mieux vaut la première image.
  ActFrameSpec frame{.layers = {ActLayerSpec{.frame_index = 0}}};
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {frame, frame}}};
  options.speeds = {0.0F};
  const ActAnimation animation = AnimationOf(options);

  CHECK(ro::render::FrameAtTime(animation.actions()[0], 1000.0F) == 0);
  CHECK(ro::render::FrameAtTime(animation.actions()[0], -50.0F) == 0);
  // Une action sans image ne peut rien rendre d'autre que zéro.
  CHECK(ro::render::FrameAtTime(ro::formats::ActAction{}, 500.0F) == 0);
}

TEST_CASE("Attache : la pièce se pose à « ancre du parent moins la sienne »", "[render][sprite]") {
  const ro::formats::ActAnchor body{.x = 30, .y = 40, .attribute = 0};
  const ro::formats::ActAnchor head{.x = 12, .y = 5, .attribute = 0};

  const glm::vec2 offset = AttachOffset(body, head);
  CHECK(NearlyEqual(offset.x, 18.0F));
  CHECK(NearlyEqual(offset.y, 35.0F));
}

TEST_CASE("Attache : des attributs discordants ne s'accrochent pas", "[render][sprite]") {
  // L'attribut identifie le point d'attache. Sans ce test, on accroche des
  // pièces qui n'ont rien à voir, et le résultat est plausible mais faux.
  const ro::formats::ActAnchor body{.x = 30, .y = 40, .attribute = 0};
  const ro::formats::ActAnchor other{.x = 12, .y = 5, .attribute = 3};

  const glm::vec2 offset = AttachOffset(body, other);
  CHECK(NearlyEqual(offset.x, 0.0F));
  CHECK(NearlyEqual(offset.y, 0.0F));
}

TEST_CASE("Attache : sans ancre, le décalage est nul", "[render][sprite]") {
  // Une pièce mal placée vaut mieux qu'un personnage amputé : l'absence
  // d'ancre n'est pas une erreur.
  CHECK(NearlyEqual(AttachOffset(std::nullopt, ro::formats::ActAnchor{}).x, 0.0F));
  CHECK(NearlyEqual(AttachOffset(ro::formats::ActAnchor{}, std::nullopt).y, 0.0F));
}

TEST_CASE("Quads : une action ou une image hors bornes rend une liste vide", "[render][sprite]") {
  const SprSprite sprite = EvenSprite();
  const ActAnimation animation = AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0}));
  CHECK(BuildFrameQuads(sprite, animation, 99, 0).empty());
  CHECK(BuildFrameQuads(sprite, animation, 0, 99).empty());
  CHECK_FALSE(FrameAnchor(animation, 99, 0).has_value());
}

TEST_CASE("Direction : la boussole du serveur n'est pas celle du .act", "[render][sprite_frame]") {
  // Est et ouest coïncident, tout le reste s'oppose. C'est ce qui rend le
  // défaut fourbe : de profil, rien ne cloche.
  CHECK(SpriteDirection(2) == 2);
  CHECK(SpriteDirection(6) == 6);
  // Nord et sud s'échangent : un personnage qui va vers le nord montre son dos.
  CHECK(SpriteDirection(0) == 4);
  CHECK(SpriteDirection(4) == 0);
  // Les diagonales aussi.
  CHECK(SpriteDirection(1) == 3);
  CHECK(SpriteDirection(3) == 1);
  CHECK(SpriteDirection(5) == 7);
  CHECK(SpriteDirection(7) == 5);
  // Une direction hors bornes revient dans le tour.
  CHECK(SpriteDirection(8) == SpriteDirection(0));
  CHECK(SpriteDirection(11) == SpriteDirection(3));
}

TEST_CASE("Direction : de l'image du .act à la boussole, et retour", "[render][sprite_frame]") {
  using ro::render::CompassOfSpriteDirection;
  using ro::render::kDirectionCount;
  for (std::size_t sprite = 0; sprite < kDirectionCount; ++sprite) {
    CHECK(SpriteDirection(CompassOfSpriteDirection(sprite)) == sprite);
  }
  for (std::size_t compass = 0; compass < kDirectionCount; ++compass) {
    CHECK(CompassOfSpriteDirection(SpriteDirection(compass)) == compass);
  }
  // La face tournée vers nous est le sud ; le dos, le nord.
  CHECK(CompassOfSpriteDirection(0) == 4);
  CHECK(CompassOfSpriteDirection(4) == 0);
  // Une direction hors bornes revient dans le tour.
  CHECK(CompassOfSpriteDirection(kDirectionCount + 1) == CompassOfSpriteDirection(1));
}

TEST_CASE("Direction : faire pivoter la caméra fait pivoter les sprites",
          "[render][sprite_frame]") {
  // Un personnage immobile ne montre pas la même face selon d'où on le
  // regarde. Un huitième de tour de caméra vaut une direction.
  CHECK(SpriteDirection(0, 45.0F) == 5);
  CHECK(SpriteDirection(0, 90.0F) == 6);
  CHECK(SpriteDirection(0, -45.0F) == 3);
  // L'angle s'arrondit au huitième le plus proche, comme chez le client.
  CHECK(SpriteDirection(0, 20.0F) == SpriteDirection(0));
  CHECK(SpriteDirection(0, 25.0F) == SpriteDirection(0, 45.0F));
  // Un tour entier, dans un sens comme dans l'autre, ne change rien.
  CHECK(SpriteDirection(3, 360.0F) == SpriteDirection(3));
  CHECK(SpriteDirection(3, -720.0F) == SpriteDirection(3));
}

TEST_CASE("Direction : une visée se découpe par son angle, arrondi ou tronqué selon le rang",
          "[render][sprite_frame]") {
  using ro::render::kAnimationCast;
  using ro::render::kAnimationIdle;
  using ro::render::kAnimationPickup;
  using ro::render::kAnimationSit;
  using ro::render::kAnimationWalk;
  using ro::render::ScreenDirectionRounds;
  using ro::render::SpriteDirectionOfDegrees;
  constexpr float kDegreesPerEighth = 45.0F;
  constexpr float kFullTurn = 360.0F;
  constexpr unsigned kServerEighthsAdded = 4U;

  // Sur un angle de boussole, les deux découpages rejoignent `SpriteDirection`,
  // caméra tournée ou non.
  for (std::size_t compass = 0; compass < ro::render::kDirectionCount; ++compass) {
    const float degrees =
        std::fmod(kDegreesPerEighth * static_cast<float>(compass + kServerEighthsAdded), kFullTurn);
    CHECK(SpriteDirectionOfDegrees(degrees, 0.0F, true) == SpriteDirection(compass));
    CHECK(SpriteDirectionOfDegrees(degrees, 0.0F, false) == SpriteDirection(compass));
    CHECK(SpriteDirectionOfDegrees(degrees, 90.0F, true) == SpriteDirection(compass, 90.0F));
    CHECK(SpriteDirectionOfDegrees(degrees, -45.0F, false) == SpriteDirection(compass, -45.0F));
  }

  // 🔴 UNE CIBLE UNE CASE À L'EST ET DEUX AU NORD : cent cinquante-trois degrés
  // chez le client. Arrondi, c'est le nord-est ; tronqué, le nord. Le même
  // frappeur montre donc l'une pendant son coup et l'autre au repos.
  const float aimed = std::atan2(1.0F, -2.0F) * 180.0F / std::numbers::pi_v<float>;
  CHECK(SpriteDirectionOfDegrees(aimed, 0.0F, true) == 5);
  CHECK(SpriteDirectionOfDegrees(aimed, 0.0F, false) == 4);
  // Un angle négatif infime ne fait pas un tour de trop.
  CHECK(SpriteDirectionOfDegrees(kFullTurn + 1.0e-6F, 0.0F, false) == 0);

  // Qui arrondit : un joueur aux trois premiers rangs et pas au coup ; un
  // monstre jamais ; un PNJ au repos seulement ; un mercenaire comme un joueur,
  // dès le deuxième.
  CHECK(ScreenDirectionRounds(4054, true, kAnimationIdle));
  CHECK(ScreenDirectionRounds(4054, true, kAnimationWalk));
  CHECK(ScreenDirectionRounds(4054, true, kAnimationSit));
  CHECK_FALSE(ScreenDirectionRounds(4054, true, kAnimationPickup));
  CHECK_FALSE(ScreenDirectionRounds(4054, true, kAnimationCast));
  CHECK_FALSE(ScreenDirectionRounds(1002, false, kAnimationIdle));
  CHECK(ScreenDirectionRounds(46, false, kAnimationIdle));
  CHECK_FALSE(ScreenDirectionRounds(46, false, kAnimationWalk));
  CHECK(ScreenDirectionRounds(6020, false, kAnimationWalk));
  CHECK_FALSE(ScreenDirectionRounds(6017, false, kAnimationIdle));
}

TEST_CASE("Image : la marche s'anime sur la DISTANCE, pas sur une horloge", "[render][frame]") {
  // Le client accumule la distance parcourue et en tire l'image :
  // (distance x 0,37 x 4) / delai, modulo le nombre d'images. Rien n'y règle de
  // cadence — c'est la distance qui fait tout, et c'est pour cela qu'un
  // personnage rapide lève les pieds plus souvent.
  using ro::render::kWalkFramesPerWorldUnit;
  using ro::render::WalkFrameAt;

  constexpr float kInterval = 96.0F;  // quatre tics de vingt-quatre
  constexpr std::size_t kImages = 8;

  // Immobile, la première image.
  CHECK(WalkFrameAt(0.0F, kInterval, kImages) == 0);

  // Une image entière tient dans quatre tics : la distance qui la déclenche est
  // quatre divisé par le facteur.
  const float one_frame = 4.0F / kWalkFramesPerWorldUnit;
  CHECK(WalkFrameAt(one_frame * 0.99F, kInterval, kImages) == 0);
  CHECK(WalkFrameAt(one_frame * 1.01F, kInterval, kImages) == 1);
  CHECK(WalkFrameAt(one_frame * 3.5F, kInterval, kImages) == 3);

  // Le cycle reboucle, il ne déborde pas.
  CHECK(WalkFrameAt(one_frame * 8.5F, kInterval, kImages) == 0);
  CHECK(WalkFrameAt(one_frame * 100.0F, kInterval, kImages) < kImages);

  // Une animation sans image, ou un délai absurde, ne calcule rien.
  CHECK(WalkFrameAt(one_frame, kInterval, 0) == 0);
  CHECK(WalkFrameAt(one_frame, 0.0F, kImages) == 0);
}

TEST_CASE("Motion : l'incantation a son rang, le DOUZIÈME", "[render][sprite]") {
  // Le .act d'un joueur porte treize rangs, et le dernier est l'incantation —
  // les outils qui lisent un .act le nomment « Casting spell », et le fichier
  // d'un prêtre y met six images là où l'attente en a trois.
  //
  // ⚠ La table d'états du CLIENT n'y mène pas : elle renvoie l'incantation
  // ordinaire vers l'attente. La lire seule fait conclure qu'il n'y a pas
  // d'animation d'incantation, et c'est faux — le jeu la joue. Le fichier
  // tranche, pas la table.
  CHECK(AnimationFor(Motion::kCast, SpriteLayout::kPlayer) == 12);
  CHECK(ActionIndex(AnimationFor(Motion::kCast, SpriteLayout::kPlayer), 0) == 96);

  // Une créature n'a que CINQ rangs : elle reste à l'attente plutôt que de
  // frapper pendant qu'elle prépare son sort.
  CHECK(AnimationFor(Motion::kCast, SpriteLayout::kCreature) == kAnimationIdle);

  // 🔴 ET ELLE NE BOUCLE PAS. Le client pose le rang et fige aussitôt son
  // image : qui incante TIENT la première des six, pendant toute sa barre.
  // La faire boucler montrait les cinq autres, que le jeu ne montre jamais là.
  CHECK_FALSE(MotionLoops(Motion::kCast));
}
