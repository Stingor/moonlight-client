// LES PIÈCES LIÉES, LES PIÈCES GARDÉES ET L'INSPIRATION de l'éditeur de style.
//
// ⚠ ÉPROUVÉES SUR CE QUI NE SE VOIT QU'À L'USAGE : deux groupes qui se fondent
// mal laissent une pièce liée à rien, et une inspiration qui teint le métal ou
// ignore un verrou rend un costume de carnaval.

#include "character/style_pieces.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::Harmony;
using moonclient::Inspire;
using moonclient::PieceMarks;

namespace {

using Rgb = std::array<std::uint8_t, 3>;

constexpr Rgb kRed = {200, 40, 40};
constexpr Rgb kBlue = {40, 60, 200};
constexpr Rgb kGrey = {128, 128, 128};
constexpr Rgb kBlack = {12, 10, 10};
constexpr std::uint32_t kSeed = 1234;

}  // namespace

TEST_CASE("Pièces : lier, fondre deux groupes, délier", "[app][style]") {
  PieceMarks marks;
  marks.Link(0, 1);
  CHECK(marks.Reached(0) == std::vector<std::size_t>{0, 1});
  marks.Link(2, 3);
  // ⚠ DEUX GROUPES SE FONDENT en un seul.
  marks.Link(1, 3);
  CHECK(marks.Reached(2) == std::vector<std::size_t>{2, 0, 1, 3});

  marks.Unlink(0);
  CHECK(marks.Reached(0) == std::vector<std::size_t>{0});
  CHECK(marks.Reached(1) == std::vector<std::size_t>{1, 2, 3});
  // 🔴 UN GROUPE RÉDUIT À UNE PIÈCE DISPARAÎT : elle ne doit pas se dire liée.
  marks.Unlink(2);
  marks.Unlink(3);
  CHECK(marks.links()[1] == 0);
  // Lier une pièce à elle-même ne fait rien.
  marks.Link(4, 4);
  CHECK(marks.links()[4] == 0);
}

TEST_CASE("Pièces : l'inspiration laisse les neutres et les gardées", "[app][style]") {
  const std::vector<Rgb> origin = {kRed, kGrey, kBlue, kBlack, kRed};
  PieceMarks marks;
  marks.ToggleLock(4);
  const auto inspired = Inspire(origin, marks, Harmony::kTriadic, kSeed);
  REQUIRE(inspired.size() == origin.size());
  CHECK(inspired[0].has_value());
  CHECK_FALSE(inspired[1].has_value());
  CHECK(inspired[2].has_value());
  CHECK_FALSE(inspired[3].has_value());
  CHECK_FALSE(inspired[4].has_value());
  // ⚠ UN MÊME TIRAGE SE REJOUE : la graine décide de tout.
  CHECK(Inspire(origin, marks, Harmony::kTriadic, kSeed) == inspired);
}

TEST_CASE("Pièces : des pièces liées prennent la même teinte", "[app][style]") {
  const std::vector<Rgb> origin = {kRed, kBlue, kRed};
  PieceMarks marks;
  marks.Link(0, 2);
  const auto inspired = Inspire(origin, marks, Harmony::kComplementary, kSeed);
  REQUIRE(inspired[0].has_value());
  REQUIRE(inspired[2].has_value());
  CHECK(*inspired[0] == *inspired[2]);
  // Et la pièce libre prend l'autre teinte de l'accord.
  REQUIRE(inspired[1].has_value());
  CHECK(*inspired[1] != *inspired[0]);
}
