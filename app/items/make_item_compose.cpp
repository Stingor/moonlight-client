// LA FABRICATION : la vue que la fenêtre dessine, composée à chaque image — la
// liste, la recette du produit choisi et ses chances, les emplacements de la
// forge, le pied. Tout y est déjà traduit et coloré : la fenêtre ne décide de
// rien.

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/ui/skin_palette.h"
#include "ro/vfs/craft_words.h"

#include "items/make_item_flow.h"
#include "items/make_item_odds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::core::TrText;

// Une chance est verte au-delà de ceci, ambre en dessous.
constexpr int kGoodChance = 80;
// Combien de produits de la liste d'avant l'explication d'une liste vide nomme.
constexpr std::size_t kMostStaleShown = 8;
// Ce qu'un Star Crumb et une pierre coûtent en réussite, et ce que le premier
// rapporte, en points de pour cent et d'attaque.
constexpr int kStarCrumbMalusPercent = 15;
constexpr int kStarCrumbAttack = 5;
constexpr int kElementMalusPercent = 25;
// Les matériaux que la forge ne propose pas parmi les optionnels : l'Emveretarcon
// et l'Oridecon sont dans la plage, mais ne s'y posent pas.
constexpr std::uint32_t kSkippedForgeFirst = 998;
constexpr std::uint32_t kSkippedForgeLast = 999;
constexpr int kAnvilScalePerPercent = 100;

[[nodiscard]] ro::ui::SkinTone ChanceTone(int chance) {
  return chance >= kGoodChance ? ro::ui::SkinTone::kGood : ro::ui::SkinTone::kWarn;
}

[[nodiscard]] int LearnedLevel(const ro::sim::World& world, int skill_id) {
  if (skill_id <= 0) {
    return 0;
  }
  const ro::sim::KnownSkill* skill = world.skill_book().Find(static_cast<std::uint16_t>(skill_id));
  return skill != nullptr ? skill->level : 0;
}

[[nodiscard]] int Known(std::int64_t value) {
  return value == ro::sim::kUnknownStat ? 0 : static_cast<int>(value);
}

// LES VALEURS EFFECTIVES : la base et le bonus de l'équipement.
[[nodiscard]] CrafterStats StatsOf(const ro::sim::World& world) {
  const ro::sim::PlayerStatus& status = world.player_status();
  return CrafterStats{.job_level = Known(status.job_level),
                      .base_level = Known(status.base_level),
                      .int_ = Known(status.int_) + Known(status.int_bonus),
                      .dex = Known(status.dex) + Known(status.dex_bonus),
                      .luk = Known(status.luk) + Known(status.luk_bonus)};
}

[[nodiscard]] bool IsElementStone(std::uint32_t item_id) {
  return item_id >= kFirstElementStone && item_id <= kLastElementStone;
}

// ── LES AIDES DES CHANCES : un titre traduit, puis une clé de catalogue par
// ligne, que le marqueur traduit au survol ─────────────────────────────────
[[nodiscard]] const char* ReplayedTitle() {
  return Tr("Calcul du serveur, rejoué à l'identique :");
}

constexpr std::array<const char*, 6> kMetalHelp = {
    TrText("job level x 20 + DEX x 10 + LUK x 10"),
    TrText("+ bonus du skill de la recette (45 à 65 % pour Iron Tempering, 35 à 55 % pour "
           "Steel, 15 à 35 % pour Enchanted Stone)"),
    TrText("+ un tirage aléatoire de 0,1 à 10 %"),
    TrText("Le chiffre affiché tient compte de ce tirage : c'est la moyenne de ses 100 issues "
           "possibles, donc la probabilité exacte."),
    TrText("DEX et LUK sont les valeurs EFFECTIVES, équipement et cartes comprises — celles que "
           "montre ta feuille de personnage."),
    TrText("Un échec consomme les matériaux sans rien produire."),
};

constexpr std::array<const char*, 12> kForgeHelp = {
    TrText("job level x 20 + DEX x 10 + LUK x 10"),
    TrText("+ niveau de l'arme (+40 % en lv1, +20 % lv2, +10 % lv3, RIEN en lv4)"),
    TrText("+ 5 % par niveau de ton skill de forge"),
    TrText("+ 1 % par niveau de Weaponry Research"),
    TrText("- 25 % si une pierre élémentaire est posée (une seule compte)"),
    TrText("- 15 % par Star Crumb posé"),
    TrText("+ enclume la MEILLEURE portée (Emperium +10, Golden +5, Oridecon +2,5)"),
    TrText("+ un tirage aléatoire de 0,1 à 10 %"),
    TrText("puis multiplié par le réglage serveur weapon_produce_rate."),
    TrText("Le chiffre tient compte du tirage : c'est la moyenne de ses 100 issues, donc la "
           "probabilité exacte. Il se recalcule quand tu changes les emplacements ci-dessous."),
    TrText("Non pris en compte : la pénalité de 30 % des classes baby."),
    TrText("Un échec consomme les matériaux sans rien produire."),
};

constexpr std::array<const char*, 7> kPharmacyHelp = {
    TrText("3 % par niveau de Pharmacy (le terme qui pèse le plus)"),
    TrText("+ 0,5 % par niveau de Potion Research"),
    TrText("+ job level x 20 + (INT / 2) x 10 + DEX x 10 + LUK x 10"),
    TrText("+ bonus du produit : +20 % pour les potions rouge/jaune/blanche, +10 % pour l'alcool, "
           "un tirage seul pour les bouteilles ; MALUS pour les potions slim et la bouteille de "
           "revêtement"),
    TrText("puis multiplié par le réglage serveur potion_produce_rate."),
    TrText("Contrairement aux métaux et à la forge, il n'y a PAS de tirage de base ici : un "
           "produit sans bonus (Blue Potion, Anodyne, Aloebera) a une chance parfaitement "
           "déterminée."),
    TrText("Non pris en compte : le bonus d'un homoncule Vanilmirth avec Instruct (+1 % par "
           "niveau) et la pénalité de 30 % des classes baby. Le chiffre est donc légèrement "
           "pessimiste dans ces deux cas."),
};

constexpr std::array<const char*, 7> kCookingHelp = {
    TrText("12 % par palier de kit au-dessus du premier (c'est le terme qui pèse le plus)"),
    TrText("- 4 % par palier de difficulté du plat"),
    TrText("+ base level x 20 + DEX x 20, et LUK qui réduit un malus fixe"),
    TrText("+ un tirage gouverné par la MAÎTRISE CULINAIRE, puis un second tirage de -1 à -4 %"),
    TrText("Le chiffre tient compte des deux tirages : c'est la moyenne exacte de toutes leurs "
           "issues."),
    TrText("La maîtrise monte à chaque plat réussi et redescend à chaque échec, d'un pas d'autant "
           "plus grand que le plat est difficile. Le jeu ne l'affiche nulle part : elle arrive ici "
           "par un paquet propre au serveur Moonlight."),
    TrText("La cuisine est la seule fabrication que les réglages serveur ne multiplient PAS, et "
           "la seule qui n'exige aucun skill."),
};

[[nodiscard]] const char* SureKitTitle() {
  return Tr(
      "Kit de niveau 15 : le serveur pose directement 100 %, sans aucun tirage. Ni vos "
      "statistiques, ni le plat visé, ni votre maîtrise culinaire n'entrent en jeu.");
}

[[nodiscard]] const char* MasteryRangeTitle() {
  return Tr(
      "La fourchette couvre toute la plage de maîtrise culinaire, de zéro au maximum : votre "
      "valeur exacte place le résultat quelque part entre ces deux bornes.");
}
constexpr const char* kMasteryRangeLine = TrText(
    "Elle n'a pas été transmise. C'est une donnée que le client ne peut pas deviner - elle "
    "vit côté serveur et ne circule dans aucun paquet du jeu d'origine ; ce serveur doit la "
    "pousser explicitement. Une maîtrise de zéro, elle, s'afficherait normalement : c'est "
    "une valeur comme une autre.");

[[nodiscard]] const char* StarCrumbTitle() {
  return Tr("Le compromis que le natif ne montre jamais.");
}
constexpr std::array<const char*, 2> kStarCrumbHelp = {
    TrText("Chaque Star Crumb ajoute +5 ATK à l'arme forgée et retranche 1500 au taux de réussite "
           "— lequel se compte sur 10000, donc 15 points de pourcentage."),
    TrText("Trois Star Crumb, c'est donc +15 ATK contre 45 points de réussite en moins. L'écran "
           "natif aligne trois emplacements et n'en dit rien."),
};

[[nodiscard]] const char* ElementTitle() {
  return Tr(
      "Une seule pierre élémentaire par arme : le serveur ne retient que la première et ne "
      "consomme même pas les suivantes.");
}
constexpr std::array<const char*, 2> kElementHelp = {
    TrText("C'est pourquoi les autres emplacements n'en proposent plus dès qu'une est posée — un "
           "second choix n'aurait aucun effet."),
    TrText("Elle coûte 25 points de réussite (2500 sur une échelle de 10000), soit plus qu'un Star "
           "Crumb. Le natif ne le dit nulle part."),
};

[[nodiscard]] const char* AnvilTitle() {
  return Tr(
      "L'enclume n'est PAS un matériau : elle n'entre dans aucune recette et n'est jamais "
      "consommée. Le serveur vérifie seulement que tu en portes une et ajoute au taux de "
      "réussite :");
}
constexpr std::array<const char*, 5> kAnvilHelp = {
    TrText("Emperium Anvil  +10 %"),
    TrText("Golden Anvil    +5 %"),
    TrText("Oridecon Anvil  +2,5 %"),
    TrText("Anvil           +0 %"),
    TrText("Seule la MEILLEURE compte : en cumuler plusieurs n'apporte rien de plus."),
};

}  // namespace

void MakeItemFlow::AddLine(std::vector<ro::ui::CraftTextLine>& into, std::string text,
                           ro::ui::SkinTone tone, bool wrapped, const char* help_title,
                           std::vector<const char*> help_lines) {
  const std::string& kept = line_texts_.emplace_back(std::move(text));
  const std::vector<const char*>& help = line_helps_.emplace_back(std::move(help_lines));
  into.push_back(ro::ui::CraftTextLine{.text = kept,
                                       .tone = tone,
                                       .wrapped = wrapped,
                                       .help_title = help_title,
                                       .help_lines = help});
}

void MakeItemFlow::Compose(const ro::sim::World& world, const ro::ui::MakeItemWindowState& state,
                           const MakeItemSettings& settings) {
  view_ = ro::ui::MakeItemView{};
  line_texts_.clear();
  line_helps_.clear();
  chance_lines_.clear();
  forge_notes_.clear();
  footer_notes_.clear();

  // LE TITRE : celui du client pour la forge et la pharmacie ; ailleurs, le sien
  // — « LIST » ne dit rien.
  const std::string_view said_title = words_ != nullptr && kind_ == ro::sim::MakeListKind::kProduce
                                          ? ro::vfs::MakeListTitle(*words_)
                                          : std::string_view();
  title_ = said_title.empty() ? std::string(Tr("Fabrication")) : std::string(said_title);
  view_.title = title_;
  view_.empty_list = empty_list_;
  if (words_ != nullptr && empty_list_) {
    view_.cant_make = ro::vfs::CantMakeText(*words_);
  }
  ComposeStale(world);
  view_.stale = !list_armed_ && !entries_.empty();
  view_.arrow_list = arrow_list_;

  rows_.clear();
  for (const Entry& entry : entries_) {
    rows_.push_back(ro::ui::MakeItemRowView{.item_id = entry.item_id,
                                            .name = entry.name,
                                            .icon = IconOf(entry.item_id),
                                            .owned = entry.owned,
                                            .craftable = entry.craftable});
  }
  view_.rows = rows_;

  if (const Entry* chosen = EntryOf(state.selected); chosen != nullptr) {
    ComposeRecipe(world, *chosen);
    if (kind_ == ro::sim::MakeListKind::kProduce && AcceptsForgeSlots(chosen->item_id)) {
      ComposeForge(world, chosen->item_id);
    }
  }
  ComposeFooter(world, state, settings);

  history_view_.clear();
  for (const LogLine& line : history_) {
    history_view_.push_back(
        ro::ui::CraftLogLineView{.text = line.text, .tone = line.tone, .time = line.time});
  }
  view_.history = history_view_;
  view_.chance = chance_lines_;
  view_.forge_notes = forge_notes_;
  view_.footer_notes = footer_notes_;

  view_.show_owned = settings.show_owned;
  view_.show_filter = settings.show_filter;
  view_.show_history = settings.show_history;
  view_.log_time = settings.log_time;
  view_.enter_key = settings.enter_key;
  view_.hover_preview = settings.hover_preview;
}

// CE QUI MANQUAIT À LA LISTE D'AVANT : le serveur ne dit jamais pourquoi il n'a
// rien proposé, et cet ensemble-là était réalisable il y a un instant.
void MakeItemFlow::ComposeStale(const ro::sim::World& world) {
  stale_texts_.clear();
  stale_lines_.clear();
  if (!empty_list_ || !entries_.empty()) {
    return;
  }
  std::size_t considered = 0;
  for (const Entry& entry : stale_entries_) {
    if (stale_texts_.size() >= kMostStaleShown) {
      break;
    }
    ++considered;
    if (entry.materials.empty()) {
      continue;
    }
    std::string missing;
    for (const Material& material : entry.materials) {
      if (material.output || material.item_id == 0) {
        continue;
      }
      const int need = material.kept ? 1 : material.quantity;
      const int have = world.inventory().AmountOf(material.item_id);
      if (have >= need) {
        continue;
      }
      if (!missing.empty()) {
        missing += ", ";
      }
      missing += ro::core::Printed("%d %s", need - have, ItemName(material.item_id).c_str());
    }
    std::string line;
    if (missing.empty()) {
      line =
          ro::core::Printed(Tr("%s : matériaux au complet — inventory plein ou niveau insuffisant"),
                            entry.name.c_str());
    } else {
      line = ro::core::Printed(Tr("%s : il manque %s"), entry.name.c_str(), missing.c_str());
    }
    stale_texts_.emplace_back(line);
  }
  for (const std::string& text : stale_texts_) {
    stale_lines_.push_back(ro::ui::MakeStaleLineView{.text = text});
  }
  view_.stale_lines = stale_lines_;
  view_.stale_more = static_cast<int>(stale_entries_.size() - considered);
}

void MakeItemFlow::ComposeRecipe(const ro::sim::World& world, const Entry& chosen) {
  view_.recipe_shown = true;
  if (chosen.materials.empty()) {
    view_.recipe_unknown = true;
    return;
  }
  if (arrow_list_) {
    recipe_title_ = Tr("Crafting result");
  } else {
    const std::string_view required =
        words_ != nullptr ? ro::vfs::RequiredForText(*words_) : std::string_view();
    recipe_title_ = required.empty() ? std::string(Tr("Matériaux requis"))
                                     : chosen.name + std::string(required);
  }
  view_.recipe_title = recipe_title_;
  ComposeChance(world, chosen);

  material_labels_.clear();
  materials_.clear();
  for (const Material& material : chosen.materials) {
    const int have = world.inventory().AmountOf(material.item_id);
    std::string name = ItemName(material.item_id);
    if (name.empty()) {
      name = ro::core::Printed("#%u", static_cast<unsigned>(material.item_id));
    }
    std::string label;
    bool enough = material.kept ? have >= 1 : have >= material.quantity;
    if (material.output) {
      // CE QUE LA SOURCE REND : une ligne de résultat, jamais un manque.
      label = ro::core::Printed("-> %d %s", material.quantity, name.c_str());
      enough = true;
    } else if (material.kept) {
      label = ro::core::Printed(Tr("%s  (requis, non consommé)"), name.c_str());
    } else if (!enough) {
      label = ro::core::Printed(Tr("%d %s  (%d — il en manque %d)"), material.quantity,
                                name.c_str(), have, material.quantity - have);
    } else {
      label = ro::core::Printed("%d %s  (%d)", material.quantity, name.c_str(), have);
    }
    material_labels_.emplace_back(label);
    materials_.push_back(ro::ui::MakeMaterialView{
        .item_id = material.item_id, .icon = IconOf(material.item_id), .enough = enough});
  }
  for (std::size_t rank = 0; rank < materials_.size(); ++rank) {
    materials_[rank].label = material_labels_[rank];
  }
  view_.materials = materials_;
}

void MakeItemFlow::ComposeChance(const ro::sim::World& world, const Entry& chosen) {
  // UNE CONVERSION EN FLÈCHES EST CERTAINE : rien à dire.
  if (arrow_list_ || book_ == nullptr) {
    return;
  }
  const CraftRecipe* recipe = book_->RecipeOf(chosen.item_id);
  const int skill = recipe != nullptr ? recipe->skill_id : kCraftByItemSkill;
  const int dish_level = recipe != nullptr ? recipe->item_level : 0;
  const int weapon_level = book_->WeaponLevel(chosen.item_id);
  const bool is_weapon = weapon_level > 0;
  const bool metal = !is_weapon && (skill == kIronTemperingSkill || skill == kSteelTemperingSkill ||
                                    skill == kEnchantedStoneSkill);
  const bool potion = !is_weapon && skill == kPharmacySkill;
  const bool cooking = !is_weapon && skill == kCraftByItemSkill && IsDishItemLevel(dish_level);
  const CrafterStats stats = StatsOf(world);

  const auto chance_line = [&](int chance, const char* title, std::vector<const char*> help) {
    AddLine(chance_lines_, ro::core::Printed(Tr("Chances : %d %%"), chance), ChanceTone(chance),
            false, title, std::move(help));
  };

  if (cooking) {
    const int kit = book_->CookingKitLevel(source_item_id_);
    if (!IsDishItemLevel(kit)) {
      AddLine(chance_lines_,
              Tr("Chances : non calculées (le kit utilisé n'a pas été identifié ; son niveau ne "
                 "circule dans aucun paquet et se déduit de l'item consommé)."),
              ro::ui::SkinTone::kFaint, true);
      return;
    }
    const std::string& inputs_text = line_texts_.emplace_back(ro::core::Printed(
        Tr("Entrées du calcul : kit niveau %d, plat niveau %d, base level %d, DEX %d, "
           "LUK %d, maîtrise %s."),
        kit, dish_level, stats.base_level, stats.dex, stats.luk,
        cook_mastery_ >= 0 ? Tr("reçue") : Tr("NON REÇUE")));
    const int chance = CookingChancePercent(kit, dish_level, cook_mastery_, stats);
    if (chance >= 0) {
      std::vector<const char*> help;
      const char* title = SureKitTitle();
      if (kit < kSureCookingKit) {
        title = ReplayedTitle();
        help.assign(kCookingHelp.begin(), kCookingHelp.end());
      }
      help.push_back(inputs_text.c_str());
      chance_line(chance, title, std::move(help));
      if (cook_mastery_ >= 0 && kit < kSureCookingKit) {
        chance_lines_.back().suffix = line_texts_.emplace_back(
            ro::core::Printed(Tr("(maîtrise %d / %d)"), cook_mastery_, kMostCookMastery));
        chance_lines_.back().suffix_tone = ro::ui::SkinTone::kFaint;
      }
      return;
    }
    const int worst = CookingChancePercent(kit, dish_level, kLeastCookMastery, stats);
    const int best = CookingChancePercent(kit, dish_level, kMostCookMastery, stats);
    if (worst >= 0 && best >= 0) {
      std::string line;
      if (worst == best) {
        line = ro::core::Printed(Tr("Chances : %d %%"), worst);
      } else {
        line = ro::core::Printed(Tr("Chances : entre %d %% et %d %%"), worst, best);
      }
      AddLine(chance_lines_, line, ChanceTone(worst), false, MasteryRangeTitle(),
              {kMasteryRangeLine, inputs_text.c_str()});
      return;
    }
    AddLine(chance_lines_,
            Tr("Chances : non calculées (maîtrise culinaire non transmise par le serveur)."),
            ro::ui::SkinTone::kFaint, true);
    return;
  }

  if (potion) {
    const int chance = PharmacyChancePercent(
        PharmacyInputs{.product = chosen.item_id,
                       .pharmacy_level = LearnedLevel(world, kPharmacySkill),
                       .potion_research_level = LearnedLevel(world, kPotionResearchSkill),
                       .produce_rate = book_->rates().potion_produce},
        stats);
    if (chance < 0) {
      AddLine(chance_lines_,
              Tr("Chances : non calculées (le fichier de recettes ne porte pas le réglage serveur "
                 "potion_produce_rate, qui multiplie le résultat)."),
              ro::ui::SkinTone::kFaint, true);
      return;
    }
    chance_line(chance, ReplayedTitle(), {kPharmacyHelp.begin(), kPharmacyHelp.end()});
    return;
  }

  if (metal) {
    chance_line(MetalChancePercent(chosen.item_id, skill, LearnedLevel(world, skill), stats),
                ReplayedTitle(), {kMetalHelp.begin(), kMetalHelp.end()});
    return;
  }

  if (is_weapon) {
    ForgeInputs forge{.weapon_level = weapon_level,
                      .recipe_skill = skill,
                      .skill_level = LearnedLevel(world, skill),
                      .weapon_research_level = LearnedLevel(world, kWeaponResearchSkill),
                      .oridecon_research_level = LearnedLevel(world, kOrideconResearchSkill),
                      .oridecon_research_fix = book_->rates().oridecon_research_fix,
                      .produce_rate = book_->rates().weapon_produce};
    for (const std::uint32_t slot : forge_slots_) {
      if (slot == kStarCrumb) {
        ++forge.star_crumbs;
      } else if (IsElementStone(slot)) {
        forge.element = true;
      }
    }
    for (const Anvil& anvil : kAnvils) {
      if (world.inventory().AmountOf(anvil.item_id) > 0) {
        forge.anvil_bonus = anvil.bonus;
        break;
      }
    }
    const int chance = ForgeChancePercent(forge, stats);
    if (chance < 0) {
      AddLine(chance_lines_,
              Tr("Chances : non calculées (le fichier de recettes ne porte pas le réglage serveur "
                 "weapon_produce_rate, qui multiplie le résultat)."),
              ro::ui::SkinTone::kFaint, true);
      return;
    }
    chance_line(chance, ReplayedTitle(), {kForgeHelp.begin(), kForgeHelp.end()});
    return;
  }

  AddLine(chance_lines_,
          Tr("Chances : non calculées pour ce skill (chaque job a sa propre formule "
             "côté serveur)."),
          ro::ui::SkinTone::kFaint, true);
}

void MakeItemFlow::ComposeForge(const ro::sim::World& world, std::uint32_t product) {
  // UN AUTRE PRODUIT VIDE LES EMPLACEMENTS : on n'emporterait pas un Star Crumb
  // sur une arme qu'on n'a pas visée.
  if (forge_for_id_ != product) {
    forge_slots_ = {};
    forge_for_id_ = product;
  }
  view_.forge_shown = true;
  for (std::size_t slot = 0; slot < forge_slots_.size(); ++slot) {
    choice_labels_[slot].clear();
    choices_[slot].clear();
    bool other_stone = false;
    for (std::size_t other = 0; other < forge_slots_.size(); ++other) {
      if (other != slot && IsElementStone(forge_slots_[other])) {
        other_stone = true;
      }
    }
    for (std::uint32_t item_id = kFirstElementStone; item_id <= kStarCrumb; ++item_id) {
      if (item_id >= kSkippedForgeFirst && item_id <= kSkippedForgeLast) {
        continue;
      }
      const int have = world.inventory().AmountOf(item_id);
      if (have <= 0) {
        continue;
      }
      int used_elsewhere = 0;
      for (std::size_t other = 0; other < forge_slots_.size(); ++other) {
        if (other != slot && forge_slots_[other] == item_id) {
          ++used_elsewhere;
        }
      }
      // ⚠ UNE SEULE PIERRE PAR ARME : le serveur ignore les suivantes.
      if (used_elsewhere >= have || (IsElementStone(item_id) && other_stone)) {
        continue;
      }
      choice_labels_[slot].emplace_back(
          ro::core::Printed("%s  (%d)", ItemName(item_id).c_str(), have - used_elsewhere));
      choices_[slot].push_back(ro::ui::ForgeChoiceView{.item_id = item_id});
    }
    for (std::size_t rank = 0; rank < choices_[slot].size(); ++rank) {
      choices_[slot][rank].label = choice_labels_[slot][rank];
    }
    const std::uint32_t chosen = forge_slots_[slot];
    chosen_labels_[slot] = chosen == 0 ? std::string() : ItemName(chosen);
    if (chosen != 0 && chosen_labels_[slot].empty()) {
      chosen_labels_[slot] = "?";
    }
    view_.forge_slots[slot] = ro::ui::ForgeSlotView{
        .chosen = chosen, .chosen_label = chosen_labels_[slot], .choices = choices_[slot]};
  }

  // ── CE QUE LES EMPLACEMENTS COÛTENT ─────────────────────────────────────────
  const int crumbs = static_cast<int>(std::ranges::count(forge_slots_, kStarCrumb));
  const auto stone = std::ranges::find_if(forge_slots_, IsElementStone);
  if (crumbs > 0) {
    AddLine(forge_notes_,
            ro::core::Printed(Tr("%d Star Crumb : +%d ATK, -%d %% de réussite"), crumbs,
                              crumbs * kStarCrumbAttack, crumbs * kStarCrumbMalusPercent),
            ro::ui::SkinTone::kWarn, false, StarCrumbTitle(),
            {kStarCrumbHelp.begin(), kStarCrumbHelp.end()});
  }
  if (stone != forge_slots_.end()) {
    AddLine(forge_notes_,
            ro::core::Printed(Tr("Élément %s : -%d %% de réussite"), ItemName(*stone).c_str(),
                              kElementMalusPercent),
            ro::ui::SkinTone::kWarn, false, ElementTitle(),
            {kElementHelp.begin(), kElementHelp.end()});
  }
  if (crumbs > 0 && stone != forge_slots_.end()) {
    AddLine(forge_notes_,
            ro::core::Printed(Tr("Total : -%d %% de réussite"),
                              crumbs * kStarCrumbMalusPercent + kElementMalusPercent),
            ro::ui::SkinTone::kBad, false);
  }

  // L'ENCLUME : la meilleure portée, et celles qui ne servent à rien.
  const Anvil* kept = nullptr;
  int others = 0;
  for (const Anvil& anvil : kAnvils) {
    if (world.inventory().AmountOf(anvil.item_id) <= 0) {
      continue;
    }
    if (kept == nullptr) {
      kept = &anvil;
    } else {
      ++others;
    }
  }
  if (kept != nullptr) {
    AddLine(forge_notes_,
            ro::core::Printed(Tr("Enclume retenue : %s (+%d %%)"), ItemName(kept->item_id).c_str(),
                              kept->bonus / kAnvilScalePerPercent),
            kept->bonus > 0 ? ro::ui::SkinTone::kGood : ro::ui::SkinTone::kFaint, false,
            AnvilTitle(), {kAnvilHelp.begin(), kAnvilHelp.end()});
    if (others > 0) {
      forge_notes_.back().suffix = line_texts_.emplace_back(
          ro::core::Printed(others > 1 ? Tr("(%d autres dans l'inventory, sans effet)")
                                       : Tr("(%d autre dans l'inventory, sans effet)"),
                            others));
      forge_notes_.back().suffix_tone = ro::ui::SkinTone::kFaint;
    }
  } else {
    AddLine(forge_notes_, Tr("Aucune enclume dans l'inventory."), ro::ui::SkinTone::kBad, false,
            AnvilTitle(), {kAnvilHelp.begin(), kAnvilHelp.end()});
  }

  // LA SUR-RÉSERVATION : plus d'emplacements qu'il n'y a d'objets.
  for (const std::uint32_t item_id : forge_slots_) {
    if (item_id == 0) {
      continue;
    }
    const int asked = static_cast<int>(std::ranges::count(forge_slots_, item_id));
    const int have = world.inventory().AmountOf(item_id);
    if (asked > have) {
      AddLine(forge_notes_,
              ro::core::Printed(Tr("%s : %d demandés, %d dans l'inventory"),
                                ItemName(item_id).c_str(), asked, have),
              ro::ui::SkinTone::kBad, false);
      break;
    }
  }
}

void MakeItemFlow::ComposeFooter(const ro::sim::World& world,
                                 const ro::ui::MakeItemWindowState& state,
                                 const MakeItemSettings& settings) {
  const bool relaunch_coming =
      auto_recast_at_ != 0 || pending_ == Pending::kRecast || pending_ == Pending::kReuseItem;
  // ⚠ LA VISIBILITÉ DU CHOIX SE DÉCIDE AU DESSIN : la fenêtre la joint à ceci.
  view_.can_make = state.open && list_armed_ && !entries_.empty() && !awaiting_ &&
                   state.selected != 0 && EntryOf(state.selected) != nullptr;
  view_.spent = state.open && !list_armed_ && !awaiting_ && !relaunch_coming;
  view_.can_relaunch =
      from_item_ ? (source_item_id_ != 0 && world.inventory().AmountOf(source_item_id_) > 0)
                 : skill_id_ > 0;
  if (!view_.can_relaunch) {
    relaunch_tip_ = from_item_ ? Tr("Il ne te reste plus l'item qui a ouvert cette liste.")
                               : Tr("Skill d'origine inconnu : relance impossible.");
  } else if (from_item_) {
    relaunch_tip_ = ro::core::Printed(
        Tr("Ré-utilise %s pour rouvrir une liste. L'item est CONSOMMÉ : le serveur le "
           "détruit avant d'exécuter son script, même si tu annules ensuite."),
        source_item_name_.c_str());
  } else {
    relaunch_tip_ =
        Tr("Relance le skill pour obtenir une nouvelle liste. Le serveur "
           "n'autorise qu'une fabrication par lancement : après chaque item "
           "produit, il faut relancer.");
  }
  view_.relaunch_tip = relaunch_tip_;

  batch_progress_.clear();
  if (batch_left_ > 0) {
    batch_progress_ = ro::core::Printed(Tr("série : %d/%d"), state.batch_target - batch_left_,
                                        state.batch_target);
  }
  view_.batch_progress = batch_progress_;

  // 🔴 SANS LA RELANCE CORRESPONDANTE, LA SÉRIE N'A PAS DE SENS : aucune liste
  // ne reviendrait après la première fabrication. Sa ligne ne se montre donc pas.
  view_.batch_shown = BatchPossible(settings);

  // ── POURQUOI « FABRIQUER » EST GRISÉ ────────────────────────────────────────
  if (!view_.can_make) {
    if (awaiting_) {
      AddLine(footer_notes_, Tr("Demande envoyée — en attente du serveur…"),
              ro::ui::SkinTone::kFaint, true);
    } else if (!list_armed_ || entries_.empty()) {
      if (!last_result_.empty()) {
        AddLine(footer_notes_, last_result_, last_result_tone_, true);
      } else {
        AddLine(footer_notes_, Tr("Aucune liste en cours."), ro::ui::SkinTone::kFaint, true);
      }
    } else if (state.selected == 0) {
      AddLine(footer_notes_, Tr("Choisis un produit."), ro::ui::SkinTone::kFaint, true);
    }
  }

  // ── LA RELANCE ──────────────────────────────────────────────────────────────
  if (auto_chain_ > 0 && auto_recast_at_ != 0) {
    std::string line;
    if (from_item_ && settings.auto_reuse_max > 0) {
      line =
          ro::core::Printed(Tr("Relance automatique… %s consommé %d/%d"), source_item_name_.c_str(),
                            auto_items_used_, settings.auto_reuse_max);
    } else if (from_item_) {
      line = ro::core::Printed(Tr("Relance automatique… %s consommé ×%d"),
                               source_item_name_.c_str(), auto_items_used_);
    } else {
      line = ro::core::Printed(Tr("Relance automatique… (%d)"), auto_chain_);
    }
    AddLine(footer_notes_, line, ro::ui::SkinTone::kWarn, true);
  } else if (!stop_reason_.empty()) {
    AddLine(footer_notes_, stop_reason_, ro::ui::SkinTone::kFaint, true);
  }
}

}  // namespace moonclient
