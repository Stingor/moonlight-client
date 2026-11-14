// La recherche dans une table triée : la plage des lignes d'une clé, et la
// première d'entre elles, sur un std::array, un std::vector ou un std::span.

#include "ro/core/sorted_table.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::RowsWithKey;
using ro::core::RowWithKey;

namespace {

struct Row {
  std::uint16_t id = 0;
  char tag = ' ';
};

// Triée par `id`, avec une clé en tête, une répétée trois fois au milieu et une
// en queue.
constexpr std::array<Row, 6> kRows = {{
    {2, 'a'},
    {5, 'b'},
    {7, 'c'},
    {7, 'd'},
    {7, 'e'},
    {9, 'f'},
}};

}  // namespace

TEST_CASE("RowsWithKey : une clé absente rend une plage vide", "[core][sorted_table]") {
  CHECK(RowsWithKey(kRows, std::uint16_t{1}, &Row::id).empty());
  CHECK(RowsWithKey(kRows, std::uint16_t{6}, &Row::id).empty());
  CHECK(RowsWithKey(kRows, std::uint16_t{10}, &Row::id).empty());
}

TEST_CASE("RowsWithKey : une clé présente une fois", "[core][sorted_table]") {
  const std::span<const Row> found = RowsWithKey(kRows, std::uint16_t{5}, &Row::id);
  REQUIRE(found.size() == 1);
  CHECK(found.data() == &kRows[1]);
}

TEST_CASE("RowsWithKey : les lignes d'une clé répétée, dans l'ordre de la table",
          "[core][sorted_table]") {
  const std::span<const Row> found = RowsWithKey(kRows, std::uint16_t{7}, &Row::id);
  REQUIRE(found.size() == 3);
  CHECK(found.data() == &kRows[2]);
  CHECK(found[0].tag == 'c');
  CHECK(found[1].tag == 'd');
  CHECK(found[2].tag == 'e');
}

TEST_CASE("RowsWithKey : la clé de tête et celle de queue", "[core][sorted_table]") {
  const std::span<const Row> head = RowsWithKey(kRows, std::uint16_t{2}, &Row::id);
  REQUIRE(head.size() == 1);
  CHECK(head.data() == &kRows.front());
  const std::span<const Row> tail = RowsWithKey(kRows, std::uint16_t{9}, &Row::id);
  REQUIRE(tail.size() == 1);
  CHECK(tail.data() == &kRows.back());
}

TEST_CASE("RowWithKey : la première ligne de la clé, ou nullptr", "[core][sorted_table]") {
  CHECK(RowWithKey(kRows, std::uint16_t{1}, &Row::id) == nullptr);
  CHECK(RowWithKey(kRows, std::uint16_t{6}, &Row::id) == nullptr);
  CHECK(RowWithKey(kRows, std::uint16_t{10}, &Row::id) == nullptr);
  CHECK(RowWithKey(kRows, std::uint16_t{2}, &Row::id) == &kRows.front());
  CHECK(RowWithKey(kRows, std::uint16_t{5}, &Row::id) == &kRows[1]);
  CHECK(RowWithKey(kRows, std::uint16_t{7}, &Row::id) == &kRows[2]);
  CHECK(RowWithKey(kRows, std::uint16_t{9}, &Row::id) == &kRows.back());
}

TEST_CASE("Table triée : une table vide ne rend rien", "[core][sorted_table]") {
  const std::vector<Row> rows;
  CHECK(RowsWithKey(rows, std::uint16_t{7}, &Row::id).empty());
  CHECK(RowWithKey(rows, std::uint16_t{7}, &Row::id) == nullptr);
  const std::span<const Row> none;
  CHECK(RowsWithKey(none, std::uint16_t{7}, &Row::id).empty());
  CHECK(RowWithKey(none, std::uint16_t{7}, &Row::id) == nullptr);
}

TEST_CASE("Table triée : un std::vector et un std::span se cherchent comme un std::array",
          "[core][sorted_table]") {
  const std::vector<Row> rows(kRows.begin(), kRows.end());
  const std::span<const Row> in_vector = RowsWithKey(rows, std::uint16_t{7}, &Row::id);
  REQUIRE(in_vector.size() == 3);
  CHECK(in_vector.data() == &rows[2]);
  CHECK(RowWithKey(rows, std::uint16_t{9}, &Row::id) == &rows.back());

  const std::span<const Row> view = kRows;
  const std::span<const Row> in_span = RowsWithKey(view, std::uint16_t{7}, &Row::id);
  REQUIRE(in_span.size() == 3);
  CHECK(in_span.data() == &kRows[2]);
  CHECK(RowWithKey(view, std::uint16_t{5}, &Row::id) == &kRows[1]);
}

TEST_CASE("Table triée : une clé plus large que le champ ne se tronque pas",
          "[core][sorted_table]") {
  // 0x10007 tronqué sur 16 bits vaudrait 7, qui est dans la table.
  constexpr std::uint32_t kWiderThanField = 0x10007;
  CHECK(RowsWithKey(kRows, kWiderThanField, &Row::id).empty());
  CHECK(RowWithKey(kRows, kWiderThanField, &Row::id) == nullptr);
  CHECK(RowWithKey(kRows, std::uint32_t{7}, &Row::id) == &kRows[2]);
}
