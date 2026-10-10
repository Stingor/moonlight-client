#include "ro/ui/skill_window.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "skill_cell.h"
#include "skill_chain.h"
#include "skill_list.h"

namespace ro::ui {
namespace {

// ── LA GRILLE, AUX MESURES DU CLIENT ────────────────────────────────────────
// Sept colonnes, et l'index de case vient des tables : ces nombres décident donc
// de la place, pas de l'apparence. Les changer désaligne l'arbre.
constexpr int kColumns = 7;
constexpr float kCellWidth = 80.0F;
constexpr float kCellHeight = 86.0F;

// 🔴 LE BITMAP D'UNE ICÔNE FAIT VINGT-QUATRE PIXELS, et c'est de LUI que dépend
// la netteté, pas du mode d'échantillonnage. Agrandi de moitié — quarante pixels
// pour vingt-quatre —, un pixel sur trois est doublé et les deux autres ne le
// sont pas : l'icône vient inégale, quel que soit le filtre. Au plus proche on
// voit les marches, interpolé on voit le flou.
//
// Le DOUBLE EXACT n'a pas ce défaut : chaque pixel de la source devient un carré
// de quatre, tous pareils. C'est ce qui fait tenir la case à quatre-vingts
// pixels de large et quatre-vingt-six de haut plutôt qu'aux soixante-douze sur
// soixante-dix-huit d'une icône de quarante.
constexpr float kIconBitmapPixels = 24.0F;
constexpr float kIconSize = 2.0F * kIconBitmapPixels;
// Ce qui sépare la case dessinée de sa voisine.
constexpr float kCellGap = 10.0F;
// La hauteur du haut de l'icône dans la case ; le rang s'écrit dessous.
constexpr float kIconTop = 7.0F;
constexpr float kLevelGap = 2.0F;
// La bande réservée au trait qui sépare deux arbres fusionnés. Sans elle, le
// trait mordrait sur les icônes voisines.
constexpr float kSplitBand = 22.0F;
constexpr float kCellRounding = 4.0F;

// ── LES COULEURS, REPRISES TELLES QUELLES ───────────────────────────────────
// Le survol pose un VOILE, pas un fond : c'est un retour de curseur, et la case
// du client n'a pas de fond du tout.
constexpr ImU32 kHoverVeil = IM_COL32(255, 255, 255, 28);
// La case survolée, et celles que la même branche relie.
constexpr ImU32 kFocusOutline = IM_COL32(255, 205, 105, 220);
constexpr ImU32 kLinkedOutline = IM_COL32(120, 160, 255, 200);
// Un rang de plus est payable tout de suite.
constexpr ImU32 kRaisableOutline = IM_COL32(120, 200, 130, 170);
constexpr float kFocusThickness = 2.0F;
constexpr float kRaisableThickness = 1.5F;
// L'icône d'une compétence NON APPRISE est assombrie, comme au client, qui la
// blitte en mode grisé tant que le rang vaut zéro.
constexpr ImU32 kUnlearnedTint = IM_COL32(105, 105, 115, 165);
// Le rang, sous l'icône. Noir dès qu'elle est apprise, gris sinon, ambre quand
// un point est réservé.
constexpr ImU32 kReservedLevelColor = IM_COL32(150, 95, 0, 255);
constexpr ImU32 kLearnedLevelColor = IM_COL32(20, 20, 25, 255);
constexpr ImU32 kUnlearnedLevelColor = IM_COL32(110, 110, 122, 255);
// ⚠ UN LISERÉ CLAIR AUTOUR DU RANG, et il n'est pas décoratif : le texte passe
// parfois sur le bas d'une icône, et sans contour il devient illisible pile à
// cet endroit.
constexpr ImU32 kLevelHalo = IM_COL32(255, 255, 255, 190);
// ── LE TEMPS D'ATTENTE, À LA CONVENTION DE LA BARRE DE RACCOURCIS ──────────
//
// 🔴 LA MÊME QUE LA BARRE, ET C'EST TOUT L'INTÉRÊT : un voile noir qui MONTE du
// bas à mesure que le temps passe, et le décompte au centre. Un joueur qui
// connaît sa barre lit la grille sans rien réapprendre.
//
// ⚠ LE VOILE EST SUR L'ICÔNE, pas sur la case : c'est l'icône qui dit quelle
// compétence attend, et le rang écrit dessous doit rester lisible.
constexpr ImU32 kCooldownVeil = IM_COL32(0, 0, 0, 150);
constexpr float kCooldownRounding = 3.0F;
constexpr ImU32 kCooldownTextColor = IM_COL32(255, 235, 150, 255);
constexpr ImU32 kCooldownTextShadow = IM_COL32(0, 0, 0, 200);
constexpr float kCooldownShadowPixels = 1.0F;
constexpr std::uint32_t kMillisecondsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;

// Le trait qui sépare deux arbres fusionnés, et son libellé.
constexpr ImU32 kSplitLineColor = IM_COL32(200, 60, 60, 200);
constexpr ImU32 kSplitLabelColor = IM_COL32(170, 40, 40, 255);
constexpr float kSplitThickness = 2.0F;
constexpr float kSplitLabelInset = 14.0F;
constexpr float kSplitLabelMargin = 6.0F;
// Les nombres de l'en-tête.
constexpr ImVec4 kPointsFreeColor(0.30F, 0.75F, 0.35F, 1.0F);
constexpr ImVec4 kPointsReservedColor(0.85F, 0.65F, 0.20F, 1.0F);
constexpr float kFilterWidthPixels = 120.0F;
// Le champ de points de la ligne du staff, mesuré sur le plus grand nombre qu'il
// accepte plutôt que sur un compte de pixels : la police change, pas le plafond.
constexpr const char* kStaffPointsWidest = "99999";
// LE MESSAGE DU MODE CLAVIER : combien de temps il se lit, et sa bulle.
constexpr double kKeyboardNoticeSeconds = 2.0;
constexpr float kKeyboardNoticePadding = 4.0F;
constexpr float kKeyboardNoticeOffset = 12.0F;
constexpr float kKeyboardNoticeRounding = 3.0F;
constexpr ImU32 kKeyboardNoticeBody = IM_COL32(15, 15, 20, 215);
constexpr ImU32 kKeyboardNoticeText = IM_COL32(255, 215, 130, 255);

// L'échelle courante : c'est elle qui décide de la taille des cases, comme elle
// décide de celle du texte.
[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Une recherche sans casse, sur des vues : le filtre se tape en minuscules et
// les noms du client sont capitalisés.
[[nodiscard]] bool Contains(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > haystack.size()) {
    return false;
  }
  const auto same = [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  };
  return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), same) !=
         haystack.end();
}

// La case survolée est-elle reliée à celle-ci ? Dans les deux sens : ce qu'elle
// réclame, et ce qu'elle ouvre.
[[nodiscard]] bool LinkedToFocus(const SkillCellView& cell, const SkillCellView* focus) {
  if (focus == nullptr || focus->skill_id == cell.skill_id) {
    return false;
  }
  for (const SkillNeedView& need : cell.needs) {
    if (need.skill_id == focus->skill_id) {
      return true;
    }
  }
  for (const SkillNeedView& need : focus->needs) {
    if (need.skill_id == cell.skill_id) {
      return true;
    }
  }
  return false;
}

// IMPOSE LE MODE D'ÉCHANTILLONNAGE DES ICÔNES, pour la suite de la liste de
// dessin.
//
// 🔴 LE MODE NET NE SE DÉDUIT PAS DE LA TEXTURE, IL S'IMPOSE. Le moteur de rendu
// d'ImGui lie un OBJET SAMPLER avant chaque lot de commandes, et un sampler lié
// ÉCRASE les paramètres de filtrage de la texture : le `GL_NEAREST` posé au
// téléversement ne décide de rien ici. Le sampler ambiant est le LINÉAIRE — donc
// ne rien faire, c'est lisser.
//
// 🔴 ET C'EST POURQUOI ON POSE LE CALLBACK DANS LES DEUX CAS, pas seulement
// quand le lissage est demandé : sans lui, la case à cocher paraîtrait morte
// puisque son état décoché donnerait déjà ce que fait son état coché.
//
// ⚠ IL FAUT LE RENDRE, ET C'EST `RestoreSampler` : la liste de dessin continue
// après la grille — l'ascenseur, l'habillage de la fenêtre — et hériterait sinon
// du mode qu'on vient de poser.
void ImposeIconSampler(ImDrawList* into, bool smooth) {
  const ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
  const ImDrawCallback wanted =
      smooth ? platform.DrawCallback_SetSamplerLinear : platform.DrawCallback_SetSamplerNearest;
  // ⚠ UN MOTEUR DE RENDU PEUT NE PAS LES OFFRIR : on dessine alors comme avant,
  // sans rien casser.
  if (wanted != nullptr) {
    into->AddCallback(wanted, nullptr);
  }
}

void RestoreSampler(ImDrawList* into) {
  const ImDrawCallback ambient = ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear;
  if (ambient != nullptr) {
    into->AddCallback(ambient, nullptr);
  }
}

// LE CÔTÉ D'UNE ICÔNE POUR QU'ELLE RESTE NETTE : le plus grand MULTIPLE ENTIER
// du bitmap qui tienne dans la place voulue.
//
// 🔴 CE N'EST PAS LA PLACE VOULUE ELLE-MÊME, et c'est tout l'objet de cette
// fonction. L'échelle d'interface multiplie tout : à 1,25 la case veut soixante
// pixels d'icône, soit deux fois et demie le bitmap — et la demie revient à
// doubler une ligne sur deux. On retombe alors au double, en laissant la
// différence en marge autour de l'icône.
//
// ⚠ JAMAIS EN DESSOUS DU BITMAP : à une échelle réduite, la troncature rendrait
// zéro, donc une icône invisible.
[[nodiscard]] float CrispIconSide(float wanted) {
  const float steps = std::floor(wanted / kIconBitmapPixels);
  return std::max(1.0F, steps) * kIconBitmapPixels;
}

// LE TEMPS D'ATTENTE D'UNE CASE : le voile qui monte, puis le décompte.
//
// ⚠ `cooldown_ms` NUL SOUS UN VOILE PRÉSENT N'EST PAS UNE FAUTE : le serveur
// annonce parfois une attente sans durée exploitable. On garde le voile — il dit
// « pas maintenant » — et on se tait sur le combien.
void PaintCooldown(ImDrawList* into, const SkillCellView& cell, ImVec2 icon_at, float side) {
  if (cell.cooldown <= 0.0F) {
    return;
  }
  const float veil = side * cell.cooldown;
  into->AddRectFilled(ImVec2(icon_at.x, icon_at.y + side - veil),
                      ImVec2(icon_at.x + side, icon_at.y + side), kCooldownVeil, kCooldownRounding);
  if (cell.cooldown_ms == 0) {
    return;
  }
  std::array<char, 16> left{};
  if (cell.cooldown_ms >= kSecondsPerMinute * kMillisecondsPerSecond) {
    // ⚠ « 143 » NE SE LIT PAS COMME « 2:23 » : passé la minute, on écrit m:ss.
    std::snprintf(left.data(), left.size(), "%u:%02u",
                  cell.cooldown_ms / (kSecondsPerMinute * kMillisecondsPerSecond),
                  (cell.cooldown_ms / kMillisecondsPerSecond) % kSecondsPerMinute);
  } else {
    // ⚠ ARRONDI AU-DESSUS : une attente qui affiche « 0 » pendant une seconde
    // entière se lit « c'est prêt », et le clic part pour rien.
    std::snprintf(left.data(), left.size(), "%u",
                  (cell.cooldown_ms + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
  }
  const ImVec2 size = ImGui::CalcTextSize(left.data());
  const ImVec2 at(icon_at.x + (side - size.x) * 0.5F, icon_at.y + (side - size.y) * 0.5F);
  into->AddText(ImVec2(at.x + kCooldownShadowPixels, at.y + kCooldownShadowPixels),
                kCooldownTextShadow, left.data());
  into->AddText(at, kCooldownTextColor, left.data());
}

// Le rang, écrit sous l'icône avec son liseré.
//
// ⚠ `icon_bottom` EST CELUI DE L'ICÔNE DESSINÉE, et il est donné plutôt que
// recalculé : elle est ramenée au multiple entier du bitmap, donc plus courte
// que la place que la case lui réserve. Le recomposer des constantes écrirait le
// rang dans le vide sous elle.
void PaintLevel(ImDrawList* into, const SkillCellView& cell, ImVec2 cell_top_left, float scale,
                float icon_bottom) {
  char text[16] = {};
  if (cell.max_level > 0) {
    std::snprintf(text, sizeof(text), "%d/%d", cell.effective_level(), cell.max_level);
  } else {
    std::snprintf(text, sizeof(text), "%d", cell.effective_level());
  }
  const ImVec2 size = ImGui::CalcTextSize(text);
  const ImVec2 at(cell_top_left.x + ((kCellWidth - kCellGap) * scale - size.x) * 0.5F,
                  icon_bottom + kLevelGap * scale);
  const ImU32 color = cell.reserved > 0            ? kReservedLevelColor
                      : cell.effective_level() > 0 ? kLearnedLevelColor
                                                   : kUnlearnedLevelColor;
  AddTextCrossHalo(into, at, color, text, kLevelHalo);
}

// Ce que l'infobulle dit d'une case : tout ce que le client éparpille entre la
// case, son survol et sa fenêtre de description.
// L'ordonnée d'une ligne : CHAQUE trait de séparation au-dessus d'elle a besoin
// de sa propre bande, sans quoi il mordrait sur les icônes voisines.
[[nodiscard]] float RowTop(float origin_y, int row, std::span<const SkillSplitView> splits,
                           float scale) {
  float y = origin_y + static_cast<float>(row) * kCellHeight * scale;
  for (const SkillSplitView& split : splits) {
    if (row >= split.row) {
      y += kSplitBand * scale;
    }
  }
  return y;
}

void PaintSplits(ImDrawList* into, const SkillGroupView& group, ImVec2 origin, float scale) {
  const float left = origin.x;
  const float right = origin.x + (static_cast<float>(kColumns) * kCellWidth - kCellGap) * scale;
  const float margin = kSplitLabelMargin * scale;
  for (const SkillSplitView& split : group.splits) {
    // Au MILIEU de la bande que `RowTop` a réservée pour lui.
    const float y = RowTop(origin.y, split.row, group.splits, scale) - kSplitBand * scale * 0.5F;
    if (split.label.empty()) {
      into->AddLine(ImVec2(left, y), ImVec2(right, y), kSplitLineColor, kSplitThickness);
      continue;
    }
    const ImVec2 size =
        ImGui::CalcTextSize(split.label.data(), split.label.data() + split.label.size());
    const float text_x = left + kSplitLabelInset * scale;
    into->AddLine(ImVec2(left, y), ImVec2(text_x - margin, y), kSplitLineColor, kSplitThickness);
    into->AddLine(ImVec2(text_x + size.x + margin, y), ImVec2(right, y), kSplitLineColor,
                  kSplitThickness);
    into->AddText(ImVec2(text_x, y - size.y * 0.5F), kSplitLabelColor, split.label.data(),
                  split.label.data() + split.label.size());
  }
}

// ── L'EN-TÊTE ───────────────────────────────────────────────────────────────
void DrawHeader(SkillWindowState& state, SkillPointTally points, const SkinArt& art,
                SkillWindowAction& action, bool anything_reserved) {
  ImGui::TextColored(points.left > 0 ? kPointsFreeColor : ImGui::GetStyle().Colors[ImGuiCol_Text],
                     core::Tr("Points : %d"), points.left);
  if (points.reserved > 0) {
    ImGui::SameLine();
    ImGui::TextColored(kPointsReservedColor, core::Tr("(%d réservé%s)"), points.reserved,
                       points.reserved > 1 ? "s" : "");
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kFilterWidthPixels * Scale());
  ImGui::InputTextWithHint("##filtre", core::Tr("Rechercher..."), state.filter,
                           sizeof(state.filter));
  // ── LA BASCULE ENTRE LES DEUX VUES ────────────────────────────────────────
  //
  // ⚠ LE BOUTON PORTE LA VUE COURANTE, pas celle qu'il donnerait : c'est le
  // plus lisible — on lit d'abord où l'on est.
  ImGui::SameLine();
  if (SkinButton(state.grid ? core::Tr("Grille") : core::Tr("Liste"), art)) {
    state.grid = !state.grid;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Bascule entre la grille d'icônes, à la place du client, et la liste "
                 "détaillée — niveau, coût, portée, prérequis."));
  }

  if (anything_reserved) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Appliquer"), art)) {
      action.apply = true;
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      action.cancel = true;
    }
  }

  // ── LA SECONDE LIGNE : LES GESTES, ET LE LISSAGE ──────────────────────────
  //
  // 🔴 « RACCOURCIS » EN TEXTE DISCRET, ET NON UN PAVÉ PERMANENT. Huit gestes
  // occupent huit lignes au-dessus de la grille ; on les découvre une fois, et
  // la place, elle, sert tous les jours. Sous le survol, ils sont là quand on
  // les cherche — et c'est le seul endroit où ils soient ÉCRITS : un geste que
  // rien n'annonce n'est pas un raccourci, c'est un secret.
  ImGui::TextDisabled("%s", core::Tr("Raccourcis"));
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Clic gauche          réserve un point (et ses prérequis manquants)\n"
                 "Ctrl + clic gauche   réserve jusqu'au niveau maximum\n"
                 "Double-clic          lance le skill (aucun point réservé)\n"
                 "Clic droit           menu (monter, lancer, niveau d'utilisation, "
                 "description)\n"
                 "Ctrl + clic droit    description du skill\n"
                 "Molette              règle le niveau auquel le skill part\n"
                 "Glisser              pose le skill sur une barre d'action\n"
                 "Survol               flèches de prérequis (ambre) et de suites (bleu)\n"
                 "Mode clavier         survol + touche tenue : pose le skill sur sa case\n"
                 "\n"
                 "Rien n'est envoyé au serveur tant que « Appliquer » n'est pas cliqué."));
  }
  // LE MODE CLAVIER, dans les deux vues : la liste aussi se survole.
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  static_cast<void>(SkinCheckBox(core::Tr("Mode clavier"), &state.keyboard_layout, art));
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Souris sur un skill, puis tiens une touche une demi-seconde : le skill "
                 "se pose dans la case de barre liée à cette touche.\n"
                 "Les cases qui portent déjà le skill survolé s'éclairent."));
  }
  // ⚠ LA CASE N'EXISTE QU'EN GRILLE : voir `SkillWindowState::smooth_icons`. En
  // liste, l'icône fait une hauteur de ligne et il n'y a rien à lisser.
  if (state.grid) {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    static_cast<void>(SkinCheckBox(core::Tr("Lisser les icônes"), &state.smooth_icons, art));
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("Filtrage bilinéaire des icônes de la grille.\n"
                   "Décoché (défaut) = pixels nets : l'icône se dessine au double "
                   "exact de son bitmap, et le client natif ne filtre pas non plus."));
    }
  }
}

// ── LA LIGNE DU STAFF ───────────────────────────────────────────────────────
//
// LA CRÉATION D'UN HOMONCULE, sur sa propre ligne : une liste de ceux que le
// serveur sait créer, et le bouton qui demande celui qu'on a choisi.
//
// ⚠ GRISÉ QUAND LE CLIENT SAIT QUE LE PERSONNAGE EN A DÉJÀ UN : le serveur
// refuserait. Sinon l'infobulle prévient que le serveur peut refuser quand
// même — un homoncule au repos ou mort à la connexion n'est pas annoncé.
void DrawMakeHomunculus(SkillWindowState& state, const GrimoireStaffView& staff, const SkinArt& art,
                        SkillWindowAction& action) {
  if (staff.homunculi.empty()) {
    return;
  }
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kStaffHomunculusComboEms);
  static_cast<void>(
      StaffHomunculusCombo("##staff_homunculus", &state.staff_homunculus, staff.homunculi, art));
  ImGui::SameLine();
  ImGui::BeginDisabled(staff.has_homunculus);
  if (SkinSmallButton(core::Tr("Créer"), art)) {
    action.staff_order = StaffSkillOrder::kMakeHomunculus;
    action.staff_homunculus_job = StaffHomunculusJobAt(staff.homunculi, state.staff_homunculus);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", staff.has_homunculus
                                ? core::Tr("@makehomun : le personnage a déjà un homoncule, le "
                                           "serveur n'en crée pas un second.")
                                : core::Tr("@makehomun : crée l'homoncule choisi. Le "
                                           "serveur refuse si le personnage en a déjà un, "
                                           "même au repos ou mort."));
  }
}

// LA LIGNE ELLE-MÊME : quatre atcommands qui REFONT l'arbre, un champ pour le
// cinquième, puis la création d'un homoncule.
//
// 🔴 LE DROIT SE RELIT ICI, À CHAQUE IMAGE, et pas une fois à l'entrée en jeu :
// un droit retiré en cours de session doit faire disparaître la ligne sans
// attendre une reconnexion. C'est le même choix que pour l'établi du staff.
//
// ⚠ ET IL N'EST QUE D'AFFICHAGE. Le serveur revalide chaque commande selon le
// groupe du compte ; ceci décide de ce qui se MONTRE, pas de ce qui est permis.
void DrawStaffRow(SkillWindowState& state, bool staff_allowed, const GrimoireStaffView& staff,
                  const SkinArt& art, SkillWindowAction& action) {
  if (!staff_allowed) {
    return;
  }
  ImGui::TextDisabled("%s", core::Tr("Staff"));
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Reset"), art)) {
    action.staff_order = StaffSkillOrder::kForgetAll;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("@resetskill : oublie tous les skills et rend les points."));
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Tout apprendre"), art)) {
    action.staff_order = StaffSkillOrder::kLearnAll;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("@allskills : apprend tous les skills de la classe."));
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Cooldowns"), art)) {
    action.staff_order = StaffSkillOrder::kClearCooldowns;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("@resetcooltime : remet à zéro le temps de recharge de tous les skills (homoncule "
                 "et mercenaire compris)."));
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::CalcTextSize(kStaffPointsWidest).x +
                          ImGui::GetStyle().FramePadding.x * 2.0F);
  // ⚠ PAS DE PAS : les flèches d'ImGui ne sont pas habillées, et une flèche nue
  // au milieu d'une ligne habillée se voit plus que le champ.
  ImGui::InputInt("##points_staff", &state.staff_points, /*step=*/0, /*step_fast=*/0);
  state.staff_points =
      std::clamp(state.staff_points, kStaffSkillPointsLeast, kStaffSkillPointsMost);
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Points"), art)) {
    action.staff_order = StaffSkillOrder::kGivePoints;
    action.staff_points = state.staff_points;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("@skpoint : donne le nombre de points de skill saisi."));
  }
  DrawMakeHomunculus(state, staff, art, action);
}

// ── LA GRILLE ───────────────────────────────────────────────────────────────
void DrawGrid(SkillWindowState& state, const SkillGroupView& group, std::string_view filter,
              const SkinArt& art, bool staff_allowed, SkillWindowAction& action) {
  const float scale = Scale();
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float cell_w = kCellWidth * scale;
  const float cell_h = kCellHeight * scale;
  const float inner = (kCellWidth - kCellGap) * scale;
  // ⚠ UNE FOIS POUR LA GRILLE : le côté net ne dépend que de l'échelle, et le
  // recalculer par case donnerait le même nombre cinquante fois.
  const float icon_side = CrispIconSide(kIconSize * scale);
  ImposeIconSampler(into, state.smooth_icons);

  // ⚠ LA CASE SURVOLÉE EST CELLE DE L'IMAGE PRÉCÉDENTE, et c'est sans
  // conséquence : le survol ne change pas sans que la souris bouge, et connaître
  // le foyer AVANT de dessiner évite un second parcours de la grille.
  const std::uint16_t focused_skill = state.hovered_skill;
  const auto focus_it = std::find_if(
      group.cells.begin(), group.cells.end(),
      [focused_skill](const SkillCellView& cell) { return cell.skill_id == focused_skill; });
  const SkillCellView* focus = focus_it == group.cells.end() ? nullptr : &*focus_it;
  const auto focus_rank = static_cast<std::size_t>(focus_it - group.cells.begin());
  std::uint16_t hovered_now = 0;
  int last_row = 0;

  // ⚠ LE CENTRE DE CHAQUE CASE, ET CELLES QUI SE MONTRENT : les flèches se
  // tracent APRÈS la grille — sinon les icônes passeraient dessus — et il leur
  // faut donc ces deux relevés. Le filtre par nom en cache : une flèche vers une
  // case absente pointerait le vide.
  std::vector<ImVec2> centers(group.cells.size());
  std::vector<char> shown(group.cells.size(), 0);

  for (std::size_t rank = 0; rank < group.cells.size(); ++rank) {
    const SkillCellView& cell = group.cells[rank];
    if (!Contains(cell.name, filter)) {
      continue;
    }
    const int row = cell.position / kColumns;
    const int column = cell.position % kColumns;
    last_row = std::max(last_row, row);
    const ImVec2 top_left(origin.x + static_cast<float>(column) * cell_w,
                          RowTop(origin.y, row, group.splits, scale));
    const ImVec2 bottom_right(top_left.x + inner, top_left.y + (kCellHeight - kCellGap) * scale);
    centers[rank] =
        ImVec2((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F);
    shown[rank] = 1;

    ImGui::PushID(static_cast<int>(cell.skill_id));
    ImGui::SetCursorScreenPos(top_left);
    // ⚠ LA CASE EST LE WIDGET. Rien ne se superpose à elle : c'est elle qui prend
    // le clic, et `IsItemHovered` sait, contrairement à un test de rectangle,
    // qu'une fenêtre posée par-dessus n'est pas la case.
    ImGui::InvisibleButton("case", ImVec2(inner, (kCellHeight - kCellGap) * scale));
    const bool hot = ImGui::IsItemHovered();
    if (hot) {
      hovered_now = cell.skill_id;
      into->AddRectFilled(top_left, bottom_right, kHoverVeil, kCellRounding * scale);
    }
    if (cell.skill_id == focused_skill) {
      into->AddRect(top_left, bottom_right, kFocusOutline, kCellRounding * scale, 0,
                    kFocusThickness * scale);
    } else if (LinkedToFocus(cell, focus)) {
      into->AddRect(top_left, bottom_right, kLinkedOutline, kCellRounding * scale, 0,
                    kFocusThickness * scale);
    } else if (cell.raisable) {
      into->AddRect(top_left, bottom_right, kRaisableOutline, kCellRounding * scale, 0,
                    kRaisableThickness * scale);
    }

    // ⚠ LA PLACE DE L'ICÔNE SE CALCULE MÊME QUAND ELLE MANQUE : le voile
    // d'attente s'y pose, et une installation sans icône a quand même des
    // compétences qui refroidissent.
    //
    // 🔴 ET ELLE SE POSE SUR UN PIXEL ENTIER. Un demi-pixel de décalage suffit à
    // défaire tout ce que le côté net vient de gagner : à l'échantillonnage au
    // plus proche, une colonne se dédouble et sa voisine disparaît — et comme le
    // centrage divise par deux une différence qui peut être impaire, le
    // demi-pixel est le cas ORDINAIRE, pas l'exception.
    const ImVec2 icon_at(std::round(top_left.x + (inner - icon_side) * 0.5F),
                         std::round(top_left.y + kIconTop * scale));
    if (cell.icon.usable()) {
      into->AddImage(cell.icon.id, icon_at, ImVec2(icon_at.x + icon_side, icon_at.y + icon_side),
                     ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                     cell.effective_level() > 0 ? IM_COL32_WHITE : kUnlearnedTint);
    }
    PaintCooldown(into, cell, icon_at, icon_side);
    // ⚠ LE RANG DE LANCEMENT, QUAND IL EST BRIDÉ : le double-clic, le menu et le
    // glisser partent tous à ce rang-là, et il faut pouvoir le voir sans ouvrir
    // le menu.
    DrawUseLevelBadge(into, cell, icon_at, icon_side);
    PaintLevel(into, cell, top_left, scale, icon_at.y + icon_side);

    SkillCellGestures(cell, group.cells, state, art, icon_side, hot, staff_allowed, action);
    ImGui::PopID();
  }
  state.hovered_skill = hovered_now;
  // ⚠ RENDU AVANT TOUT LE RESTE : les traits, les flèches et l'habillage de la
  // fenêtre partagent cette liste de dessin, et hériteraient du mode posé.
  RestoreSampler(into);

  PaintSplits(into, group, origin, scale);

  // ── LES FLÈCHES DE CHAÎNE, AUTOUR DE LA CASE SURVOLÉE ─────────────────────
  //
  // 🔴 SEULEMENT AU SURVOL, et jamais en permanence : trente cases et leurs
  // dépendances feraient un plat de spaghettis où l'on ne lirait plus rien. Au
  // survol, elles répondent exactement à la question qu'on se pose à cet
  // instant-là — d'où vient cette compétence, et qu'ouvre-t-elle ?
  //
  // ⚠ APRÈS LA GRILLE : dessinées avant, les icônes leur passeraient dessus.
  if (focus != nullptr && shown[focus_rank] != 0) {
    const float trim = inner * kChainTrimShare;
    WalkChain(into, group, centers, shown, focus_rank, /*upstream=*/true, trim);
    WalkChain(into, group, centers, shown, focus_rank, /*upstream=*/false, trim);
  }

  // ⚠ RÉSERVER LA PLACE CONSOMMÉE : la grille est peinte en coordonnées
  // absolues, et sans cela ImGui ne lui connaîtrait aucune étendue — l'ascenseur
  // serait mort.
  ImGui::SetCursorScreenPos(origin);
  const float bands = static_cast<float>(group.splits.size()) * kSplitBand * scale;
  ImGui::Dummy(ImVec2(static_cast<float>(kColumns) * cell_w,
                      static_cast<float>(last_row + 1) * cell_h + bands));
}

// LE MESSAGE DU MODE CLAVIER, contre la souris, le temps de se lire.
//
// ⚠ SUR LA LISTE DE PREMIER PLAN, et non dans une infobulle : celle de la
// compétence survolée occupe déjà la seule qu'ImGui ouvre, et les deux se
// fondraient en une.
void DrawKeyboardNotice(SkillWindowState& state) {
  if (state.keyboard_notice.empty()) {
    return;
  }
  const double now_s = ImGui::GetTime();
  if (state.keyboard_notice_fresh) {
    state.keyboard_notice_fresh = false;
    state.keyboard_notice_until_s = now_s + kKeyboardNoticeSeconds;
  }
  if (now_s >= state.keyboard_notice_until_s) {
    state.keyboard_notice.clear();
    return;
  }
  const float scale = Scale();
  const ImVec2 text = ImGui::CalcTextSize(state.keyboard_notice.c_str());
  const float padding = kKeyboardNoticePadding * scale;
  // AU-DESSUS DU CURSEUR : l'infobulle de la compétence prend le dessous.
  const ImVec2 mouse = ImGui::GetMousePos();
  const ImVec2 top_left(mouse.x + kKeyboardNoticeOffset * scale,
                        mouse.y - text.y - 2.0F * padding - kKeyboardNoticeOffset * scale);
  const ImVec2 bottom_right(top_left.x + text.x + 2.0F * padding,
                            top_left.y + text.y + 2.0F * padding);
  ImDrawList* const into = ImGui::GetForegroundDrawList();
  into->AddRectFilled(top_left, bottom_right, kKeyboardNoticeBody, kKeyboardNoticeRounding * scale);
  into->AddText(ImVec2(top_left.x + padding, top_left.y + padding), kKeyboardNoticeText,
                state.keyboard_notice.c_str());
}

}  // namespace

void ImposeSkillIconSampler(ImDrawList* into, bool smooth) { ImposeIconSampler(into, smooth); }

void RestoreSkillIconSampler(ImDrawList* into) { RestoreSampler(into); }

void PaintSkillCooldown(ImDrawList* into, const SkillCellView& cell, ImVec2 icon_at, float side) {
  PaintCooldown(into, cell, icon_at, side);
}

float SkillGridWidth() { return static_cast<float>(kColumns) * kCellWidth * Scale(); }

SkillWindowAction DrawGrimoireTab(SkillWindowState& state, std::span<const SkillGroupView> groups,
                                  SkillPointTally points, bool staff_allowed, const SkinArt& art,
                                  const GrimoireStaffView& staff) {
  SkillWindowAction action;
  // Relevé de nouveau par la case survolée, s'il y en a une : voir
  // `SkillCellGestures`.
  state.keyboard_skill = 0;
  state.keyboard_level = 0;
  DrawKeyboardNotice(state);
  DrawHeader(state, points, art, action, points.reserved > 0);
  DrawStaffRow(state, staff_allowed, staff, art, action);

  if (groups.empty()) {
    ImGui::TextDisabled(core::Tr("Aucun skill."));
    return action;
  }
  if (state.group >= static_cast<int>(groups.size())) {
    // Le groupe retenu a disparu : un changement de métier peut retirer un
    // onglet, et s'y accrocher montrerait une grille vide.
    state.group = 0;
  }
  if (BeginSkinTabBar("metiers", art)) {
    for (int rank = 0; rank < static_cast<int>(groups.size()); ++rank) {
      char label[64] = {};
      std::snprintf(label, sizeof(label), "%.*s###grp%d",
                    static_cast<int>(groups[static_cast<std::size_t>(rank)].label.size()),
                    groups[static_cast<std::size_t>(rank)].label.data(), rank);
      if (ImGui::BeginTabItem(label)) {
        state.group = rank;
        ImGui::EndTabItem();
      }
    }
    EndSkinTabBar();
  }
  if (ImGui::BeginChild("arbre", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None)) {
    // 🔴 DEUX LECTURES DU MÊME ARBRE : la grille en montre la DISPOSITION du
    // client, la liste en montre la STRUCTURE. Les cases de la grille sont à la
    // place que ses tables leur donnent — choisie pour tenir en sept colonnes, pas
    // pour dire ce qui mène à quoi.
    const SkillGroupView& shown = groups[static_cast<std::size_t>(state.group)];
    if (state.grid) {
      DrawGrid(state, shown, state.filter, art, staff_allowed, action);
    } else {
      DrawSkillList(state, shown, state.filter, art, staff_allowed, action);
    }
    // L'ascenseur de l'enfant, peint tant qu'il est la fenêtre courante :
    // sans cela il défilerait sans rien montrer.
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();
  return action;
}

}  // namespace ro::ui
