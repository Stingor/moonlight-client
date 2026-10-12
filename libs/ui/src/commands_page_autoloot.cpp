// L'ONGLET « AUTOLOOTS » DE LA PAGE « COMMANDS SETTINGS » : le taux, le seuil de
// prix, les types, les rares et les MVP. La liste « @autolootid » et ses presets
// vivent dans `commands_page_presets.cpp`.

#include <algorithm>
#include <array>
#include <initializer_list>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "commands_page_parts.h"

namespace ro::ui::commands_page {
namespace {

// Les largeurs des champs, en pixels d'interface.
constexpr float kRateSliderWidth = 130.0F;
constexpr float kMinZenyInputWidth = 130.0F;

// Les pas des boutons du seuil de prix, en zeny.
constexpr int kMinZenySmallStep = 1000;
constexpr int kMinZenyLargeStep = 10000;

// UN TYPE D'ITEM, et son bit dans le masque du serveur — le rang du type.
struct AutolootType {
  const char* label;
  std::uint16_t bit;
};

// ⚠ LES NOMS DES TYPES RESTENT EN ANGLAIS : ce sont ceux que le serveur écrit
// quand il confirme le réglage dans le chat.
constexpr std::array<AutolootType, 10> kAutolootTypes = {{
    {"Healing", 1U << 0U},
    {"Usable", 1U << 2U},
    {"Etc", 1U << 3U},
    {"Armor", 1U << 4U},
    {"Weapon", 1U << 5U},
    {"Card", 1U << 6U},
    {"Pet Egg", 1U << 7U},
    {"Pet Armor", 1U << 8U},
    {"Ammo", 1U << 10U},
    {"Cash", 1U << 11U},
}};

// LES MOTS QUE LE DESSIN ET LA RECHERCHE PARTAGENT : les commandes, telles
// qu'elles se tapent, et les libellés marqués.
constexpr const char* kRateCommand = "@autoloot";
constexpr const char* kMinZenyLabel = "@autolootpognon (z)";
constexpr const char* kTypeCommand = "@autoloottype";
constexpr const char* kRaresLabel = core::TrText("Autoloot rares");
constexpr const char* kMvpLabel = core::TrText("Autoloot MVP");
constexpr const char* kMvpRewardsLabel = core::TrText("Autoloot MVP rewards (actif par défaut)");

void AskMinZeny(int zeny, CommandsPageAction& asked) {
  asked.field = CommandsField::kAutolootMinZeny;
  asked.value = ClampAutolootMinZeny(zeny);
}

void DrawRate(const CommandsPageView& view, const SkinArt& art, CommandsPageAction& asked) {
  int rate = view.autoloot_rate_percent;
  ImGui::SetNextItemWidth(Px(kRateSliderWidth));
  if (SkinSliderInt(kRateCommand, &rate, 0, kCommandsAutolootRateHighest, art, "%d%%")) {
    asked.field = CommandsField::kAutolootRate;
    asked.value = rate;
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Reset##rate"), art)) {
    asked.field = CommandsField::kAutolootRate;
    asked.value = 0;
  }
}

void DrawMinZeny(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                 CommandsPageAction& asked) {
  // 🔴 LA SAISIE PART QUAND ON QUITTE LE CHAMP, PAS À CHAQUE CHIFFRE : le serveur
  // confirme chaque réglage dans le chat, et taper « 20000 » y écrirait cinq
  // seuils, dont quatre que le joueur n'a jamais voulus.
  int zeny = state.min_zeny_editing ? state.min_zeny_draft : view.autoloot_min_zeny;
  ImGui::SetNextItemWidth(Px(kMinZenyInputWidth));
  ImGui::InputInt(kMinZenyLabel, &zeny, 0, 0);
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    state.min_zeny_editing = false;
    AskMinZeny(zeny, asked);
  } else if (ImGui::IsItemActive()) {
    state.min_zeny_editing = true;
    state.min_zeny_draft = zeny;
  } else {
    state.min_zeny_editing = false;
  }
  ImGui::SameLine();
  SkinHelpMarker(core::Tr("Autoloot des items ayant au minimum le prix de revente configuré."));
  const int current = view.autoloot_min_zeny;
  if (SkinSmallButton(core::Tr("-10kz"), art)) {
    AskMinZeny(current - kMinZenyLargeStep, asked);
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("-1kz"), art)) {
    AskMinZeny(current - kMinZenySmallStep, asked);
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("+1kz"), art)) {
    AskMinZeny(current + kMinZenySmallStep, asked);
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("+10kz"), art)) {
    AskMinZeny(current + kMinZenyLargeStep, asked);
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Reset##pognon"), art)) {
    AskMinZeny(0, asked);
  }
}

void DrawTypes(const CommandsPageView& view, const SkinArt& art, CommandsPageAction& asked) {
  if (!ImGui::TreeNode(kTypeCommand)) {
    return;
  }
  ImGui::TextUnformatted("@autoloottype :");
  ImGui::SameLine();
  SkinHelpMarker(
      core::Tr("Cochez les types d'items à lootter automatiquement.\nHealing=0 Usable=2 Etc=3 "
               "Armor=4 Weapon=5\nCard=6 PetEgg=7 PetArmor=8 Ammo=10 Cash=11"));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Reset##type"), art)) {
    asked.field = CommandsField::kAutolootTypes;
    asked.value = 0;
  }
  if (ImGui::BeginTable("aloottype", 2)) {
    for (const AutolootType& type : kAutolootTypes) {
      ImGui::TableNextColumn();
      bool checked = (view.autoloot_type_mask & type.bit) != 0;
      if (SkinCheckBox(type.label, &checked, art)) {
        const std::uint16_t mask =
            checked ? static_cast<std::uint16_t>(view.autoloot_type_mask | type.bit)
                    : static_cast<std::uint16_t>(view.autoloot_type_mask & ~type.bit);
        asked.field = CommandsField::kAutolootTypes;
        asked.value = mask;
      }
    }
    ImGui::EndTable();
  }
  ImGui::TreePop();
}

}  // namespace

void AppendAutolootTerms(std::vector<OptionBodyTerm>& into, int tab) {
  for (const char* command : {kRateCommand, kMinZenyLabel, kTypeCommand, kAutolootIdCommand}) {
    into.push_back({.text = command, .tab = tab});
  }
  for (const char* label : {kRaresLabel, kMvpLabel, kMvpRewardsLabel, kPresetsLabel}) {
    into.push_back({.text = Translated(label), .tab = tab});
  }
  for (const AutolootType& type : kAutolootTypes) {
    into.push_back({.text = type.label, .tab = tab});
  }
}

void DrawAutolootTab(const CommandsPageView& view, CommandsPageState& state, const SkinArt& art,
                     CommandsPageAction& asked) {
  ImGui::Spacing();
  DrawRate(view, art, asked);
  ImGui::Separator();
  DrawMinZeny(view, state, art, asked);
  ImGui::Separator();
  DrawTypes(view, art, asked);
  ImGui::Separator();
  Toggle(Translated(kRaresLabel), view.autoloot_rares,
         core::Tr("Autolooting: Toutes les Cards\nOld Blue Box (603)\nYggdrasil Berry "
                  "(607)\nYggdrasil Seed (608)\nOld Card Album (616)\nOld Purple Box (617)\nGift "
                  "Box (644)\nGold (969)\n"
                  "Temporal Crystal (6607)\nCoagulated Spell (6608)\nJitterbug's Tooth "
                  "(6719)\nFragment of Agony (7436)\nFragment of Misery (7437)\nFragment of "
                  "Hatred (7438)\n"
                  "Piece_Of_Memory_Red (7439)\nTreasure Box (7444)\nCursed Water "
                  "(12020)\nElemental Converter Fire (12114)\nElemental Converter Water (12115)\n"
                  "Elemental Converter Earth (12116)\nElemental Converter Wind (12117)\nMystical "
                  "Card Album (12246)\nSentimental Fragment (22687)\nCursed Fragment (23016)"),
         CommandsField::kAutolootRares, art, asked);
  ImGui::Separator();
  Toggle(Translated(kMvpLabel), view.autoloot_mvp,
         core::Tr("Loot automatiquement les MVP\nquelque soit leur taux de drop. (@autolootmvp)"),
         CommandsField::kAutolootMvp, art, asked);
  Toggle(Translated(kMvpRewardsLabel), view.autoloot_mvp_rewards,
         core::Tr("Les drops de récompense des MVP sont lootés\nautomatiquement par "
                  "défaut.\nDécocher pour désactiver. (@autolootmvpreward)"),
         CommandsField::kAutolootMvpRewards, art, asked);
  ImGui::Separator();
  DrawAutolootIdNode(view, state, art, asked);
}

}  // namespace ro::ui::commands_page
