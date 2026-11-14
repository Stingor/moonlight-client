// LE CODE D'UN CALQUE : la teinture d'un chapeau, d'une cape, des cheveux ou de
// la peau, que le joueur copie et colle à côté du code v7 du corps.
//
// 🔴 ÉPROUVÉ SUR DES CHAÎNES LITTÉRALES : un aller-retour passerait avec
// n'importe quel format, et c'est la chaîne que des joueurs se passent.

#include "ro/sim/style_layer_code.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/style.h"

using ro::sim::DecodeStyleLayerCode;
using ro::sim::EncodeStyleLayerCode;
using ro::sim::StyleAdjust;
using ro::sim::StyleLayerCode;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRanks;

namespace {

// Une coiffe en classes de teinte : la première classe et les gris réglés.
[[nodiscard]] StyleLayerCode TwoClasses() {
  StyleLayerCode code;
  code.layer = StyleLayerKind::kHeadgear;
  code.profile = 2;
  code.adjusts[0] = StyleAdjust{.hue = 120, .saturation = 50, .value = -10, .absolute = true};
  code.adjusts[12] = StyleAdjust{.hue = -30, .saturation = 0, .value = 5, .absolute = false};
  return code;
}

constexpr const char* kTwoClasses = "L1:2:2:1001:007832f601ffe2000500";

}  // namespace

TEST_CASE("Code de calque : seuls les réglages non neutres s'écrivent", "[sim][style][layers]") {
  CHECK(EncodeStyleLayerCode(TwoClasses()) == kTwoClasses);

  StyleLayerCode bare;
  bare.layer = StyleLayerKind::kSkin;
  bare.profile = 1;
  CHECK(EncodeStyleLayerCode(bare) == "L1:4:1:0000:");
}

TEST_CASE("Code de calque : ce qui a été écrit se relit à l'identique", "[sim][style][layers]") {
  const auto read = DecodeStyleLayerCode(kTwoClasses);
  REQUIRE(read.has_value());
  CHECK(read->layer == StyleLayerKind::kHeadgear);
  CHECK(read->profile == 2);
  CHECK(read->adjusts[0].hue == 120);
  CHECK(read->adjusts[0].saturation == 50);
  CHECK(read->adjusts[0].value == -10);
  CHECK(read->adjusts[0].absolute);
  CHECK(read->adjusts[12].hue == -30);
  CHECK(read->adjusts[12].value == 5);
  CHECK_FALSE(read->adjusts[12].absolute);
  CHECK(read->adjusts[5].hue == 0);
  // Les espaces d'un collage depuis le chat sont tolérés, les majuscules aussi.
  CHECK(DecodeStyleLayerCode("  L1:2:2:1001:007832F601FFE2000500 \n").has_value());
}

TEST_CASE("Code de calque : ce que le serveur refuserait ne se lit pas", "[sim][style][layers]") {
  // Un code v7 n'est pas un code de calque.
  CHECK_FALSE(DecodeStyleLayerCode("7:00000000:-1:-1:-1:").has_value());
  // Un calque inconnu, ou la suite du corps, qui voyage avec lui.
  CHECK_FALSE(DecodeStyleLayerCode("L1:5:1:0000:").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L1:0:1:0000:").has_value());
  // La tête et la peau n'ont que les rampes.
  CHECK_FALSE(DecodeStyleLayerCode("L1:1:2:0000:").has_value());
  // La peau n'a qu'un rang.
  CHECK_FALSE(DecodeStyleLayerCode("L1:4:1:0002:0000000000").has_value());
  // Les rampes n'ont que huit rangs.
  CHECK_FALSE(DecodeStyleLayerCode("L1:3:1:0100:0000000000").has_value());
  // « Absolu » vaut zéro ou un ; une teinte absolue n'est pas négative.
  CHECK_FALSE(DecodeStyleLayerCode("L1:3:1:0001:0000000002").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L1:3:1:0001:ffe2000001").has_value());
  // Un réglage tronqué, ou une queue de trop.
  CHECK_FALSE(DecodeStyleLayerCode("L1:3:1:0001:00000000").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L1:3:1:0000:00").has_value());
}

TEST_CASE("Code de calque : les rangs de chaque calque et de chaque profil",
          "[sim][style][layers]") {
  CHECK(StyleLayerRanks(StyleLayerKind::kHead, 1) == 8);
  CHECK(StyleLayerRanks(StyleLayerKind::kHead, 2) == 0);
  CHECK(StyleLayerRanks(StyleLayerKind::kSkin, 1) == 1);
  CHECK(StyleLayerRanks(StyleLayerKind::kHeadgear, 2) == 13);
  CHECK(StyleLayerRanks(StyleLayerKind::kGarment, 1) == 8);
  CHECK(StyleLayerRanks(StyleLayerKind::kGarment, 3) == 0);
  CHECK(StyleLayerRanks(StyleLayerKind::kCompanion, 1) == 8);
  CHECK(StyleLayerRanks(StyleLayerKind::kCompanion, 2) == 0);
  CHECK(StyleLayerRanks(StyleLayerKind::kHatEffect, 1) == 0);
}

TEST_CASE("Code de calque : la teinture d'un compagnon se partage aussi", "[sim][style][layers]") {
  StyleLayerCode code;
  code.layer = StyleLayerKind::kCompanion;
  code.profile = ro::sim::kStyleLayerCodeRampProfile;
  code.adjusts[2] = {.hue = 120, .saturation = 10, .value = -5, .absolute = false};
  code.key = 1002;
  code.retouches.push_back({.index = 9, .red = 1, .green = 2, .blue = 3});
  const std::optional<StyleLayerCode> read = DecodeStyleLayerCode(EncodeStyleLayerCode(code));
  REQUIRE(read.has_value());
  CHECK(read->layer == StyleLayerKind::kCompanion);
  CHECK(read->key == 1002);
  CHECK(read->adjusts[2].hue == 120);
  CHECK(read->retouches.size() == 1);
  // Les effets de chapeau ne s'écrivent pas en code de calque.
  CHECK_FALSE(DecodeStyleLayerCode("L1:8:1:0000:").has_value());
}

// ── LE PROFIL MIXTE : `L2:` ─────────────────────────────────────────────────

TEST_CASE("Code de calque : un calque sans reste ni retouche reste en L1",
          "[sim][style][layers][rest]") {
  StyleLayerCode code = TwoClasses();
  code.key = 77;
  CHECK(EncodeStyleLayerCode(code).starts_with("L1:"));
}

TEST_CASE("Code de calque : le profil mixte s'écrit en L2 avec sa clé",
          "[sim][style][layers][rest]") {
  StyleLayerCode code;
  code.layer = StyleLayerKind::kHeadgear;
  code.profile = 1;
  code.key = 0x11;
  code.adjusts[0] = StyleAdjust{.hue = 1};
  code.rest[12] = StyleAdjust{.hue = 2, .absolute = true};
  code.retouches = {ro::sim::StyleRetouch{.index = 5, .red = 0xAA, .green = 0xBB, .blue = 0xCC}};
  const std::string text = EncodeStyleLayerCode(code);
  CHECK(text == "L2:2:1:00000011:0001:0001000000:1000:0002000001:01:05aabbcc");
  const auto read = DecodeStyleLayerCode(text);
  REQUIRE(read.has_value());
  CHECK(read->key == 0x11);
  CHECK(read->rest[12].hue == 2);
  CHECK(read->rest[12].absolute);
  CHECK(read->retouches == code.retouches);
}

TEST_CASE("Code de calque : un L2 interdit ne se lit pas", "[sim][style][layers][rest]") {
  // L'index zéro, le magenta pur, le désordre, la peau, le reste au profil 2.
  CHECK_FALSE(DecodeStyleLayerCode("L2:2:1:00000011:0000::0000::01:00aabbcc").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L2:2:1:00000011:0000::0000::01:05ff00ff").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L2:2:1:00000011:0000::0000::02:0600000005000000").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L2:4:1:00000000:0000::0000::01:05aabbcc").has_value());
  CHECK_FALSE(DecodeStyleLayerCode("L2:2:2:00000011:0000::0001:0001000000:00:").has_value());
  // Au profil 2, les retouches valent.
  CHECK(DecodeStyleLayerCode("L2:2:2:00000011:0000::0000::01:05aabbcc").has_value());
}
