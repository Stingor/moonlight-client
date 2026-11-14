#include "ro/render/screen_text.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/font_atlas.h"

using ro::render::ComposeCenteredLabel;
using ro::render::ComposeText;
using ro::render::FontAtlas;
using ro::render::kNameColor;
using ro::render::MeasureText;
using ro::render::ScreenQuad;

namespace {

// La police du système sert de sujet : ces essais portent sur la MISE EN PAGE,
// pas sur le dessin des lettres. Là où il n'y en a pas, ils se taisent plutôt
// que d'échouer — un runner d'intégration nu n'est pas une régression.
constexpr int kPixelHeight = 14;

std::optional<FontAtlas> SystemFont() {
  const std::array<const char*, 3> candidates = {
      "C:\\Windows\\Fonts\\arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"};
  for (const char* path : candidates) {
    if (!std::filesystem::exists(path)) {
      continue;
    }
    if (auto atlas = FontAtlas::Load(path, kPixelHeight); atlas) {
      return std::move(*atlas);
    }
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE("Texte : une police gravée porte ses glyphes et ses mesures", "[render][text]") {
  const std::optional<FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  // L'image est gravée une fois pour toutes, et l'ASCII imprimable y est.
  CHECK(font->image().width > 0);
  CHECK(font->image().height > 0);
  CHECK(font->line_height() > 0.0F);
  CHECK(font->ascent() > 0.0F);
  CHECK(font->GlyphFor(U'A') != nullptr);
  CHECK(font->GlyphFor(U'é') != nullptr);
  // LA LIGATURE FRANÇAISE N'EST PAS DANS LATIN-1, et sans elle « nœud »
  // s'écrivait « nud » à l'écran — un glyphe manquant ne se dessine pas, et
  // rien ne le signale.
  CHECK(font->GlyphFor(U'œ') != nullptr);
  CHECK(font->GlyphFor(U'Œ') != nullptr);
  // Le coréen n'est pas gravé : c'est une limite connue, pas une panne.
  CHECK(font->GlyphFor(U'가') == nullptr);
}

TEST_CASE("Texte : la largeur croît avec ce qu'on écrit", "[render][text]") {
  const std::optional<FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  CHECK(MeasureText(*font, "") == Catch::Approx(0.0F));
  CHECK(MeasureText(*font, "MM") > MeasureText(*font, "M"));
  // Un blanc ne dessine rien mais compte : sans cela, deux mots se colleraient.
  CHECK(MeasureText(*font, "a b") > MeasureText(*font, "ab"));
}

TEST_CASE("Texte : un nom s'écrit centré, doublé de son ombre", "[render][text]") {
  const std::optional<FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  const std::string name = "Stingor";
  const std::vector<ScreenQuad> plain = ComposeText(*font, name, glm::vec2{0.0F, 0.0F}, kNameColor);
  REQUIRE_FALSE(plain.empty());

  // L'ombre double le texte : deux fois plus de quads, pas un de plus.
  const std::vector<ScreenQuad> label = ComposeCenteredLabel(*font, name, glm::vec2{100.0F, 50.0F});
  CHECK(label.size() == plain.size() * 2);

  // Et le tout est centré sur l'abscisse donnée, à un demi-glyphe près.
  float left = label.front().min.x;
  float right = label.front().max.x;
  for (const ScreenQuad& quad : label) {
    left = std::min(left, quad.min.x);
    right = std::max(right, quad.max.x);
  }
  CHECK((left + right) * 0.5F == Catch::Approx(100.0F).margin(2.0F));
}

TEST_CASE("Texte : un octet isolé ne fait pas boucler l'écriture", "[render][text]") {
  const std::optional<FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  // Les noms viennent du serveur : ce sont des octets, pas une promesse d'UTF-8.
  // Une séquence tronquée doit s'écrire de travers, jamais tourner en rond.
  const std::string broken = std::string("Sting") + '\xC3';
  CHECK(MeasureText(*font, broken) >= MeasureText(*font, "Sting"));
  CHECK_FALSE(ComposeText(*font, broken, glm::vec2{0.0F, 0.0F}, kNameColor).empty());
}

TEST_CASE("Texte : un paragraphe se replie à la largeur donnée", "[render][text]") {
  const std::optional<FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  const std::string_view phrase = "le renard brun et rapide saute par-dessus le chien";

  // Sans borne, tout tient sur une ligne : c'est ce que veut dire zéro, et
  // l'appelant n'a donc pas de cas à traiter.
  CHECK(ro::render::WrapTextLines(*font, phrase, 0.0F).size() == 1);

  // Borné, il se coupe — et AUCUNE ligne ne dépasse.
  const float most = ro::render::MeasureText(*font, phrase) / 3.0F;
  const std::vector<std::string_view> lines = ro::render::WrapTextLines(*font, phrase, most);
  CHECK(lines.size() >= 3);
  for (const std::string_view line : lines) {
    CHECK(ro::render::MeasureText(*font, line) <= most);
    // Une ligne vide ferait un trou, et la boucle tournerait sans avancer.
    CHECK_FALSE(line.empty());
  }

  // 🔴 UN MOT PLUS LARGE QUE LA BULLE EXISTE — une adresse collée, un nom
  // à rallonge. Le laisser passer annulerait la borne qu'on vient de poser, et
  // c'est précisément le cas où la bulle sortirait de l'écran.
  const std::string_view glued = "unmotbeaucouptroplongpourtenirdanslaplacedonnee";
  const float narrow = ro::render::MeasureText(*font, glued) / 4.0F;
  const std::vector<std::string_view> cut = ro::render::WrapTextLines(*font, glued, narrow);
  CHECK(cut.size() >= 4);
  for (const std::string_view line : cut) {
    CHECK(ro::render::MeasureText(*font, line) <= narrow);
  }
}

TEST_CASE("Texte : un paragraphe POUSSE VERS LE HAUT", "[render][text]") {
  const std::optional<FontAtlas> font = SystemFont();
  if (!font.has_value()) {
    SUCCEED("aucune police système : rien à vérifier ici");
    return;
  }
  // ⚠ LE POINT DONNÉ EST LA DERNIÈRE LIGNE. Une bulle naît au-dessus d'une tête :
  // prendre la première pour référence ferait descendre le texte SUR le
  // personnage à mesure que la phrase s'allonge.
  constexpr float kBaseline = 300.0F;
  const std::string_view phrase = "une phrase assez longue pour tenir sur deux lignes";
  const float most = ro::render::MeasureText(*font, phrase) * 0.5F;

  float lowest = 0.0F;
  for (const ro::render::ScreenQuad& quad :
       ro::render::ComposeCenteredParagraph(*font, phrase, glm::vec2{400.0F, kBaseline}, most)) {
    lowest = std::max(lowest, quad.max.y);
  }
  // Rien ne descend sous la ligne de base d'une dernière ligne, jambages mis à
  // part : la bulle est donc bien AU-DESSUS du point qu'on lui a donné.
  CHECK(lowest < kBaseline + font->line_height());
}
