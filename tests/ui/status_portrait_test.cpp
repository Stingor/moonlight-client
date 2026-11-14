// Les règles du portrait de statut qu'un banc peut prouver : ce que le cadre du
// niveau écrit, la disposition d'origine, et ce qu'un fichier édité à la main
// ne peut pas imposer.

#include "ro/ui/status_portrait.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::ui::DefaultPortraitPartRect;
using ro::ui::DefaultPortraitParts;
using ro::ui::LevelFormat;
using ro::ui::LevelFormatOf;
using ro::ui::PortraitLevelText;
using ro::ui::PortraitPart;
using ro::ui::StatusPortraitLook;

namespace {

constexpr std::optional<std::int32_t> kBase = 99;
constexpr std::optional<std::int32_t> kJob = 70;
constexpr std::optional<std::int32_t> kUnknown = std::nullopt;

}  // namespace

TEST_CASE("Portrait : les trois formats du niveau", "[ui][portrait]") {
  CHECK(PortraitLevelText(kBase, kJob, LevelFormat::kSlashed) == "99/70");
  CHECK(PortraitLevelText(kBase, kJob, LevelFormat::kLabelled) == "Base 99 · Job 70");
  CHECK(PortraitLevelText(kBase, kJob, LevelFormat::kBaseOnly) == "99");
}

TEST_CASE("Portrait : une part inconnue ne s'écrit pas", "[ui][portrait]") {
  // Ni zéro ni tiret : un niveau que le serveur n'a pas dit n'est pas un niveau.
  CHECK(PortraitLevelText(kBase, kUnknown, LevelFormat::kSlashed) == "99");
  CHECK(PortraitLevelText(kUnknown, kJob, LevelFormat::kSlashed) == "/70");
  CHECK(PortraitLevelText(kBase, kUnknown, LevelFormat::kLabelled) == "Base 99");
  CHECK(PortraitLevelText(kUnknown, kJob, LevelFormat::kLabelled) == "Job 70");
  CHECK(PortraitLevelText(kUnknown, kJob, LevelFormat::kBaseOnly).empty());
  for (const LevelFormat format :
       {LevelFormat::kSlashed, LevelFormat::kLabelled, LevelFormat::kBaseOnly}) {
    CHECK(PortraitLevelText(kUnknown, kUnknown, format).empty());
  }
}

TEST_CASE("Portrait : un format relu hors bornes retombe sur 99/70", "[ui][portrait]") {
  CHECK(LevelFormatOf(0) == LevelFormat::kSlashed);
  CHECK(LevelFormatOf(1) == LevelFormat::kLabelled);
  CHECK(LevelFormatOf(2) == LevelFormat::kBaseOnly);
  CHECK(LevelFormatOf(-1) == LevelFormat::kSlashed);
  CHECK(LevelFormatOf(3) == LevelFormat::kSlashed);
}

TEST_CASE("Portrait : la disposition d'origine", "[ui][portrait]") {
  const StatusPortraitLook look;
  // ⚠ LES DEUX GROUPES ÉTEINTS PAR DÉFAUT : on les allume pour les poser.
  for (const ro::ui::PortraitGroupLook& group : look.groups) {
    CHECK_FALSE(group.shown);
    CHECK(group.sticky);
    CHECK_FALSE(group.border);
  }
  CHECK_FALSE(look.AnyShown());
  // Le verrou n'est pas au portrait : la vue porte celui des jauges, ouvert
  // tant qu'on ne dit rien.
  CHECK_FALSE(ro::ui::StatusPortraitView().locked);
  CHECK(LevelFormatOf(look.level_format) == LevelFormat::kSlashed);

  const ro::ui::HudRect head = DefaultPortraitPartRect(PortraitPart::kHead);
  CHECK(head.x == 60.0F);
  CHECK(head.y == 60.0F);
  CHECK(head.width == 100.0F);
  CHECK(head.height == 100.0F);
  // Les trois étiquettes s'empilent sous le portrait, sans se chevaucher.
  float below = head.y + head.height;
  for (const PortraitPart part :
       {PortraitPart::kName, PortraitPart::kClass, PortraitPart::kLevel}) {
    const ro::ui::HudRect rect = DefaultPortraitPartRect(part);
    CHECK(rect.y >= below);
    below = rect.y + rect.height;
  }

  const auto parts = DefaultPortraitParts();
  for (const auto& part : parts) {
    CHECK(part.shown);
    CHECK(part.rounding == 4.0F);
    CHECK(part.text_scale == 1.0F);
    CHECK(part.background == IM_COL32(13, 13, 18, 199));
  }
  // Le pseudo en doré, la classe en blanc, le niveau en bleu clair.
  CHECK(parts[static_cast<std::size_t>(PortraitPart::kName)].text == IM_COL32(255, 209, 77, 255));
  CHECK(parts[static_cast<std::size_t>(PortraitPart::kClass)].text == IM_COL32_WHITE);
  CHECK(parts[static_cast<std::size_t>(PortraitPart::kLevel)].text == IM_COL32(92, 199, 255, 255));
}

TEST_CASE("Portrait : deux groupes, le portrait seul et les trois étiquettes", "[ui][portrait]") {
  using ro::ui::PortraitGroup;
  CHECK(ro::ui::PortraitGroupOf(PortraitPart::kHead) == PortraitGroup::kHead);
  for (const PortraitPart part :
       {PortraitPart::kName, PortraitPart::kClass, PortraitPart::kLevel}) {
    CHECK(ro::ui::PortraitGroupOf(part) == PortraitGroup::kLabels);
  }

  // Un cadre paraît quand son groupe ET lui sont allumés, sans rien devoir à
  // l'autre groupe.
  StatusPortraitLook look;
  look.group(PortraitGroup::kLabels).shown = true;
  CHECK(look.AnyShown());
  CHECK_FALSE(look.PartShown(PortraitPart::kHead));
  CHECK(look.PartShown(PortraitPart::kName));
  look.parts[static_cast<std::size_t>(PortraitPart::kName)].shown = false;
  CHECK_FALSE(look.PartShown(PortraitPart::kName));
  CHECK(look.PartShown(PortraitPart::kLevel));

  look.group(PortraitGroup::kLabels).shown = false;
  look.group(PortraitGroup::kHead).shown = true;
  CHECK(look.PartShown(PortraitPart::kHead));
  CHECK_FALSE(look.PartShown(PortraitPart::kLevel));
}
