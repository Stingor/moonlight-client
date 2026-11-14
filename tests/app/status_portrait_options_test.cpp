// LES ONGLETS « PORTRAIT » ET « ÉTIQUETTES » : leurs défauts, leurs lignes,
// leurs bornes, et la plage d'identifiants qu'ils revendiquent.

#include "options/status_portrait_options.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "game/settings_file.h"
#include "support/temp_directory.h"

using moonclient::BoundedFraming;
using moonclient::SettingsFile;
using ro::test_support::TempDirectory;
using ro::ui::LevelFormat;
using ro::ui::LevelFormatOf;

using moonclient::kBasicInfoTabCount;
using moonclient::kLabelsTab;
using moonclient::kPortraitAnimationCount;
using moonclient::kPortraitDefaultAnimation;
using moonclient::kPortraitOffsetMost;
using moonclient::kPortraitPartRowBase;
using moonclient::kPortraitRowBase;
using moonclient::kPortraitTab;
using moonclient::kPortraitZoomLeast;
using moonclient::kPortraitZoomMost;
using moonclient::PortraitPartField;
using moonclient::PortraitPartRowId;
using moonclient::PortraitRow;
using moonclient::PortraitRowId;
using moonclient::PortraitTab;
using moonclient::StatusPortraitOptions;
using ro::ui::OptionRowView;
using ro::ui::OptionsWindowAction;
using ro::ui::PortraitGroup;
using ro::ui::PortraitPart;

namespace {

constexpr bool kUnlocked = false;
constexpr bool kLocked = true;

// Les quatre lignes qui ouvrent chaque onglet, dans l'ordre.
const std::vector<int> kPortraitGroupRows = {
    PortraitRowId(PortraitRow::kShown), PortraitRowId(PortraitRow::kSticky),
    PortraitRowId(PortraitRow::kBorder), PortraitRowId(PortraitRow::kResetLayout)};
const std::vector<int> kLabelsGroupRows = {
    PortraitRowId(PortraitRow::kLabelsShown), PortraitRowId(PortraitRow::kLabelsSticky),
    PortraitRowId(PortraitRow::kLabelsBorder), PortraitRowId(PortraitRow::kLabelsResetLayout)};

[[nodiscard]] OptionsWindowAction Asked(int row) {
  OptionsWindowAction asked;
  asked.changed = row;
  return asked;
}

[[nodiscard]] OptionsWindowAction Switched(PortraitRow row, bool on) {
  OptionsWindowAction asked = Asked(PortraitRowId(row));
  asked.on = on;
  return asked;
}

[[nodiscard]] const OptionRowView* Row(std::span<const OptionRowView> rows, int id) {
  const auto found = std::ranges::find(rows, id, &OptionRowView::id);
  return found != rows.end() ? &*found : nullptr;
}

[[nodiscard]] std::vector<OptionRowView> Composed(StatusPortraitOptions& page, PortraitTab tab,
                                                  bool locked = kUnlocked) {
  page.Compose(tab, locked);
  return {page.rows().begin(), page.rows().end()};
}

[[nodiscard]] std::size_t PartRank(PortraitPart part) { return static_cast<std::size_t>(part); }

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream in(path);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("Portrait : les réglages d'origine", "[app][options][portrait]") {
  // ⚠ LES DÉFAUTS D'ORIGINE, ceux des structures : la disposition que le jeu
  // livre se passe au constructeur, voir `factory_layout_test.cpp`.
  const StatusPortraitOptions page;
  CHECK_FALSE(page.look().AnyShown());
  CHECK(page.framing().head_only);
  CHECK(page.framing().zoom == 1.0F);
  CHECK(page.framing().offset_x == 0.0F);
  CHECK(page.framing().offset_y == 0.0F);
  // La garde, de face, animée, cape montrée en personnage entier.
  CHECK(page.framing().animation == kPortraitDefaultAnimation);
  CHECK(page.framing().direction == 0);
  CHECK(page.framing().animated);
  CHECK(page.framing().garment);
}

TEST_CASE("Portrait : une valeur hors bornes est ramenée", "[app][options][portrait]") {
  StatusPortraitOptions page;
  OptionsWindowAction asked = Asked(PortraitRowId(PortraitRow::kZoom));
  asked.value = 9.0F;
  REQUIRE(page.Change(asked));
  CHECK(page.framing().zoom == kPortraitZoomMost);
  asked.value = 0.0F;
  REQUIRE(page.Change(asked));
  CHECK(page.framing().zoom == kPortraitZoomLeast);

  asked = Asked(PortraitRowId(PortraitRow::kOffsetY));
  asked.value = -4.0F;
  REQUIRE(page.Change(asked));
  CHECK(page.framing().offset_y == -kPortraitOffsetMost);

  asked = Asked(PortraitRowId(PortraitRow::kAnimation));
  asked.chosen = 40;
  REQUIRE(page.Change(asked));
  CHECK(page.framing().animation == kPortraitAnimationCount - 1);

  asked = Asked(PortraitRowId(PortraitRow::kDirection));
  asked.chosen = -1;
  REQUIRE(page.Change(asked));
  CHECK(page.framing().direction == 7);

  asked = Asked(PortraitPartRowId(PortraitPart::kLevel, PortraitPartField::kTextScale));
  asked.value = 10.0F;
  REQUIRE(page.Change(asked));
  CHECK(page.look().parts[PartRank(PortraitPart::kLevel)].text_scale ==
        ro::ui::kPortraitTextScaleMost);
}

TEST_CASE("Portrait : chaque réinitialisation repose les cadres de son groupe, et rien d'autre",
          "[app][options][portrait]") {
  StatusPortraitOptions page;
  auto& head = page.look().parts[PartRank(PortraitPart::kHead)];
  auto& name = page.look().parts[PartRank(PortraitPart::kName)];
  auto& level = page.look().parts[PartRank(PortraitPart::kLevel)];
  head.rect.x = 400.0F;
  name.rect.x = 500.0F;
  level.rect.y = 600.0F;
  name.text = IM_COL32(1, 2, 3, 255);

  // Celle du portrait ne touche pas aux étiquettes.
  REQUIRE(page.Change(Asked(PortraitRowId(PortraitRow::kResetLayout))));
  CHECK(head.rect.x == ro::ui::DefaultPortraitPartRect(PortraitPart::kHead).x);
  CHECK(name.rect.x == 500.0F);
  CHECK(level.rect.y == 600.0F);

  // Celle des étiquettes les repose toutes trois, sans toucher au portrait ni
  // aux couleurs.
  head.rect.x = 400.0F;
  REQUIRE(page.Change(Asked(PortraitRowId(PortraitRow::kLabelsResetLayout))));
  CHECK(name.rect.x == ro::ui::DefaultPortraitPartRect(PortraitPart::kName).x);
  CHECK(level.rect.y == ro::ui::DefaultPortraitPartRect(PortraitPart::kLevel).y);
  CHECK(name.text == IM_COL32(1, 2, 3, 255));
  CHECK(head.rect.x == 400.0F);
}

TEST_CASE("Portrait : la page ne prend que ses lignes", "[app][options][portrait]") {
  StatusPortraitOptions page;
  CHECK_FALSE(page.Change(Asked(kPortraitRowBase - 1)));
  CHECK_FALSE(page.Change(Asked(kPortraitRowBase + static_cast<int>(PortraitRow::kCount))));
  CHECK_FALSE(
      page.Change(Asked(kPortraitPartRowBase + static_cast<int>(ro::ui::kPortraitPartCount) *
                                                   moonclient::kPortraitFieldsPerPart)));
  CHECK(page.Change(Asked(PortraitPartRowId(PortraitPart::kName, PortraitPartField::kShown))));
}

TEST_CASE("Portrait : les deux interrupteurs sont indépendants", "[app][options][portrait]") {
  StatusPortraitOptions page;
  REQUIRE(page.Change(Switched(PortraitRow::kShown, true)));
  CHECK(page.look().group(PortraitGroup::kHead).shown);
  CHECK_FALSE(page.look().group(PortraitGroup::kLabels).shown);

  REQUIRE(page.Change(Switched(PortraitRow::kLabelsShown, true)));
  REQUIRE(page.Change(Switched(PortraitRow::kShown, false)));
  CHECK_FALSE(page.look().group(PortraitGroup::kHead).shown);
  CHECK(page.look().group(PortraitGroup::kLabels).shown);

  // L'aimant et la bordure de même.
  REQUIRE(page.Change(Switched(PortraitRow::kSticky, false)));
  REQUIRE(page.Change(Switched(PortraitRow::kLabelsBorder, true)));
  CHECK_FALSE(page.look().group(PortraitGroup::kHead).sticky);
  CHECK(page.look().group(PortraitGroup::kLabels).sticky);
  CHECK_FALSE(page.look().group(PortraitGroup::kHead).border);
  CHECK(page.look().group(PortraitGroup::kLabels).border);
}

TEST_CASE("Portrait : un interrupteur éteint grise son onglet, et lui seul",
          "[app][options][portrait]") {
  StatusPortraitOptions page;
  page.look().group(PortraitGroup::kLabels).shown = true;

  // Le portrait éteint : tout son onglet se grise, sauf son interrupteur.
  const std::vector<OptionRowView> portrait = Composed(page, PortraitTab::kPortrait);
  REQUIRE_FALSE(portrait.empty());
  CHECK(portrait.front().id == PortraitRowId(PortraitRow::kShown));
  CHECK_FALSE(portrait.front().disabled);
  for (std::size_t rank = 1; rank < portrait.size(); ++rank) {
    CHECK(portrait[rank].disabled);
  }
  // Les étiquettes allumées : rien n'y est grisé par lui.
  const std::vector<OptionRowView> labels = Composed(page, PortraitTab::kLabels);
  for (const int id : kLabelsGroupRows) {
    const OptionRowView* row = Row(labels, id);
    REQUIRE(row != nullptr);
    CHECK_FALSE(row->disabled);
  }
  const OptionRowView* level =
      Row(labels, PortraitPartRowId(PortraitPart::kLevel, PortraitPartField::kShown));
  REQUIRE(level != nullptr);
  CHECK_FALSE(level->disabled);

  // Et l'inverse.
  page.look().group(PortraitGroup::kLabels).shown = false;
  page.look().group(PortraitGroup::kHead).shown = true;
  const std::vector<OptionRowView> dark = Composed(page, PortraitTab::kLabels);
  CHECK_FALSE(dark.front().disabled);
  for (std::size_t rank = 1; rank < dark.size(); ++rank) {
    CHECK(dark[rank].disabled);
  }
  const std::vector<OptionRowView> lit = Composed(page, PortraitTab::kPortrait);
  const OptionRowView* zoom = Row(lit, PortraitRowId(PortraitRow::kZoom));
  REQUIRE(zoom != nullptr);
  CHECK_FALSE(zoom->disabled);
}

TEST_CASE("Portrait : le verrou de la page grise l'aimant des deux onglets",
          "[app][options][portrait]") {
  StatusPortraitOptions page;
  for (ro::ui::PortraitGroupLook& group : page.look().groups) {
    group.shown = true;
  }
  const std::vector<OptionRowView> open = Composed(page, PortraitTab::kPortrait, kUnlocked);
  const std::vector<OptionRowView> shut = Composed(page, PortraitTab::kPortrait, kLocked);
  const std::vector<OptionRowView> open_labels = Composed(page, PortraitTab::kLabels, kUnlocked);
  const std::vector<OptionRowView> shut_labels = Composed(page, PortraitTab::kLabels, kLocked);
  CHECK_FALSE(Row(open, PortraitRowId(PortraitRow::kSticky))->disabled);
  CHECK(Row(shut, PortraitRowId(PortraitRow::kSticky))->disabled);
  CHECK_FALSE(Row(open_labels, PortraitRowId(PortraitRow::kLabelsSticky))->disabled);
  CHECK(Row(shut_labels, PortraitRowId(PortraitRow::kLabelsSticky))->disabled);
  // Le reste ne se fige pas : on règle une couleur cadres verrouillés.
  CHECK_FALSE(Row(shut, PortraitRowId(PortraitRow::kBorder))->disabled);
  CHECK_FALSE(Row(shut_labels, PortraitRowId(PortraitRow::kLabelsResetLayout))->disabled);
}

TEST_CASE("Portrait : les deux onglets ferment la page, après les jauges",
          "[app][options][portrait]") {
  CHECK(kPortraitTab == ro::ui::kStatBarCount);
  CHECK(kLabelsTab == kPortraitTab + 1);
  CHECK(kBasicInfoTabCount == kLabelsTab + 1);
}

TEST_CASE("Portrait : chaque onglet s'ouvre sur les réglages de son groupe",
          "[app][options][portrait]") {
  StatusPortraitOptions page;
  const auto opens_on = [](const std::vector<OptionRowView>& rows, const std::vector<int>& head) {
    REQUIRE(rows.size() > head.size());
    for (std::size_t rank = 0; rank < head.size(); ++rank) {
      CHECK(rows[rank].id == head[rank]);
    }
  };
  opens_on(Composed(page, PortraitTab::kPortrait), kPortraitGroupRows);
  opens_on(Composed(page, PortraitTab::kLabels), kLabelsGroupRows);
}

TEST_CASE("Portrait : chaque onglet porte ses lignes, et rien de l'autre",
          "[app][options][portrait]") {
  StatusPortraitOptions page;
  const std::vector<OptionRowView> portrait = Composed(page, PortraitTab::kPortrait);
  const std::vector<OptionRowView> labels = Composed(page, PortraitTab::kLabels);

  // 🔴 AUCUNE LIGNE DANS LES DEUX : la recherche, qui compose tous les
  // onglets, ne montre rien deux fois.
  for (const OptionRowView& row : portrait) {
    CHECK(Row(labels, row.id) == nullptr);
  }

  // Le personnage et son cadre.
  for (const PortraitRow row :
       {PortraitRow::kHeadOnly, PortraitRow::kGarment, PortraitRow::kZoom, PortraitRow::kOffsetX,
        PortraitRow::kOffsetY, PortraitRow::kAnimation, PortraitRow::kDirection,
        PortraitRow::kAnimated}) {
    CHECK(Row(portrait, PortraitRowId(row)) != nullptr);
  }
  for (const PortraitPartField field :
       {PortraitPartField::kBackground, PortraitPartField::kRounding}) {
    CHECK(Row(portrait, PortraitPartRowId(PortraitPart::kHead, field)) != nullptr);
  }
  // Le portrait n'a ni interrupteur à lui — c'est celui du groupe —, ni couleur
  // de texte, ni taille.
  for (const PortraitPartField field :
       {PortraitPartField::kShown, PortraitPartField::kText, PortraitPartField::kTextScale}) {
    CHECK(Row(portrait, PortraitPartRowId(PortraitPart::kHead, field)) == nullptr);
  }

  // Le format du niveau, puis les trois étiquettes, chacune entière.
  CHECK(Row(labels, PortraitRowId(PortraitRow::kLevelFormat)) != nullptr);
  for (const PortraitPart part :
       {PortraitPart::kName, PortraitPart::kClass, PortraitPart::kLevel}) {
    for (int field = 0; field < moonclient::kPortraitFieldsPerPart; ++field) {
      CHECK(Row(labels, PortraitPartRowId(part, static_cast<PortraitPartField>(field))) != nullptr);
    }
  }
}

TEST_CASE("Portrait : les réglages font l'aller-retour par le fichier",
          "[app][options][portrait]") {
  const TempDirectory folder;
  const std::filesystem::path path = folder.path() / "settings.yml";
  {
    StatusPortraitOptions page;
    SettingsFile file(path);
    page.Declare(file);
    OptionsWindowAction asked = Asked(PortraitRowId(PortraitRow::kZoom));
    asked.value = 1.5F;
    REQUIRE(page.Change(asked));
    REQUIRE(page.Change(Switched(PortraitRow::kHeadOnly, false)));
    asked = Asked(PortraitRowId(PortraitRow::kLevelFormat));
    asked.chosen = static_cast<int>(LevelFormat::kLabelled);
    REQUIRE(page.Change(asked));
    REQUIRE(page.Change(Switched(PortraitRow::kLabelsShown, true)));
    REQUIRE(page.Change(Switched(PortraitRow::kLabelsSticky, false)));
    REQUIRE(page.Change(Switched(PortraitRow::kLabelsBorder, true)));
    REQUIRE(page.Change(Switched(PortraitRow::kBorder, true)));
    page.look().parts[PartRank(PortraitPart::kClass)].rect.x = 321.0F;
    file.Touch();
    file.Flush();
  }
  const std::string written = Contents(path);
  CHECK(written.find("labels_shown: true") != std::string::npos);
  CHECK(written.find("labels_snapped: false") != std::string::npos);
  CHECK(written.find("labels_border: true") != std::string::npos);

  StatusPortraitOptions page;
  SettingsFile file(path);
  page.Declare(file);
  file.Load();
  CHECK(page.framing().zoom == 1.5F);
  CHECK_FALSE(page.framing().head_only);
  CHECK(LevelFormatOf(page.look().level_format) == LevelFormat::kLabelled);
  CHECK(page.look().parts[PartRank(PortraitPart::kClass)].rect.x == 321.0F);
  const ro::ui::PortraitGroupLook& head = page.look().group(PortraitGroup::kHead);
  const ro::ui::PortraitGroupLook& labels = page.look().group(PortraitGroup::kLabels);
  CHECK_FALSE(head.shown);
  CHECK(head.sticky);
  CHECK(head.border);
  CHECK(labels.shown);
  CHECK_FALSE(labels.sticky);
  CHECK(labels.border);
}

TEST_CASE("Portrait : le verrou et l'interrupteur du cadre du portrait quittent le fichier",
          "[app][options][portrait]") {
  const TempDirectory folder;
  // Écrit par une version où le portrait avait son verrou, et son cadre son
  // interrupteur à lui.
  folder.WriteText("settings.yml",
                   "portrait_shown: true\n"
                   "portrait_locked: true\n"
                   "portrait_head_shown: false\n");
  StatusPortraitOptions page;
  SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  file.Load();
  // Le cadre du portrait s'allume par son groupe, quoi qu'en dise le fichier.
  CHECK(page.look().PartShown(PortraitPart::kHead));
  file.Flush();
  const std::string written = Contents(folder.path() / "settings.yml");
  CHECK(written.find("portrait_locked") == std::string::npos);
  CHECK(written.find("portrait_head_shown") == std::string::npos);
  CHECK(written.find("portrait_shown: true") != std::string::npos);
}

TEST_CASE("Portrait : une valeur relue hors bornes retombe dans les bornes",
          "[app][options][portrait]") {
  const TempDirectory folder;
  folder.WriteText("settings.yml",
                   "portrait_zoom: 50\n"
                   "portrait_offset_x: -9\n"
                   "portrait_offset_y: 9\n"
                   "portrait_action: 40\n"
                   "portrait_orientation: -3\n"
                   "portrait_level_format: 7\n");
  StatusPortraitOptions page;
  SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  file.Load();
  // Le fichier a bien été lu : la valeur brute est là…
  REQUIRE(page.framing().zoom == 50.0F);
  // … et ce que la vignette lit est borné.
  const moonclient::PortraitFramingSettings bounded = BoundedFraming(page.framing());
  CHECK(bounded.zoom == kPortraitZoomMost);
  CHECK(bounded.offset_x == -kPortraitOffsetMost);
  CHECK(bounded.offset_y == kPortraitOffsetMost);
  CHECK(bounded.animation == kPortraitAnimationCount - 1);
  // La direction boucle : trois crans avant la face.
  CHECK(bounded.direction == 5);
  CHECK(LevelFormatOf(page.look().level_format) == LevelFormat::kSlashed);
}
