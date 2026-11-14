#include "world/actor_styles.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/log.h"
#include "ro/formats/palette.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/style.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using moonclient::ActorStyles;
using ro::render::ActorLook;
using ro::render::ActorOutfit;
using ro::render::ActorWardrobe;
using ro::render::kDirectionCount;
using ro::render::LoadSpriteCatalog;
using ro::sim::StyleBook;
using ro::sim::StyleRecipe;
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
using ro::test_support::TempDirectory;
using ro::vfs::Utf8ToCp949;
using ro::vfs::Vfs;

namespace {

// Un sprite ROUGE d'un pixel plein : c'est sa palette qui compte ici, pas sa
// forme.
std::vector<std::uint8_t> RedSpriteBytes() {
  SprBuildOptions options;
  options.indexed = {{2, 2, {1, 1, 1, 1}}};
  options.palette = {{0, 0, 0}, {255, 0, 0}, {200, 0, 0}};
  return BuildSpr(options);
}

// Un groupe de huit actions — une par direction —, une image chacune.
std::vector<std::uint8_t> AnimationBytes() {
  ActBuildOptions options;
  for (std::size_t action = 0; action < kDirectionCount; ++action) {
    ActFrameSpec frame;
    ActLayerSpec layer;
    layer.frame_index = 0;
    frame.layers.push_back(layer);
    frame.anchors.push_back(ActAnchorSpec{});
    ActActionSpec spec;
    spec.frames.push_back(std::move(frame));
    options.actions.push_back(std::move(spec));
    options.speeds.push_back(1.0F);
  }
  return BuildAct(options);
}

GrfFileSpec Entry(std::string_view utf8_name, std::vector<std::uint8_t> content) {
  auto cp949 = Utf8ToCp949(utf8_name);
  REQUIRE(cp949.has_value());
  return GrfFileSpec{*cp949, std::move(content)};
}

// LE NUMÉRO DE COIFFURE N'EST PAS UN NUMÉRO DE FICHIER, et les deux premières
// coupes sont PERMUTÉES : chez un homme, la coiffure 1 se dessine dans
// `2_남` et la coiffure 2 dans `1_남`. C'est exactement ce qui rend cet essai
// utile — un portage qui confondrait les deux passerait sur n'importe quelle
// autre valeur.
constexpr std::uint16_t kHairWornStyle = 1;
constexpr std::string_view kHairWornFile = "2";
constexpr std::uint16_t kHairAskedStyle = 2;
constexpr std::string_view kHairAskedFile = "1";

// La teinte de vêtement d'essai, et la BLEUE qu'elle pose là où le sprite est
// rouge : c'est cet écart qui prouve que le fichier a bien été lu.
constexpr std::int16_t kBluePalette = 7;
constexpr std::uint8_t kPaintedIndex = 1;

constexpr std::uint32_t kPlayer = 2000;
constexpr std::uint32_t kOther = 2001;

// Un réglage quelconque : ce qui compte est qu'il diffère de son opposé.
constexpr std::int16_t kSomeHueTurn = 40;

// `body` remplace le sprite du CORPS : le banc en veut un de la taille d'un
// vrai, les essais se contentent du pixel rouge qui rend leurs assertions
// lisibles.
void MakeClient(const TempDirectory& client, std::vector<std::uint8_t> body = RedSpriteBytes()) {
  client.WriteText("data/yaml_files/class_sprites.yml", "version: 1\nclasses:\n");

  std::vector<GrfFileSpec> files;
  files.push_back(Entry("data\\sprite\\인간족\\몸통\\남\\초보자_남.spr", std::move(body)));
  files.push_back(Entry("data\\sprite\\인간족\\몸통\\남\\초보자_남.act", AnimationBytes()));
  for (const std::string_view number : {kHairWornFile, kHairAskedFile}) {
    const std::string base = "data\\sprite\\인간족\\머리통\\남\\" + std::string(number) + "_남";
    files.push_back(Entry(base + ".spr", RedSpriteBytes()));
    files.push_back(Entry(base + ".act", AnimationBytes()));
  }
  files.push_back(Entry("data\\palette\\몸\\body_7.pal", BuildPal({{0, 0, 0}, {0, 0, 255}})));
  client.WriteFile("sprites.grf", BuildGrf(files));
}

ActorLook Novice() {
  ActorLook look;
  look.player = true;
  look.sex = 1;
  look.hair = kHairWornStyle;
  return look;
}

// Une recette qui n'impose RIEN : tous les champs à leur sentinelle.
StyleRecipe Nothing() { return StyleRecipe{}; }

void Remember(StyleBook& book, std::uint32_t gid, std::uint32_t body_key,
              const StyleRecipe& recipe) {
  book.BeginBatchFor(gid);
  book.Remember(gid, body_key, recipe, /*is_fallback=*/true);
}

}  // namespace

TEST_CASE("Styles : un acteur du monde ne reçoit que la teinte de vêtement et l'identifiant",
          "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
  REQUIRE(body_key != 0);

  StyleBook book;
  StyleRecipe recipe = Nothing();
  recipe.body_palette = kBluePalette;
  recipe.hair_palette = 12;
  recipe.hair_style = static_cast<std::int16_t>(kHairAskedStyle);
  Remember(book, kPlayer, body_key, recipe);

  ActorStyles styles;
  ActorLook look = Novice();
  look.hair_palette = 5;
  styles.Dress(kPlayer, look, wardrobe, book);

  CHECK(look.style != 0);
  CHECK(look.body_palette == kBluePalette);
  // 🔴 LA COIFFURE ET LA COULEUR RESTENT CELLES DE L'ACTEUR : le serveur les a
  // posées lui-même, et celles du lot datent de la dernière validation. Après
  // un passage chez le styliste, elles remettaient l'ancienne coupe.
  CHECK(look.hair_palette == 5);
  CHECK(look.hair == kHairWornStyle);

  // ⚠ UNE TÊTE DE GROUPE OU DE GUILDE PREND LES TROIS NUMÉROS, sans identifiant
  // de style : elle n'a pas de costume, et rien n'est à rendre à la garde-robe.
  ActorLook head = Novice();
  moonclient::ImposeStyleNumbers(kPlayer, head, wardrobe, book);
  CHECK(head.style == 0);
  CHECK(head.body_palette == kBluePalette);
  CHECK(head.hair_palette == 12);
  CHECK(head.hair == kHairAskedStyle);
}

TEST_CASE("Styles : le livre distingue « pas encore reçu » de « plus rien »", "[app][styles]") {
  // 🔴 LES DEUX RENDENT `Find` VIDE. Seul le second doit effacer les couleurs
  // gardées pour la tablée : sans la révision, entrer en jeu avant le lot les
  // effaçait.
  StyleBook book;
  CHECK(book.RevisionOf(kPlayer) == 0);
  Remember(book, kPlayer, 1U, Nothing());
  const std::uint32_t received = book.RevisionOf(kPlayer);
  CHECK(received != 0);
  book.Forget(kPlayer);
  CHECK(book.Find(kPlayer) == nullptr);
  CHECK(book.RevisionOf(kPlayer) != received);
  CHECK(book.RevisionOf(kOther) == 0);
}

TEST_CASE("Styles : « rien d'imposé » laisse le personnage tel qu'il est", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
  StyleBook book;
  // 🔴 LA SENTINELLE N'EST PAS UN CHOIX. Une recette qui ne parle que du
  // costume ne doit pas rendre le joueur chauve de couleur, ni lui reprendre
  // la teinte que le serveur lui a donnée.
  Remember(book, kPlayer, body_key, Nothing());

  ActorStyles styles;
  ActorLook look = Novice();
  look.body_palette = 3;
  look.hair_palette = 5;
  styles.Dress(kPlayer, look, wardrobe, book);

  CHECK(look.body_palette == 3);
  CHECK(look.hair_palette == 5);
  CHECK(look.hair == kHairWornStyle);
}

TEST_CASE("Styles : la teinte de base atteint VRAIMENT la palette composée", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
  StyleBook book;
  StyleRecipe painted = Nothing();
  painted.body_palette = kBluePalette;
  Remember(book, kPlayer, body_key, painted);
  Remember(book, kOther, body_key, Nothing());

  ActorStyles styles;
  ActorLook blue = Novice();
  styles.Dress(kPlayer, blue, wardrobe, book);
  ActorLook native = Novice();
  styles.Dress(kOther, native, wardrobe, book);

  // ⚠ LA COMPOSITION, ET PAS SEULEMENT L'APPARENCE : c'est le chaînon qui
  // manquait — les trois numéros arrivaient jusqu'au registre et s'y
  // arrêtaient, si bien que le joueur était vu dans les couleurs de son sprite
  // pendant que les autres clients le voyaient dans celles de sa recette.
  ActorOutfit* dressed = wardrobe.OutfitFor(blue);
  ActorOutfit* bare = wardrobe.OutfitFor(native);
  REQUIRE(dressed != nullptr);
  REQUIRE(bare != nullptr);
  REQUIRE_FALSE(dressed->pieces().empty());
  REQUIRE_FALSE(bare->pieces().empty());
  const ro::formats::Palette* with = dressed->pieces()[0].palette;
  const ro::formats::Palette* without = bare->pieces()[0].palette;
  REQUIRE(with != nullptr);
  REQUIRE(without != nullptr);
  // ⚠ EN ENTIERS : un canal est un `std::uint8_t`, et Catch2 en imprime le
  // CARACTÈRE. Un échec disait « 'm' == 0 », ce qui ne se relit pas.
  const auto channel = [](const ro::formats::Palette& palette, std::size_t rank) {
    return static_cast<int>(palette.entries[kPaintedIndex][rank]);
  };
  CHECK(channel(*with, 2) == 255);
  CHECK(channel(*with, 0) == 0);
  CHECK(channel(*without, 0) == 255);
  CHECK(channel(*without, 2) == 0);
}

TEST_CASE("Styles : un identifiant tient tant que la recette ne bouge pas", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
  StyleBook book;
  StyleRecipe recipe = Nothing();
  recipe.ramps[0].hue = kSomeHueTurn;
  recipe.hair_style = static_cast<std::int16_t>(kHairAskedStyle);
  Remember(book, kPlayer, body_key, recipe);

  ActorStyles styles;
  ActorLook first = Novice();
  styles.Dress(kPlayer, first, wardrobe, book);
  ActorLook again = Novice();
  styles.Dress(kPlayer, again, wardrobe, book);
  CHECK(first.style != 0);
  CHECK(first.style == again.style);

  // ⚠ UN CHANGEMENT DE COIFFURE SEULE N'EN EST PAS UN : la palette d'un style
  // ne dépend que de ses réglages, et l'acteur porte déjà la coupe que le
  // serveur a posée.
  recipe.hair_style = static_cast<std::int16_t>(kHairWornStyle);
  Remember(book, kPlayer, body_key, recipe);
  ActorLook second = Novice();
  styles.Dress(kPlayer, second, wardrobe, book);
  CHECK(second.style == first.style);

  // 🔴 UN RÉGLAGE CHANGÉ DONNE UN IDENTIFIANT NEUF : une tenue déjà composée
  // retient l'ADRESSE de sa palette, et la réécrire sous le même identifiant
  // ne se verrait qu'à la prochaine éviction.
  recipe.ramps[0].hue = static_cast<std::int16_t>(-kSomeHueTurn);
  Remember(book, kPlayer, body_key, recipe);
  ActorLook third = Novice();
  styles.Dress(kPlayer, third, wardrobe, book);
  CHECK(third.style != 0);
  CHECK(third.style != first.style);
}

TEST_CASE("Styles : les porteurs d'une même recette partagent un identifiant", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
  StyleBook book;
  StyleRecipe shared = Nothing();
  shared.ramps[0].hue = kSomeHueTurn;
  Remember(book, kPlayer, body_key, shared);
  // ⚠ SES TROIS NUMÉROS N'Y ENTRENT PAS : la palette d'un style n'en dépend pas.
  StyleRecipe same_ramps = shared;
  same_ramps.hair_palette = 12;
  Remember(book, kOther, body_key, same_ramps);

  ActorStyles styles;
  ActorLook mine = Novice();
  styles.Dress(kPlayer, mine, wardrobe, book);
  ActorLook theirs = Novice();
  styles.Dress(kOther, theirs, wardrobe, book);
  REQUIRE(mine.style != 0);
  // 🔴 UN SEUL IDENTIFIANT, donc une seule palette et les mêmes textures : deux
  // porteurs d'un code partagé n'en coûtent qu'un.
  CHECK(theirs.style == mine.style);
  CHECK(wardrobe.OutfitFor(mine) == wardrobe.OutfitFor(theirs));

  // ⚠ L'IDENTIFIANT VIT TANT QU'UN PORTEUR LE GARDE.
  book.Forget(kPlayer);
  styles.Sweep(wardrobe, book);
  CHECK_FALSE(wardrobe.SetStyle(mine.style, ro::render::PaletteRecipe{}));
  book.Forget(kOther);
  styles.Sweep(wardrobe, book);
  CHECK(wardrobe.SetStyle(mine.style, ro::render::PaletteRecipe{}));
}

TEST_CASE("Styles : sans recette, l'apparence reste native", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  StyleBook book;
  ActorStyles styles;
  ActorLook look = Novice();
  look.body_palette = 3;
  styles.Dress(kPlayer, look, wardrobe, book);
  CHECK(look.style == 0);
  CHECK(look.body_palette == 3);
  CHECK(look.hair == kHairWornStyle);
}

TEST_CASE("Styles : l'identifiant remplacé survit jusqu'au balayage", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
  StyleBook book;
  StyleRecipe first = Nothing();
  first.ramps[0].hue = kSomeHueTurn;
  Remember(book, kPlayer, body_key, first);

  ActorStyles styles;
  ActorLook before = Novice();
  styles.Dress(kPlayer, before, wardrobe, book);
  REQUIRE(before.style != 0);
  const ActorOutfit* drawn = wardrobe.OutfitFor(before);
  REQUIRE(drawn != nullptr);

  StyleRecipe second = Nothing();
  second.ramps[0].hue = static_cast<std::int16_t>(-kSomeHueTurn);
  Remember(book, kPlayer, body_key, second);
  ActorLook after = Novice();
  styles.Dress(kPlayer, after, wardrobe, book);
  CHECK(after.style != before.style);

  // 🔴 LA SCÈNE DE L'IMAGE D'AVANT DÉSIGNE ENCORE CETTE TENUE, et la désignation à
  // la souris la relit avant la composition suivante. La garde-robe qui refuse
  // l'identifiant prouve qu'elle le tient toujours.
  CHECK_FALSE(wardrobe.SetStyle(before.style, ro::render::PaletteRecipe{}));
  CHECK(wardrobe.OutfitFor(before) == drawn);

  styles.Sweep(wardrobe, book);
  CHECK(wardrobe.SetStyle(before.style, ro::render::PaletteRecipe{}));
  CHECK_FALSE(wardrobe.SetStyle(after.style, ro::render::PaletteRecipe{}));
}

TEST_CASE("Styles : les couleurs retirées rendent leur identifiant au balayage", "[app][styles]") {
  TempDirectory client;
  MakeClient(client);
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());
  auto catalog = LoadSpriteCatalog(*vfs);
  REQUIRE(catalog.has_value());
  ActorWardrobe wardrobe(*vfs, std::move(*catalog));

  StyleBook book;
  Remember(book, kPlayer, wardrobe.BodyKeyFor(Novice()), Nothing());
  ActorStyles styles;
  ActorLook worn = Novice();
  styles.Dress(kPlayer, worn, wardrobe, book);
  REQUIRE(worn.style != 0);

  book.Forget(kPlayer);
  ActorLook bare = Novice();
  styles.Dress(kPlayer, bare, wardrobe, book);
  CHECK(bare.style == 0);
  CHECK_FALSE(wardrobe.SetStyle(worn.style, ro::render::PaletteRecipe{}));

  styles.Sweep(wardrobe, book);
  CHECK(wardrobe.SetStyle(worn.style, ro::render::PaletteRecipe{}));
}

// ── LE BANC : ce que coûte un joueur stylé ──────────────────────────────────

namespace {

// UN CORPS DE LA TAILLE D'UN VRAI. Relevé sur huit corps de l'installation :
// de 102 à 118 images, de 305 000 à 459 000 pixels. Cent dix images de soixante
// sur soixante en font 396 000.
constexpr std::int32_t kBenchFrames = 110;
constexpr std::int32_t kBenchSide = 60;
// Huit pièces de seize nuances : les huit rampes d'un corps ordinaire.
constexpr std::size_t kBenchPieces = 8;
constexpr int kBenchShades = 16;
// Chaque pièce descend du clair au sombre, et la suivante repart du clair :
// c'est ce qui découpe les rampes.
constexpr int kBenchBrightest = 255;
constexpr int kBenchShadeStep = 12;
constexpr std::array<ro::test_support::SpriteColor, kBenchPieces> kBenchPieceColors = {{
    {255, 40, 40},
    {40, 255, 40},
    {40, 40, 255},
    {255, 255, 40},
    {40, 255, 255},
    {255, 40, 255},
    {255, 140, 40},
    {140, 40, 255},
}};
// Ce que le banc fait varier, vingt fois : la coiffe d'un joueur, ou le porteur
// d'un même code.
constexpr int kBenchRounds = 20;
// Des coiffes que la table ne connaît pas : l'apparence change, et rien d'autre
// ne se charge.
constexpr std::uint16_t kFirstUnknownHeadgear = 100;
constexpr std::uint32_t kFirstBearer = 3000;
constexpr std::int16_t kBenchHueTurn = 30;

std::vector<std::uint8_t> BenchBodyBytes() {
  constexpr int kPaintedIndices = static_cast<int>(kBenchPieces) * kBenchShades;
  SprBuildOptions options;
  for (std::int32_t frame = 0; frame < kBenchFrames; ++frame) {
    ro::test_support::SprIndexedFrameSpec spec;
    spec.width = kBenchSide;
    spec.height = kBenchSide;
    spec.indices.reserve(static_cast<std::size_t>(kBenchSide) * kBenchSide);
    for (std::int32_t pixel = 0; pixel < kBenchSide * kBenchSide; ++pixel) {
      // L'index zéro reste le transparent.
      spec.indices.push_back(static_cast<std::uint8_t>(1 + (pixel + frame) % kPaintedIndices));
    }
    options.indexed.push_back(std::move(spec));
  }
  options.palette.push_back({0, 0, 0});
  for (const ro::test_support::SpriteColor& piece : kBenchPieceColors) {
    for (int shade = 0; shade < kBenchShades; ++shade) {
      const int level = kBenchBrightest - shade * kBenchShadeStep;
      const auto dim = [level](std::uint8_t channel) {
        return static_cast<std::uint8_t>(channel * level / kBenchBrightest);
      };
      options.palette.push_back({dim(piece.red), dim(piece.green), dim(piece.blue)});
    }
  }
  return BuildSpr(options);
}

[[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point since) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since)
      .count();
}

}  // namespace

// Un banc, lancé à la main : il mesure, il ne juge pas. Ses chiffres ne valent
// que pour la configuration bâtie (Debug ou Release), qu'il faut citer avec eux.
// Le nombre de palettes de corps distinctes compte les regravures : chacune
// téléverse de nouveau toutes les images du corps.
TEST_CASE("Banc styles : une coiffe après l'autre, vingt porteurs d'un même code",
          "[app][styles][.bench]") {
  TempDirectory client;
  MakeClient(client, BenchBodyBytes());
  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"sprites.grf"}});
  REQUIRE(vfs.has_value());

  {
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    ActorWardrobe wardrobe(*vfs, std::move(*catalog));
    ro::render::PaletteRecipe recipe;
    for (ro::render::RampAdjust& adjust : recipe.ramps) {
      adjust.hue = kBenchHueTurn;
    }
    const std::uint32_t style = wardrobe.NewStyleId();
    REQUIRE(wardrobe.SetStyle(style, recipe));
    std::set<const ro::formats::Palette*> palettes;
    const auto start = std::chrono::steady_clock::now();
    for (int round = 0; round < kBenchRounds; ++round) {
      ActorLook look = Novice();
      look.style = style;
      look.headgears = {0, 0, static_cast<std::uint16_t>(kFirstUnknownHeadgear + round)};
      ActorOutfit* outfit = wardrobe.OutfitFor(look);
      REQUIRE(outfit != nullptr);
      palettes.insert(outfit->pieces().front().palette);
    }
    const double spent_ms = MillisecondsSince(start);
    ro::core::LogInfo(
        "banc styles : un joueur stylé, {} coiffes : {:.2f} ms, {:.3f} ms par coiffe, {} "
        "palette(s) de corps",
        kBenchRounds, spent_ms, spent_ms / kBenchRounds, palettes.size());
    CHECK_FALSE(palettes.contains(nullptr));
  }

  {
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    ActorWardrobe wardrobe(*vfs, std::move(*catalog));
    const std::uint32_t body_key = wardrobe.BodyKeyFor(Novice());
    REQUIRE(body_key != 0);
    StyleRecipe told = Nothing();
    for (ro::sim::StyleAdjust& adjust : told.ramps) {
      adjust.hue = kBenchHueTurn;
    }
    StyleBook book;
    for (int bearer = 0; bearer < kBenchRounds; ++bearer) {
      Remember(book, kFirstBearer + static_cast<std::uint32_t>(bearer), body_key, told);
    }
    ActorStyles styles;
    std::set<const ro::formats::Palette*> palettes;
    std::set<std::uint32_t> styles_given;
    const auto start = std::chrono::steady_clock::now();
    for (int bearer = 0; bearer < kBenchRounds; ++bearer) {
      ActorLook look = Novice();
      styles.Dress(kFirstBearer + static_cast<std::uint32_t>(bearer), look, wardrobe, book);
      ActorOutfit* outfit = wardrobe.OutfitFor(look);
      REQUIRE(outfit != nullptr);
      palettes.insert(outfit->pieces().front().palette);
      styles_given.insert(look.style);
    }
    const double spent_ms = MillisecondsSince(start);
    ro::core::LogInfo(
        "banc styles : {} porteurs d'un même code : {:.2f} ms, {:.3f} ms par porteur, {} "
        "style(s), {} palette(s) de corps",
        kBenchRounds, spent_ms, spent_ms / kBenchRounds, styles_given.size(), palettes.size());
    CHECK_FALSE(styles_given.contains(0U));
    styles.Sweep(wardrobe, StyleBook{});
  }

  {
    // LA CLÉ DU CORPS, que le registre demande à chaque image pour chaque
    // joueur stylé : une minute d'une ville de cinquante joueurs stylés à
    // soixante images par seconde en fait cent quatre-vingt mille.
    constexpr int kKeyRequests = 180'000;
    auto catalog = LoadSpriteCatalog(*vfs);
    REQUIRE(catalog.has_value());
    ActorWardrobe wardrobe(*vfs, std::move(*catalog));
    const ActorLook look = Novice();
    const std::uint32_t expected = wardrobe.BodyKeyFor(look);
    REQUIRE(expected != 0);
    int same = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int request = 0; request < kKeyRequests; ++request) {
      same += wardrobe.BodyKeyFor(look) == expected ? 1 : 0;
    }
    const double spent_ms = MillisecondsSince(start);
    ro::core::LogInfo("banc styles : {} clés de corps : {:.2f} ms, {:.3f} µs par clé", kKeyRequests,
                      spent_ms, spent_ms * 1000.0 / kKeyRequests);
    CHECK(same == kKeyRequests);
  }
}
