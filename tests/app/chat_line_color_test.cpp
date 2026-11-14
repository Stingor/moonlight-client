// LA TEINTE D'UNE BULLE : ses octets se lisent dans l'ordre inverse de ceux
// d'une ligne du fil.
#include "chat/chat_line_color.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

// Rouge plein et vert à moitié, écrits pour une ligne (0xRRGGBB)…
constexpr std::uint32_t kOrangeAsLine = 0xFF8000;
// … et les mêmes octets, lus par une bulle (0x00BBGGRR) : du bleu.
constexpr std::uint32_t kSameBytesAsBubble = kOrangeAsLine;

}  // namespace

TEST_CASE("Bulle : la couleur se lit le rouge dans l'octet de poids faible", "[app][chat]") {
  CHECK(moonclient::ColorOfShownRgb(kOrangeAsLine) == IM_COL32(0xFF, 0x80, 0x00, 0xFF));
  CHECK(moonclient::ColorOfBubbleBgr(kSameBytesAsBubble) == IM_COL32(0x00, 0x80, 0xFF, 0xFF));
}

TEST_CASE("Bulle : zéro garde la couleur du thème", "[app][chat]") {
  CHECK(moonclient::ColorOfBubbleBgr(0) == 0);
}
