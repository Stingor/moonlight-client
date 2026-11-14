// L'HABILLAGE DU SKIN : ses thèmes, le retour d'un réglage à son thème, sa
// réinitialisation, et le fichier qui le garde dans le dossier du skin.

#include "options/skin_look_options.h"

#include <algorithm>
#include <fstream>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "options/skin_look_code.h"
#include "support/temp_directory.h"

using moonclient::kSkinLookRowBase;
using moonclient::SkinLookOptions;
using moonclient::SkinLookPresets;
using moonclient::SkinLookRow;
using ro::test_support::TempDirectory;
using ro::ui::OptionsWindowAction;
using ro::ui::ProfileGesture;
using ro::ui::ProfilesPageAction;
using ro::ui::SkinLook;

namespace {

[[nodiscard]] int RowId(SkinLookRow row) { return kSkinLookRowBase + static_cast<int>(row); }

// Le thème que montre la liste, lu comme le joueur le lit.
[[nodiscard]] int ShownPreset(SkinLookOptions& options) {
  std::vector<ro::ui::OptionRowView> rows;
  options.ComposeInto(rows);
  REQUIRE_FALSE(rows.empty());
  REQUIRE(rows.front().id == RowId(SkinLookRow::kPreset));
  return rows.front().chosen;
}

// La rangée d'un réglage, lue comme le joueur la voit.
[[nodiscard]] ro::ui::OptionRowView RowOf(SkinLookOptions& options, SkinLookRow row) {
  std::vector<ro::ui::OptionRowView> rows;
  options.ComposeInto(rows);
  const auto found = std::ranges::find(rows, RowId(row), &ro::ui::OptionRowView::id);
  REQUIRE(found != rows.end());
  return *found;
}

constexpr int kClassique = 0;
constexpr int kSombre = 1;
constexpr int kSepia = 2;
// L'entrée de la liste qui dit « retouché ».
constexpr int kRetouche = 3;
constexpr ImU32 kRed = IM_COL32(200, 0, 0, 255);

}  // namespace

TEST_CASE("Habillage du skin : un habillage jamais touché est la palette du jeu", "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  CHECK(options.look() == SkinLook{});
  CHECK(ShownPreset(options) == 0);
}

TEST_CASE("Habillage du skin : un thème garde l'arrondi du joueur", "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  constexpr float kRounding = 6.0F;
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kRounding), .value = kRounding}));
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
  CHECK(options.look().body == SkinLookPresets()[kSombre].body);
  CHECK(options.look().text == SkinLookPresets()[kSombre].text);
  CHECK(options.look().rounding == kRounding);
  // L'arrondi ne fait pas sortir du thème ; une couleur, si.
  CHECK(ShownPreset(options) == kSombre);
  REQUIRE(options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kBody), .color = kRed}));
  CHECK(options.look().body == kRed);
  CHECK(ShownPreset(options) == kRetouche);
}

TEST_CASE("Habillage du skin : un réglage retouché se rétablit au thème chargé", "[app][skin]") {
  TempDirectory skin;
  {
    SkinLookOptions options;
    options.Open(skin.path(), skin.path() / "profils.yaml");
    REQUIRE(options.Change(
        OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
    REQUIRE(options.Change(
        OptionsWindowAction{.changed = RowId(SkinLookRow::kButtonText), .color = kRed}));
    CHECK(options.theme_rank() == kSombre);
    CHECK(ShownPreset(options) == kRetouche);
    // Seul le réglage retouché porte son bouton.
    CHECK(RowOf(options, SkinLookRow::kButtonText).revert != nullptr);
    CHECK_FALSE(RowOf(options, SkinLookRow::kButtonText).at_reference);
    CHECK(RowOf(options, SkinLookRow::kBody).at_reference);
    CHECK(RowOf(options, SkinLookRow::kOpacity).at_reference);
    options.Flush();
  }
  // ⚠ LE THÈME CHARGÉ SE GARDE AU FICHIER : rouvert, le retour vise encore le
  // Sombre, et non la palette du jeu.
  SkinLookOptions again;
  again.Open(skin.path(), skin.path() / "profils.yaml");
  CHECK(again.theme_rank() == kSombre);
  CHECK(again.look().button_text == kRed);
  REQUIRE(again.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kButtonText), .revert = true}));
  CHECK(again.look().button_text == SkinLookPresets()[kSombre].button_text);
  CHECK(ShownPreset(again) == kSombre);
}

TEST_CASE("Habillage du skin : la liste dit de quel thème l'habillage est retouché",
          "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSepia}));
  constexpr float kHalf = 0.5F;
  REQUIRE(
      options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kOpacity), .value = kHalf}));
  const ro::ui::OptionRowView preset = RowOf(options, SkinLookRow::kPreset);
  REQUIRE(preset.choices.size() > static_cast<std::size_t>(kRetouche));
  CHECK(std::string_view(preset.choices[kRetouche]) == "Sépia (retouché)");
  CHECK_FALSE(RowOf(options, SkinLookRow::kOpacity).at_reference);
  REQUIRE(
      options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kOpacity), .revert = true}));
  CHECK(options.look().opacity == SkinLookPresets()[kSepia].opacity);
  CHECK(ShownPreset(options) == kSepia);
}

TEST_CASE("Habillage du skin : « retouché » ne paraît que s'il ramène quelque chose",
          "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  // Sans retouche, rien à choisir de plus que les thèmes.
  CHECK(RowOf(options, SkinLookRow::kPreset).choices.size() == SkinLookPresets().size());

  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
  REQUIRE(options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kBody), .color = kRed}));
  // UN THÈME ESSAYÉ PAR-DESSUS met la retouche de côté, et la liste la garde.
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kClassique}));
  CHECK(options.look().body == SkinLookPresets()[kClassique].body);
  const ro::ui::OptionRowView preset = RowOf(options, SkinLookRow::kPreset);
  REQUIRE(preset.choices.size() > static_cast<std::size_t>(kRetouche));
  CHECK(std::string_view(preset.choices[kRetouche]) == "Sombre (retouché)");
  CHECK(preset.chosen == kClassique);

  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kRetouche}));
  CHECK(options.look().body == kRed);
  CHECK(options.theme_rank() == kSombre);
  CHECK(ShownPreset(options) == kRetouche);
}

TEST_CASE("Habillage du skin : un fichier d'avant les teintes prend celles de son thème",
          "[app][skin]") {
  // UN HABILLAGE SOMBRE RETOUCHÉ, écrit avant les teintes : ni thème, ni
  // boutons. Le texte du corps est blanc, le titre noir.
  TempDirectory skin;
  const SkinLook& dark = SkinLookPresets()[kSombre];
  constexpr ImU32 kWhite = IM_COL32(255, 255, 255, 255);
  constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
  {
    std::ofstream file(skin.path() / "look.yaml");
    file << "album_image_brightness: 0.6\n"
         << "body_color: " << dark.body << "\n"
         << "border_color: " << dark.border << "\n"
         << "title_text_color: " << kBlack << "\n"
         << "text_color: " << kWhite << "\n"
         << "active_tab_color: " << dark.active_tab << "\n"
         << "idle_tab_color: " << dark.idle_tab << "\n"
         << "input_color: " << dark.input << "\n"
         << "table_header_color: " << dark.table_header << "\n"
         << "popup_color: " << dark.popup << "\n"
         << "slot_color: " << dark.slot << "\n"
         << "doll_color: " << dark.doll << "\n"
         << "album_paper_color: " << dark.card << "\n"
         << "card_band_color: " << dark.card_band << "\n"
         << "card_band_text_color: " << dark.card_band_text << "\n";
  }
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  CHECK(options.theme_rank() == kSombre);
  // Ce que le joueur a choisi reste à lui.
  CHECK(options.look().text == kWhite);
  CHECK(options.look().title_text == kBlack);
  // Ce qui est venu avec les teintes vient du thème : un libellé clair sur un
  // bouton foncé, et non le blanc sur blanc qu'il avait.
  CHECK(options.look().button_tint == dark.button_tint);
  CHECK(options.look().button_hover_tint == dark.button_hover_tint);
  CHECK(options.look().button_text == dark.button_text);
  CHECK(options.look().check_tint == dark.check_tint);
  CHECK(options.look().slider_tint == dark.slider_tint);
  CHECK(options.look().item_tab_tint == dark.item_tab_tint);
  CHECK(options.look().item_grid_tint == dark.item_grid_tint);
  // Et ce qui est venu depuis, aussi : le texte secondaire, les liens, la
  // sélection et les infobulles.
  CHECK(options.look().faint_text == dark.faint_text);
  CHECK(options.look().link == dark.link);
  CHECK(options.look().selection == dark.selection);
  CHECK(options.look().paper == dark.paper);
  CHECK(options.look().good_text == dark.good_text);
  // ⚠ SAUF LES IMAGES DE LA FENÊTRE : elles passent sous le titre noir.
  CHECK(options.look().art_tint == SkinLook{}.art_tint);
  CHECK_FALSE(RowOf(options, SkinLookRow::kArtTint).at_reference);
}

TEST_CASE("Habillage du skin : un fichier des teintes prend les textes de son thème",
          "[app][skin]") {
  // ÉCRIT AVANT LES TEXTES : il dit son thème, mais ni le texte secondaire, ni
  // les liens, ni la sélection, ni les infobulles. Ce qu'il porte reste à lui.
  TempDirectory skin;
  const SkinLook& sepia = SkinLookPresets()[kSepia];
  {
    std::ofstream file(skin.path() / "look.yaml");
    file << "theme: " << kSepia << "\n"
         << "body_color: " << kRed << "\n"
         << "button_text_color: " << sepia.button_text << "\n";
  }
  {
    SkinLookOptions options;
    options.Open(skin.path(), skin.path() / "profils.yaml");
    CHECK(options.theme_rank() == kSepia);
    CHECK(options.look().body == kRed);
    CHECK(options.look().faint_text == sepia.faint_text);
    CHECK(options.look().link == sepia.link);
    CHECK(options.look().selection == sepia.selection);
    CHECK(options.look().paper == sepia.paper);
    CHECK(options.look().paper_text == sepia.paper_text);
    CHECK(options.look().warn_text == sepia.warn_text);
    // ⚠ LES TEINTES, ELLES, SONT CELLES DU FICHIER : il les connaissait déjà, et
    // une absente y vaut son défaut.
    CHECK(options.look().button_tint == SkinLook{}.button_tint);
    options.Flush();
  }
  // Relu, le fichier dit désormais ce qu'il connaît : rien n'est repris.
  SkinLookOptions again;
  again.Open(skin.path(), skin.path() / "profils.yaml");
  CHECK(again.look().button_tint == SkinLook{}.button_tint);
  CHECK(again.look().faint_text == sepia.faint_text);
}

TEST_CASE("Habillage du skin : le papier clair qu'avait le thème Sombre devient le sien",
          "[app][skin]") {
  // ÉCRIT QUAND LE THÈME SOMBRE AVAIT UN PAPIER CLAIR : le fichier le porte sans
  // que le joueur l'ait choisi.
  constexpr ImU32 kOldPaper = IM_COL32(236, 238, 244, 255);
  constexpr ImU32 kOldPaperText = IM_COL32(20, 22, 28, 255);
  const SkinLook& dark = SkinLookPresets()[kSombre];
  const auto write = [&](const TempDirectory& skin, ImU32 paper_text) {
    std::ofstream file(skin.path() / "look.yaml");
    file << "theme: " << kSombre << "\n"
         << "version: 4\n"
         << "tooltip_color: " << kOldPaper << "\n"
         << "tooltip_text_color: " << paper_text << "\n";
  };
  SECTION("intact, il prend le papier sombre et son texte") {
    TempDirectory skin;
    write(skin, kOldPaperText);
    SkinLookOptions options;
    options.Open(skin.path(), skin.path() / "profils.yaml");
    CHECK(options.look().paper == dark.paper);
    CHECK(options.look().paper_text == dark.paper_text);
    CHECK(RowOf(options, SkinLookRow::kPaper).at_reference);
  }
  SECTION("son texte retouché, il garde les deux") {
    TempDirectory skin;
    write(skin, kRed);
    SkinLookOptions options;
    options.Open(skin.path(), skin.path() / "profils.yaml");
    CHECK(options.look().paper == kOldPaper);
    CHECK(options.look().paper_text == kRed);
  }
}

TEST_CASE("Habillage du skin : un thème collé garde l'arrondi du joueur", "[app][skin]") {
  constexpr float kRounding = 6.0F;
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kRounding), .value = kRounding}));
  SkinLook shared = SkinLookPresets()[kSepia];
  shared.link = kRed;
  REQUIRE(options.PasteCode(moonclient::SkinLookCode(shared, kSepia)));
  CHECK(options.look().link == kRed);
  CHECK(options.look().rounding == kRounding);
  CHECK(options.theme_rank() == kSepia);
  // Une ligne qui n'est pas un thème ne pose rien.
  CHECK_FALSE(options.PasteCode("bonjour"));
  CHECK(options.look().link == kRed);
}

TEST_CASE("Habillage du skin : l'aperçu s'épingle en tête de la page", "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  std::vector<ro::ui::OptionRowView> pinned;
  options.ComposeSample(pinned);
  CHECK(pinned.empty());
  options.Open(skin.path(), skin.path() / "profils.yaml");
  options.ComposeSample(pinned);
  REQUIRE(pinned.size() == 1);
  CHECK(pinned.front().kind == ro::ui::OptionKind::kPanel);
  CHECK(static_cast<bool>(pinned.front().panel));
}

TEST_CASE("Habillage du skin : réinitialiser rend la palette du jeu", "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
  REQUIRE(options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kReset)}));
  CHECK(options.look() == SkinLook{});
  CHECK(options.theme_rank() == kClassique);
  // « Rétablir cette page », sur la page du skin, fait de même.
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSepia}));
  options.RestoreDefaults();
  CHECK(options.look() == SkinLook{});
  CHECK(options.theme_rank() == kClassique);
}

TEST_CASE("Habillage du skin : les bornes tiennent, et une ligne d'ailleurs n'est pas la sienne",
          "[app][skin]") {
  TempDirectory skin;
  SkinLookOptions options;
  options.Open(skin.path(), skin.path() / "profils.yaml");
  REQUIRE(
      options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kOpacity), .value = 0.0F}));
  CHECK(options.look().opacity > 0.0F);
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kArtBrightness), .value = 4.0F}));
  CHECK(options.look().art_brightness <= 1.0F);
  CHECK_FALSE(options.Change(OptionsWindowAction{.changed = kSkinLookRowBase - 1}));
  CHECK_FALSE(options.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kCount)}));
}

TEST_CASE("Habillage du skin : le dossier du skin le garde d'un démarrage à l'autre",
          "[app][skin]") {
  TempDirectory skin;
  {
    SkinLookOptions options;
    options.Open(skin.path(), skin.path() / "profils.yaml");
    REQUIRE(options.Change(
        OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
    REQUIRE(options.Change(
        OptionsWindowAction{.changed = RowId(SkinLookRow::kCardBand), .color = kRed}));
    options.Flush();
  }
  SkinLookOptions again;
  again.Open(skin.path(), skin.path() / "profils.yaml");
  CHECK(again.look().body == SkinLookPresets()[kSombre].body);
  CHECK(again.look().art_brightness == SkinLookPresets()[kSombre].art_brightness);
  CHECK(again.look().card_band == kRed);
}

TEST_CASE("Habillage du skin : un profil enregistré se pose sur un autre skin", "[app][skin]") {
  // ⚠ LES PROFILS SONT COMMUNS À TOUS LES SKINS ; le dossier du skin ne garde
  // que l'habillage présent.
  TempDirectory skin;
  TempDirectory other_skin;
  const std::filesystem::path profiles = skin.path() / "profils.yaml";
  {
    SkinLookOptions options;
    options.Open(skin.path(), profiles);
    REQUIRE(options.Change(
        OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
    options.AskProfiles(ProfilesPageAction{.gesture = ProfileGesture::kSaveAs, .new_name = "Nuit"});
    options.Tick(0);
    options.Flush();
  }
  SkinLookOptions elsewhere;
  elsewhere.Open(other_skin.path(), profiles);
  REQUIRE(elsewhere.look() == SkinLook{});
  elsewhere.AskProfiles(ProfilesPageAction{.gesture = ProfileGesture::kLoad, .target = "Nuit"});
  elsewhere.Tick(0);
  CHECK(elsewhere.look().body == SkinLookPresets()[kSombre].body);
  CHECK(elsewhere.look().art_brightness == SkinLookPresets()[kSombre].art_brightness);
}

TEST_CASE("Habillage du skin : le dernier profil posé se repose au démarrage", "[app][skin]") {
  TempDirectory skin;
  const std::filesystem::path profiles = skin.path() / "profils.yaml";
  {
    SkinLookOptions options;
    options.Open(skin.path(), profiles);
    REQUIRE(options.Change(
        OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
    options.AskProfiles(ProfilesPageAction{.gesture = ProfileGesture::kSaveAs, .new_name = "Nuit"});
    options.Tick(0);
    options.Flush();
  }
  // « Nuit » reste le profil posé : une retouche le suit, et non « Default ».
  {
    SkinLookOptions again;
    again.Open(skin.path(), profiles);
    CHECK(again.look().body == SkinLookPresets()[kSombre].body);
    REQUIRE(again.Change(OptionsWindowAction{.changed = RowId(SkinLookRow::kBody), .color = kRed}));
    constexpr std::uint32_t kQuiet = 60000;
    again.Tick(0);
    again.Tick(kQuiet);
    again.Flush();
  }
  // Un skin neuf, les mêmes profils : « Nuit » porte la retouche.
  TempDirectory other_skin;
  SkinLookOptions elsewhere;
  elsewhere.Open(other_skin.path(), profiles);
  elsewhere.AskProfiles(ProfilesPageAction{.gesture = ProfileGesture::kLoad, .target = "Nuit"});
  elsewhere.Tick(0);
  CHECK(elsewhere.look().body == kRed);
}

TEST_CASE("Habillage du skin : rouvert sur un autre skin, il en prend l'habillage", "[app][skin]") {
  // ⚠ C'EST CE QUE FAIT UN SKIN CHANGÉ EN JEU : l'habillage du premier s'écrit,
  // et celui du second se lit — jamais un mélange des deux.
  TempDirectory first;
  TempDirectory second;
  const std::filesystem::path profiles = first.path() / "profils.yaml";
  SkinLookOptions options;
  options.Open(first.path(), profiles);
  REQUIRE(options.Change(
      OptionsWindowAction{.changed = RowId(SkinLookRow::kPreset), .chosen = kSombre}));
  options.Open(second.path(), profiles);
  CHECK(options.look() == SkinLook{});
  options.Open(first.path(), profiles);
  CHECK(options.look().body == SkinLookPresets()[kSombre].body);
}
