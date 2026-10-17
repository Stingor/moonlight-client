#include "chat/setting_toggle_command.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/ui/entity_bars.h"

#include "options/game_option_ids.h"
#include "options/graphics_options.h"

namespace moonclient {
namespace {

using ro::core::Tr;

struct Word {
  std::string_view word;
  SettingToggle toggle;
};

// LES MOTS DE CHAQUE BASCULE. Le premier est son nom dans le client du joueur ;
// ceux qui suivent sont ses doubles — `/dpaqmffpa` et `/doaqmffpa` sont
// « /엠블렘 » et « /앰블렘 » tapés sur un clavier coréen resté en mode latin.
constexpr auto kWords = std::to_array<Word>({
    {"/monsterhp", SettingToggle::kMonsterHp},
    {"/emblem", SettingToggle::kGuildEmblem},
    {"/dpaqmffpa", SettingToggle::kGuildEmblem},
    {"/doaqmffpa", SettingToggle::kGuildEmblem},
    {"/window", SettingToggle::kWindowSnap},
    {"/wi", SettingToggle::kWindowSnap},
    {"/miss", SettingToggle::kMissShown},
    {"/skillfail", SettingToggle::kSkillFailShown},
    {"/sf", SettingToggle::kSkillFailShown},
    {"/quake", SettingToggle::kScreenQuake},
});

constexpr char kSpace = ' ';

[[nodiscard]] ro::ui::OptionsWindowAction Ticked(int row_id, bool on) {
  return ro::ui::OptionsWindowAction{.changed = row_id, .on = on};
}

[[nodiscard]] int EntityBarShownRow(ro::ui::EntityBarKind kind) {
  return EntityBarRow(static_cast<std::size_t>(kind), EntityBarField::kShown);
}

}  // namespace

std::optional<SettingToggle> ReadSettingToggle(std::string_view line) {
  const std::size_t start = line.find_first_not_of(kSpace);
  if (start == std::string_view::npos) {
    return std::nullopt;
  }
  line.remove_prefix(start);
  const std::string_view first = line.substr(0, line.find(kSpace));
  const auto found = std::ranges::find_if(
      kWords, [&](const Word& known) { return ro::core::AsciiEqualsFolded(first, known.word); });
  if (found == kWords.end()) {
    return std::nullopt;
  }
  return found->toggle;
}

std::vector<ro::ui::OptionsWindowAction> SettingToggleChanges(SettingToggle toggle, bool on) {
  switch (toggle) {
    case SettingToggle::kMonsterHp:
      // LES DEUX ESPÈCES DE MONSTRE : le client du joueur ne fait pas d'un boss
      // une espèce à part, et sa barre se tait avec celle des autres.
      return {Ticked(EntityBarShownRow(ro::ui::EntityBarKind::kMonster), on),
              Ticked(EntityBarShownRow(ro::ui::EntityBarKind::kBoss), on)};
    case SettingToggle::kGuildEmblem:
      return {Ticked(static_cast<int>(GameOption::kEntityNamesEmblem), on)};
    case SettingToggle::kWindowSnap:
      return {Ticked(static_cast<int>(GameOption::kAlignGridMagnet), on)};
    case SettingToggle::kMissShown:
      return {Ticked(static_cast<int>(GameOption::kDamageMiss), on)};
    case SettingToggle::kSkillFailShown:
      return {Ticked(static_cast<int>(GameOption::kSkillFailShown), on)};
    case SettingToggle::kScreenQuake:
      return {Ticked(GraphicsRowId(GraphicsRow::kScreenQuake), on)};
  }
  return {};
}

const char* SettingToggleEcho(SettingToggle toggle, bool on) {
  switch (toggle) {
    case SettingToggle::kMonsterHp:
      return on ? Tr("La barre de vie d'un monstre attaqué s'affiche.")
                : Tr("La barre de vie d'un monstre attaqué ne s'affiche plus.");
    case SettingToggle::kGuildEmblem:
      return on ? Tr("Emblème de guilde affiché") : Tr("Emblème de guilde masqué");
    case SettingToggle::kWindowSnap:
      return on ? Tr("Aimantation des fenêtres activée. [/wi ON]")
                : Tr("Aimantation des fenêtres désactivée. [/wi OFF]");
    case SettingToggle::kMissShown:
      return on ? Tr("Affichage de « Miss » activé") : Tr("Affichage de « Miss » désactivé");
    case SettingToggle::kSkillFailShown:
      return on ? Tr("Les messages d'échec de skill seront affichés. [Affichage ON]")
                : Tr("Les messages d'échec de skill ne seront plus affichés. [Affichage OFF]");
    case SettingToggle::kScreenQuake:
      return on ? Tr("Tremblements d'écran activés") : Tr("Tremblements d'écran désactivés");
  }
  return "";
}

}  // namespace moonclient
