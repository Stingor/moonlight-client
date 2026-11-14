#include "ro/render/skill_area.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::render::ComposeSkillAreaCells;
using ro::render::ComposeSkillAreaTile;
using ro::render::EffectGroundCellPaint;
using ro::render::kDefaultSkillAreaColor;
using ro::render::kLargestSkillAreaGapPercent;
using ro::render::kSkillAreaTileEdgeWidth;
using ro::render::kSkillAreaTileInnerAlpha;
using ro::render::kSkillAreaTilePixels;
using ro::render::SkillAreaColorBytes;
using ro::render::SkillAreaFill;
using ro::render::SkillAreaLook;
using ro::render::SkillAreaPattern;
using ro::render::SkillAreaTexturePath;

namespace {

// La zone de Storm Gust, mesurée sur l'installation du joueur : neuf sur neuf.
constexpr std::uint16_t kStormGustSide = 9;

[[nodiscard]] bool Couvre(const std::vector<EffectGroundCellPaint>& cells, std::int32_t x,
                          std::int32_t y) {
  return std::ranges::any_of(cells, [x, y](const EffectGroundCellPaint& cell) {
    return cell.cell_offset_x == x && cell.cell_offset_y == y;
  });
}

// Le premier octet du texel (x, y) d'une image RGBA.
std::size_t TexelAt(const ro::formats::Image& tile, std::int32_t x, std::int32_t y) {
  constexpr std::size_t kChannels = 4;
  const auto row = static_cast<std::size_t>(y) * static_cast<std::size_t>(tile.width);
  return (row + static_cast<std::size_t>(x)) * kChannels;
}

}  // namespace

TEST_CASE("Zone d'un sort : un côté impair se centre sur la case visée", "[render][skillarea]") {
  const SkillAreaLook look;
  const std::vector<EffectGroundCellPaint> cells =
      ComposeSkillAreaCells(look, kStormGustSide, kStormGustSide);
  REQUIRE(cells.size() == static_cast<std::size_t>(kStormGustSide) * kStormGustSide);

  // Neuf de côté, c'est quatre cases de chaque bord : la case visée est au
  // milieu, et les écarts vont de -4 à +4.
  CHECK(Couvre(cells, 0, 0));
  CHECK(Couvre(cells, -4, -4));
  CHECK(Couvre(cells, 4, 4));
  CHECK_FALSE(Couvre(cells, 5, 0));
  CHECK_FALSE(Couvre(cells, 0, -5));
}

TEST_CASE("Zone d'un sort : un côté PAIR penche, comme chez le client", "[render][skillarea]") {
  // ⚠ DIVISION ENTIÈRE, et c'est une fidélité et non une approximation : le
  // client écrit `cx + i - largeur / 2`. Un côté pair n'a pas de milieu, et
  // arrondir autrement décalerait notre zone d'une case par rapport à la sienne
  // — visible seulement là où les deux se comparent, donc jamais rattrapé.
  const SkillAreaLook look;
  const std::vector<EffectGroundCellPaint> cells = ComposeSkillAreaCells(look, 4, 4);
  REQUIRE(cells.size() == 16);
  CHECK(Couvre(cells, -2, -2));
  CHECK(Couvre(cells, 1, 1));
  CHECK_FALSE(Couvre(cells, 2, 0));
}

TEST_CASE("Zone d'un sort : sans côté déclaré, aucune case", "[render][skillarea]") {
  // Le cas ORDINAIRE : presque aucune compétence ne déclare de zone, et
  // dessiner un carreau au centre serait inventer.
  const SkillAreaLook look;
  CHECK(ComposeSkillAreaCells(look, 0, 0).empty());
  CHECK(ComposeSkillAreaCells(look, 9, 0).empty());
  CHECK(ComposeSkillAreaCells(look, 0, 9).empty());
}

TEST_CASE("Zone d'un sort : le joint ne creuse que le carreau plein", "[render][skillarea]") {
  SkillAreaLook look;
  look.gap_percent = 12.0F;

  // 🔴 CREUSER UN CONTOUR LE RAPETISSE AU LIEU DE L'ESPACER : les deux motifs
  // qui dessinent un cadre ou des équerres occupent déjà toute la case, et leur
  // joint est dans l'image.
  look.pattern = SkillAreaPattern::kCorners;
  CHECK(SkillAreaFill(look) == Catch::Approx(1.0F));
  look.pattern = SkillAreaPattern::kFrame;
  CHECK(SkillAreaFill(look) == Catch::Approx(1.0F));

  look.pattern = SkillAreaPattern::kSolid;
  CHECK(SkillAreaFill(look) == Catch::Approx(0.88F));

  // ⚠ BORNÉ AU DESSIN, et non à la saisie : un fichier de configuration écrit à
  // la main n'est pas passé par le curseur, et un joint de deux cents pour cent
  // retournerait le quad.
  look.gap_percent = 400.0F;
  CHECK(SkillAreaFill(look) == Catch::Approx(1.0F - kLargestSkillAreaGapPercent / 100.0F));
  look.gap_percent = -50.0F;
  CHECK(SkillAreaFill(look) == Catch::Approx(1.0F));
}

TEST_CASE("Zone d'un sort : deux motifs ont une image, le plein n'en a pas",
          "[render][skillarea]") {
  // ⚠ AUCUN ASSET N'EST VERSIONNÉ ICI : les deux images appartiennent à
  // l'installation du joueur. Le carreau plein, lui, n'est pas un fichier mais
  // un aplat — rendre un chemin vide est sa réponse, pas un oubli.
  CHECK_FALSE(SkillAreaTexturePath(SkillAreaPattern::kCorners).empty());
  CHECK_FALSE(SkillAreaTexturePath(SkillAreaPattern::kFrame).empty());
  // 🔴 LE PLEIN EN A UN AUSSI : un lot sans image est
  // SAUTÉ par le rendu, si bien qu'un chemin vide donnerait un motif qui ne
  // peindrait RIEN — le réglage paraîtrait cassé. Son nom ne désigne aucune
  // archive ; il sert de clé au téléversement de l'aplat.
  CHECK_FALSE(SkillAreaTexturePath(SkillAreaPattern::kSolid).empty());
  CHECK(ro::render::SkillAreaIsPainted(SkillAreaTexturePath(SkillAreaPattern::kSolid)));
  // Et les deux vraies textures, elles, se lisent dans l'archive du joueur.
  CHECK_FALSE(ro::render::SkillAreaIsPainted(SkillAreaTexturePath(SkillAreaPattern::kCorners)));
  CHECK_FALSE(ro::render::SkillAreaIsPainted(SkillAreaTexturePath(SkillAreaPattern::kFrame)));
  // Et les deux ne sont pas la même : le marqueur de destination signale une
  // case par ses coins, le cadre l'entoure.
  CHECK(SkillAreaTexturePath(SkillAreaPattern::kCorners) !=
        SkillAreaTexturePath(SkillAreaPattern::kFrame));
}

TEST_CASE("Zone d'un sort : chaque case porte l'image ENTIÈRE", "[render][skillarea]") {
  // Le motif se répète case par case ; l'étirer sur la zone ferait un seul
  // grand carré au lieu de neuf carreaux.
  const SkillAreaLook look;
  const std::vector<EffectGroundCellPaint> cells = ComposeSkillAreaCells(look, 3, 3);
  REQUIRE(cells.size() == 9);
  for (const EffectGroundCellPaint& cell : cells) {
    CHECK(cell.texture_uv[0].x == Catch::Approx(0.0F));
    CHECK(cell.texture_uv[3].x == Catch::Approx(1.0F));
    CHECK(cell.texture_uv[3].y == Catch::Approx(1.0F));
  }
}

TEST_CASE("Zone d'un sort : la couleur se déballe dans le bon ordre", "[render][skillarea]") {
  // 🔴 L'ORDRE EST CE QUI CASSE EN SILENCE. Un entier de couleur range ses
  // composantes alpha-bleu-vert-rouge du poids fort au faible ; les recopier
  // telles quelles échange le rouge et le bleu — invisible sur le blanc du
  // défaut, criant sur tout le reste. L'essai emploie donc une valeur dont les
  // trois canaux DIFFÈRENT.
  const std::array<std::uint8_t, 4> rgba = SkillAreaColorBytes(0x40C08020U);
  CHECK(rgba[0] == 0x20);  // rouge
  CHECK(rgba[1] == 0x80);  // vert
  CHECK(rgba[2] == 0xC0);  // bleu
  CHECK(rgba[3] == 0x40);  // alpha

  // Et le défaut est bien le blanc à moitié transparent.
  const std::array<std::uint8_t, 4> par_defaut = SkillAreaColorBytes(kDefaultSkillAreaColor);
  CHECK(par_defaut[0] == 255);
  CHECK(par_defaut[1] == 255);
  CHECK(par_defaut[2] == 255);
  CHECK(par_defaut[3] == 128);
}

TEST_CASE("Zone d'un sort : le carreau plein est DESSINÉ, pas uni", "[render][skillarea]") {
  // 🔴 UN APLAT UNI NE SE LIT PAS COMME UN CARRELAGE : neuf cases voisines y
  // font une seule tache, et l'on ne voit plus ni la grille ni où le sort
  // s'arrête. C'est l'ALPHA qui dessine — la teinte du quad donne la couleur.
  const ro::formats::Image tile = ComposeSkillAreaTile();
  REQUIRE(tile.width == kSkillAreaTilePixels);
  REQUIRE(tile.height == kSkillAreaTilePixels);
  REQUIRE(tile.pixels.size() ==
          static_cast<std::size_t>(kSkillAreaTilePixels) * kSkillAreaTilePixels * 4);

  const auto alpha_at = [&tile](std::int32_t x, std::int32_t y) {
    return tile.pixels[TexelAt(tile, x, y) + 3];
  };
  const auto white_at = [&tile](std::int32_t x, std::int32_t y) {
    const std::size_t at = TexelAt(tile, x, y);
    return tile.pixels[at] == 255 && tile.pixels[at + 1] == 255 && tile.pixels[at + 2] == 255;
  };

  const std::int32_t middle = kSkillAreaTilePixels / 2;
  // 🔴 LE BORD EXACT N'EST PAS OPAQUE, et c'est le fondu : le texel qui tombe
  // sur la frontière n'est couvert qu'à MOITIÉ par la forme. Exiger 255 là
  // reviendrait à exiger une arête franche — celle qu'on vient justement
  // d'enlever.
  CHECK(alpha_at(middle, 0) > 0);
  CHECK(alpha_at(middle, 0) < 255);

  // Le liseré est le plus opaque, un peu EN DEDANS du bord ; le ventre est plus
  // doux. C'est ce rapport qui rend chaque case à l'œil.
  const std::uint8_t on_the_rim = alpha_at(middle, 2);
  CHECK(on_the_rim > alpha_at(middle, 0));
  CHECK(on_the_rim > kSkillAreaTileInnerAlpha);
  CHECK(alpha_at(middle, middle) == kSkillAreaTileInnerAlpha);

  // 🔴 LE PROFIL N'EST PAS MONOTONE, ET C'EST LE POINT : il MONTE du bord fondu
  // vers le liseré plein, puis REDESCEND vers le ventre. Le pic est le liseré ;
  // un profil qui ne ferait que décroître serait un halo, et la case n'aurait
  // plus de contour.
  std::int32_t at_peak = 0;
  for (std::int32_t from_edge = 0; from_edge < kSkillAreaTilePixels / 2; ++from_edge) {
    if (alpha_at(middle, from_edge) > alpha_at(middle, at_peak)) {
      at_peak = from_edge;
    }
  }
  CHECK(alpha_at(middle, at_peak) > alpha_at(middle, 0));
  CHECK(alpha_at(middle, at_peak) > kSkillAreaTileInnerAlpha);
  // ⚠ ET LE PIC EST CONTRE LE BORD, pas au milieu : c'est ce qui en fait un
  // contour. Le laisser dériver vers le centre donnerait une case qui brille en
  // son milieu — l'inverse de ce qu'on veut lire.
  CHECK(at_peak < static_cast<std::int32_t>(kSkillAreaTileEdgeWidth));

  // ⚠ LES QUATRE COINS SONT ÉVIDÉS, et c'est ce qui sépare les diagonales :
  // sans eux, quatre cases se rejoignent en un point et la grille se referme.
  CHECK(alpha_at(0, 0) == 0);
  CHECK(alpha_at(kSkillAreaTilePixels - 1, 0) == 0);
  CHECK(alpha_at(0, kSkillAreaTilePixels - 1) == 0);
  CHECK(alpha_at(kSkillAreaTilePixels - 1, kSkillAreaTilePixels - 1) == 0);

  // ⚠ ET L'IMAGE EST BLANCHE PARTOUT : y peindre une couleur la multiplierait
  // par celle du réglage, et le nuancier ne dirait plus ce qu'on voit.
  CHECK(white_at(middle, middle));
  CHECK(white_at(middle, 0));
}
