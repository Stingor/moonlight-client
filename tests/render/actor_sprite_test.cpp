#include "ro/render/actor_sprite.h"

#include <cmath>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::DecodePalette;
using ro::formats::SprSprite;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::AttachTo;
using ro::render::ComposeActor;
using ro::render::ComposedActor;
using ro::render::PalettePick;
using ro::render::ResolveImages;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildPal;
using ro::test_support::BuildSpr;
using ro::test_support::SprBgraFrameSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Un sprite d'une seule image 2×2, entièrement à l'index 1.
SprSprite TinySprite() {
  SprIndexedFrameSpec frame{.width = 2, .height = 2, .indices = {1, 1, 1, 1}};
  SprBuildOptions options;
  options.indexed = {frame};
  options.palette = {SpriteColor{}, SpriteColor{.red = 200, .green = 100, .blue = 50}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

// Une animation d'un calque, avec l'ancre donnée, répétée sur huit directions.
ActAnimation AnimationWithAnchor(std::int32_t anchor_x, std::int32_t anchor_y) {
  ActFrameSpec frame{.layers = {ActLayerSpec{.frame_index = 0}}};
  frame.anchors = {ActAnchorSpec{.x = anchor_x, .y = anchor_y}};
  ActBuildOptions options;
  options.actions.assign(ro::render::kDirectionCount, ActActionSpec{.frames = {frame}});
  options.speeds.assign(ro::render::kDirectionCount, 6.0F);
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

// Le centre du quad de rang `index`.
glm::vec2 CenterOf(const ComposedActor& actor, std::size_t index) {
  return (actor.quads[index].corners[0] + actor.quads[index].corners[2]) * 0.5F;
}

}  // namespace

TEST_CASE("Acteur : la tête se pose sur le corps par leurs ancres", "[render][actor]") {
  const SprSprite body_spr = TinySprite();
  const ActAnimation body_act = AnimationWithAnchor(30, -40);
  const SprSprite head_spr = TinySprite();
  const ActAnimation head_act = AnimationWithAnchor(10, -5);

  const ActorPiece body{.sprite = &body_spr, .animation = &body_act};
  const ActorPiece head{.sprite = &head_spr, .animation = &head_act, .is_head = true};

  const ComposedActor actor = ComposeActor(body, std::vector<ActorPiece>{head}, 0, 0);
  REQUIRE(actor.quads.size() == 2);
  REQUIRE(actor.sources.size() == 2);

  // 30 − 10 = 20 en X, −40 − (−5) = −35 en Y.
  const glm::vec2 shift = CenterOf(actor, 1) - CenterOf(actor, 0);
  CHECK(NearlyEqual(shift.x, 20.0F));
  CHECK(NearlyEqual(shift.y, -35.0F));
}

TEST_CASE("Acteur : une coiffe s'accroche à l'ancre de la TÊTE", "[render][actor]") {
  // L'ancrage suit une CHAÎNE : la tête se pose sur le corps, la coiffe sur la
  // tête. Chaque maillon hérite du décalage de la précédente et y ajoute
  // l'écart entre les DEUX ancres en jeu — celle du porteur et la sienne.
  const SprSprite spr = TinySprite();
  const ActAnimation body_act = AnimationWithAnchor(100, 0);
  const ActAnimation head_act = AnimationWithAnchor(60, 0);
  const ActAnimation hat_act = AnimationWithAnchor(50, 0);

  const ActorPiece body{.sprite = &spr, .animation = &body_act};
  const ActorPiece head{.sprite = &spr, .animation = &head_act, .is_head = true};
  ActorPiece hat{.sprite = &spr, .animation = &hat_act};
  hat.part = ro::render::ActorPart::kHeadgearTop;
  hat.attach = AttachTo::kHead;

  const ComposedActor actor = ComposeActor(body, std::vector<ActorPiece>{head, hat}, 0, 0);
  REQUIRE(actor.quads.size() == 3);

  // Corps, tête, coiffe : c'est l'ordre de dessin de face.
  // Tête : 100 − 60 = 40. Chapeau : ce décalage, plus l'écart entre l'ancre de
  // la TÊTE et la sienne, 60 − 50 = 10, soit 50. Le prendre sur l'ancre du
  // CORPS ajouterait une seconde fois l'écart tête-corps et l'enverrait à 90.
  CHECK(NearlyEqual((CenterOf(actor, 1) - CenterOf(actor, 0)).x, 40.0F));
  CHECK(NearlyEqual((CenterOf(actor, 2) - CenterOf(actor, 0)).x, 50.0F));
}

TEST_CASE("Acteur : la tête est désignée, pas devinée", "[render][actor]") {
  // Une cape s'accroche au CORPS, une coiffe à la TÊTE. Si la tête était
  // déduite de la première pièce venue, la cape en tiendrait lieu.
  const SprSprite spr = TinySprite();
  const ActAnimation body_act = AnimationWithAnchor(100, 0);
  const ActAnimation cape_act = AnimationWithAnchor(90, 0);
  const ActAnimation head_act = AnimationWithAnchor(60, 0);
  const ActAnimation hat_act = AnimationWithAnchor(50, 0);

  const ActorPiece body{.sprite = &spr, .animation = &body_act};
  ActorPiece cape{.sprite = &spr, .animation = &cape_act};
  cape.part = ro::render::ActorPart::kGarment;
  const ActorPiece head{.sprite = &spr, .animation = &head_act, .is_head = true};
  ActorPiece hat{.sprite = &spr, .animation = &hat_act};
  hat.part = ro::render::ActorPart::kHeadgearTop;
  hat.attach = AttachTo::kHead;

  const ComposedActor actor = ComposeActor(body, std::vector<ActorPiece>{cape, head, hat}, 0, 0);
  REQUIRE(actor.quads.size() == 4);
  // Ordre de dessin : corps, tête, coiffe, cape.
  CHECK(NearlyEqual((CenterOf(actor, 1) - CenterOf(actor, 0)).x, 40.0F));
  CHECK(NearlyEqual((CenterOf(actor, 2) - CenterOf(actor, 0)).x, 50.0F));
  // La cape reste sur l'ancre du corps : 100 − 90 = 10.
  CHECK(NearlyEqual((CenterOf(actor, 3) - CenterOf(actor, 0)).x, 10.0F));
}

TEST_CASE("Acteur : une pièce REPLIE l'action du corps sur son compte", "[render][actor]") {
  // Toutes les pièces partagent l'action du corps. Une pièce qui en porte moins
  // la replie modulo son propre compte, et ne se fige PAS sur sa pose de repos :
  // celle d'un costume peut être tout autre chose que son animation en pose
  // assise. La lui refuser tout court la ferait disparaître — un personnage
  // décapité dès qu'il marche.
  const SprSprite spr = TinySprite();

  // Le corps porte 24 actions, la pièce seulement 8, chacune avec une ancre
  // distincte : c'est l'ancre qui révèle sur laquelle on est retombé.
  ActBuildOptions body_options;
  ActFrameSpec body_frame{.layers = {ActLayerSpec{.frame_index = 0}}};
  body_frame.anchors = {ActAnchorSpec{.x = 0, .y = 0}};
  body_options.actions.assign(24, ActActionSpec{.frames = {body_frame}});
  body_options.speeds.assign(24, 6.0F);
  auto body_act = ActAnimation::Parse(BuildAct(body_options));
  REQUIRE(body_act.has_value());

  ActBuildOptions piece_options;
  for (std::int32_t index = 0; index < 8; ++index) {
    ActFrameSpec frame{.layers = {ActLayerSpec{.frame_index = 0}}};
    frame.anchors = {ActAnchorSpec{.x = index * 10, .y = 0}};
    piece_options.actions.push_back(ActActionSpec{.frames = {frame}});
    piece_options.speeds.push_back(6.0F);
  }
  auto piece_act = ActAnimation::Parse(BuildAct(piece_options));
  REQUIRE(piece_act.has_value());

  const ActorPiece body{.sprite = &spr, .animation = &*body_act};
  const ActorPiece head{.sprite = &spr, .animation = &*piece_act, .is_head = true};

  // Action 18 du corps : la pièce n'en a que 8, elle joue donc la 18 % 8 = 2,
  // dont l'ancre vaut 20. Le décalage est « ancre du corps moins la sienne ».
  const ComposedActor actor = ComposeActor(body, std::vector<ActorPiece>{head}, 18, 0);
  REQUIRE(actor.quads.size() == 2);
  CHECK(NearlyEqual((CenterOf(actor, 1) - CenterOf(actor, 0)).x, -20.0F));
}

TEST_CASE("Acteur : une image hors du compte d'une pièce retombe à ZÉRO", "[render][actor]") {
  // Et non modulo : la pièce reprendrait sinon son animation depuis un point
  // arbitraire du cycle, et se désynchroniserait du corps.
  const SprSprite spr = TinySprite();

  ActFrameSpec first{.layers = {ActLayerSpec{.frame_index = 0}}};
  first.anchors = {ActAnchorSpec{.x = 7, .y = 0}};
  ActFrameSpec later{.layers = {ActLayerSpec{.frame_index = 0}}};
  later.anchors = {ActAnchorSpec{.x = 99, .y = 0}};

  // Le corps a quatre images par action, la pièce une seule.
  ActBuildOptions body_options;
  body_options.actions.assign(9, ActActionSpec{.frames = {first, later, later, later}});
  body_options.speeds.assign(9, 6.0F);
  auto body_act = ActAnimation::Parse(BuildAct(body_options));
  REQUIRE(body_act.has_value());

  ActBuildOptions piece_options;
  piece_options.actions.assign(9, ActActionSpec{.frames = {first}});
  piece_options.speeds.assign(9, 6.0F);
  auto piece_act = ActAnimation::Parse(BuildAct(piece_options));
  REQUIRE(piece_act.has_value());

  const ActorPiece body{.sprite = &spr, .animation = &*body_act};
  const ActorPiece head{.sprite = &spr, .animation = &*piece_act, .is_head = true};

  // Image 3 de l'action 8 : le corps est à l'ancre 99, la pièce retombe sur sa
  // seule image, d'ancre 7. Le décalage vaut donc 92.
  const ComposedActor actor = ComposeActor(body, std::vector<ActorPiece>{head}, 8, 3);
  REQUIRE(actor.quads.size() == 2);
  CHECK(NearlyEqual((CenterOf(actor, 1) - CenterOf(actor, 0)).x, 92.0F));
}

TEST_CASE("Acteur : une palette externe recolore la pièce", "[render][actor]") {
  // C'est ainsi que RO teinte cheveux et vêtements. La palette du .spr reste
  // la valeur par défaut.
  const SprSprite spr = TinySprite();
  const ActAnimation act = AnimationWithAnchor(0, 0);

  auto tint =
      DecodePalette(BuildPal({SpriteColor{}, SpriteColor{.red = 7, .green = 8, .blue = 9}}));
  REQUIRE(tint.has_value());

  const ActorPiece plain{.sprite = &spr, .animation = &act};
  const ActorPiece dyed{.sprite = &spr, .animation = &act, .palette = &*tint};

  const std::vector<ro::formats::Image> natural = ResolveImages(ComposeActor(plain, {}, 0, 0));
  const std::vector<ro::formats::Image> recolored = ResolveImages(ComposeActor(dyed, {}, 0, 0));
  REQUIRE(natural.size() == 1);
  REQUIRE(recolored.size() == 1);

  CHECK(natural[0].pixels[0] == 200);
  CHECK(recolored[0].pixels[0] == 7);
  CHECK(recolored[0].pixels[1] == 8);
  CHECK(recolored[0].pixels[2] == 9);
}

TEST_CASE("Acteur : l'ordre de DESSIN suit la direction, pas la liste", "[render][actor]") {
  // La liste porte les dépendances d'ancrage ; le dessin, lui, se réordonne
  // quand le personnage se retourne. Confondre les deux ferait décrocher les
  // coiffes à chaque demi-tour.
  // Chaque pièce porte une ancre distincte : son décalage la rend
  // identifiable dans la sortie, sans quoi le test ne prouverait rien.
  const SprSprite spr = TinySprite();
  const ActAnimation body_act = AnimationWithAnchor(0, 0);      // décalage 0
  const ActAnimation head_act = AnimationWithAnchor(-50, 0);    // décalage +50
  const ActAnimation shield_act = AnimationWithAnchor(-90, 0);  // décalage +90

  ActorPiece body{.sprite = &spr, .animation = &body_act};
  ActorPiece head{.sprite = &spr, .animation = &head_act};
  head.part = ro::render::ActorPart::kHead;
  head.is_head = true;
  ActorPiece shield{.sprite = &spr, .animation = &shield_act};
  shield.part = ro::render::ActorPart::kShield;

  const std::vector<ActorPiece> pieces = {head, shield};

  // De face : corps, tête, bouclier.
  const ComposedActor front = ComposeActor(body, pieces, 0, 0);
  REQUIRE(front.quads.size() == 3);
  CHECK(NearlyEqual(CenterOf(front, 0).x, 0.0F));
  CHECK(NearlyEqual(CenterOf(front, 1).x, 50.0F));
  CHECK(NearlyEqual(CenterOf(front, 2).x, 90.0F));

  // De dos : le bouclier passe EN PREMIER, donc derrière tout le reste.
  const ComposedActor back = ComposeActor(body, pieces, 3, 0);
  REQUIRE(back.quads.size() == 3);
  CHECK(NearlyEqual(CenterOf(back, 0).x, 90.0F));
  CHECK(NearlyEqual(CenterOf(back, 1).x, 0.0F));
  CHECK(NearlyEqual(CenterOf(back, 2).x, 50.0F));
}

TEST_CASE("Acteur : chaque quad dit de quelle partie il vient", "[render][actor]") {
  // Le mélange se choisit par CALQUE : la lame d'aura ne rend additive que
  // l'arme. Sans la partie de chaque quad, le renderer ne peut que tout rendre
  // additif ou rien.
  const SprSprite spr = TinySprite();
  const ActAnimation body_act = AnimationWithAnchor(0, 0);
  const ActAnimation head_act = AnimationWithAnchor(-50, 0);
  const ActAnimation shield_act = AnimationWithAnchor(-90, 0);
  ActorPiece body{.sprite = &spr, .animation = &body_act};
  ActorPiece head{.sprite = &spr, .animation = &head_act};
  head.part = ro::render::ActorPart::kHead;
  head.is_head = true;
  ActorPiece shield{.sprite = &spr, .animation = &shield_act};
  shield.part = ro::render::ActorPart::kShield;

  // De dos, le bouclier passe en premier : la partie suit l'ordre de DESSIN.
  const ComposedActor back = ComposeActor(body, std::vector<ActorPiece>{head, shield}, 3, 0);
  REQUIRE(back.parts.size() == back.quads.size());
  CHECK(back.parts == std::vector<ro::render::ActorPart>{ro::render::ActorPart::kShield,
                                                         ro::render::ActorPart::kBody,
                                                         ro::render::ActorPart::kHead});

  // Un quad sans partie connue compte pour le corps.
  ComposedActor bare;
  bare.quads.resize(1);
  CHECK(ro::render::PartOfQuad(bare, 0) == ro::render::ActorPart::kBody);
}

TEST_CASE("Acteur : un calque s'ajoute au décor quand un effet le demande", "[render][actor]") {
  using ro::render::ActorPart;
  using ro::render::LayerDraw;
  using ro::render::PartBit;
  constexpr ActorPart kBody = ActorPart::kBody;
  constexpr ActorPart kHead = ActorPart::kHead;
  constexpr ActorPart kWeapon = ActorPart::kWeapon;
  constexpr ActorPart kShield = ActorPart::kShield;
  ComposedActor composed;
  composed.quads.resize(4);
  composed.parts = {ActorPart::kBody, ActorPart::kHead, ActorPart::kWeapon, ActorPart::kShield};
  std::vector<LayerDraw> draws;

  // Aucun effet : tout recouvre, dans l'ordre des calques.
  ro::render::LayerDrawsOf(composed, ro::render::kAllActorParts, 0, false, draws);
  CHECK(draws ==
        std::vector<LayerDraw>{
            {0, false, kBody}, {1, false, kHead}, {2, false, kWeapon}, {3, false, kShield}});

  // La lame d'aura : l'ARME seule s'ajoute.
  ro::render::LayerDrawsOf(composed, ro::render::kAllActorParts, PartBit(ActorPart::kWeapon), false,
                           draws);
  CHECK(draws ==
        std::vector<LayerDraw>{
            {0, false, kBody}, {1, false, kHead}, {2, true, kWeapon}, {3, false, kShield}});

  // La rage : le corps et la tête s'ajoutent, l'arme et le bouclier recouvrent.
  ro::render::LayerDrawsOf(composed, ro::render::kAllActorParts, ro::render::kBodyAndHeadwearParts,
                           false, draws);
  CHECK(draws == std::vector<LayerDraw>{
                     {0, true, kBody}, {1, true, kHead}, {2, false, kWeapon}, {3, false, kShield}});

  // L'arme doublée : une seconde fois, en addition, juste après elle.
  ro::render::LayerDrawsOf(composed, ro::render::kAllActorParts, 0, true, draws);
  CHECK(draws == std::vector<LayerDraw>{{0, false, kBody},
                                        {1, false, kHead},
                                        {2, false, kWeapon},
                                        {2, true, kWeapon},
                                        {3, false, kShield}});

  // Une copie qui ne reprend que certains calques ne peint pas les autres.
  const auto body_and_head =
      static_cast<ro::render::ActorPartSet>(PartBit(ActorPart::kBody) | PartBit(ActorPart::kHead));
  ro::render::LayerDrawsOf(composed, body_and_head, ro::render::kAllActorParts, false, draws);
  CHECK(draws == std::vector<LayerDraw>{{0, true, kBody}, {1, true, kHead}});
}

TEST_CASE("Acteur : les pièces d'une même partie gardent l'ordre donné", "[render][actor]") {
  // Rien n'empêche l'appelant de fournir deux pièces du même rang ; elles se
  // dessinent alors dans l'ordre de la liste plutôt que dans un ordre indéfini.
  const SprSprite spr = TinySprite();
  const ActAnimation first_act = AnimationWithAnchor(0, 0);
  const ActAnimation second_act = AnimationWithAnchor(40, 0);

  ActorPiece body{.sprite = &spr, .animation = &first_act};
  ActorPiece low{.sprite = &spr, .animation = &first_act};
  low.part = ro::render::ActorPart::kHeadgearLow;
  ActorPiece other_low{.sprite = &spr, .animation = &second_act};
  other_low.part = ro::render::ActorPart::kHeadgearLow;

  const ComposedActor actor = ComposeActor(body, std::vector<ActorPiece>{low, other_low}, 0, 0);
  REQUIRE(actor.quads.size() == 3);
  // La seconde coiffe est décalée de −40 : elle se reconnaît à sa position.
  CHECK(NearlyEqual((CenterOf(actor, 2) - CenterOf(actor, 1)).x, -40.0F));
}

TEST_CASE("Acteur : un corps absent ne compose rien", "[render][actor]") {
  const ComposedActor actor = ComposeActor(ActorPiece{}, {}, 0, 0);
  CHECK(actor.quads.empty());
  CHECK(actor.sources.empty());
}

namespace {

// Une animation d'UNE action, portant le nombre d'images demandé, à la cadence
// donnée en unités RO.
ActAnimation AnimationOfFrames(std::size_t frames, float speed_units) {
  ActActionSpec action;
  for (std::size_t image = 0; image < frames; ++image) {
    action.frames.push_back(ActFrameSpec{.layers = {ActLayerSpec{.frame_index = 0}}});
  }
  ActBuildOptions options;
  options.actions = {action};
  options.speeds = {speed_units};
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

}  // namespace

TEST_CASE("Coiffe animée : un multiple exact ouvre une animation propre", "[render][actor]") {
  // La tête porte trois images d'attente — les trois orientations du regard.
  // Une coiffe qui en porte vingt-quatre en ouvre huit par image de tête, et
  // les parcourt au fil du temps, à SA cadence. Sans cette règle elle reste
  // figée sur la première de son groupe.
  const ActAnimation head = AnimationOfFrames(3, 6.0F);
  const ActAnimation hat = AnimationOfFrames(24, 2.0F);

  // À l'instant zéro, la première du groupe de l'image de tête.
  auto first = ro::render::AlternateFrame(head, hat, 0, 1, 0.0F);
  REQUIRE(first.has_value());
  CHECK(*first == 8);

  // La cadence de la COIFFE est deux unités, soit quarante-huit millisecondes.
  auto second = ro::render::AlternateFrame(head, hat, 0, 1, 48.0F);
  REQUIRE(second.has_value());
  CHECK(*second == 9);

  // Le groupe fait huit images, puis recommence — sans déborder sur le suivant.
  auto wrapped = ro::render::AlternateFrame(head, hat, 0, 1, 48.0F * 8.0F);
  REQUIRE(wrapped.has_value());
  CHECK(*wrapped == 8);
}

TEST_CASE("Coiffe animée : hors multiple exact, la pièce suit l'acteur", "[render][actor]") {
  // La règle du jeu demande un multiple STRICT. Tout le reste — autant
  // d'images, moins, ou un compte qui ne tombe pas juste — suit l'acteur image
  // pour image, comme n'importe quelle pièce.
  const ActAnimation head = AnimationOfFrames(3, 6.0F);
  CHECK_FALSE(ro::render::AlternateFrame(head, AnimationOfFrames(3, 6.0F), 0, 1, 0.0F).has_value());
  CHECK_FALSE(ro::render::AlternateFrame(head, AnimationOfFrames(2, 6.0F), 0, 1, 0.0F).has_value());
  CHECK_FALSE(ro::render::AlternateFrame(head, AnimationOfFrames(7, 6.0F), 0, 1, 0.0F).has_value());
  // Une cadence déclarée NULLE vaut quatre : sans ce repli, une division par
  // zéro remplacerait l'animation.
  auto without_speed = ro::render::AlternateFrame(head, AnimationOfFrames(6, 0.0F), 0, 0, 96.0F);
  REQUIRE(without_speed.has_value());
  CHECK(*without_speed == 1);
}

// ── LA PIPETTE : L'INDEX SOUS UN POINT ──────────────────────────────────────

namespace {

// Un sprite de deux images 4×4. La première range les index 1 à 16 dans
// l'ordre de lecture ; la seconde est transparente sauf son coin haut-gauche.
SprSprite PickableSprite() {
  SprIndexedFrameSpec ordered{.width = 4, .height = 4, .indices = {}};
  for (std::uint8_t pixel = 0; pixel < 16; ++pixel) {
    ordered.indices.push_back(static_cast<std::uint8_t>(pixel + 1));
  }
  SprIndexedFrameSpec mostly_clear{
      .width = 4, .height = 4, .indices = {99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
  SprBuildOptions options;
  options.indexed = {ordered, mostly_clear};
  options.palette = {SpriteColor{}};
  for (int shade = 1; shade < 100; ++shade) {
    options.palette.push_back(SpriteColor{.red = static_cast<std::uint8_t>(shade)});
  }
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

// Un quad de 4×4 posé à l'origine, coins dans l'ordre haut-gauche, haut-droit,
// bas-droit, bas-gauche.
ro::render::SpriteQuad SquareQuad(int image_index, bool mirrored = false) {
  ro::render::SpriteQuad quad;
  quad.image_index = image_index;
  quad.section = 0;
  quad.mirrored = mirrored;
  quad.corners = {glm::vec2{0.0F, 0.0F}, glm::vec2{4.0F, 0.0F}, glm::vec2{4.0F, 4.0F},
                  glm::vec2{0.0F, 4.0F}};
  return quad;
}

// L'index seul que la pipette rend sous ce point, pour les essais qui ne
// regardent pas la partie.
std::optional<std::uint8_t> IndexAt(const ComposedActor& actor, glm::vec2 at) {
  const std::optional<PalettePick> pick = ro::render::PaletteIndexAt(actor, at);
  return pick.has_value() ? std::optional<std::uint8_t>(pick->index) : std::nullopt;
}

// Un voile BGRA de 4×4 : sa rangée du haut est opaque, la deuxième porte un
// alpha de quinze — que le quartet du client ramène à ZÉRO —, et le reste est
// transparent.
SprSprite BgraVeil() {
  constexpr std::uint8_t kOpaque = 255;
  constexpr std::uint8_t kBelowQuartet = 15;
  constexpr std::uint8_t kClear = 0;
  constexpr std::int32_t kSide = 4;
  SprBgraFrameSpec veil{.width = kSide, .height = kSide, .pixels = {}, .alpha = {}};
  for (std::int32_t row = 0; row < kSide; ++row) {
    for (std::int32_t column = 0; column < kSide; ++column) {
      veil.pixels.push_back(SpriteColor{.blue = kOpaque});
      veil.alpha.push_back(row == 0 ? kOpaque : (row == 1 ? kBelowQuartet : kClear));
    }
  }
  SprBuildOptions options;
  options.bgra = {veil};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

}  // namespace

TEST_CASE("Pipette : elle rend l'INDEX du pixel, jamais sa couleur", "[render][actor]") {
  // 🔴 C'EST TOUTE LA RAISON D'ÊTRE DE CETTE FONCTION. Deux pièces du costume
  // peuvent porter la même teinte après réglage ; une comparaison de couleurs
  // les confondrait, l'index les distingue par construction.
  const SprSprite sprite = PickableSprite();
  ComposedActor actor;
  actor.quads = {SquareQuad(0)};
  actor.sources = {ro::render::SpriteImageKey{&sprite, nullptr, 0, 0}};

  // L'image range 1 à 16 dans l'ordre de lecture : le coin haut-gauche porte
  // le 1, la case suivante le 2, et la première case de la ligne d'en dessous
  // le 5.
  CHECK(IndexAt(actor, glm::vec2{0.5F, 0.5F}) == std::uint8_t{1});
  CHECK(IndexAt(actor, glm::vec2{1.5F, 0.5F}) == std::uint8_t{2});
  CHECK(IndexAt(actor, glm::vec2{0.5F, 1.5F}) == std::uint8_t{5});
  CHECK(IndexAt(actor, glm::vec2{3.5F, 3.5F}) == std::uint8_t{16});

  // Hors du quad, personne ne répond.
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{-0.5F, 2.0F}).has_value());
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{4.5F, 2.0F}).has_value());
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{2.0F, 4.5F}).has_value());
}

TEST_CASE("Pipette : le miroir échange les abscisses de texture", "[render][actor]") {
  // ⚠ IL NE RETOURNE PAS LE QUAD : c'est la texture qui se lit à l'envers, et
  // une pipette qui l'ignorerait désignerait la manche opposée.
  const SprSprite sprite = PickableSprite();
  ComposedActor actor;
  actor.quads = {SquareQuad(0, /*mirrored=*/true)};
  actor.sources = {ro::render::SpriteImageKey{&sprite, nullptr, 0, 0}};

  CHECK(IndexAt(actor, glm::vec2{0.5F, 0.5F}) == std::uint8_t{4});
  CHECK(IndexAt(actor, glm::vec2{3.5F, 0.5F}) == std::uint8_t{1});
}

TEST_CASE("Pipette : un pixel transparent laisse répondre le calque du dessous",
          "[render][actor]") {
  // 🔴 SANS CELA, UNE MANCHE TRANSLUCIDE MASQUERAIT LE TORSE. C'est le premier
  // calque OPAQUE qui répond, exactement comme à l'écran.
  const SprSprite sprite = PickableSprite();
  ComposedActor actor;
  // Le dessous porte les index ordonnés, le dessus n'est opaque qu'en haut à
  // gauche.
  actor.quads = {SquareQuad(0), SquareQuad(1)};
  actor.sources = {ro::render::SpriteImageKey{&sprite, nullptr, 0, 0},
                   ro::render::SpriteImageKey{&sprite, nullptr, 0, 1}};

  // Là où le calque du dessus est opaque, c'est LUI qui répond.
  CHECK(IndexAt(actor, glm::vec2{0.5F, 0.5F}) == std::uint8_t{99});
  // Ailleurs, la question passe au calque du dessous.
  CHECK(IndexAt(actor, glm::vec2{1.5F, 0.5F}) == std::uint8_t{2});
  CHECK(IndexAt(actor, glm::vec2{3.5F, 3.5F}) == std::uint8_t{16});
}

TEST_CASE("Pipette : une image BGRA n'a aucun index à désigner", "[render][actor]") {
  // Elle porte ses couleurs par pixel : il n'y a rien à y lire.
  const SprSprite veil = BgraVeil();
  ComposedActor actor;
  ro::render::SpriteQuad bgra = SquareQuad(0);
  bgra.section = 1;
  actor.quads = {bgra};
  actor.sources = {ro::render::SpriteImageKey{&veil, nullptr, 1, 0}};
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{0.5F, 0.5F}).has_value());
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{2.0F, 2.0F}).has_value());
}

TEST_CASE("Pipette : la partie accompagne l'index", "[render][actor]") {
  // 🔴 UN INDEX NE VEUT RIEN DIRE SANS SON CALQUE : le pantin porte le corps ET
  // la tête, et le 99 des cheveux cherché dans les pièces du corps allumait
  // une pièce que le curseur ne désignait pas.
  const SprSprite sprite = PickableSprite();
  ComposedActor actor;
  // Le corps dessous, la tête dessus, opaque en haut à gauche seulement.
  actor.quads = {SquareQuad(0), SquareQuad(1)};
  actor.sources = {ro::render::SpriteImageKey{&sprite, nullptr, 0, 0},
                   ro::render::SpriteImageKey{&sprite, nullptr, 0, 1}};
  actor.parts = {ActorPart::kBody, ActorPart::kHead};

  CHECK(ro::render::PaletteIndexAt(actor, glm::vec2{0.5F, 0.5F}) ==
        PalettePick{.part = ActorPart::kHead, .index = 99});
  CHECK(ro::render::PaletteIndexAt(actor, glm::vec2{1.5F, 0.5F}) ==
        PalettePick{.part = ActorPart::kBody, .index = 2});
}

TEST_CASE("Pipette : un calque BGRA qui peint le point arrête la question", "[render][actor]") {
  // 🔴 IL CACHE LE DESSOUS. Laisser répondre le corps qu'il recouvre désignerait
  // une pièce que le joueur ne voit pas, et l'image BGRA elle-même n'a aucun
  // index à rendre.
  const SprSprite sprite = PickableSprite();
  const SprSprite veil = BgraVeil();
  ComposedActor actor;
  ro::render::SpriteQuad bgra = SquareQuad(0);
  bgra.section = 1;
  actor.quads = {SquareQuad(0), bgra};
  actor.sources = {ro::render::SpriteImageKey{&sprite, nullptr, 0, 0},
                   ro::render::SpriteImageKey{&veil, nullptr, 1, 0}};
  actor.parts = {ActorPart::kBody, ActorPart::kHeadgearTop};

  // La rangée opaque du voile arrête la pipette.
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{0.5F, 0.5F}).has_value());
  CHECK_FALSE(ro::render::PaletteIndexAt(actor, glm::vec2{3.5F, 0.5F}).has_value());
  // ⚠ UN ALPHA QUE LE QUARTET EFFACE NE PEINT PAS : le corps répond, comme il
  // se voit à l'écran.
  CHECK(ro::render::PaletteIndexAt(actor, glm::vec2{0.5F, 1.5F}) ==
        PalettePick{.part = ActorPart::kBody, .index = 5});
  // Là où le voile est transparent, le corps répond aussi.
  CHECK(ro::render::PaletteIndexAt(actor, glm::vec2{3.5F, 3.5F}) ==
        PalettePick{.part = ActorPart::kBody, .index = 16});
}

TEST_CASE("Coiffe animée : seules les coiffes et la cape en portent une", "[render][actor]") {
  // Le client ne fait passer par cette règle que les trois coiffes et la cape.
  CHECK(ro::render::HasOwnAnimation(ro::render::ActorPart::kHeadgearLow));
  CHECK(ro::render::HasOwnAnimation(ro::render::ActorPart::kHeadgearMiddle));
  CHECK(ro::render::HasOwnAnimation(ro::render::ActorPart::kHeadgearTop));
  CHECK(ro::render::HasOwnAnimation(ro::render::ActorPart::kGarment));
  CHECK_FALSE(ro::render::HasOwnAnimation(ro::render::ActorPart::kHead));
  CHECK_FALSE(ro::render::HasOwnAnimation(ro::render::ActorPart::kWeapon));
  CHECK_FALSE(ro::render::HasOwnAnimation(ro::render::ActorPart::kBody));
}

TEST_CASE("Image BGRA : l'alpha se ramène aux QUATRE BITS du client", "[render][actor]") {
  // 🔴 LE CADRE GRIS AUTOUR DES PARTICULES. Le client crée la texture d'une
  // section BGRA en `D3DFMT_A4R4G4B4`, et n'en écrit que le quartet de poids
  // fort. `particle1.spr` porte sur ses 256 texels de bord un noir à alpha
  // QUINZE, 238 fois : chez le client il ne se voit pas, et sans ce
  // quantifieur il dessinerait le rectangle du sprite.
  ro::formats::Image image;
  image.width = 4;
  image.height = 1;
  // Quatre pixels : le bord de `particle1`, le premier cran qui survit, un
  // presque-opaque, et le plein.
  for (const std::uint8_t alpha :
       {std::uint8_t{15}, std::uint8_t{16}, std::uint8_t{240}, std::uint8_t{255}}) {
    image.pixels.insert(image.pixels.end(), {9, 9, 9, alpha});
  }

  const ro::formats::Image quantifie = ro::render::WithClientAlphaQuartet(image);

  REQUIRE(quantifie.pixels.size() == image.pixels.size());
  CHECK(quantifie.pixels[3] == 0);
  CHECK(quantifie.pixels[7] == 17);
  CHECK(quantifie.pixels[11] == 255);
  CHECK(quantifie.pixels[15] == 255);
  // ⚠ ET LA COULEUR NE BOUGE PAS. Les quatre bits par canal du client sont une
  // contrainte de sa carte graphique, pas une règle : les porter coûterait les
  // dégradés sans rien corriger.
  for (std::size_t pixel = 0; pixel < quantifie.pixels.size(); pixel += 4) {
    CHECK(quantifie.pixels[pixel] == 9);
    CHECK(quantifie.pixels[pixel + 1] == 9);
    CHECK(quantifie.pixels[pixel + 2] == 9);
  }
}

namespace {

// Une image de tête comme le jeu les range : un calque vide, puis la tête, à
// cette ordonnée, et une ancre. En 2.4, sans dimensions déclarées.
ActAnimation HeadFrame(float layer_y, std::int32_t anchor_y, std::uint8_t minor = 4,
                       std::int32_t declared_width = 0, std::int32_t declared_height = 0) {
  ActFrameSpec frame{.layers = {ActLayerSpec{.frame_index = -1},
                                ActLayerSpec{.offset_y = layer_y,
                                             .frame_index = 0,
                                             .declared_width = declared_width,
                                             .declared_height = declared_height}}};
  frame.anchors = {ActAnchorSpec{.x = 1, .y = anchor_y}};
  ActBuildOptions options;
  options.minor = minor;
  options.actions = {ActActionSpec{.frames = {frame}}};
  options.speeds = {4.0F};
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

}  // namespace

TEST_CASE("Acteur : la place de la tête, coin de son calque plus son attache", "[render][actor]") {
  const SprSprite head_spr = TinySprite();
  // Un novice debout, puis assis : ses deux premières images, relevées dans
  // l'installation.
  const ActAnimation body_standing = AnimationWithAnchor(1, -56);
  const ActAnimation head_standing = HeadFrame(-69.0F, -56);
  const ActAnimation body_seated = AnimationWithAnchor(1, -37);
  const ActAnimation head_seated = HeadFrame(-48.0F, -35);

  // Le CENTRE du calque, moins la demi-image arrondie au-dessus : l'image fait
  // deux sur deux.
  const std::optional<glm::vec2> standing =
      ro::render::HeadPlace(body_standing, 0, 0, head_spr, head_standing, 0, 0);
  REQUIRE(standing.has_value());
  CHECK(*standing == glm::vec2(-1.0F, -70.0F));
  // Assis, l'attache tire la tête de deux pixels plus haut que son calque : elle
  // descend de dix-neuf en tout.
  const std::optional<glm::vec2> seated =
      ro::render::HeadPlace(body_seated, 0, 0, head_spr, head_seated, 0, 0);
  REQUIRE(seated.has_value());
  CHECK(*seated - *standing == glm::vec2(0.0F, 19.0F));

  // En 2.5, ce sont les dimensions DÉCLARÉES qui comptent.
  const ActAnimation declared = HeadFrame(-69.0F, -56, 5, 4, 6);
  CHECK(ro::render::HeadPlace(body_standing, 0, 0, head_spr, declared, 0, 0) ==
        glm::vec2(-2.0F, -72.0F));
  // Sans calque de tête, pas de place.
  CHECK_FALSE(ro::render::HeadPlace(body_standing, 0, 0, head_spr, body_standing, 0, 0));

  // LA COMPOSITION LA RETIENT, et seulement quand il y a une tête.
  const ActorPiece body{.sprite = &head_spr, .animation = &body_seated};
  const ActorPiece head{.sprite = &head_spr, .animation = &head_seated, .is_head = true};
  CHECK(ComposeActor(body, std::vector<ActorPiece>{head}, 0, 0).head_place == seated);
  CHECK_FALSE(ComposeActor(body, {}, 0, 0).head_place.has_value());
}
