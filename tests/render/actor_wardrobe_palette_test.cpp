#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/body_sprite_key.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "support/actor_client.h"
#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::ActorWardrobe;
using ro::render::AttachTo;
using ro::render::ComposedActor;
using ro::render::kAnimationDie;
using ro::render::kAnimationIdle;
using ro::render::kAnimationWalk;
using ro::render::kDirectionCount;
using ro::render::LoadSpriteCatalog;
using ro::render::PaletteRecipe;
using ro::render::RampAdjust;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildPal;
using ro::test_support::BuildSpr;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Utf8ToCp949;
using ro::vfs::Vfs;

using ro::test_support::AnimationBytes;
using ro::test_support::BodySpriteBytes;
using ro::test_support::kBag;
using ro::test_support::kBrokenHeadgear;
using ro::test_support::kFrameIntervalMs;
using ro::test_support::kFrameSpeed;
using ro::test_support::kFramesPerAction;
using ro::test_support::kGoggles;
using ro::test_support::kHoledHairPalette;
using ro::test_support::kSummoner;
using ro::test_support::kSwordman;
using ro::test_support::kWings;
using ro::test_support::MakeClient;
using ro::test_support::Novice;
using ro::test_support::Poring;
using ro::test_support::SingleActionBytes;
using ro::test_support::SpriteBytes;

// ── LE STYLE : des couleurs de corps que le joueur a composées ──────────────

namespace {

// Un métier qui a une tenue alternative, et le numéro de celle-ci.
constexpr std::uint32_t kRuneKnight = 4060;
constexpr std::uint16_t kRuneKnightOutfit = 4332;

// Une recette qui fait pivoter la première rampe d'un demi-tour : assez pour
// que la palette change à coup sûr, et sans toucher aux autres rangs.
[[nodiscard]] PaletteRecipe HalfTurn() {
  PaletteRecipe recipe;
  recipe.ramps[0] = RampAdjust{.hue = 180};
  return recipe;
}

// La palette du CORPS d'une tenue, ou nulle si elle n'en porte pas.
[[nodiscard]] const ro::formats::Palette* BodyPalette(const ActorOutfit& outfit) {
  return outfit.pieces().empty() ? nullptr : outfit.pieces().front().palette;
}

// La pièce de la TÊTE d'une tenue, ou nulle.
[[nodiscard]] const ActorPiece* HeadPiece(const ActorOutfit& outfit) {
  const auto head =
      std::ranges::find_if(outfit.pieces(), [](const ActorPiece& piece) { return piece.is_head; });
  return head == outfit.pieces().end() ? nullptr : &*head;
}

// La clé d'image du CORPS dans la pose d'attente, le seul que le style repeint.
[[nodiscard]] ro::render::SpriteImageKey BodyImageKey(ActorOutfit& outfit) {
  const ro::formats::SprSprite* body = outfit.pieces().front().sprite;
  const ComposedActor& composed = outfit.Composed(kAnimationIdle, 0, 0.0F);
  const auto found = std::ranges::find(composed.sources, body, &ro::render::SpriteImageKey::sprite);
  REQUIRE(found != composed.sources.end());
  return *found;
}

// LE TROU DE LA COULEUR DE CHEVEUX D'ESSAI, rempli par le sprite : son index
// deux est vert, et non le noir que la palette y laisse.
void CheckHoleFilled(const ro::formats::Palette& palette) {
  const auto channel = [&palette](std::size_t index, std::size_t rank) {
    return static_cast<int>(palette.entries[index][rank]);
  };
  // L'index que la couleur peint : son bleu.
  CHECK(channel(1, 2) == 255);
  CHECK(channel(1, 0) == 0);
  // Celui qu'elle laisse vide : le vert du sprite.
  CHECK(channel(2, 1) == 255);
}

}  // namespace

TEST_CASE("Garde-robe : deux apparences qui ne diffèrent que par leur STYLE ne se partagent pas",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 7;
  REQUIRE(wardrobe.SetStyle(kStyle, HalfTurn()));

  ActorLook plain = Novice();
  ActorLook styled = Novice();
  styled.style = kStyle;

  ActorOutfit* natif = wardrobe.OutfitFor(plain);
  ActorOutfit* teint = wardrobe.OutfitFor(styled);
  REQUIRE(natif != nullptr);
  REQUIRE(teint != nullptr);
  // 🔴 DEUX TENUES, ET C'EST TOUT L'OBJET DU CHAMP : partagées, la palette de
  // l'un repeindrait l'autre.
  CHECK(natif != teint);
  CHECK(BodyPalette(*natif) == nullptr);
  REQUIRE(BodyPalette(*teint) != nullptr);
  // Le corps est le même sprite : seule sa palette change.
  CHECK(natif->pieces().front().sprite == teint->pieces().front().sprite);
}

TEST_CASE("Garde-robe : la palette d'un style est celle que la recette peint",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 3;
  REQUIRE(wardrobe.SetStyle(kStyle, HalfTurn()));
  ActorLook look = Novice();
  // Une palette de vêtement officielle : elle entre dans la FUSION, donc dans
  // les rampes détectées.
  look.body_palette = 7;
  look.style = kStyle;

  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const ro::formats::Palette* painted = BodyPalette(*outfit);
  REQUIRE(painted != nullptr);

  // La même chaîne, refaite à la main : fusion, détection, application.
  const ro::formats::SprSprite& body = *outfit->pieces().front().sprite;
  REQUIRE(body.palette().has_value());
  ActorLook without = look;
  without.style = 0;
  ActorOutfit* native = wardrobe.OutfitFor(without);
  REQUIRE(native != nullptr);
  const ro::formats::Palette* official = BodyPalette(*native);
  REQUIRE(official != nullptr);

  const ro::formats::Palette base = ro::render::MergeServerPalette(*body.palette(), official);
  const auto usage = ro::render::CountIndexUsage(body);
  const auto ramps = ro::render::DetectRamps(base, usage);
  const ro::formats::Palette expected = ro::render::ApplyRecipe(base, ramps, HalfTurn());

  CHECK(painted->entries == expected.entries);
  // Et elle ne vaut pas la palette officielle : la recette a bien mordu.
  CHECK_FALSE(painted->entries == official->entries);
}

TEST_CASE("Garde-robe : une recette ne se remplace jamais sous le même identifiant",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  CHECK_FALSE(wardrobe.SetStyle(0, HalfTurn()));
  CHECK(wardrobe.SetStyle(1, HalfTurn()));
  // 🔴 REFUSÉ : une tenue déjà composée retient l'ADRESSE de sa palette, et la
  // réécrire sous le même pointeur laisserait des textures d'hier.
  CHECK_FALSE(wardrobe.SetStyle(1, PaletteRecipe{}));
}

TEST_CASE("Garde-robe : une recette ne prend pas l'identifiant d'une palette toute faite",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 7;
  REQUIRE(wardrobe.SetStylePalette(kStyle, ro::formats::Palette{}));
  // 🔴 REFUSÉ : les palettes toutes faites passent AVANT les recettes, si bien
  // qu'un acteur posé sous cet identifiant porterait celle de l'éditeur.
  CHECK_FALSE(wardrobe.SetStyle(kStyle, HalfTurn()));

  // Oubliée, la palette libère l'identifiant.
  wardrobe.ForgetStyle(kStyle);
  CHECK(wardrobe.SetStyle(kStyle, HalfTurn()));

  // Et ceux qu'elle distribue ne sont jamais nuls ni deux fois les mêmes.
  const std::uint32_t first = wardrobe.NewStyleId();
  const std::uint32_t second = wardrobe.NewStyleId();
  CHECK(first != 0);
  CHECK(second != first);
}

TEST_CASE("Garde-robe : un style oublié rend l'acteur à son apparence native",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 5;
  REQUIRE(wardrobe.SetStyle(kStyle, HalfTurn()));
  ActorLook look = Novice();
  look.style = kStyle;
  REQUIRE(BodyPalette(*wardrobe.OutfitFor(look)) != nullptr);

  wardrobe.ForgetStyle(kStyle);
  // La tenue est refaite, et le corps retrouve la palette de son .spr.
  CHECK(BodyPalette(*wardrobe.OutfitFor(look)) == nullptr);
  // L'identifiant est libre : c'est ce qui permet de reposer une recette.
  CHECK(wardrobe.SetStyle(kStyle, HalfTurn()));
}

TEST_CASE("Garde-robe : une palette de style reposée ne se confond pas avec l'ancienne",
          "[render][wardrobe][palette]") {
  // 🔴 L'ADRESSE NE SUFFIT PAS : la palette d'un style oublié est détruite, et
  // la suivante peut naître au même endroit. Le cache de textures, qui s'indexe
  // sur la clé d'image, servirait alors les couleurs de la précédente. Le
  // numéro de série, lui, ne revient jamais.
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 5;
  ActorLook look = Novice();
  look.style = kStyle;
  // La clé d'image du CORPS, le seul que le style repeint.
  const auto body_key = [&wardrobe, &look]() {
    ActorOutfit* outfit = wardrobe.OutfitFor(look);
    REQUIRE(outfit != nullptr);
    const ro::formats::SprSprite* body = outfit->pieces().front().sprite;
    const ComposedActor& composed = outfit->Composed(kAnimationIdle, 0, 0.0F);
    const auto found =
        std::ranges::find(composed.sources, body, &ro::render::SpriteImageKey::sprite);
    REQUIRE(found != composed.sources.end());
    return *found;
  };

  REQUIRE(wardrobe.SetStyle(kStyle, HalfTurn()));
  const ro::render::SpriteImageKey before = body_key();
  CHECK(before.palette_serial != 0);

  wardrobe.ForgetStyle(kStyle);
  REQUIRE(wardrobe.SetStyle(kStyle, PaletteRecipe{}));
  const ro::render::SpriteImageKey after = body_key();
  CHECK(after.palette_serial != 0);
  CHECK_FALSE(after == before);
}

TEST_CASE("Garde-robe : un style inconnu ne fait pas disparaître le personnage",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook look = Novice();
  // Le lot du serveur peut décrire un joueur avant que sa recette n'arrive.
  look.style = 99;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  CHECK_FALSE(outfit->pieces().empty());
  CHECK(BodyPalette(*outfit) == nullptr);
}

// ── LES VIGNETTES D'UN CHOIX ────────────────────────────────────────────────

TEST_CASE("Garde-robe : la vignette d'une coiffure est une TÊTE, et rien d'autre",
          "[render][wardrobe][palette]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  // ⚠ UNE CASE DE GRILLE FAIT TRENTE PIXELS : un personnage entier y réduit sa
  // tête à cinq, et l'on choisirait une coupe qu'on ne voit pas.
  ActorOutfit* head = wardrobe.HeadOnly(Novice());
  REQUIRE(head != nullptr);
  REQUIRE(head->pieces().size() == 1);
  CHECK(head->pieces()[0].part == ActorPart::kHead);
  CHECK(head->pieces()[0].is_head);

  // ⚠ CE QUI N'EST PAS LA TÊTE NE LA CHANGE PAS : sans cette réduction, deux
  // joueurs qui essaient la même coupe avec des bottes différentes
  // composeraient chacun la leur, et la grille paierait quatre-vingts sprites
  // par tenue croisée.
  ActorLook dressed = Novice();
  dressed.headgears = {0, 0, kGoggles};
  dressed.body_palette = 7;
  CHECK(wardrobe.HeadOnly(dressed) == head);

  // Une coupe sans sprite laisse une case vide, et c'est la bonne réponse :
  // tous les numéros n'existent pas.
  ActorLook absent = Novice();
  absent.hair = 7;
  CHECK(wardrobe.HeadOnly(absent) == nullptr);
}

TEST_CASE("Garde-robe : la vignette d'une teinte montre la palette FUSIONNÉE",
          "[render][wardrobe][palette]") {
  // 🔴 LE DÉFAUT QUE CET ESSAI EXISTE POUR EMPÊCHER. Une palette officielle ne
  // définit qu'une PARTIE des index et laisse le reste en noir ; les corps de
  // troisième et quatrième classe emploient massivement ces autres-là. Montrer
  // le fichier brut donnerait des vignettes à moitié noires — c'est-à-dire
  // l'inverse de ce que le joueur obtiendra s'il choisit cette teinte.
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  // `body_8.pal` ne peint QUE l'index un, en bleu, et laisse tout le reste
  // noir.
  constexpr std::int16_t kSparsePalette = 8;
  ActorOutfit* swatch = wardrobe.BodySwatch(Novice(), kSparsePalette);
  REQUIRE(swatch != nullptr);
  REQUIRE(swatch->pieces().size() == 1);
  CHECK(swatch->pieces()[0].part == ActorPart::kBody);
  const ro::formats::Palette* painted = swatch->pieces()[0].palette;
  REQUIRE(painted != nullptr);

  const auto channel = [painted](std::size_t index, std::size_t rank) {
    return static_cast<int>(painted->entries[index][rank]);
  };
  // L'index que la teinte peint : le bleu du fichier.
  CHECK(channel(1, 2) == 255);
  CHECK(channel(1, 0) == 0);
  // 🔴 ET CELUI QU'ELLE LAISSE NOIR : rapatrié du sprite, donc ROUGE. Sans la
  // fusion il serait noir, et la vignette montrerait un corps sans corps.
  CHECK(channel(2, 0) > 0);
  CHECK(channel(2, 2) == 0);
}

TEST_CASE("Garde-robe : une teinte absente ne se montre pas nue", "[render][wardrobe][palette]") {
  // 🔴 RENDRE LE SPRITE SANS SA TEINTE afficherait la couleur d'origine sous un
  // numéro qui promet autre chose, et le joueur choisirait une teinte qu'il ne
  // recevra jamais. Une case vide dit la vérité.
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::int16_t kNoSuchPalette = 511;
  CHECK(wardrobe.BodySwatch(Novice(), kNoSuchPalette) == nullptr);

  // Zéro n'est pas une teinte absente : c'est « aucune palette externe », et le
  // corps garde alors celle de son sprite.
  ActorOutfit* native = wardrobe.BodySwatch(Novice(), 0);
  REQUIRE(native != nullptr);
  CHECK(native->pieces()[0].palette != nullptr);
}

TEST_CASE("Garde-robe : un corps porté garde les couleurs que sa palette ne définit pas",
          "[render][wardrobe][palette]") {
  // Un Chevalier sur son Peco, teint d'un numéro qui ne définit pas les index de l'oiseau,
  // sortait noir : le corps porté recevait le fichier brut.
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  // `body_8.pal` ne peint QUE l'index un, en bleu.
  ActorLook look = Novice();
  look.body_palette = 8;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const ro::formats::Palette* painted = outfit->pieces()[0].palette;
  REQUIRE(painted != nullptr);
  CHECK(static_cast<int>(painted->entries[1][2]) == 255);
  // L'index qu'elle laisse noir vient du sprite, donc rouge.
  CHECK(static_cast<int>(painted->entries[2][0]) > 0);
  CHECK(static_cast<int>(painted->entries[2][2]) == 0);
}

TEST_CASE("Garde-robe : une tête garde les couleurs que sa palette de cheveux ne définit pas",
          "[render][wardrobe][palette]") {
  // 🔴 MESURÉ SUR L'INSTALLATION : dix-huit têtes humaines et quatorze têtes
  // Doram emploient des index que certaines couleurs de cheveux laissent
  // vides. Reçue brute, la palette les peignait en noir opaque.
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook look = Novice();
  look.hair_palette = kHoledHairPalette;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const ActorPiece* head = HeadPiece(*outfit);
  REQUIRE(head != nullptr);
  REQUIRE(head->palette != nullptr);
  CheckHoleFilled(*head->palette);

  // ⚠ LA VIGNETTE AUSSI : elle montre la couleur qu'on choisira.
  ActorOutfit* sample = wardrobe.HeadOnly(look);
  REQUIRE(sample != nullptr);
  REQUIRE(sample->pieces().front().palette != nullptr);
  CheckHoleFilled(*sample->pieces().front().palette);
}

TEST_CASE(
    "Garde-robe : un style se partage entre deux apparences qui ne diffèrent que par "
    "l'équipement",
    "[render][wardrobe][palette]") {
  // 🔴 UN JOUEUR STYLÉ QUI CHANGE DE COIFFE NE RECALCULE RIEN ET NE REGRAVE
  // RIEN : la palette peinte ne dépend que du style, du corps et de sa teinte.
  // La ranger sous l'apparence entière refaisait toute la chaîne, et donnait
  // aux images du corps de nouvelles clés de texture.
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 4;
  REQUIRE(wardrobe.SetStyle(kStyle, HalfTurn()));
  ActorLook bare = Novice();
  bare.style = kStyle;
  ActorLook geared = bare;
  geared.headgears = {0, 0, kGoggles};

  ActorOutfit* first = wardrobe.OutfitFor(bare);
  ActorOutfit* second = wardrobe.OutfitFor(geared);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  REQUIRE(first != second);
  REQUIRE(BodyPalette(*first) != nullptr);
  CHECK(BodyPalette(*first) == BodyPalette(*second));
  CHECK(first->pieces().front().palette_serial != 0);
  CHECK(BodyImageKey(*first) == BodyImageKey(*second));

  // ⚠ UNE AUTRE TEINTE DE VÊTEMENT EST UNE AUTRE BASE, donc une autre palette.
  ActorLook dyed = bare;
  dyed.body_palette = 7;
  dyed.body_palette_names_file = true;
  ActorOutfit* third = wardrobe.OutfitFor(dyed);
  REQUIRE(third != nullptr);
  CHECK(BodyPalette(*third) != BodyPalette(*first));
}

TEST_CASE("Garde-robe : un style oublié publie les séries de ses palettes",
          "[render][wardrobe][palette]") {
  // 🔴 LE CACHE DE TEXTURES NE LES OUBLIE PAS DE LUI-MÊME : ses clés portent
  // l'adresse d'une palette détruite, et chaque style changé resterait en
  // mémoire vidéo jusqu'au changement de carte.
  TempDirectory client;
  MakeClient(client, BodySpriteBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  constexpr std::uint32_t kStyle = 6;
  REQUIRE(wardrobe.SetStyle(kStyle, HalfTurn()));
  ActorLook look = Novice();
  look.style = kStyle;
  ActorOutfit* outfit = wardrobe.OutfitFor(look);
  REQUIRE(outfit != nullptr);
  const std::uint32_t drawn = BodyImageKey(*outfit).palette_serial;
  REQUIRE(drawn != 0);
  CHECK(wardrobe.TakeRetiredPaletteSerials().empty());

  wardrobe.ForgetStyle(kStyle);
  CHECK(wardrobe.TakeRetiredPaletteSerials() == std::vector<std::uint32_t>{drawn});
  CHECK(wardrobe.TakeRetiredPaletteSerials().empty());
}

TEST_CASE("Garde-robe : les vignettes rendues se recomposent", "[render][wardrobe][palette]") {
  // ⚠ UNE GRILLE REFERMÉE REND SES TÊTES ET LEURS PALETTES : la suivante
  // recompose les siennes, fusion comprise.
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  ActorLook look = Novice();
  look.hair_palette = kHoledHairPalette;
  REQUIRE(wardrobe.HeadOnly(look) != nullptr);
  REQUIRE(wardrobe.BodySwatch(Novice(), 7) != nullptr);
  CHECK(wardrobe.sample_count() == 2);

  wardrobe.ForgetSamples();
  CHECK(wardrobe.sample_count() == 0);
  ActorOutfit* head = wardrobe.HeadOnly(look);
  REQUIRE(head != nullptr);
  REQUIRE(head->pieces().front().palette != nullptr);
  CheckHoleFilled(*head->pieces().front().palette);
  CHECK_FALSE(head->Composed(kAnimationIdle, 0, 0.0F).quads.empty());
  CHECK(wardrobe.BodySwatch(Novice(), 7) != nullptr);
}

TEST_CASE("Garde-robe : un costume absent donne la clé du corps ordinaire",
          "[render][wardrobe][palette]") {
  // 🔴 LA CLÉ DÉSIGNE LE CORPS QU'ON DESSINE. Une tenue alternative que
  // l'installation n'a pas fait retomber sur le corps ordinaire du métier :
  // une clé prise sur le premier candidat nommerait une variante de couleurs
  // que ce client ne montre pas.
  TempDirectory client;
  MakeClient(client);
  ActorLook dressed;
  dressed.player = true;
  dressed.sex = 1;
  dressed.job = kRuneKnight;
  dressed.body_style = kRuneKnightOutfit;
  std::vector<std::string> paths;
  {
    auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
    REQUIRE(vfs.has_value());
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    paths = ro::render::BodySpritePaths(*catalog, dressed);
  }
  REQUIRE(paths.size() == 2);
  // Seul le corps ordinaire est installé.
  client.WriteFile("costume.grf",
                   BuildGrf({ro::test_support::Entry(paths[1] + ".spr", SpriteBytes()),
                             ro::test_support::Entry(paths[1] + ".act", AnimationBytes(2))}));
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf", "costume.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t key = wardrobe.BodyKeyFor(dressed);
  CHECK(key == ro::render::BodySpriteKey(paths[1]));
  CHECK(key != ro::render::BodySpriteKey(paths[0]));
  // ⚠ ET LA MÊME À CHAQUE DEMANDE : elle se calcule une fois par sprite.
  CHECK(wardrobe.BodyKeyFor(dressed) == key);
  // Sans aucun corps, la sentinelle.
  ActorLook nobody = dressed;
  nobody.job = kSwordman;
  nobody.body_style = 0;
  CHECK(wardrobe.BodyKeyFor(nobody) == 0);
}
