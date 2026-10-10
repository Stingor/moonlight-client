#include "options/monster_sheet_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kAnimateKey = "sheet_monster_animate";
constexpr const char* kGuardiansKey = "sheet_monster_guardians";

[[nodiscard]] OptionRowView Toggle(MonsterSheetRow row, const char* label, const char* help,
                                   bool on) {
  return OptionRowView{.id = MonsterSheetRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

}  // namespace

void MonsterSheetOptions::Declare(SettingsFile& into) {
  into.Declare(kAnimateKey, &animate_);
  into.Declare(kGuardiansKey, &show_guardians_);
}

void MonsterSheetOptions::Compose() {
  rows_.clear();
  rows_.push_back(OptionRowView{
      .id = MonsterSheetRowId(MonsterSheetRow::kIntro),
      .label = Tr("Remplace la fenêtre « Monster Info » qu'ouvre le skill Sense. Elle ajoute "
                  "ce que le paquet du skill ne transporte pas : nom fiable, EXP, ATK/MATK, stats "
                  "de base, modes, drops, lieux d'apparition et skills."),
      .kind = OptionKind::kNote});
  rows_.push_back(Toggle(MonsterSheetRow::kAnimate, Tr("Animer le sprite du monstre"),
                         Tr("La fenêtre native n'affiche que la première image de l'animation "
                            "d'attente.\nDécoché, on la reproduit à l'identique."),
                         animate_));
  rows_.push_back(Toggle(MonsterSheetRow::kGuardians, Tr("Afficher les Gardiens de forteresse"),
                         Tr("Le client d'origine refuse d'ouvrir la fiche des cinq Gardiens\n"
                            "(Archer, Knight, Soldier, Sword, Bow) : leurs PV resteraient cachés\n"
                            "pendant la Guerre d'Emperium. Coché, la fiche s'ouvre quand même."),
                         show_guardians_));
}

bool MonsterSheetOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kMonsterSheetRowBase ||
      asked.changed >= kMonsterSheetRowBase + static_cast<int>(MonsterSheetRow::kCount)) {
    return false;
  }
  switch (static_cast<MonsterSheetRow>(asked.changed - kMonsterSheetRowBase)) {
    case MonsterSheetRow::kAnimate:
      animate_ = asked.on;
      break;
    case MonsterSheetRow::kGuardians:
      show_guardians_ = asked.on;
      break;
    case MonsterSheetRow::kIntro:
    case MonsterSheetRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
