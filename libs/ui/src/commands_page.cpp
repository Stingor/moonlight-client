// LA PAGE « COMMANDS SETTINGS » : l'onglet « Général » et ce que les deux
// onglets partagent. L'onglet « Autoloots » vit à côté, dans
// `commands_page_autoloot.cpp`.

#include "ro/ui/commands_page.h"

#include <algorithm>
#include <array>
#include <cstring>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "commands_page_parts.h"

namespace ro::ui {
namespace commands_page {

float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

void Toggle(const char* label, bool value, const char* help, CommandsField field,
            const SkinArt& art, CommandsPageAction& asked) {
  bool on = value;
  if (SkinCheckBox(label, &on, art)) {
    asked.field = field;
    asked.value = on ? 1 : 0;
  }
  ImGui::SameLine();
  SkinHelpMarker(help);
}

void FaintText(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

}  // namespace commands_page

namespace {

using commands_page::Px;
using commands_page::Toggle;

// La largeur de la liste de `@noks`, en pixels d'interface.
constexpr float kKillStealComboWidth = 100.0F;

// 🔴 LES ARGUMENTS LITTÉRAUX DE `@noks` CÔTÉ SERVEUR, PAS DES LIBELLÉS : traduits,
// l'interface dirait « Groupe » pour un mode qui s'écrit « party ».
constexpr std::array<const char*, 4> kKillStealLabels = {"Off", "Self", "Party", "Guild"};

void DrawGeneralTab(const CommandsPageView& view, const SkinArt& art, CommandsPageAction& asked) {
  if (ImGui::BeginTable("split", 2)) {
    ImGui::TableNextColumn();
    Toggle(core::Tr("Show EXP gain"), view.show_exp,
           core::Tr("Affiche le gain d'EXP dans le chat log. (@showexp)"), CommandsField::kShowExp,
           art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Show Zeny gain"), view.show_zeny,
           core::Tr("Affiche le gain de Zeny dans le chat log. (@showzeny)"),
           CommandsField::kShowZeny, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Show mob info"), view.show_mob_info,
           core::Tr("Affiche la RACE et l'ÉLÉMENT des monstres,\nsous leur nom. (Thx Doo - "
                    "@showmobinfo)"),
           CommandsField::kShowMobInfo, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Separate Kills"), view.separate_kills,
           core::Tr("Affiche un séparateur dans le chat log entre chaque kill de mobs. (Demandez "
                    "à Spider - @separate)"),
           CommandsField::kSeparateKills, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Block EXP Gain"), view.block_exp,
           core::Tr("Bloque le gain d'EXP. (@blockexp)"), CommandsField::kBlockExp, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Show Skill Delay"), view.show_skill_delay,
           core::Tr("Affiche un message dans le chat quand un skill\néchoue à cause du cooldown. "
                    "(@showdelay)"),
           CommandsField::kShowSkillDelay, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Show Speed"), view.show_speed,
           core::Tr("Affiche la valeur de vitesse de déplacement et d'attaque\ndans le chat lors "
                    "d'un changement comme après\nun buff style AgiUP ou Card. (@showspeed)"),
           CommandsField::kShowSpeed, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Sell Stuff"), view.sell_stuff,
           core::Tr("Permet la vente d'items améliorés (refine > 0),\ncartes, munitions et items "
                    "slotés chez les PNJ marchands.\nDésactiver pour protéger ces items. "
                    "(@sellstuff)"),
           CommandsField::kSellStuff, art, asked);
    ImGui::TableNextColumn();
    Toggle(
        core::Tr("Sell Item"), view.sell_item,
        core::Tr("Permet la vente des items du groupe IG_SELLITEM chez les PNJ marchands.\n"
                 "Désactiver pour les protéger. (@sellitem)\n\n"
                 "Groupe SELLITEM :\nGreen Potion (506)\nWhite Slim Potion (547)\nLucky Candy "
                 "(570)\n"
                 "Old Blue Box (603)\nYggdrasil Berry (607)\nYggdrasil Seed (608)\nOld Card Album "
                 "(616)\n"
                 "Old Violet Box (617)\nGift Box (644)\nPoison Bottle (678)\nGold (969)\n"
                 "Temporal Crystal (6607)\nCoagulated Spell (6608)\nJitterbug's Tooth (6719)\n"
                 "Fire Bottle (7135)\nAcid Bottle (7136)\nCoating Bottle (7139)\n"
                 "Fragment of Agony (7436)\nFragment of Misery (7437)\nFragment of Hatred "
                 "(7438)\nPiece of Memory Red (7439)\n"
                 "Ice Scale (7562)\nCursed Water (12020)\nElemental Converter Fire "
                 "(12114)\nElemental Converter Water (12115)\n"
                 "Elemental Converter Earth (12116)\nElemental Converter Wind (12117)\nMystical "
                 "Card Album (12246)"),
        CommandsField::kSellItem, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("No Ask"), view.no_ask,
           core::Tr("Refuse automatiquement les invitations\nde trade, de guilde et d'alliance. "
                    "(@noask)"),
           CommandsField::kNoAsk, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Wings"), view.wings,
           core::Tr("Active ou désactive le sprite alternatif des Angel wings et Devil wings "
                    "(Moonlight 2005 vibe - @wings)"),
           CommandsField::kWings, art, asked);
    ImGui::TableNextColumn();
    Toggle(core::Tr("Fly Wing marker"), view.fly_wing_marker,
           core::Tr("Marque sur la carte le point d'arrivée après une Fly Wing\nou un Téléport "
                    "niveau 1. (@flywinglast)"),
           CommandsField::kFlyWingMarker, art, asked);
    ImGui::EndTable();
  }

  // `@noks` : quatre modes, dans l'ordre du fil.
  const int shown =
      std::clamp(view.kill_steal_guard, 0, static_cast<int>(kKillStealLabels.size()) - 1);
  ImGui::SetNextItemWidth(Px(kKillStealComboWidth));
  int step = 0;
  const bool open =
      BeginSkinCombo("@noks", kKillStealLabels[static_cast<std::size_t>(shown)], art, &step);
  if (const int to = SkinComboStepped(shown, step, static_cast<int>(kKillStealLabels.size()));
      to != shown) {
    asked.field = CommandsField::kKillStealGuard;
    asked.value = to;
  }
  if (open) {
    for (int rank = 0; rank < static_cast<int>(kKillStealLabels.size()); ++rank) {
      const bool selected = rank == shown;
      if (ImGui::Selectable(kKillStealLabels[static_cast<std::size_t>(rank)], selected) &&
          !selected) {
        asked.field = CommandsField::kKillStealGuard;
        asked.value = rank;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    EndSkinCombo();
  }
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Kill Steal Protection — empêche d'autres joueurs de voler vos kills MVP.\nSelf = "
               "toi seulement, Party = ta party, Guild = ta guilde. (@noks)"));
}

}  // namespace

CommandsPageAction DrawCommandsPage(const CommandsPageView& view, CommandsPageState& state,
                                    const SkinArt& art) {
  CommandsPageAction asked;
  // ⚠ UNE PAGE MUETTE SE DIT : grisée sans un mot, elle se lirait comme cassée.
  if (!view.told) {
    commands_page::FaintText(
        core::Tr("Ces réglages sont ceux que le serveur tient pour ton personnage : ils "
                 "s'activent une fois en jeu."));
  }
  ImGui::BeginDisabled(!view.told);
  if (BeginSkinTabBar("CommandsSettingsTabs", art)) {
    if (ImGui::BeginTabItem(core::Tr("Général"))) {
      DrawGeneralTab(view, art, asked);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(core::Tr("Autoloots"))) {
      commands_page::DrawAutolootTab(view, state, art, asked);
      ImGui::EndTabItem();
    }
    EndSkinTabBar();
  }
  ImGui::EndDisabled();
  return asked;
}

int ClampAutolootMinZeny(int zeny) {
  const int bounded = std::clamp(zeny, 0, kCommandsMinZenyHighest);
  return bounded / kCommandsMinZenyStep * kCommandsMinZenyStep;
}

std::uint8_t PresetSlotForName(std::span<const CommandsPresetView> presets, const char* name) {
  std::array<bool, kCommandsPresetSlots + 1> used{};
  for (const CommandsPresetView& preset : presets) {
    if (std::strcmp(preset.name, name) == 0) {
      return preset.slot;
    }
    if (preset.slot <= kCommandsPresetSlots) {
      used[preset.slot] = true;
    }
  }
  for (std::uint8_t slot = 1; slot <= kCommandsPresetSlots; ++slot) {
    if (!used[slot]) {
      return slot;
    }
  }
  return 0;
}

}  // namespace ro::ui
