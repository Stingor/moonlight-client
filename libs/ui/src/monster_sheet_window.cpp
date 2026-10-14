#include "ro/ui/monster_sheet_window.h"

#include <cfloat>
#include <cinttypes>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "monster_sheet_parts.h"

namespace ro::ui {
namespace monster_sheet {
namespace {

// La taille à la première ouverture, et la plus petite qu'on accepte.
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 420.0F;
constexpr float kLeastWidthPixels = 430.0F;
constexpr float kLeastHeightPixels = 300.0F;

// Le cadre du sprite, son fond, son arrondi.
constexpr float kSpriteBoxWidthPixels = 116.0F;
constexpr float kSpriteBoxHeightPixels = 132.0F;
constexpr ImU32 kSpriteBoxColor = IM_COL32(24, 22, 20, 90);
constexpr float kSpriteBoxRounding = 3.0F;
// Ce qui tient lieu de sprite quand il n'y en a pas : un disque, et une ligne
// au pied du cadre.
constexpr float kPlaceholderRadiusPixels = 18.0F;
constexpr int kPlaceholderSegments = 16;
constexpr ImU32 kPlaceholderColor = IM_COL32(120, 110, 100, 160);
constexpr ImU32 kPlaceholderTextColor = IM_COL32(200, 190, 170, 200);
constexpr float kPlaceholderTextInsetPixels = 6.0F;
constexpr float kPlaceholderTextRisePixels = 18.0F;

// Le nombre d'orientations d'un .ACT.
constexpr int kDirections = 8;

// L'écart entre le nom et son qualificatif, entre les mots de l'homonymie.
constexpr float kTightGapPixels = 4.0F;

// Les modes rangés trois par ligne.
constexpr int kModeColumns = 3;

// ── PRÉCISION ET ESQUIVE, CALCULÉES ICI ─────────────────────────────────────
//
// Le paquet ne les porte pas : le serveur ne les calcule qu'à l'APPARITION
// d'un monstre. Mais ses formules d'avant la refonte ne dépendent que de champs
// qu'on a déjà : HIT = niveau + DEX, FLEE = niveau + AGI.
//
// La chance de toucher : 80 % + HIT de l'attaquant − FLEE de la cible, bornée
// entre 5 et 100 %.
constexpr int kHitBasePercent = 80;
constexpr int kHitLeastPercent = 5;
constexpr int kHitMostPercent = 100;
// Les seuils qui colorent une chance : sûre, douteuse, mauvaise.
constexpr int kHitSurePercent = 90;
constexpr int kHitPoorPercent = 40;
constexpr int kStruckRarelyPercent = 20;
constexpr int kStruckOftenPercent = 80;
// Le critique se compte en DIXIÈMES de pour cent : un point de LUK du monstre
// en retire deux, et le jet ne dépasse jamais mille.
constexpr int kTenthsPerPercent = 10;
constexpr int kCritTenthsPerLuk = 2;
constexpr int kCritMostTenths = 1000;
constexpr int kPercent = 100;

// Les centièmes de pour cent d'un gain d'EXP.
constexpr std::uint64_t kHundredthsOfPercent = 10000;
constexpr std::uint64_t kHundredthsPerPercent = 100;
// Les dixièmes de case par seconde d'une marche : mille millisecondes, fois dix.
constexpr unsigned kTenthCellMilliseconds = 10000;

// Les bits de comportement qui changent la façon de combattre le monstre — un
// relevé de trente-deux bits n'aiderait personne. Ceux du serveur, `e_mode`.
constexpr std::uint32_t kModeAggressive = 0x0000004;
constexpr std::uint32_t kModeAssist = 0x0000008;
constexpr std::uint32_t kModeLooter = 0x0000002;
constexpr std::uint32_t kModeStatusImmune = 0x4000000;
constexpr std::uint32_t kModeKnockbackImmune = 0x0200000;
constexpr std::uint32_t kModeDetector = 0x2000000;
constexpr std::uint32_t kModeChangeTargetChase = 0x0002000;
// Dans l'ordre où la fiche les montre.
constexpr std::uint32_t kModeBits[] = {
    kModeAggressive,        kModeAssist,          kModeLooter,
    kModeStatusImmune,      kModeKnockbackImmune, kModeDetector,
    kModeChangeTargetChase,
};

[[nodiscard]] const char* ModeWord(std::uint32_t bit) {
  switch (bit) {
    case kModeAggressive:
      return core::Tr("Agressif");
    case kModeAssist:
      return core::Tr("Assist");
    case kModeLooter:
      return core::Tr("Ramasse");
    case kModeStatusImmune:
      return core::Tr("Insensible aux altérations");
    case kModeKnockbackImmune:
      return core::Tr("Insensible au recul");
    case kModeDetector:
      return core::Tr("Détecteur");
    case kModeChangeTargetChase:
      return core::Tr("Change de cible en chasse");
    default:
      return "?";
  }
}

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

[[nodiscard]] int HitChancePercent(int attacker_hit, int defender_flee) {
  const int rate = kHitBasePercent + attacker_hit - defender_flee;
  return rate < kHitLeastPercent ? kHitLeastPercent
                                 : (rate > kHitMostPercent ? kHitMostPercent : rate);
}

// ── LE SPRITE ───────────────────────────────────────────────────────────────
//
// 🔴 LE CADRE EST UNE ZONE ACTIVE : la molette tourne le monstre, un clic le
// titille. Le dessin est à `app` — il joue l'animation et ses sons — ; ici, on
// ne fait que poser l'image et relever les gestes.
void DrawSprite(MonsterSheetWindowState& state, const MonsterSheetView& view,
                MonsterSheetAnswer& answer) {
  const ImVec2 box(kSpriteBoxWidthPixels * Scale(), kSpriteBoxHeightPixels * Scale());
  state.sprite_box_width = box.x;
  state.sprite_box_height = box.y;
  const ImVec2 top_left = ImGui::GetCursorScreenPos();
  const ImVec2 bottom_right(top_left.x + box.x, top_left.y + box.y);
  ImDrawList* const list = ImGui::GetWindowDrawList();
  list->AddRectFilled(top_left, bottom_right, kSpriteBoxColor, kSpriteBoxRounding);
  if (view.sprite.usable()) {
    const float shake = view.sprite_shake_px * Scale();
    list->AddImage(view.sprite.id, ImVec2(top_left.x + shake, top_left.y),
                   ImVec2(bottom_right.x + shake, bottom_right.y));
  }
  // 🔴 LE REMPLAÇANT NE DÉPEND QUE DE L'ABSENCE D'ART : une animation de mort
  // se termine sur des images VIDES, et une image vide se dessine… vide.
  if (view.sprite_missing) {
    const ImVec2 middle((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F);
    list->AddCircleFilled(middle, Px(kPlaceholderRadiusPixels), kPlaceholderColor,
                          kPlaceholderSegments);
    list->AddText(ImVec2(top_left.x + Px(kPlaceholderTextInsetPixels),
                         bottom_right.y - Px(kPlaceholderTextRisePixels)),
                  kPlaceholderTextColor,
                  view.sprite_is_model ? core::Tr("modèle 3D") : core::Tr("pas de sprite"));
  }

  ImGui::InvisibleButton("##fiche_monstre_sprite", box);
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  // La molette passe par le portail : elle revient au cadre quand le geste lui
  // est destiné, et reste à la fiche tant qu'on la parcourt.
  const float wheel = WheelOver(state.wheel, ImGui::GetItemID(), hovered);
  if (wheel != 0.0F) {
    state.sprite_direction =
        (state.sprite_direction + (wheel > 0.0F ? 1 : kDirections - 1)) % kDirections;
  }
  // ⚠ LE TEMPS MORT ENTRE DEUX CLICS EST À `app`, et il est SILENCIEUX : c'est
  // une réaction, pas une commande.
  if (ImGui::IsItemClicked()) {
    answer.poked = true;
  }
  // Temporisée : l'aide ne doit pas cacher le sprite dès qu'on passe dessus.
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
    SkinTooltipFormat("%s", core::Tr("Molette : tourner le monstre\nClic : le titiller"));
  }
}

// Le nom, ses badges, et les lignes de la fiche.
void DrawIdentity(const MonsterSheetView& view, const SkinArt& art, MonsterSheetAnswer& answer) {
  ImGui::BeginGroup();
  Colored(kTitleColor, view.name.empty() ? core::Tr("(sans nom)") : view.name.c_str());
  // Le qualificatif fait partie du TITRE : c'est là qu'il faut lever le doute.
  if (view.qualifier != nullptr) {
    ImGui::SameLine(0.0F, Px(kTightGapPixels));
    Colored(kTitleColor, Printed("(%s)", view.qualifier).c_str());
  }
  ImGui::SameLine();
  Label(Printed("#%u", view.shown_id).c_str());
  DrawBossBadge(view.boss_rank);
  // ── POSER LE MONSTRE DANS LA BARRE DE CHAT ────────────────────────────────
  //
  // 🔴 C'EST ICI QUE LE NOM EXISTE : le client ne sait pas nommer un monstre,
  // et le lien emporte celui que cette fiche affiche.
  if (view.can_post_link && !view.name.empty()) {
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Lien##mi_chatlink"), art)) {
      answer.post_link = true;
    }
    SkinTooltip(core::Tr("Poser ce monstre dans la barre de chat."));
  }
  if (view.database_link) {
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Base de données##mi_database"), art)) {
      answer.open_database = true;
    }
    SkinTooltip(core::Tr("Ouvrir ce monstre dans la base de données."));
  }

  const MonsterSheetWords& words = view.words;
  ImGui::Text("%.*s : %d", static_cast<int>(words.level.size()), words.level.data(), view.level);
  ImGui::Text("%.*s : %.*s", static_cast<int>(words.race.size()), words.race.data(),
              static_cast<int>(words.race_value.size()), words.race_value.data());
  ImGui::Text("%.*s : %.*s", static_cast<int>(words.element.size()), words.element.data(),
              static_cast<int>(words.element_value.size()), words.element_value.data());
  ImGui::SameLine();
  // Le niveau élémentaire, que le paquet de Sense ne transporte pas.
  Label(Printed(core::Tr("niv. %u"), static_cast<unsigned>(view.element_level)).c_str());
  ImGui::Text("%.*s : %.*s", static_cast<int>(words.size.size()), words.size.data(),
              static_cast<int>(words.size_value.size()), words.size_value.data());

  // ── LES MODES, EN BADGES, TROIS PAR LIGNE ─────────────────────────────────
  //
  // ⚠ EN TABLE ET NON EN LIGNE CONTINUE : un monstre qui en cumule cinq
  // débordait de la fenêtre.
  int shown_modes = 0;
  for (const std::uint32_t bit : kModeBits) {
    if ((view.mode & bit) != 0) {
      ++shown_modes;
    }
  }
  if (shown_modes == 0) {
    Label(core::Tr("[Passif]"));
  } else if (ImGui::BeginTable(
                 "##mi_modes", kModeColumns,
                 ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
    for (const std::uint32_t bit : kModeBits) {
      if ((view.mode & bit) == 0) {
        continue;
      }
      ImGui::TableNextColumn();
      Colored(kBlueColor, Printed("[%s]", ModeWord(bit)).c_str());
    }
    ImGui::EndTable();
  }
  ImGui::EndGroup();
}

// ── « PLUSIEURS MONSTRES PORTENT CE NOM » ───────────────────────────────────
//
// Il n'énonce QUE du vérifiable : combien partagent le nom, lequel est
// celui-ci, et lequel est le plus ancien. Rien du butin ni des apparitions —
// « pas de butin » n'a jamais voulu dire « variante ».
void DrawNamesakeNote(const MonsterSheetView& view, const SkinArt& art,
                      MonsterSheetAnswer& answer) {
  if (view.namesake_count <= 1 || view.name.empty()) {
    return;
  }
  ImGui::Separator();
  Colored(kAmberColor, Printed(core::Tr("%u monstres portent le nom « %s »."),
                               static_cast<unsigned>(view.namesake_count), view.name.c_str())
                           .c_str());
  if (!view.aegis_name.empty()) {
    ImGui::TextUnformatted(core::Tr("Celui-ci est"));
    ImGui::SameLine(0.0F, Px(kTightGapPixels));
    Colored(kBlueColor, view.aegis_name.c_str());
    ImGui::SameLine(0.0F, Px(kTightGapPixels));
    Label(Printed("#%u", view.shown_id).c_str());
  }
  // ⚠ ABSENT QUAND C'EST LUI : un bouton qui recharge la page courante est un
  // piège.
  if (view.namesake_origin != 0 && view.namesake_origin != view.shown_id) {
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Voir l'original##mi_namesake"), art)) {
      answer.open_monster = view.namesake_origin;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltipFormat(
          core::Tr("Ouvrir la fiche du monstre #%u,\nle plus ancien à porter ce nom."),
          view.namesake_origin);
    }
  }
}

// Le relevé de Sense porte l'état RÉEL de l'exemplaire croisé ; la base porte
// la référence. Un écart est une information, pas un défaut : on ne le montre
// donc QUE s'il y en a un.
void DrawSenseNote(const MonsterSheetView& view) {
  if (!view.sense.has_value() || view.sense->sprite_class != view.sprite_class) {
    return;
  }
  const MonsterSheetSenseView& sense = *view.sense;
  const bool differs = sense.hp != view.hp || sense.def != view.def || sense.mdef != view.mdef ||
                       sense.level != view.level;
  if (!differs) {
    return;
  }
  const MonsterSheetWords& words = view.words;
  const std::string def(words.def);
  const std::string mdef(words.mdef);
  Colored(kSenseNoteColor,
          Printed(core::Tr("Relevé Sense (exemplaire croisé) : niveau %u, PV %u, %s %u, %s %u"),
                  static_cast<unsigned>(sense.level), sense.hp, def.c_str(),
                  static_cast<unsigned>(sense.def), mdef.c_str(), static_cast<unsigned>(sense.mdef))
              .c_str());
  if (ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s", core::Tr("Ces chiffres viennent du monstre effectivement visé par "
                                     "Sense.\nIls peuvent différer de la base de données "
                                     "(modificateurs de serveur,\nGuerre d'Emperium, monstre "
                                     "invoqué par script)."));
  }
}

void DrawHeader(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                MonsterSheetAnswer& answer) {
  DrawSprite(state, view, answer);
  ImGui::SameLine();
  DrawIdentity(view, art, answer);
  DrawNamesakeNote(view, art, answer);
  DrawSenseNote(view);
}

// ── « CE QU'IL VOUS RAPPORTE » ──────────────────────────────────────────────
//
// 🔴 LE CHIFFRE VIENT DU SERVEUR, entièrement calculé. Le client ne multiplie
// rien : il ne connaît aucun des termes du gain. Le pour cent, en revanche, se
// calcule ici : c'est de l'arithmétique sur des chiffres déjà justes.
[[nodiscard]] std::string PercentOfBar(std::uint32_t got, std::uint32_t next) {
  if (next == 0) {
    return "-";
  }
  const std::uint64_t hundredths =
      static_cast<std::uint64_t>(got) * kHundredthsOfPercent / static_cast<std::uint64_t>(next);
  // Un gain réel sous le centième : « 0,00 % » se lirait comme « rien ».
  if (hundredths == 0 && got > 0) {
    return "< 0,01 %";
  }
  return Printed("%" PRIu64 ",%02" PRIu64 " %%", hundredths / kHundredthsPerPercent,
                 hundredths % kHundredthsPerPercent);
}

// Combien de monstres pour un niveau ENTIER : le client ne sait pas où en est
// la barre, et un chiffre qui fondrait à chaque monstre tué ne se comparerait
// plus d'une fiche à l'autre.
[[nodiscard]] std::string KillsForLevel(std::uint32_t got, std::uint32_t next) {
  if (got == 0 || next == 0) {
    return "-";
  }
  const std::uint64_t kills = (static_cast<std::uint64_t>(next) + got - 1) / got;
  return Printed("%" PRIu64, kills);
}

void DrawExpGain(const MonsterSheetView& view) {
  if (!view.exp_gain.has_value()) {
    return;
  }
  const MonsterSheetExpView& gain = *view.exp_gain;
  if (gain.base_exp == 0 && gain.job_exp == 0) {
    return;
  }
  ImGui::Spacing();
  Colored(kTitleColor, core::Tr("Ce qu'il vous rapporte"));
  if (!ImGui::BeginTable(
          "##mi_expgain", 4,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
    return;
  }
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  Label(core::Tr("EXP de base"));
  Help(core::Tr(
      "Ce que ce monstre vous donnerait VRAIMENT, et non la valeur de la base de données "
      "affichée plus haut.\n\nLe serveur applique tout ce qui pèse sur le gain : le taux du "
      "serveur, le malus de haut niveau, vos cartes et items qui augmentent l'EXP contre cette "
      "race ou cette classe, un Battle Manual actif, le statut VIP et un éventuel événement "
      "d'EXP.\n\nEstimation pour un monstre que vous tuez SEUL, en lui ayant infligé la "
      "totalité des dégâts. En groupe, ou en partageant les dégâts, votre part est plus "
      "petite ; certaines ZONES ont au contraire un multiplicateur d'EXP qui leur est propre."));
  ImGui::TableNextColumn();
  Colored(kBlueColor, GroupThousands(gain.base_exp).c_str());
  ImGui::TableNextColumn();
  if (gain.base_level_maxed) {
    Label(core::Tr("Niveau maximum"));
    ImGui::TableNextColumn();
    Colored(kGreenColor, core::Tr("atteint"));
  } else {
    Label(core::Tr("de votre niveau"));
    ImGui::TableNextColumn();
    Colored(kGreenColor, PercentOfBar(gain.base_exp, gain.next_base_exp).c_str());
  }

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  Label(core::Tr("EXP de job"));
  ImGui::TableNextColumn();
  Colored(kBlueColor, GroupThousands(gain.job_exp).c_str());
  ImGui::TableNextColumn();
  if (gain.job_level_maxed) {
    Label(core::Tr("Job maximum"));
    ImGui::TableNextColumn();
    Colored(kGreenColor, core::Tr("atteint"));
  } else {
    Label(core::Tr("de votre job"));
    ImGui::TableNextColumn();
    Colored(kGreenColor, PercentOfBar(gain.job_exp, gain.next_job_exp).c_str());
  }

  if (!gain.base_level_maxed || !gain.job_level_maxed) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    Label(core::Tr("Pour un niveau"));
    Help(
        core::Tr("Nombre de ces monstres à tuer pour un niveau COMPLET, pas pour terminer "
                 "celui que vous avez entamé."));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(
        KillsForLevel(gain.base_exp, gain.base_level_maxed ? 0 : gain.next_base_exp).c_str());
    ImGui::TableNextColumn();
    Label(core::Tr("Pour un job"));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(
        KillsForLevel(gain.job_exp, gain.job_level_maxed ? 0 : gain.next_job_exp).c_str());
  }
  ImGui::EndTable();
  // Daté du moment où le serveur l'a calculé : un changement de niveau le fait
  // redemander, un buff d'EXP pris entre-temps non.
  LabelWrapped(
      core::Tr("Calculé par le serveur pour votre personnage, en supposant que vous "
               "le tuez seul. Hors partage de groupe et hors bonus de zone."));
}

// Une ligne « libellé, valeur, libellé, valeur ».
void StatRow(const char* first_label, const std::string& first, const char* second_label,
             const std::string& second) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  Label(first_label);
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(first.c_str());
  ImGui::TableNextColumn();
  Label(second_label);
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(second.c_str());
}

// UNE CASE D'EXP : l'EXP RÉELLE — celle que le serveur donne, ses taux
// appliqués —, et au survol l'EXP écrite dans la base avec le taux qui la
// multiplie, quand la base de données les connaît.
void ExpCell(std::uint32_t real, std::uint32_t written, std::int64_t rate_percent, bool known) {
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(GroupThousands(real).c_str());
  if (known && ImGui::IsItemHovered()) {
    SkinTooltipFormat(core::Tr("EXP écrite dans la base : %s\nTaux du serveur : %lld %%"),
                      GroupThousands(written).c_str(), static_cast<long long>(rate_percent));
  }
}

void ExpRows(const MonsterSheetView& view) {
  const bool known = view.written_exp.has_value();
  const MonsterSheetWrittenExpView written =
      view.written_exp.value_or(MonsterSheetWrittenExpView{});
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  Label(core::Tr("EXP de base"));
  ExpCell(view.base_exp, written.base_exp, written.base_rate_percent, known);
  ImGui::TableNextColumn();
  Label(core::Tr("EXP de job"));
  ExpCell(view.job_exp, written.job_exp, written.job_rate_percent, known);
}

// ⚠ `speed` EST LE TEMPS DE MARCHE D'UNE CASE : plus il est petit, plus le
// monstre est rapide. On le retourne en cases par seconde, l'unité dans
// laquelle un joueur pense sa fuite.
[[nodiscard]] std::string WalkSpeed(int walk_ms_per_cell) {
  if (walk_ms_per_cell <= 0) {
    return core::Tr("immobile");
  }
  const auto per_cell = static_cast<unsigned>(walk_ms_per_cell);
  const unsigned tenths = (kTenthCellMilliseconds + per_cell / 2) / per_cell;
  return Printed(core::Tr("%u,%u case/s"), tenths / kTenthsPerPercent, tenths % kTenthsPerPercent);
}

void DrawVersus(const MonsterSheetView& view, int mob_hit, int mob_flee) {
  if (!view.own.has_value()) {
    return;
  }
  const MonsterSheetOwnView& own = *view.own;
  ImGui::Spacing();
  Colored(kTitleColor, core::Tr("Face à votre personnage"));
  if (!ImGui::BeginTable(
          "##mi_versus", 2,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
    return;
  }
  const auto line = [](const char* label, const char* help, const std::string& value, ImU32 color) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    Label(label);
    Help(help);
    ImGui::TableNextColumn();
    Colored(color, value.c_str());
  };

  const int you_hit = HitChancePercent(own.hit, mob_flee);
  line(core::Tr("Vous le touchez"),
       core::Tr("Chance qu'une attaque normale porte.\n\n80 % + votre HIT - le FLEE du monstre, "
                "bornée entre 5 et 100 %.\n\nVotre HIT vaut votre NIVEAU DE BASE + votre DEX : "
                "la moitié du calcul vient donc de votre niveau, pas de vos points. Le serveur "
                "plafonne la part du niveau (600 actuellement), au-delà seule la DEX compte "
                "encore.\n\nNe tient compte ni des skills, ni des cartes conditionnelles."),
       Printed("%d %%", you_hit),
       you_hit >= kHitSurePercent ? kGreenColor
                                  : (you_hit <= kHitPoorPercent ? kRedColor : kAmberColor));

  // 🔴 BORNÉ À 100 % : le jet est « tirage sur mille < critique », et afficher
  // « 314 % » laisserait croire à un multiplicateur de DÉGÂTS.
  int crit_tenths = own.critical_percent * kTenthsPerPercent - view.luk * kCritTenthsPerLuk;
  crit_tenths =
      crit_tenths < 0 ? 0 : (crit_tenths > kCritMostTenths ? kCritMostTenths : crit_tenths);
  line(core::Tr("Vos critiques sur lui"),
       core::Tr("Chance qu'une attaque soit un coup critique. Ce n'est PAS un multiplicateur de "
                "dégâts.\n\nVotre CRIT moins 0,2 point par LUK du monstre. Un critique remplace "
                "le jet de précision (il ne rate jamais) et ignore la DEF."),
       Printed("%d,%d %%", crit_tenths / kTenthsPerPercent, crit_tenths % kTenthsPerPercent),
       crit_tenths > 0 ? kBlueColor : kLabelColor);

  // L'esquive parfaite est DÉDUITE du chiffre : elle se joue avant tout le
  // reste et annule l'attaque. Deux lignes feraient faire la multiplication au
  // lecteur.
  const int raw = HitChancePercent(mob_hit, own.flee);
  int dodge = own.perfect_dodge_percent;
  dodge = dodge < 0 ? 0 : (dodge > kPercent ? kPercent : dodge);
  const int net = raw * (kPercent - dodge) / kPercent;
  line(core::Tr("Il vous touche"),
       core::Tr("Chance qu'une de ses attaques vous atteigne réellement, esquive parfaite "
                "déduite.\n\n80 % + HIT du monstre - votre FLEE, bornée entre 5 et 100 %, puis "
                "amputée de votre esquive parfaite, qui annule l'attaque avant tout jet (elle "
                "bloque même un critique).\n\nVotre FLEE vaut votre NIVEAU DE BASE + votre AGI : "
                "monter de niveau vous fait esquiver davantage, sans dépenser un point. Le "
                "serveur plafonne la part du niveau (600 actuellement), au-delà seule l'AGI "
                "compte encore.\n\nVotre FLEE chute par ailleurs de 10 % par monstre au-delà du "
                "deuxième qui vous prend pour cible : en pack, le chiffre réel est plus élevé."),
       Printed("%d %%", net),
       net <= kStruckRarelyPercent ? kGreenColor
                                   : (net >= kStruckOftenPercent ? kRedColor : kAmberColor));
  ImGui::EndTable();
}

void DrawStatsTab(const MonsterSheetView& view) {
  if (ImGui::BeginTable(
          "##mi_stats", 4,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
    // Les milliers ne se séparent que sur ce qui atteint le million : PV, SP,
    // EXP. Partout ailleurs ce serait du bruit.
    StatRow(core::Tr("PV"), GroupThousands(view.hp), core::Tr("SP"), GroupThousands(view.sp));
    ExpRows(view);
    StatRow(core::Tr("ATK"), Printed("%d ~ %d", view.atk_min, view.atk_max), core::Tr("MATK"),
            Printed("%d ~ %d", view.matk_min, view.matk_max));
    const std::string def(view.words.def);
    const std::string mdef(view.words.mdef);
    StatRow(def.c_str(), Printed("%d%% (+%d)", view.def, view.def_bonus), mdef.c_str(),
            Printed("%d%% (+%d)", view.mdef, view.mdef_bonus));
    StatRow(core::Tr("STR"), Printed("%d", view.str), core::Tr("AGI"), Printed("%d", view.agi));
    StatRow(core::Tr("VIT"), Printed("%d", view.vit), core::Tr("INT"),
            Printed("%d", view.int_stat));
    StatRow(core::Tr("DEX"), Printed("%d", view.dex), core::Tr("LUK"), Printed("%d", view.luk));
    StatRow(core::Tr("Portée d'attaque"),
            Printed(core::Tr("%u case(s)"), static_cast<unsigned>(view.attack_range_cells)),
            core::Tr("Vitesse de dépl."), WalkSpeed(view.walk_ms_per_cell));
    if (view.mvp_exp > 0) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      Label(core::Tr("EXP MVP"));
      ExpCell(view.mvp_exp, view.written_exp.has_value() ? view.written_exp->mvp_exp : 0,
              view.written_exp.has_value() ? view.written_exp->mvp_rate_percent : 0,
              view.written_exp.has_value());
    }
    ImGui::EndTable();
  }

  // Les lignes d'EXP ci-dessus sont celles que le serveur donne à tous, ses
  // taux appliqués. Ce que le joueur, lui, en tirerait vient juste après.
  DrawExpGain(view);

  const int mob_hit = view.level + view.dex;
  const int mob_flee = view.level + view.agi;
  ImGui::Spacing();
  Colored(kTitleColor, core::Tr("Précision et esquive"));
  if (ImGui::BeginTable(
          "##mi_hitflee", 4,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    Label(core::Tr("HIT"));
    Help(
        core::Tr("Précision du monstre, comparée à votre FLEE.\n\nHIT = niveau du monstre + sa "
                 "DEX."));
    ImGui::TableNextColumn();
    ImGui::Text("%d", mob_hit);
    ImGui::TableNextColumn();
    Label(core::Tr("FLEE"));
    Help(
        core::Tr("Esquive du monstre, comparée à votre HIT.\n\nFLEE = niveau du monstre + son "
                 "AGI."));
    ImGui::TableNextColumn();
    ImGui::Text("%d", mob_flee);

    // 🔴 UN MONSTRE NE FAIT JAMAIS DE CRITIQUE SUR CE SERVEUR, ni d'esquive
    // parfaite : les deux sont réservés aux joueurs. Afficher un « CRIT du
    // monstre » serait faux ; on dit pourquoi.
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    Label(core::Tr("Critique"));
    Help(
        core::Tr("Sur ce serveur, un monstre ne fait JAMAIS de coup critique : la mécanique est "
                 "réservée aux joueurs et aux mercenaires. Ce n'est pas une donnée manquante, "
                 "c'est zéro par conception."));
    ImGui::TableNextColumn();
    Colored(kGreenColor, core::Tr("aucun"));
    ImGui::TableNextColumn();
    Label(core::Tr("Esquive parfaite"));
    Help(
        core::Tr("De même, un monstre n'a pas d'esquive parfaite : elle est réservée aux "
                 "joueurs."));
    ImGui::TableNextColumn();
    Colored(kGreenColor, core::Tr("aucune"));
    ImGui::EndTable();
  }
  DrawVersus(view, mob_hit, mob_flee);
}

void DrawTabs(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
              MonsterSheetAnswer& answer) {
  if (!BeginSkinTabBar("##monsterinfo_tabs", art)) {
    return;
  }
  if (ImGui::BeginTabItem(core::Tr("Stats"))) {
    DrawStatsTab(view);
    ImGui::EndTabItem();
  }
  if (view.resist_known && ImGui::BeginTabItem(core::Tr("Résistances"))) {
    DrawResistTab(view);
    ImGui::EndTabItem();
  }
  // ⚠ LE COMPTE FAIT PARTIE DU LIBELLÉ, l'identité non : `###` la fige, sans
  // quoi l'onglet choisi sauterait à chaque fiche d'une autre longueur.
  const std::string drops = Printed("Drops (%zu)###mi_drops", view.drops.size());
  if (ImGui::BeginTabItem(drops.c_str())) {
    DrawDropsTab(state, view, art, answer);
    ImGui::EndTabItem();
  }
  const std::string spawns = Printed("Spawns (%zu)###mi_spawns", view.spawns.size());
  if (ImGui::BeginTabItem(spawns.c_str())) {
    DrawSpawnsTab(state, view, art, answer);
    ImGui::EndTabItem();
  }
  if (!view.skills.empty()) {
    const std::string skills = Printed("Skills (%zu)###mi_skills", view.skills.size());
    if (ImGui::BeginTabItem(skills.c_str())) {
      DrawSkillsTab(state, view, art, answer);
      ImGui::EndTabItem();
    }
  }
  EndSkinTabBar();
}

}  // namespace

}  // namespace monster_sheet

MonsterSheetAnswer DrawMonsterSheetWindow(MonsterSheetWindowState& state,
                                          const MonsterSheetView& view, const SkinArt& art) {
  using namespace monster_sheet;
  MonsterSheetAnswer answer;
  if (!state.open) {
    return answer;
  }

  // Le titre est le NOM du monstre, comme la description d'un objet porte celui
  // de l'objet. ⚠ `###` FIGE L'IDENTITÉ : changer de monstre ne déplace ni ne
  // retaille la fenêtre.
  const std::string_view named = view.phase == MonsterSheetPhase::kReady && !view.name.empty()
                                     ? std::string_view(view.name)
                                     : std::string_view();
  const std::size_t language = core::LanguageSerial();
  if (state.title.empty() || named != state.title_for || language != state.title_language) {
    state.title_for = std::string(named);
    state.title_language = language;
    state.title = named.empty() ? std::string(core::Tr("Fiche de monstre")) : state.title_for;
    state.title += "###";
    state.title += kMonsterSheetWindowId;
  }

  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  if (BeginSkinnedWindow(state.title.c_str(), &state.open, art, SkinnedTitle::kDescription,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing)) {
    answer = DrawMonsterSheetBody(state, view, art);
  }
  EndSkinnedWindow();
  return answer;
}

MonsterSheetAnswer DrawMonsterSheetBody(MonsterSheetWindowState& state,
                                        const MonsterSheetView& view, const SkinArt& art) {
  using namespace monster_sheet;
  MonsterSheetAnswer answer;
  switch (view.phase) {
    case MonsterSheetPhase::kAsking:
      Label(core::Tr("Interrogation du serveur..."));
      break;
    case MonsterSheetPhase::kUnknown:
      Colored(kRedColor,
              Printed(core::Tr("Monstre inconnu du serveur (classe %u)."), view.shown_id).c_str());
      // Le relevé de Sense, s'il vise ce monstre : mieux que rien.
      if (view.sense.has_value() && view.sense->sprite_class == view.shown_id) {
        ImGui::Separator();
        ImGui::Text(core::Tr("Relevé Sense : niveau %u, PV %u, DEF %u, MDEF %u"),
                    static_cast<unsigned>(view.sense->level), view.sense->hp,
                    static_cast<unsigned>(view.sense->def),
                    static_cast<unsigned>(view.sense->mdef));
      }
      break;
    case MonsterSheetPhase::kReady:
      DrawHeader(state, view, art, answer);
      ImGui::Separator();
      DrawTabs(state, view, art, answer);
      break;
  }
  return answer;
}

}  // namespace ro::ui
