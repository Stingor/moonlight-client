// CE QUE LA POLICE SAIT DESSINER, demandé à ImGui plutôt que supposé.
//
// ⚠ UN CARACTÈRE ABSENT NE SE VOIT PAS DANS L'ÉDITEUR : il s'y affiche
// parfaitement, et sort en « ? » à l'écran. C'est le genre de défaut qu'on ne
// découvre qu'en jouant, et seulement si l'on regarde — d'où cet essai, qui
// recoupe les plages de `libs/ui/src/font_atlas_build.cpp` avec ce que l'atlas
// rend vraiment, et qui les tient donc accordées à celles de
// `scripts/check_translations.py`.

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/font_setup.h"
#include "ro/ui/system_fonts.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// ⚠ LA QUESTION SE POSE À LA POLICE CUITE, pas à la source. Depuis ImGui 1.92
// l'atlas est dynamique : `ImFont` décrit ce qu'on a chargé, et c'est
// `ImFontBaked` — la police à une taille donnée — qui porte les glyphes.
[[nodiscard]] bool CanDraw(unsigned int codepoint) {
  ImFont* font = ImGui::GetFont();
  if (font == nullptr) {
    return false;
  }
  ImFontBaked* baked = font->GetFontBaked(ImGui::GetFontSize());
  return baked != nullptr && baked->FindGlyphNoFallback(static_cast<ImWchar>(codepoint)) != nullptr;
}

// Ce que les textes de l'interface emploient au-delà du latin-1, et que les
// plages doivent couvrir. Un caractère ajouté ici sans être dans les plages fait
// échouer l'essai — c'est le but.
constexpr std::array<unsigned int, 5> kTypography = {
    0x0153,  // œ, le « cœur » et le « bœuf »
    0x2014,  // le tiret cadratin
    0x2019,  // l'apostrophe courbe
    0x2026,  // les points de suspension
    0xFFFD,  // le carré « caractère manquant »
};

// La police qui couvre tout, si cette machine l'a. ⚠ L'ESSAI NE PEUT PAS EN
// DÉPENDRE : l'intégration bâtit aussi cette couche sur un runner Linux, où
// aucun fichier de Windows n'existe.
constexpr std::string_view kCompleteFont = "C:\\Windows\\Fonts\\malgun.ttf";
// Une seconde, pour vérifier que la bascule change vraiment de dessin.
constexpr std::string_view kOtherFont = "C:\\Windows\\Fonts\\verdana.ttf";

[[nodiscard]] bool HaveCompleteFont() {
  std::error_code ignored;
  return std::filesystem::exists(std::filesystem::path(kCompleteFont), ignored);
}

}  // namespace

TEST_CASE("Police : celle d'ImGui s'arrête au latin-1", "[ui][police]") {
  // ⚠ C'EST LA MESURE QUI JUSTIFIE TOUT LE RESTE. ProggyClean n'a AUCUN glyphe
  // au-delà de U+00FF : sans police chargée, le tiret cadratin, les points de
  // suspension et le « œ » sortent tous en « ? ».
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(CanDraw(0x00E9));  // é : le latin accentué passe
  CHECK(CanDraw(0x00AB));  // « : les guillemets français aussi
  for (const unsigned int codepoint : kTypography) {
    INFO("U+" << std::hex << codepoint);
    CHECK_FALSE(CanDraw(codepoint));
  }
  ui.EndFrame();
}

TEST_CASE("Police : une vraie police comble les trous", "[ui][police]") {
  if (!HaveCompleteFont()) {
    SUCCEED("cette machine n'a pas la police complète : rien à mesurer");
    return;
  }
  ro::ui::UiContext ui;
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "complète", .regular = kCompleteFont}};
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0});

  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(CanDraw(0x00E9));
  // ⚠ CE SONT LES PLAGES DE `font_atlas_build.cpp` QU'ON VÉRIFIE, et par elles
  // celles de `check_translations.py`. Une plage retirée là-bas sans que
  // personne ne s'en aperçoive rendrait les « ? » de l'interface.
  for (const unsigned int codepoint : kTypography) {
    INFO("U+" << std::hex << codepoint);
    CHECK(CanDraw(codepoint));
  }
  ui.EndFrame();
}

TEST_CASE("Police : une famille introuvable ne casse rien", "[ui][police]") {
  // ⚠ UNE POLICE ABSENTE N'EST PAS UNE PANNE : l'interface retombe sur celle
  // d'ImGui, avec ses trous, et le joueur peut jouer. Refuser de démarrer pour
  // un fichier de police serait hors de proportion.
  ro::ui::UiContext ui;
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "fantôme", .regular = "Z:\\rien\\du\\tout.ttf"}};
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0});
  CHECK_FALSE(ui.FontFamilyUsable(0));

  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(CanDraw(0x00E9));  // le latin-1 reste dessiné : la police intégrée tient
  ui.EndFrame();
}

TEST_CASE("Police : changer de famille ne rebâtit rien", "[ui][police]") {
  // ⚠ C'EST TOUT L'INTÉRÊT DE LES BAKER TOUTES À L'OUVERTURE : la bascule ne coûte
  // qu'un changement de pointeur, donc elle peut avoir lieu PENDANT la partie.
  // Sans cet essai, un réglage inerte ne se voit qu'en jouant — et il l'a été.
  if (!HaveCompleteFont()) {
    SUCCEED("cette machine n'a pas la police complète : rien à mesurer");
    return;
  }
  ro::ui::UiContext ui;
  const std::array<ro::ui::FontFamilySpec, 2> families = {
      ro::ui::FontFamilySpec{.label = "complète", .regular = kCompleteFont},
      ro::ui::FontFamilySpec{.label = "autre", .regular = kOtherFont}};
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0});
  ImFont* const first = ui.ChosenFont(ro::ui::FontStyle::kRegular);
  REQUIRE(first != nullptr);

  ui.ChooseFont(1);
  CHECK(ui.chosen_font() == 1);
  if (ui.FontFamilyUsable(1)) {
    CHECK(ui.ChosenFont(ro::ui::FontStyle::kRegular) != first);
  }

  // Moins de zéro : la police intégrée, qui est un choix et non un repli.
  ui.ChooseFont(-1);
  CHECK(ui.chosen_font() == -1);
  CHECK(ui.ChosenFont(ro::ui::FontStyle::kRegular) != first);

  // Un rang hors table retombe sur la première, jamais sur une lecture hors
  // bornes : il peut venir d'un fichier de réglages trafiqué.
  ui.ChooseFont(99);
  CHECK(ui.chosen_font() == 0);
}

TEST_CASE("Police : choisir par le nom saute les familles absentes", "[ui][police]") {
  // 🔴 LE RÉGLAGE COMPTE DANS LA TABLE ENTIÈRE, L'ATLAS DANS LES FAMILLES
  // TROUVÉES. Quand la deuxième de trois manque sur la machine, la troisième est
  // au rang un de l'atlas : choisie par son rang du réglage, deux, elle
  // tomberait hors de l'atlas ; par son nom, elle tombe juste. Les fichiers
  // n'ont pas à exister — c'est le nom qu'on éprouve, pas le dessin.
  ro::ui::UiContext ui;
  const std::array<ro::ui::FontFamilySpec, 2> found = {
      ro::ui::FontFamilySpec{.label = "première", .regular = "Z:\\premiere.ttf"},
      ro::ui::FontFamilySpec{.label = "troisième", .regular = "Z:\\troisieme.ttf"}};
  ui.UseFonts(ro::ui::FontSetup{.families = found, .chosen = 0});

  ui.ChooseFontNamed("troisième");
  CHECK(ui.chosen_font() == 1);
  // Celle qui manque : le défaut reprend, comme au démarrage.
  ui.ChooseFontNamed("deuxième");
  CHECK(ui.chosen_font() == 0);
}

TEST_CASE("Police : une taille NON DEMANDEE n'est pas celle de la police integree",
          "[ui][police]") {
  // 🔴 LA MESURE QUI EXPLIQUE UN ECRAN ENTIER. On croyait que laisser la taille a
  // zero revenait a prendre celle de la police integree -- treize. Il n'en est
  // rien : une police chargee sans taille n'a pas de taille de reference, et
  // l'interface retombe alors sur SON defaut, qui vaut VINGT.
  //
  // Un tiers de trop, sur TOUT : le texte, la hauteur des champs, celle des
  // rangees, celle du pied. Et rien ne le signale -- le texte est net, les
  // marges sont justes, et l'on cherche le defaut du mauvais cote.
  if (!HaveCompleteFont()) {
    SUCCEED("cette machine n'a pas la police complète : rien à mesurer");
    return;
  }
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "complète", .regular = kCompleteFont}};

  ro::ui::UiContext unsized;
  unsized.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0, .size_pixels = 0.0F});
  unsized.BeginFrame(kViewport, kFrameSeconds);
  const float tombee = ImGui::GetFontSize();
  unsized.EndFrame();
  CHECK(tombee > ro::ui::kDefaultFontPixels);

  ro::ui::UiContext requested;
  requested.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0});
  requested.BeginFrame(kViewport, kFrameSeconds);
  // ⚠ LE DEFAUT DE `FontSetup` EST CE QU'ON MESURE : le laisser vide doit donner
  // la taille de l'habillage, pas celle qu'ImGui choisirait.
  CHECK(ImGui::GetFontSize() == ro::ui::kDefaultFontPixels);
  requested.EndFrame();
}

TEST_CASE("Police : l'echelle MULTIPLIE la taille, elle ne la remplace pas", "[ui][police]") {
  if (!HaveCompleteFont()) {
    SUCCEED("cette machine n'a pas la police complète : rien à mesurer");
    return;
  }
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "complète", .regular = kCompleteFont}};
  ro::ui::UiContext ui;
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0});

  // ⚠ ET LE RESULTAT EST ARRONDI A L'ENTIER. L'interface le fait elle-meme --
  // sa disposition ne sait pas encore placer du texte de taille fractionnaire --
  // donc quinze fois une fois et demie ne rend pas 22,5 mais 23. Attendre le
  // produit exact ferait echouer cet essai sur une regle qui n'est pas la notre.
  ui.SetInterfaceScale(1.5F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(ImGui::GetFontSize() == 23.0F);
  ui.EndFrame();

  // ⚠ ET LE RETOUR A UN REND EXACTEMENT LA TAILLE DE DEPART. Le style, lui, est
  // tronque au pixel a chaque passage : la police, non.
  ui.SetInterfaceScale(1.0F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(ImGui::GetFontSize() == ro::ui::kDefaultFontPixels);
  ui.EndFrame();
}

TEST_CASE("Police : celles du dossier du jeu se trouvent par leur nom", "[ui][police]") {
  if (!HaveCompleteFont()) {
    SUCCEED("cette machine n'a pas la police complète : rien à mesurer");
    return;
  }
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "complète", .regular = kCompleteFont}};
  const std::array<ro::ui::FontFamilySpec, 1> loose = {
      ro::ui::FontFamilySpec{.label = "déposée", .regular = kOtherFont}};
  ro::ui::UiContext ui;
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0, .loose = loose});

  REQUIRE(ui.LooseFontNames() == std::vector<std::string>{"déposée"});
  ImFont* const dropped = ui.FontNamed("déposée", ro::ui::FontStyle::kBold);
  REQUIRE(dropped != nullptr);
  // ⚠ UNE POLICE À ELLE, et non celle de l'interface sous un autre nom.
  CHECK(dropped != ui.ChosenFont(ro::ui::FontStyle::kRegular));
  // Une famille se trouve aussi par son nom — sans gras, elle prend le normal.
  CHECK(ui.FontNamed("complète", ro::ui::FontStyle::kBold) ==
        ui.ChosenFont(ro::ui::FontStyle::kRegular));
  CHECK(ui.FontNamed("inconnue", ro::ui::FontStyle::kRegular) == nullptr);
}

TEST_CASE("Police : un fichier du dossier du jeu absent ou vide est écarté", "[ui][police]") {
  // ⚠ UN FICHIER VIDE ARRÊTERAIT LE PROGRAMME s'il atteignait ImGui : c'est
  // ce que cet essai garde. Le dossier du jeu se remplit à la main.
  const std::filesystem::path empty =
      std::filesystem::temp_directory_path() / "moonclient_police_vide.ttf";
  {
    std::ofstream touched(empty, std::ios::binary);
  }
  const std::string empty_path = empty.string();
  const std::array<ro::ui::FontFamilySpec, 2> loose = {
      ro::ui::FontFamilySpec{.label = "absente", .regular = "Z:\\rien\\du\\tout.ttf"},
      ro::ui::FontFamilySpec{.label = "vide", .regular = empty_path}};
  ro::ui::UiContext ui;
  ui.UseFonts(ro::ui::FontSetup{.loose = loose});
  CHECK(ui.LooseFontNames().empty());
  CHECK(ui.FontNamed("vide", ro::ui::FontStyle::kRegular) == nullptr);
  std::error_code ignored;
  std::filesystem::remove(empty, ignored);
}

// ── LA POLICE DU BANDEAU DU NOM DE LA CARTE ─────────────────────────────────

TEST_CASE("Police du bandeau : Arial gras, ou sa jumelle métrique hors de Windows",
          "[ui][police][map_sign]") {
  const std::span<const std::string_view> candidates = ro::ui::MapSignFontCandidates();
  REQUIRE_FALSE(candidates.empty());
#ifdef _WIN32
  CHECK(candidates.front().ends_with("arialbd.ttf"));
#else
  // Sous Linux, un chemin n'existe que si le fichier a été trouvé : c'est le
  // nom de la table qu'on éprouve, par la source quand elle est là.
  const std::string_view source = ro::ui::MapSignFontSource();
  if (!source.empty()) {
    CHECK((source.ends_with("LiberationSans-Bold.ttf") || source.ends_with("DejaVuSans-Bold.ttf")));
  }
#endif
  // La source est la PREMIÈRE candidate présente, ou rien.
  const std::string_view source_found = ro::ui::MapSignFontSource();
  for (const std::string_view candidate : candidates) {
    std::error_code ignored;
    if (!candidate.empty() && std::filesystem::exists(std::filesystem::path(candidate), ignored)) {
      CHECK(source_found == candidate);
      return;
    }
  }
  CHECK(source_found.empty());
}

TEST_CASE("Police du bandeau : un fichier absent laisse la police courante",
          "[ui][police][map_sign]") {
  ro::ui::UiContext ui;
  ui.UseFonts(ro::ui::FontSetup{.map_sign_source = "Z:\\rien\\du\\tout.ttf"});
  CHECK(ui.MapSignFont() == nullptr);
}

namespace {

// Combien de pixels du glyphe, gravé à cette taille, ne sont ni vides ni
// pleins : la trace du lissage. Une police sans lissage n'en a aucun.
[[nodiscard]] int HalfToneTexels(ImFont* font, float size_px, ImWchar codepoint) {
  ImFontBaked* baked = font->GetFontBaked(size_px);
  const ImFontGlyph* glyph = baked != nullptr ? baked->FindGlyphNoFallback(codepoint) : nullptr;
  if (glyph == nullptr) {
    return -1;
  }
  ImFontAtlas* atlas = ImGui::GetIO().Fonts;
  ImFontAtlasRect rect;
  if (!atlas->GetCustomRect(glyph->PackId, &rect)) {
    return -1;
  }
  ImTextureData* texture = atlas->TexData;
  // L'opacité est le dernier octet d'un pixel RGBA, le seul d'un pixel Alpha8.
  const int alpha_byte = texture->BytesPerPixel - 1;
  constexpr unsigned char kEmpty = 0;
  constexpr unsigned char kFull = 255;
  int half_tones = 0;
  for (int y = rect.y; y < rect.y + rect.h; ++y) {
    for (int x = rect.x; x < rect.x + rect.w; ++x) {
      const unsigned char alpha =
          static_cast<const unsigned char*>(texture->GetPixelsAt(x, y))[alpha_byte];
      if (alpha != kEmpty && alpha != kFull) {
        ++half_tones;
      }
    }
  }
  return half_tones;
}

}  // namespace

TEST_CASE("Police du bandeau : gravée sans lissage, à la différence de l'interface",
          "[ui][police][map_sign]") {
  const std::string_view source = ro::ui::MapSignFontSource();
  if (source.empty()) {
    SUCCEED("cette machine n'a ni Arial gras ni sa jumelle : rien à mesurer");
    return;
  }
  // Le MÊME fichier, une fois comme famille de l'interface, une fois comme
  // police du bandeau : seul le lissage les sépare.
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "témoin", .regular = source}};
  ro::ui::UiContext ui;
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0, .map_sign_source = source});
  ImFont* const sign = ui.MapSignFont();
  REQUIRE(sign != nullptr);
  CHECK(sign != ui.ChosenFont(ro::ui::FontStyle::kRegular));

  // La taille d'un titre sur 720 lignes : 36 pixels de cellule.
  constexpr float kTitlePx = 36.0F;
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(HalfToneTexels(sign, kTitlePx, 'M') == 0);
  CHECK(HalfToneTexels(ui.ChosenFont(ro::ui::FontStyle::kRegular), kTitlePx, 'M') > 0);
  ui.EndFrame();
}
