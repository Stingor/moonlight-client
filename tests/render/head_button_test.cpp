// LE BOUTON DE STATUT À LA TÊTE DU PERSONNAGE : son cadre, la tête qui y tombe
// sans jamais en sortir, et l'image qui ne se refait que quand la tête change.

#include "ro/render/head_button.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/temp_directory.h"

using ro::formats::Image;
using ro::render::ActorWardrobe;
using ro::render::ButtonColor;
using ro::render::ComposeHeadButton;
using ro::render::CutCornerBox;
using ro::render::DrawHeadButtonFrame;
using ro::render::HeadButtonFillArea;
using ro::render::HeadButtonImages;
using ro::render::HeadButtonPortraitArea;
using ro::render::HeadPortraitOf;
using ro::render::kHeadButtonDensity;
using ro::render::kHeadButtonHeightPx;
using ro::render::kHeadButtonPressDrop;
using ro::render::kHeadButtonWidthPx;
using ro::render::LoadSpriteCatalog;
using ro::render::StatusHeadArt;
using ro::render::StatusHeadKey;
using ro::test_support::kSummoner;
using ro::test_support::MakeClient;
using ro::test_support::Novice;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

constexpr std::int32_t kChannels = 4;
constexpr std::size_t kAlpha = 3;
constexpr std::uint8_t kOpaque = 255;

struct Pixel {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t alpha = 0;

  [[nodiscard]] bool operator==(const Pixel&) const = default;
};

Pixel At(const Image& image, std::int32_t x, std::int32_t y) {
  const auto at = static_cast<std::size_t>((y * image.width + x) * kChannels);
  return Pixel{.red = image.pixels[at],
               .green = image.pixels[at + 1],
               .blue = image.pixels[at + 2],
               .alpha = image.pixels[at + kAlpha]};
}

bool Shows(const Image& image, std::int32_t x, std::int32_t y, ButtonColor color) {
  return At(image, x, y) ==
         Pixel{.red = color.red, .green = color.green, .blue = color.blue, .alpha = kOpaque};
}

// Une tête opaque, d'une seule couleur : tout pixel de l'image qui diffère du
// cadre seul est un pixel de tête.
Image SolidHead(std::int32_t width, std::int32_t height) {
  Image head;
  head.width = width;
  head.height = height;
  for (std::int32_t pixel = 0; pixel < width * height; ++pixel) {
    head.pixels.insert(head.pixels.end(), {250, 10, 200, kOpaque});
  }
  return head;
}

// La boîte des pixels qui diffèrent entre deux images de même taille.
struct Changed {
  std::int32_t left = kHeadButtonWidthPx;
  std::int32_t top = kHeadButtonHeightPx;
  std::int32_t right = -1;
  std::int32_t bottom = -1;
  std::size_t count = 0;
};

// 🔴 AUCUN PIXEL DE TÊTE HORS DE LA FACE : chaque pixel touché, ET SES HUIT
// VOISINS, sont du fond dans le cadre seul. La tête ne recouvre donc ni ne
// jouxte le liseré, le filet, l'ombre ou le transparent.
Changed CheckInside(const Image& composed, bool pressed) {
  const Image frame = DrawHeadButtonFrame(pressed);
  REQUIRE(composed.width == frame.width);
  REQUIRE(composed.height == frame.height);
  const ButtonColor fill =
      pressed ? ro::render::kHeadButtonPressedFillColor : ro::render::kHeadButtonFillColor;
  Changed changed;
  std::size_t touching = 0;
  for (std::int32_t y = 0; y < frame.height; ++y) {
    for (std::int32_t x = 0; x < frame.width; ++x) {
      if (At(composed, x, y) == At(frame, x, y)) {
        continue;
      }
      ++changed.count;
      changed.left = std::min(changed.left, x);
      changed.right = std::max(changed.right, x);
      changed.top = std::min(changed.top, y);
      changed.bottom = std::max(changed.bottom, y);
      for (std::int32_t dy = -1; dy <= 1; ++dy) {
        for (std::int32_t dx = -1; dx <= 1; ++dx) {
          const std::int32_t nx = x + dx;
          const std::int32_t ny = y + dy;
          const bool on_fill = nx >= 0 && ny >= 0 && nx < frame.width && ny < frame.height &&
                               Shows(frame, nx, ny, fill);
          touching += on_fill ? 0 : 1;
        }
      }
    }
  }
  CHECK(touching == 0);
  return changed;
}

ActorWardrobe MakeWardrobe(Vfs& vfs) {
  auto catalog = LoadSpriteCatalog(vfs);
  REQUIRE(catalog.has_value());
  return ActorWardrobe(vfs, std::move(*catalog));
}

StatusHeadKey NoviceKey() {
  const ro::render::ActorLook novice = Novice();
  return StatusHeadKey{.character_id = 150000,
                       .job = novice.job,
                       .sex = novice.sex,
                       .hair = novice.hair,
                       .hair_palette = novice.hair_palette};
}

}  // namespace

TEST_CASE("Bouton de tête : le cadre est celui des icônes de menu", "[render][head_button]") {
  const Image normal = DrawHeadButtonFrame(/*pressed=*/false);
  const Image pressed = DrawHeadButtonFrame(/*pressed=*/true);
  // 🔴 À DOUBLE DENSITÉ : la place exacte d'une icône du jeu, 32 sur 34.
  CHECK(normal.width == 32 * kHeadButtonDensity);
  CHECK(normal.height == 34 * kHeadButtonDensity);
  CHECK(pressed.width == normal.width);
  CHECK(pressed.height == normal.height);

  // Le coin haut-gauche est coupé ; le liseré commence à la coupe.
  CHECK(At(normal, 0, 0).alpha == 0);
  CHECK(Shows(normal, 4, 0, ro::render::kHeadButtonRimColor));
  CHECK(Shows(normal, 0, 30, ro::render::kHeadButtonRimColor));
  // Le filet sombre en haut à gauche, sous le liseré ; le fond au milieu.
  CHECK(Shows(normal, 30, 2, ro::render::kHeadButtonFiletColor));
  CHECK(Shows(normal, 2, 30, ro::render::kHeadButtonFiletColor));
  CHECK(Shows(normal, 30, 30, ro::render::kHeadButtonFillColor));
  // En bas et à droite, le liseré touche le fond, sans filet.
  CHECK(Shows(normal, 30, 57, ro::render::kHeadButtonFillColor));
  CHECK(Shows(normal, 30, 58, ro::render::kHeadButtonRimColor));
  CHECK(Shows(normal, 58, 30, ro::render::kHeadButtonRimColor));
  // L'ombre : deux pixels d'interface à droite, trois en bas.
  CHECK(Shows(normal, 62, 30, ro::render::kHeadButtonShadowColor));
  CHECK(Shows(normal, 30, 64, ro::render::kHeadButtonShadowColor));
  CHECK(At(normal, 30, 66).alpha == 0);

  // ENFONCÉ, TOUT DESCEND D'UN PIXEL D'INTERFACE, sur un fond plus clair.
  const std::int32_t drop = kHeadButtonPressDrop * kHeadButtonDensity;
  CHECK(At(pressed, 4, 0).alpha == 0);
  CHECK(Shows(pressed, 4, drop, ro::render::kHeadButtonRimColor));
  CHECK(Shows(pressed, 30, 2 + drop, ro::render::kHeadButtonPressedFiletColor));
  CHECK(Shows(pressed, 30, 30 + drop, ro::render::kHeadButtonPressedFillColor));
  CHECK(Shows(pressed, 30, 64 + drop, ro::render::kHeadButtonShadowColor));
}

TEST_CASE("Bouton de tête : la tête remplit la face sans jamais en sortir",
          "[render][head_button]") {
  for (const auto& [width, height] :
       {std::pair{2, 2}, std::pair{27, 31}, std::pair{40, 22}, std::pair{90, 120}}) {
    const HeadButtonImages images = ComposeHeadButton(SolidHead(width, height));
    REQUIRE_FALSE(images.empty());
    for (const bool pressed : {false, true}) {
      const Image& composed = pressed ? images.pressed : images.normal;
      const Changed changed = CheckInside(composed, pressed);
      REQUIRE(changed.count > 0);
      // 🔴 ELLE REMPLIT : sur son axe le plus serré, elle va d'un bord à
      // l'autre de la place qui lui revient, à un pixel près.
      const CutCornerBox area = HeadButtonPortraitArea(pressed);
      const bool fills_width = changed.left <= area.left + 1 && changed.right >= area.right - 1;
      const bool fills_height = changed.top <= area.top + 1 && changed.bottom >= area.bottom - 1;
      CHECK((fills_width || fills_height));
    }
    // L'enfoncé est le normal descendu d'un pixel d'interface.
    const Changed normal = CheckInside(images.normal, false);
    const Changed pressed = CheckInside(images.pressed, true);
    CHECK(pressed.top == normal.top + kHeadButtonPressDrop * kHeadButtonDensity);
    CHECK(pressed.left == normal.left);
  }
}

TEST_CASE("Bouton de tête : la place de la tête laisse un pixel de fond sur tout son tour",
          "[render][head_button]") {
  for (const bool pressed : {false, true}) {
    const CutCornerBox fill = HeadButtonFillArea(pressed);
    const CutCornerBox area = HeadButtonPortraitArea(pressed);
    CHECK(area.left == fill.left + 1);
    CHECK(area.right == fill.right - 1);
    CHECK(area.top == fill.top + 1);
    CHECK(area.bottom == fill.bottom - 1);
    // Les coins de la place sont coupés : son coin carré toucherait le filet.
    CHECK_FALSE(area.Holds(area.left, area.top));
    CHECK(area.Holds((area.left + area.right) / 2, area.top));
  }
}

TEST_CASE("Bouton de tête : une tête invisible ne donne rien", "[render][head_button]") {
  CHECK(ComposeHeadButton(Image{}).empty());
  Image transparent = SolidHead(4, 4);
  for (std::size_t at = kAlpha; at < transparent.pixels.size(); at += kChannels) {
    transparent.pixels[at] = 0;
  }
  CHECK(ComposeHeadButton(transparent).empty());
}

TEST_CASE("Bouton de tête : la tête seule vient de la garde-robe", "[render][head_button]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  ActorWardrobe wardrobe = MakeWardrobe(*vfs);

  // Le sprite de tête de l'essai : deux pixels sur deux, rouges.
  const Image head = HeadPortraitOf(wardrobe, Novice());
  REQUIRE(head.width == 2);
  REQUIRE(head.height == 2);
  CHECK(At(head, 0, 0) == Pixel{.red = 255, .green = 0, .blue = 0, .alpha = kOpaque});
  CHECK(At(head, 1, 1) == Pixel{.red = 255, .green = 0, .blue = 0, .alpha = kOpaque});

  // ⚠ LES COIFFES NE COMPTENT PAS : c'est la tête seule.
  ro::render::ActorLook dressed = Novice();
  dressed.headgears = {0, 0, ro::test_support::kGoggles};
  CHECK(HeadPortraitOf(wardrobe, dressed).pixels == head.pixels);

  // Un Doram a sa propre tête.
  ro::render::ActorLook doram = Novice();
  doram.job = kSummoner;
  doram.hair = 3;
  CHECK_FALSE(HeadPortraitOf(wardrobe, doram).empty());

  // Une coupe sans sprite ne donne rien.
  ro::render::ActorLook absent = Novice();
  absent.hair = 7;
  CHECK(HeadPortraitOf(wardrobe, absent).empty());
}

TEST_CASE("Bouton de tête : l'image se refait quand la tête change, et seulement alors",
          "[render][head_button]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  ActorWardrobe wardrobe = MakeWardrobe(*vfs);
  StatusHeadArt art(wardrobe);

  // 🔴 TANT QU'AUCUN PERSONNAGE N'EST CHARGÉ, rien : le Statut garde son icône.
  CHECK_FALSE(art.Refresh(std::nullopt));
  CHECK(art.images().empty());
  CHECK(art.composed_count() == 0);

  StatusHeadKey key = NoviceKey();
  CHECK(art.Refresh(key));
  CHECK_FALSE(art.images().empty());
  CHECK(art.composed_count() == 1);

  // RIEN NE CHANGE, RIEN NE SE REFAIT : le bouton se dessine à chaque image.
  CHECK_FALSE(art.Refresh(key));
  CHECK_FALSE(art.Refresh(key));
  CHECK(art.composed_count() == 1);

  // Chaque champ de la clé refait l'image : la couleur de cheveux, la coupe,
  // le genre, le métier — un Doram —, et le personnage lui-même.
  std::size_t expected = 1;
  const auto refreshes = [&art, &expected](const StatusHeadKey& changed) {
    CHECK(art.Refresh(changed));
    ++expected;
    CHECK(art.composed_count() == expected);
    CHECK_FALSE(art.Refresh(changed));
    CHECK(art.composed_count() == expected);
  };
  key.hair_palette = 4;
  refreshes(key);
  key.character_id = 150001;
  refreshes(key);
  // UN STYLE VALIDÉ prend un identifiant neuf : la tête se refait, et
  // l'apparence qu'elle demande le porte, comme celle d'un acteur.
  key.style = 7;
  refreshes(key);
  CHECK(ro::render::HeadLookOf(key).style == 7);
  key.style = 0;
  refreshes(key);
  key.job = kSummoner;
  key.hair = 3;
  refreshes(key);
  CHECK_FALSE(art.images().empty());
  key = NoviceKey();
  refreshes(key);

  // UNE COUPE SANS SPRITE vide les images, une fois, sans se retenter.
  key.hair = 7;
  refreshes(key);
  CHECK(art.images().empty());
  // Revenue sur une coupe qui existe, la tête revient.
  key.hair = NoviceKey().hair;
  refreshes(key);
  CHECK_FALSE(art.images().empty());

  // Le genre est un champ de la clé : sans sprite de femme dans cette
  // installation, l'image se vide.
  key.sex = 0;
  refreshes(key);
  CHECK(art.images().empty());
  key.sex = NoviceKey().sex;
  refreshes(key);

  // LE PERSONNAGE QUITTÉ, les images se vident ; rien ne se recompose.
  CHECK(art.Refresh(std::nullopt));
  CHECK(art.images().empty());
  CHECK(art.composed_count() == expected);
  CHECK_FALSE(art.Refresh(std::nullopt));
}
