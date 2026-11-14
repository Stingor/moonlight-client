#include "ro/render/companion_cart.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support/sprite_builder.h"

using Catch::Approx;
using ro::render::BornCart;
using ro::render::CarriesCart;
using ro::render::Cart;
using ro::render::CartAction;
using ro::render::CartArtFor;
using ro::render::CartButtonImage;
using ro::render::CartFrameCount;
using ro::render::FalconGround;
using ro::render::FalconMaster;
using ro::render::FalconPoint;
using ro::render::StepCart;

namespace {

constexpr std::uint32_t kMerchant = 5;
constexpr std::uint32_t kSuperNovice = 23;
constexpr std::uint32_t kBabyMerchant = 4028;
constexpr std::uint32_t kBabySuperNovice = 4045;
constexpr std::uint32_t kOptionHide = 0x2;
constexpr float kGroundHeight = -3.0F;
constexpr std::size_t kFramesPerAction = 6;
// La cadence que déclarent toutes les charrettes de l'installation.
constexpr float kInstalledDelay = 4.0F;

const FalconGround kFlatGround = [](float, float) { return kGroundHeight; };
const CartFrameCount kSixFrames = [](std::size_t) { return kFramesPerAction; };

FalconMaster MasterAt(float x, float z) {
  return FalconMaster{FalconPoint{x, kGroundHeight, z}, 0.0F};
}

}  // namespace

TEST_CASE("la charrette se choisit sur le type et le métier montré", "[render][cart]") {
  CHECK(CartArtFor(1, kMerchant, 0)->spr_path == "data\\sprite\\이팩트\\손수레.spr");
  CHECK(CartArtFor(2, kMerchant, 0)->act_path == "data\\sprite\\이팩트\\손수레1.act");
  CHECK(CartArtFor(13, kMerchant, 0)->spr_path == "data\\sprite\\이팩트\\마도카트.spr");

  SECTION("un bébé prend l'image de l'adulte et ses propres mouvements, jusqu'au type cinq") {
    CHECK(CartArtFor(5, kBabyMerchant, 0)->spr_path == "data\\sprite\\이팩트\\손수레4.spr");
    CHECK(CartArtFor(5, kBabyMerchant, 0)->act_path == "data\\sprite\\이팩트\\베이비손수레4.act");
    CHECK(CartArtFor(6, kBabyMerchant, 0)->spr_path == "data\\sprite\\이팩트\\손수레5베이비.spr");
    CHECK(CartArtFor(10, kBabyMerchant, 0)->act_path ==
          "data\\sprite\\이팩트\\선물상자카트_미니.act");
  }
  SECTION("un super novice n'a que la sienne, quel que soit le type") {
    CHECK(CartArtFor(9, kSuperNovice, 0)->spr_path == "data\\sprite\\이팩트\\슈노손수레.spr");
    CHECK(CartArtFor(1, kBabySuperNovice, 0)->act_path ==
          "data\\sprite\\이팩트\\베이비슈노손수레.act");
  }
  SECTION("un costume rend le corps d'un adulte ordinaire") {
    CHECK(CartArtFor(1, kBabyMerchant, ro::render::kOptionWedding)->act_path ==
          "data\\sprite\\이팩트\\손수레.act");
  }
  SECTION("un type nul ou inconnu de la table ne dessine rien") {
    CHECK_FALSE(CartArtFor(0, kMerchant, 0).has_value());
    CHECK_FALSE(CartArtFor(-1, kMerchant, 0).has_value());
    CHECK_FALSE(CartArtFor(ro::render::kMostCartType + 1, kMerchant, 0).has_value());
  }
}

TEST_CASE("un bouton de la liste montre la charrette de l'adulte", "[render][cart]") {
  CHECK(ro::render::CartListArtFor(5)->act_path == "data\\sprite\\이팩트\\손수레4.act");
  CHECK(ro::render::CartListArtFor(11)->spr_path == "data\\sprite\\이팩트\\포링싣은카트.spr");
  CHECK_FALSE(ro::render::CartListArtFor(0).has_value());
}

TEST_CASE("caché, le marchand perd sa charrette", "[render][cart]") {
  CHECK(CarriesCart(1, 0));
  CHECK_FALSE(CarriesCart(0, 0));
  CHECK_FALSE(CarriesCart(1, kOptionHide));
}

TEST_CASE("la charrette naît cinq unités derrière son maître, sur son sol", "[render][cart]") {
  FalconMaster master = MasterAt(10.0F, 20.0F);
  master.heading_degrees = 90.7F;
  const Cart cart = BornCart(master);
  // Le cap tronqué à quatre-vingt-dix degrés : le recul se fait sur l'abscisse.
  CHECK(cart.position.x == Approx(5.0F));
  CHECK(cart.position.z == Approx(20.0F).margin(1e-4));
  CHECK(cart.position.y == Approx(kGroundHeight));
}

TEST_CASE("la charrette est traînée au bout de sa longe, et ses roues suivent le chemin",
          "[render][cart]") {
  Cart cart;
  cart.position = FalconPoint{0.0F, 0.0F, 0.0F};

  SECTION("à portée de longe, elle ne bouge pas et ne tourne pas ses roues") {
    StepCart(cart, MasterAt(4.0F, 0.0F), kFlatGround, kSixFrames, kInstalledDelay, 0.0F);
    CHECK(cart.position.x == Approx(0.0F));
    CHECK(cart.travelled_units == Approx(0.0F));
    CHECK(cart.frame == 0);
    // Elle se tourne pourtant vers lui, et se pose sur le sol.
    CHECK(cart.heading_degrees == Approx(90.0F));
    CHECK(cart.position.y == Approx(kGroundHeight));
  }
  SECTION("au-delà, elle revient à cinq unités, dans l'axe qui la sépare de lui") {
    StepCart(cart, MasterAt(12.0F, 0.0F), kFlatGround, kSixFrames, kInstalledDelay, 0.0F);
    CHECK(cart.position.x == Approx(7.0F));
    CHECK(cart.position.z == Approx(0.0F));
    CHECK(cart.travelled_units == Approx(7.0F));
    // `7 × 0,23 × 4 / 4` = 1,61 : l'image un.
    CHECK(cart.frame == 1);
    // Les pas suivants : dix-neuf unités de plus, `26 × 0,23` = 5,98 → image 5.
    StepCart(cart, MasterAt(31.0F, 0.0F), kFlatGround, kSixFrames, kInstalledDelay, 0.0F);
    CHECK(cart.frame == 5);
    // Et le compte boucle sur les six images : `27 × 0,23` = 6,21 → image 0.
    StepCart(cart, MasterAt(32.0F, 0.0F), kFlatGround, kSixFrames, kInstalledDelay, 0.0F);
    CHECK(cart.frame == 0);
  }
  SECTION("l'action montre le cap du tic précédent") {
    cart.heading_degrees = 180.0F;
    StepCart(cart, MasterAt(12.0F, 0.0F), kFlatGround, kSixFrames, kInstalledDelay, 0.0F);
    CHECK(cart.shown_heading_degrees == Approx(180.0F));
    CHECK(cart.heading_degrees == Approx(90.0F));
    CHECK(CartAction(cart, 0.0F) == ro::render::FalconScreenDirection(180.0F, 0.0F));
  }
}

TEST_CASE("la cadence de la charrette est celle de son action zéro, jamais sous un tic",
          "[render][cart]") {
  ro::test_support::ActBuildOptions options;
  options.actions.push_back(ro::test_support::ActActionSpec{{ro::test_support::ActFrameSpec{}}});
  options.speeds = {0.5F};
  const auto slow = ro::formats::ActAnimation::Parse(ro::test_support::BuildAct(options));
  REQUIRE(slow.has_value());
  CHECK(ro::render::CartFrameDelay(*slow) == Approx(ro::render::kCartMinFrameDelay));
  options.speeds = {kInstalledDelay};
  const auto installed = ro::formats::ActAnimation::Parse(ro::test_support::BuildAct(options));
  REQUIRE(installed.has_value());
  CHECK(ro::render::CartFrameDelay(*installed) == Approx(kInstalledDelay));
}

namespace {

using ro::test_support::SprIndexedFrameSpec;

// Les deux couleurs du sprite de charrette synthétique : la roue, puis la caisse.
constexpr std::uint8_t kWheelIndex = 1;
constexpr std::uint8_t kBodyIndex = 2;
// Les dimensions d'une roue et d'une caisse de l'installation.
constexpr std::int32_t kWheelWidth = 6;
constexpr std::int32_t kWheelHeight = 22;

struct ExtentX {
  std::int32_t first = -1;
  std::int32_t last = -1;
};

// Un sprite de deux images pleines : la roue, puis une caisse de la taille
// donnée.
ro::formats::SprSprite CartSprite(std::int32_t body_width, std::int32_t body_height) {
  ro::test_support::SprBuildOptions options;
  options.indexed.push_back(
      SprIndexedFrameSpec{kWheelWidth, kWheelHeight,
                          std::vector<std::uint8_t>(
                              static_cast<std::size_t>(kWheelWidth * kWheelHeight), kWheelIndex)});
  options.indexed.push_back(SprIndexedFrameSpec{
      body_width, body_height,
      std::vector<std::uint8_t>(static_cast<std::size_t>(body_width * body_height), kBodyIndex)});
  options.palette = {{0, 0, 0, 0}, {200, 10, 10, 0}, {10, 10, 200, 0}};
  auto sprite = ro::formats::SprSprite::Parse(ro::test_support::BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

ro::test_support::ActLayerSpec WheelLayer(float offset_x, float offset_y) {
  ro::test_support::ActLayerSpec layer;
  layer.offset_x = offset_x;
  layer.offset_y = offset_y;
  layer.frame_index = 0;
  layer.declared_width = kWheelWidth;
  layer.declared_height = kWheelHeight;
  return layer;
}

ro::test_support::ActLayerSpec BodyLayer(float offset_y, std::int32_t width, std::int32_t height) {
  ro::test_support::ActLayerSpec layer;
  layer.offset_y = offset_y;
  layer.frame_index = 1;
  layer.declared_width = width;
  layer.declared_height = height;
  return layer;
}

ro::formats::ActAnimation CartAnimation(std::vector<ro::test_support::ActLayerSpec> layers) {
  ro::test_support::ActFrameSpec frame;
  frame.layers = std::move(layers);
  ro::test_support::ActBuildOptions options;
  options.actions.push_back(ro::test_support::ActActionSpec{{frame}});
  auto animation = ro::formats::ActAnimation::Parse(ro::test_support::BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

// Le rouge d'un pixel du bouton : 200 pour la roue, 10 pour la caisse, 0 hors
// de tout calque.
std::uint8_t RedAt(const ro::formats::Image& image, std::int32_t x, std::int32_t y) {
  return image.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                       static_cast<std::size_t>(x)) *
                      4];
}

bool CoveredAt(const ro::formats::Image& image, std::int32_t x, std::int32_t y) {
  return image.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                       static_cast<std::size_t>(x)) *
                          4 +
                      3] != 0;
}

// Les colonnes que couvre la caisse sur la rangée `y`.
ExtentX BodyExtent(const ro::formats::Image& image, std::int32_t y) {
  ExtentX extent;
  for (std::int32_t x = 0; x < image.width; ++x) {
    if (CoveredAt(image, x, y) && RedAt(image, x, y) == 10) {
      if (extent.first < 0) {
        extent.first = x;
      }
      extent.last = x;
    }
  }
  return extent;
}

}  // namespace

TEST_CASE("le coin d'un calque est son écart moins la moitié arrondie vers le haut",
          "[render][cart]") {
  ro::formats::ActLayer layer;
  layer.offset_x = -17.0F;
  layer.offset_y = -22.0F;
  layer.declared_width = 7;
  layer.declared_height = 41;
  const auto corner = ro::render::CartButtonLayerCorner(layer, 25, 99, 99);
  REQUIRE(corner.has_value());
  CHECK(corner->x == -21);
  CHECK(corner->y == -43);

  SECTION("sans dimension déclarée, la taille est celle de l'image") {
    layer.declared_width.reset();
    layer.declared_height.reset();
    const auto from_image = ro::render::CartButtonLayerCorner(layer, 23, 6, 22);
    REQUIRE(from_image.has_value());
    CHECK(from_image->x == -20);
    CHECK(from_image->y == -33);
  }
  SECTION("avant la 2.0, l'écart reste tel quel") {
    const auto raw = ro::render::CartButtonLayerCorner(layer, 12, 6, 22);
    REQUIRE(raw.has_value());
    CHECK(raw->x == -17);
    CHECK(raw->y == -22);
  }
  SECTION("une échelle nulle n'a pas de coin") {
    layer.scale_x = 0.0F;
    CHECK_FALSE(ro::render::CartButtonLayerCorner(layer, 25, 6, 22).has_value());
  }
}

TEST_CASE("le bouton de la liste colle les roues sous la caisse", "[render][cart]") {
  // La forme des charrettes de l'installation : deux roues étroites à ±17, puis
  // une caisse large, le premier calque étant une ROUE.
  constexpr std::int32_t kBodyWidth = 38;
  constexpr std::int32_t kBodyHeight = 41;
  const ro::formats::SprSprite sprite = CartSprite(kBodyWidth, kBodyHeight);
  const ro::formats::ActAnimation animation =
      CartAnimation({WheelLayer(17.0F, -10.0F), WheelLayer(-17.0F, -10.0F),
                     BodyLayer(-22.0F, kBodyWidth, kBodyHeight)});

  const ro::formats::Image image = CartButtonImage(sprite, animation);
  REQUIRE(image.width == ro::render::kCartButtonSidePixels);

  // La première roue, centrée : son coin en (50 − 3, 50 − 11).
  CHECK(RedAt(image, 47, 58) == 200);
  CHECK(RedAt(image, 52, 58) == 200);
  CHECK_FALSE(CoveredAt(image, 53, 58));
  // La caisse, posée par son coin : de 14 à 51, de 17 à 57.
  const ExtentX body = BodyExtent(image, 30);
  CHECK(body.first == 14);
  CHECK(body.last == 51);
  CHECK_FALSE(CoveredAt(image, 30, 16));
  CHECK(CoveredAt(image, 30, 17));
  // La seconde roue, de 13 à 18 : elle déborde d'un pixel à gauche de la
  // caisse, comme la première à droite. Les deux roues encadrent la caisse.
  CHECK(RedAt(image, 13, 50) == 200);
  CHECK_FALSE(CoveredAt(image, 12, 50));
  const std::int32_t wheels_middle_twice = (13 + 18) + (47 + 52);
  CHECK(body.first + body.last == wheels_middle_twice / 2);
}

TEST_CASE("le bouton de la liste pose la caisse au milieu de sa case", "[render][cart]") {
  // La forme de la charrette de type 1 : une seule roue au centre, sous une
  // caisse de 32 × 35. Le jeu montre sa caisse centrée sur x = 50 et y = 43 du
  // bouton, soit à 50 et 58 de la fenêtre avec le décalage du type un.
  constexpr std::int32_t kBodyWidth = 32;
  constexpr std::int32_t kBodyHeight = 35;
  const ro::formats::SprSprite sprite = CartSprite(kBodyWidth, kBodyHeight);
  const ro::formats::ActAnimation animation =
      CartAnimation({WheelLayer(0.0F, -12.0F), BodyLayer(-18.0F, kBodyWidth, kBodyHeight)});

  const ro::formats::Image image = CartButtonImage(sprite, animation);
  const ExtentX body = BodyExtent(image, 40);
  CHECK(body.first == 34);
  CHECK(body.last == 65);
  constexpr std::int32_t kProbeColumn = 36;
  std::int32_t body_top = -1;
  std::int32_t body_bottom = -1;
  for (std::int32_t y = 0; y < image.height; ++y) {
    if (CoveredAt(image, kProbeColumn, y) && RedAt(image, kProbeColumn, y) == 10) {
      body_top = body_top < 0 ? y : body_top;
      body_bottom = y;
    }
  }
  CHECK(body_top == 26);
  CHECK(body_bottom == 60);
}

TEST_CASE("le bouton de la liste retourne un calque en miroir, sans le teinter", "[render][cart]") {
  // Une image de deux colonnes : roue à gauche, caisse à droite.
  ro::test_support::SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{2, 1, {kWheelIndex, kBodyIndex}});
  options.palette = {{0, 0, 0, 0}, {200, 10, 10, 0}, {10, 10, 200, 0}};
  const auto sprite = ro::formats::SprSprite::Parse(ro::test_support::BuildSpr(options));
  REQUIRE(sprite.has_value());
  ro::test_support::ActLayerSpec layer;
  layer.frame_index = 0;
  layer.mirrored = true;
  layer.tint = {0, 0, 0, 128};
  layer.declared_width = 2;
  layer.declared_height = 1;
  const ro::formats::ActAnimation animation = CartAnimation({layer});

  const ro::formats::Image image = CartButtonImage(*sprite, animation);
  CHECK(RedAt(image, 49, 50) == 10);
  CHECK(RedAt(image, 50, 50) == 200);
  CHECK(image.pixels[(50 * static_cast<std::size_t>(image.width) + 50) * 4 + 3] == 255);
}
