// LA PAGE « COMMANDS SETTINGS » : l'onglet « Général » et ce que les deux
// onglets partagent. L'onglet « Autoloots » vit à côté, dans
// `commands_page_autoloot.cpp`.

#include "ro/ui/commands_page.h"

#include <algorithm>
#include <array>
#include <cstring>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "commands_page_parts.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace commands_page {

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

}  // namespace commands_page

namespace {

using commands_page::Toggle;

// La largeur de la liste de `@noks`, en pixels d'interface.
constexpr float kKillStealComboWidth = 100.0F;

// 🔴 LES ARGUMENTS LITTÉRAUX DE `@noks` CÔTÉ SERVEUR, PAS DES LIBELLÉS : traduits,
// l'interface dirait « Groupe » pour un mode qui s'écrit « party ».
constexpr std::array<const char*, 4> kKillStealLabels = {"Off", "Self", "Party", "Guild"};

// UNE CASE DE L'ONGLET « GÉNÉRAL » : son libellé et son aide, marqués pour le
// catalogue et traduits en les lisant, la commande de chat qu'elle remplace, le
// réglage qu'elle demande et le champ de la vue qui la coche.
//
// ⚠ UNE TABLE, ET NON DES APPELS EN FILE : la recherche du panneau lit les mêmes
// libellés que le dessin. Deux listes divergeraient au premier ajout, et la
// case nouvelle ne se trouverait pas.
struct GeneralToggle {
  const char* label;
  const char* help;
  const char* command;
  CommandsField field;
  bool CommandsPageView::* on;
};

constexpr std::array<GeneralToggle, 12> kGeneralToggles = {{
    {core::TrText("Show EXP gain"),
     core::TrText("Affiche le gain d'EXP dans le chat log. (@showexp)"), "@showexp",
     CommandsField::kShowExp, &CommandsPageView::show_exp},
    {core::TrText("Show Zeny gain"),
     core::TrText("Affiche le gain de Zeny dans le chat log. (@showzeny)"), "@showzeny",
     CommandsField::kShowZeny, &CommandsPageView::show_zeny},
    {core::TrText("Show mob info"),
     core::TrText("Affiche la RACE et l'ÉLÉMENT des monstres,\nsous leur nom. (Thx Doo - "
                  "@showmobinfo)"),
     "@showmobinfo", CommandsField::kShowMobInfo, &CommandsPageView::show_mob_info},
    {core::TrText("Separate Kills"),
     core::TrText("Affiche un séparateur dans le chat log entre chaque kill de mobs. (Demandez "
                  "à Spider - @separate)"),
     "@separate", CommandsField::kSeparateKills, &CommandsPageView::separate_kills},
    {core::TrText("Block EXP Gain"), core::TrText("Bloque le gain d'EXP. (@blockexp)"), "@blockexp",
     CommandsField::kBlockExp, &CommandsPageView::block_exp},
    {core::TrText("Show Skill Delay"),
     core::TrText("Affiche un message dans le chat quand un skill\néchoue à cause du cooldown. "
                  "(@showdelay)"),
     "@showdelay", CommandsField::kShowSkillDelay, &CommandsPageView::show_skill_delay},
    {core::TrText("Show Speed"),
     core::TrText("Affiche la valeur de vitesse de déplacement et d'attaque\ndans le chat lors "
                  "d'un changement comme après\nun buff style AgiUP ou Card. (@showspeed)"),
     "@showspeed", CommandsField::kShowSpeed, &CommandsPageView::show_speed},
    {core::TrText("Sell Stuff"),
     core::TrText("Permet la vente d'items améliorés (refine > 0),\ncartes, munitions et items "
                  "slotés chez les PNJ marchands.\nDésactiver pour protéger ces items. "
                  "(@sellstuff)"),
     "@sellstuff", CommandsField::kSellStuff, &CommandsPageView::sell_stuff},
    {core::TrText("Sell Item"),
     core::TrText("Permet la vente des items du groupe IG_SELLITEM chez les PNJ marchands.\n"
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
     "@sellitem", CommandsField::kSellItem, &CommandsPageView::sell_item},
    {core::TrText("No Ask"),
     core::TrText("Refuse automatiquement les invitations\nde trade, de guilde et d'alliance. "
                  "(@noask)"),
     "@noask", CommandsField::kNoAsk, &CommandsPageView::no_ask},
    {core::TrText("Wings"),
     core::TrText("Active ou désactive le sprite alternatif des Angel wings et Devil wings "
                  "(Moonlight 2005 vibe - @wings)"),
     "@wings", CommandsField::kWings, &CommandsPageView::wings},
    {core::TrText("Fly Wing marker"),
     core::TrText("Marque sur la carte le point d'arrivée après une Fly Wing\nou un Téléport "
                  "niveau 1. (@flywinglast)"),
     "@flywinglast", CommandsField::kFlyWingMarker, &CommandsPageView::fly_wing_marker},
}};

// Les noms des deux onglets, marqués : la recherche les lit aussi.
constexpr const char* kGeneralTabLabel = core::TrText("Général");
constexpr const char* kAutolootTabLabel = core::TrText("Autoloots");

// La commande de la protection des kills : c'est aussi le libellé de sa liste.
constexpr const char* kKillStealCommand = "@noks";

void DrawGeneralTab(const CommandsPageView& view, const SkinArt& art, CommandsPageAction& asked) {
  if (ImGui::BeginTable("split", 2)) {
    for (const GeneralToggle& toggle : kGeneralToggles) {
      ImGui::TableNextColumn();
      Toggle(commands_page::Translated(toggle.label), view.*toggle.on,
             commands_page::Translated(toggle.help), toggle.field, art, asked);
    }
    ImGui::EndTable();
  }

  // `@noks` : quatre modes, dans l'ordre du fil.
  const int shown =
      std::clamp(view.kill_steal_guard, 0, static_cast<int>(kKillStealLabels.size()) - 1);
  ImGui::SetNextItemWidth(Px(kKillStealComboWidth));
  int step = 0;
  const bool open = BeginSkinCombo(kKillStealCommand,
                                   kKillStealLabels[static_cast<std::size_t>(shown)], art, &step);
  if (const int to = SkinComboStepped(shown, step, static_cast<int>(kKillStealLabels.size()));
      to != shown) {
    asked.field = CommandsField::kKillStealGuard;
    asked.value = to;
  }
  if (open) {
    ComboKeys keys(shown, static_cast<int>(kKillStealLabels.size()));
    for (int rank = 0; rank < static_cast<int>(kKillStealLabels.size()); ++rank) {
      const bool selected = rank == shown;
      if (keys.Took(rank,
                    ImGui::Selectable(kKillStealLabels[static_cast<std::size_t>(rank)], selected) &&
                        !selected)) {
        asked.field = CommandsField::kKillStealGuard;
        asked.value = rank;
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
    FaintText(
        core::Tr("Ces réglages sont ceux que le serveur tient pour ton personnage : ils "
                 "s'activent une fois en jeu."));
  }
  ImGui::BeginDisabled(!view.told);
  if (BeginSkinTabBar("CommandsSettingsTabs", art)) {
    // ⚠ LA LANGUETTE AVANT SON CONTENU : voir `OfferChatLink`. Un onglet demandé
    // s'impose une seule fois, sans quoi le clic sur l'autre serait annulé.
    const auto tab_item = [&](const char* label, CommandsTab tab) {
      const ImGuiTabItemFlags flags =
          state.tab_wanted == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
      const bool opened = ImGui::BeginTabItem(label, nullptr, flags);
      if (OfferChatLink(view.offers_links)) {
        asked.link_tab = tab;
      }
      if (opened) {
        state.tab = tab;
      }
      return opened;
    };
    if (tab_item(commands_page::Translated(kGeneralTabLabel), CommandsTab::kGeneral)) {
      DrawGeneralTab(view, art, asked);
      ImGui::EndTabItem();
    }
    if (tab_item(commands_page::Translated(kAutolootTabLabel), CommandsTab::kAutoloot)) {
      commands_page::DrawAutolootTab(view, state, art, asked);
      ImGui::EndTabItem();
    }
    EndSkinTabBar();
  }
  // ⚠ CONSOMMÉ QUOI QU'IL ARRIVE : une demande qui survivrait rouvrirait son
  // onglet à chaque image.
  state.tab_wanted.reset();
  ImGui::EndDisabled();
  return asked;
}

void AppendCommandsPageTerms(std::vector<OptionBodyTerm>& into) {
  const auto general = static_cast<int>(CommandsTab::kGeneral);
  const auto autoloot = static_cast<int>(CommandsTab::kAutoloot);
  into.push_back({.text = commands_page::Translated(kGeneralTabLabel), .tab = general});
  for (const GeneralToggle& toggle : kGeneralToggles) {
    into.push_back({.text = commands_page::Translated(toggle.label), .tab = general});
    into.push_back({.text = toggle.command, .tab = general});
  }
  into.push_back({.text = kKillStealCommand, .tab = general});
  into.push_back({.text = commands_page::Translated(kAutolootTabLabel), .tab = autoloot});
  commands_page::AppendAutolootTerms(into, autoloot);
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
