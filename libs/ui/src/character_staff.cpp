#include "ro/ui/character_staff.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "character_panes.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Le champ d'un ajusteur, mesuré sur le PLUS GRAND montant saisissable — le
// zeny — et non sur celui de la rangée.
//
// ⚠ UNE SEULE LARGEUR POUR TOUTES : une largeur par rangée ferait respirer la
// colonne, et un gabarit à cinq chiffres couperait en deux la saisie d'un
// million de zeny.
constexpr const char* kWidestAmount = "999999999";
// Une classe de la liste : son nom et son numéro.
constexpr std::size_t kJobRowBytes = 96;

[[nodiscard]] float AmountFieldWidth() {
  return ImGui::CalcTextSize(kWidestAmount).x + ImGui::GetStyle().FramePadding.x * 2.0F;
}

// LES SIX NOMS, ET LA COMMANDE QUI VA AVEC. ⚠ CE SONT AUSSI LES NOMS DES
// COMMANDES DU SERVEUR, en minuscules : `@str`, `@agi`… C'est `app` qui les
// compose — ici on ne montre que le rang.
[[nodiscard]] const char* AttributeName(std::size_t rank) {
  // ⚠ DES LITTÉRAUX, un par branche : `core::Tr` veut voir sa chaîne à l'appel.
  switch (rank) {
    case 0:
      return core::Tr("STR");
    case 1:
      return core::Tr("AGI");
    case 2:
      return core::Tr("VIT");
    case 3:
      return core::Tr("INT");
    case 4:
      return core::Tr("DEX");
    default:
      return core::Tr("LUK");
  }
}

// POSE LE CURSEUR EN FACE D'UNE RANGÉE DU VOLET DES NOMBRES.
//
// 🔴 JAMAIS EN ARRIÈRE. Un contrôle plus haut qu'une rangée de texte — un menu
// fait une hauteur de cadre — déborde forcément sur la rangée suivante, et
// reculer le curseur le ferait chevaucher le précédent. L'alignement est donc au
// MIEUX : exact sur les six caractéristiques, qui font la même hauteur des deux
// côtés, approché ensuite.
void AlignTo(const CharacterStatRows& rows, float top, float offset) {
  if (!rows.measured) {
    return;
  }
  const float wanted = top + offset;
  if (wanted > ImGui::GetCursorPosY()) {
    ImGui::SetCursorPosY(wanted);
  }
}

// UNE RANGÉE D'AJUSTEMENT : « - », la saisie du pas, « + ».
//
// 🔴 LE SIGNE VIENT DU BOUTON, JAMAIS DE LA SAISIE. Un « -5 » tapé dans le champ
// puis un clic sur « + » enverrait « @str +-5 », que le serveur lirait comme il
// pourrait. Le champ ne porte donc qu'un montant positif, borné ici.
void AdjustRow(const char* id, int* step, int most, const char* tip, const SkinArt& art,
               StaffOrder order, StaffAction& action) {
  ImGui::PushID(id);
  *step = std::clamp(*step, 1, most);
  const float square = ImGui::GetFrameHeight();
  if (SkinSmallButton("-", art, ImVec2(square, square))) {
    action.order = order;
    action.amount = -*step;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(tip);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(AmountFieldWidth());
  // ⚠ PAS DE PAS : les flèches d'ImGui ne sont pas habillées, et une flèche nue
  // au milieu d'une ligne habillée se voit plus que le champ.
  ImGui::InputInt("##pas", step, /*step=*/0, /*step_fast=*/0);
  if (ImGui::IsItemHovered()) {
    SkinTooltip(tip);
  }
  ImGui::SameLine();
  if (SkinSmallButton("+", art, ImVec2(square, square))) {
    action.order = order;
    action.amount = *step;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(tip);
  }
  ImGui::PopID();
}

// ── LES SIX AJUSTEURS, EN FACE DE STR..LUK ──────────────────────────────────
void DrawAttributeRows(StaffPaneState& state, const CharacterStatRows& rows, float top,
                       const SkinArt& art, StaffAction& action) {
  for (std::size_t rank = 0; rank < state.stat_step.size(); ++rank) {
    AlignTo(rows, top, rows.attribute[rank]);
    char tip[192] = {};
    std::snprintf(tip, sizeof(tip),
                  core::Tr("Ajoute ou retire le montant saisi à %s. C'est un AJUSTEMENT, "
                           "pas une valeur cible."),
                  AttributeName(rank));
    const StaffOrder before = action.order;
    AdjustRow(AttributeName(rank), &state.stat_step[rank], kStatStepMost, tip, art,
              StaffOrder::kAdjustStat, action);
    // ⚠ « L'ORDRE A CHANGÉ », et non « le rang vaut encore zéro » : STR EST le
    // rang zéro, et une rangée plus bas se croirait l'auteure de son ajustement.
    if (action.order != before) {
      action.stat = static_cast<int>(rank);
    }
  }
}

// ── LE MENU DES CLASSES ─────────────────────────────────────────────────────
//
// 🔴 L'APERÇU MONTRE LA CLASSE COURANTE, JAMAIS LE DERNIER CHOIX CLIQUÉ. Le
// serveur a pu le refuser, et la classe change aussi par d'autres chemins —
// monture, déguisement, quête de métier. Montrer ce qu'on a demandé plutôt que
// ce qu'on est ferait mentir le volet.
void DrawJobPicker(StaffPaneState& state, const StaffPaneView& view, const SkinArt& art,
                   StaffAction& action) {
  const char* shown = core::Tr("(classe inconnue)");
  for (const StaffJobView& one : view.jobs) {
    if (one.id == view.own_job) {
      shown = one.label.data();
      break;
    }
  }
  ImGui::SetNextItemWidth(-1.0F);
  // 🔴 SANS MOLETTE : un cran changerait de classe, et la commande MAXE le
  // personnage au passage.
  if (!BeginSkinCombo("##classe", shown, art)) {
    if (ImGui::IsItemHovered()) {
      SkinTooltipWarned(
          core::Tr("Change de classe."),
          core::Tr("Sur ce serveur la commande enchaîne d'elle-même un niveau de base et de job "
                   "au maximum, tous les skills et toutes les caractéristiques — changer de "
                   "classe MAXE le personnage, ce n'est pas un simple changement d'apparence."));
    }
    return;
  }
  const bool just_opened = ImGui::IsWindowAppearing();
  if (just_opened) {
    state.job_filter[0] = '\0';
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##filtre_classe", core::Tr("Rechercher..."), state.job_filter.data(),
                           state.job_filter.size());

  // Le filtre est insensible à la casse et porte sur le libellé ET l'identifiant :
  // « 4054 » comme « rune » trouvent le Rune Knight.
  std::string needle(state.job_filter.data());
  std::ranges::transform(needle, needle.begin(), [](unsigned char letter) {
    return static_cast<char>(std::tolower(letter));
  });

  // ⚠ UN EN-TÊTE AU CHANGEMENT, ET UN DRAPEAU POUR LA PREMIÈRE : la famille de
  // départ est une valeur comme une autre, et la comparer à elle-même sauterait
  // l'en-tête du premier groupe.
  StaffJobFamily family = StaffJobFamily::kNovice;
  bool has_family = false;
  std::vector<std::string> rows;
  std::vector<const StaffJobView*> kept;
  int own_rank = kListNoEntry;
  for (const StaffJobView& one : view.jobs) {
    std::array<char, kJobRowBytes> row{};
    std::snprintf(row.data(), row.size(), "%.*s (%u)", static_cast<int>(one.label.size()),
                  one.label.data(), one.id);
    std::string hay(row.data());
    std::ranges::transform(hay, hay.begin(), [](unsigned char letter) {
      return static_cast<char>(std::tolower(letter));
    });
    if (!needle.empty() && hay.find(needle) == std::string::npos) {
      continue;
    }
    if (one.id == view.own_job) {
      own_rank = static_cast<int>(kept.size());
    }
    rows.emplace_back(row.data());
    kept.push_back(&one);
  }
  // 🔴 LE CLAVIER DÉSIGNE, ENTRÉE PREND : un pas de flèche changerait de classe.
  ComboKeys keys(own_rank, static_cast<int>(kept.size()), {}, ComboKeyChoice::kOnEnter);
  for (std::size_t rank = 0; rank < kept.size(); ++rank) {
    const StaffJobView& one = *kept[rank];
    if (!has_family || family != one.family) {
      has_family = true;
      family = one.family;
      ImGui::TextDisabled("%s", StaffJobFamilyLabel(family));
    }
    const bool own = one.id == view.own_job;
    if (keys.Took(static_cast<int>(rank),
                  ImGui::Selectable(rows[rank].c_str(), keys.marked() == static_cast<int>(rank)))) {
      action.order = StaffOrder::kChangeJob;
      action.job_id = one.id;
      ImGui::CloseCurrentPopup();
    }
    if (own && just_opened) {
      ImGui::SetScrollHereY(0.5F);
    }
  }
  EndSkinCombo();
}

// ── LA VITESSE DE MARCHE ────────────────────────────────────────────────────
void DrawSpeed(StaffPaneState& state, const StaffPaneView& view, const SkinArt& art,
               StaffAction& action) {
  // 🔴 UN FRONT, ET NON UN ALIGNEMENT À CHAQUE IMAGE : après notre propre envoi
  // le serveur renvoie la valeur qu'on affiche déjà, donc le curseur ne saute
  // jamais en arrière le temps de l'aller-retour.
  if (view.server_speed != state.speed_seen) {
    state.speed_seen = view.server_speed;
    state.speed = view.server_speed;
    state.speed_sent = view.server_speed;
  }
  ImGui::SetNextItemWidth(-1.0F);
  static_cast<void>(
      SkinSliderInt("##vitesse", &state.speed, kWalkSpeedFastest, kWalkSpeedSlowest, art, "%d"));
  // 🔴 ENVOYÉE AU RELÂCHEMENT, JAMAIS PENDANT LE GLISSER. Chaque pose est une
  // ligne de conversation ET un recalcul complet côté serveur : une par image de
  // glisser noierait la conversation et le ferait travailler pour rien.
  const bool released = ImGui::IsItemDeactivatedAfterEdit();
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Vitesse de marche. PLUS BAS = PLUS RAPIDE ; 150 est la vitesse "
                 "normale, 20 le plancher du serveur.\nEnvoyée au relâchement."));
  }
  if (released && state.speed != state.speed_sent) {
    action.order = StaffOrder::kSetSpeed;
    action.amount = state.speed;
    state.speed_sent = state.speed;
  }
  if (SkinButton(core::Tr("Vitesse normale"), art,
                 ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    action.order = StaffOrder::kNormalSpeed;
    state.speed = kWalkSpeedNormal;
    state.speed_sent = kWalkSpeedNormal;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Rend au personnage sa vitesse par défaut (150)."));
  }
}

// ── NIVEAUX, POINTS ET ZENY ─────────────────────────────────────────────────
//
// Ces rangées portent un LIBELLÉ devant l'ajusteur, contrairement aux six
// caractéristiques dont le nom est déjà écrit en face, dans le volet des
// nombres. Une colonne COMMUNE aux trois : sans elle, « Zeny » décalerait son
// « - » d'une dizaine de pixels par rapport à ceux de « Base » et « Job ».
[[nodiscard]] float LabelColumnWidth() {
  return std::max(
      std::max(ImGui::CalcTextSize(core::Tr("Base")).x, ImGui::CalcTextSize(core::Tr("Job")).x),
      ImGui::CalcTextSize("Zeny").x);
}

void LabelledRow(const char* label, float column) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(0.0F, 0.0F);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + column - ImGui::CalcTextSize(label).x +
                       ImGui::GetStyle().ItemSpacing.x);
}

void DrawLevelsAndZeny(StaffPaneState& state, const SkinArt& art, StaffAction& action) {
  const float column = LabelColumnWidth();
  LabelledRow(core::Tr("Base"), column);
  AdjustRow("blvl", &state.base_level_step, kLevelStepMost,
            core::Tr("Gagne ou perd le nombre de niveaux de BASE saisi."), art,
            StaffOrder::kAdjustBaseLevel, action);
  LabelledRow(core::Tr("Job"), column);
  AdjustRow("jlvl", &state.job_level_step, kLevelStepMost,
            core::Tr("Gagne ou perd le nombre de niveaux de JOB saisi."), art,
            StaffOrder::kAdjustJobLevel, action);
  // « Zeny » est un terme du jeu : il ne passe pas par le catalogue.
  LabelledRow("Zeny", column);
  AdjustRow("zeny", &state.zeny_step, kZenyStepMost,
            core::Tr("Donne ou retire le montant saisi. Un retrait plus grand que la "
                     "bourse la vide sans passer en négatif."),
            art, StaffOrder::kAdjustZeny, action);

  // Les points de caractéristique : une SAISIE et un bouton, pas un ajusteur —
  // la commande DONNE un nombre de points, et « en retirer » n'a pas de sens en
  // face.
  ImGui::SetNextItemWidth(AmountFieldWidth());
  ImGui::InputInt("##points_staff", &state.stat_points, /*step=*/0, /*step_fast=*/0);
  state.stat_points = std::clamp(state.stat_points, 1, kStatPointsMost);
  ImGui::SameLine();
  if (SkinButton(core::Tr("Points"), art, ImVec2(ImGui::GetContentRegionAvail().x, 0.0F))) {
    action.order = StaffOrder::kGiveStatPoints;
    action.amount = state.stat_points;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Donne le nombre de points de caractéristique saisi — de quoi "
                 "essayer les boutons « + » du volet des nombres."));
  }
}

// ── LE RAFFINAGE ────────────────────────────────────────────────────────────
//
// 🔴 UN MENU D'EMPLACEMENT PLUTÔT QU'UN GESTE SUR LA CASE DU MANNEQUIN. Le clic
// droit y ouvre déjà la description et le double-clic déséquipe : inventer un
// troisième geste caché sur la même case ne se découvrirait pas, et se
// tromperait un jour de bouton sur une pièce qu'on ne voulait pas toucher.
[[nodiscard]] const char* RefinePlaceLabel(int rank) {
  // ⚠ SEULE LA PREMIÈRE ENTRÉE PASSE PAR LE CATALOGUE : traduire « Weapon » y
  // ferait entrer les termes du jeu dans la liste des textes manquants, un par
  // emplacement.
  return rank == 0 ? core::Tr("Toutes les pièces")
                   : kRefinePlaces[static_cast<std::size_t>(rank)].label;
}

void DrawRefine(StaffPaneState& state, const SkinArt& art, StaffAction& action) {
  ImGui::SetNextItemWidth(-1.0F);
  int step = 0;
  const bool open =
      BeginSkinCombo("##raffinage_place", RefinePlaceLabel(state.refine_place), art, &step);
  state.refine_place =
      SkinComboStepped(state.refine_place, step, static_cast<int>(kRefinePlaces.size()));
  if (open) {
    ComboKeys keys(state.refine_place, static_cast<int>(kRefinePlaces.size()));
    for (int rank = 0; rank < static_cast<int>(kRefinePlaces.size()); ++rank) {
      if (keys.Took(rank, ImGui::Selectable(RefinePlaceLabel(rank), rank == state.refine_place))) {
        state.refine_place = rank;
      }
    }
    EndSkinCombo();
  } else if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Emplacement visé par le raffinage. « Toutes les pièces » couvre "
                 "tout ce qui est porté."));
  }
  AdjustRow("raffinage", &state.refine_step, kRefineStepMost,
            core::Tr("Ajoute ou retire des niveaux de raffinage sur l'emplacement choisi. "
                     "Le maximum est +10 sur ce serveur (pré-renewal)."),
            art, StaffOrder::kRefine, action);
  if (action.order == StaffOrder::kRefine) {
    action.mask = kRefinePlaces[static_cast<std::size_t>(state.refine_place)].mask;
  }
}

// ── L'ÉTAT VISUEL ───────────────────────────────────────────────────────────
void DrawOptionPopup(StaffPaneState& state, const SkinArt& art, StaffAction& action) {
  if (!BeginSkinPopup("##etat_visuel")) {
    return;
  }
  ImGui::TextDisabled("%s", core::Tr("Coché = présent. Le reste est retiré."));
  ImGui::Separator();
  char cart[24] = {};
  if (state.option_cart == 0) {
    std::snprintf(cart, sizeof(cart), "%s", core::Tr("aucun"));
  } else {
    std::snprintf(cart, sizeof(cart), "Cart %d", state.option_cart);
  }
  ImGui::SetNextItemWidth(ImGui::CalcTextSize("Cart 9").x * 3.0F);
  int step = 0;
  const bool open = BeginSkinCombo("Cart##etat_cart", cart, art, &step);
  state.option_cart = SkinComboStepped(state.option_cart, step, kCartLevelMost + 1);
  if (open) {
    ComboKeys keys(state.option_cart, kCartLevelMost + 1);
    for (int level = 0; level <= kCartLevelMost; ++level) {
      char row[24] = {};
      if (level == 0) {
        std::snprintf(row, sizeof(row), "%s", core::Tr("aucun"));
      } else {
        std::snprintf(row, sizeof(row), "Cart %d", level);
      }
      if (keys.Took(level, ImGui::Selectable(row, level == state.option_cart))) {
        state.option_cart = level;
      }
    }
    EndSkinCombo();
  }
  for (std::size_t rank = 0; rank < kOptionFlags.size(); ++rank) {
    static_cast<void>(SkinCheckBox(kOptionFlags[rank].label, &state.option_flags[rank], art));
  }
  ImGui::Separator();
  if (SkinButton(core::Tr("Appliquer"), art)) {
    std::uint32_t mask = kCartOptionBits[static_cast<std::size_t>(state.option_cart)];
    for (std::size_t rank = 0; rank < kOptionFlags.size(); ++rank) {
      if (state.option_flags[rank]) {
        mask |= kOptionFlags[rank].bit;
      }
    }
    action.order = StaffOrder::kSetOption;
    action.mask = mask;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
}

// UNE CASE QUI MONTRE UN ÉTAT QUE LE SERVEUR GARDE ET ANNONCE. ⚠ ELLE NE GARDE
// RIEN : le clic ne fait que demander la bascule, et l'écho du serveur la coche.
void DrawServerToggle(const char* label, const char* tooltip, bool told, StaffOrder order,
                      const SkinArt& art, StaffAction& action) {
  bool shown = told;
  if (SkinCheckBox(label, &shown, art)) {
    action.order = order;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(tooltip);
  }
}

// ── OBJETS ET ÉTAT ──────────────────────────────────────────────────────────
void DrawBodyAndGear(StaffPaneState& state, const StaffPaneView& view, const SkinArt& art,
                     StaffAction& action) {
  const ImVec2 full(ImGui::GetContentRegionAvail().x, 0.0F);
  if (SkinButton(core::Tr("Soigner"), art, full)) {
    action.order = StaffOrder::kHeal;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Rend tous les points de vie et de magie."));
  }
  if (SkinButton(core::Tr("Réparer tout"), art, full)) {
    action.order = StaffOrder::kRepairAll;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Répare toutes les pièces endommagées de l'inventory."));
  }
  // ⚠ LA CASE NE GARDE RIEN : elle montre ce que le serveur annonce, et le clic
  // ne fait que demander la bascule. Cochée d'avance, elle mentirait jusqu'à
  // l'écho — et pour de bon si le serveur refusait.
  bool hidden = view.hidden;
  if (SkinCheckBox(core::Tr("Invisible"), &hidden, art)) {
    action.order = StaffOrder::kToggleHide;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Invisible aux joueurs et aux monstres (@hide). Le serveur le garde d'une "
                 "connexion à l'autre."));
  }
  DrawServerToggle(core::Tr("Killer"),
                   core::Tr("Vous pouvez attaquer les joueurs hors des cartes PvP (@killer). Le "
                            "serveur le garde d'une connexion à l'autre."),
                   view.killer, StaffOrder::kToggleKiller, art, action);
  DrawServerToggle(core::Tr("Killable"),
                   core::Tr("Les joueurs peuvent vous attaquer hors des cartes PvP (@killable). "
                            "Le serveur le garde d'une connexion à l'autre."),
                   view.killable, StaffOrder::kToggleKillable, art, action);
  DrawServerToggle(core::Tr("Cadence joueur"),
                   core::Tr("Les plafonds GM d'ASPD et de delay de skill sont ignorés : vous "
                            "jouez à la cadence d'un joueur (@playertest). Le serveur le garde "
                            "d'une connexion à l'autre."),
                   view.player_test, StaffOrder::kTogglePlayerTest, art, action);
  DrawServerToggle(core::Tr("Sans délai"),
                   core::Tr("Vos skills n'ont plus aucune limite de cadence : ni cooldown, ni "
                            "after-cast delay, ni incantation (@nodelay). Le serveur le garde "
                            "d'une connexion à l'autre."),
                   view.no_delay, StaffOrder::kToggleNoDelay, art, action);
  bool ignored = view.battle_ignored;
  if (SkinCheckBox(core::Tr("Ignoré des monstres"), &ignored, art)) {
    action.order = StaffOrder::kSetBattleIgnore;
    action.amount = ignored ? 1 : 0;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Les monstres ne vous attaquent plus (@battleignore). Le serveur l'oublie à "
                 "la déconnexion : le client le repose à chaque connexion tant que la case "
                 "est cochée."));
  }

  if (SkinButton(core::Tr("État visuel"), art, full)) {
    // 🔴 AMORCÉ SUR CE QUE LE CLIENT SAIT. La commande REMPLACE l'état complet :
    // une case oubliée retirerait la monture, le faucon ou le cart sans que
    // personne l'ait demandé.
    for (std::size_t rank = 0; rank < kOptionFlags.size(); ++rank) {
      const std::uint32_t bit = kOptionFlags[rank].bit;
      state.option_flags[rank] = (bit == kOptionFlags[0].bit && view.riding) ||
                                 (bit == kOptionFlags[1].bit && view.falcon);
    }
    state.option_cart =
        view.cart_level > 0 && view.cart_level <= kCartLevelMost ? view.cart_level : 0;
    ImGui::OpenPopup("##etat_visuel");
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltipWarned(
        core::Tr("Pose l'état visuel du personnage (monture, faucon, Hiding, Ruwach...)."),
        core::Tr("La commande REMPLACE l'état complet : ce qui n'est pas coché est RETIRÉ. Les "
                 "cases sont amorcées sur l'état connu du client pour ne pas perdre la monture "
                 "ou le cart en chemin.\nElle remet aussi à zéro les statuts (pétrifié, gelé, "
                 "maudit...)."));
  }
  DrawOptionPopup(state, art, action);

  // 🔴 VIDER L'INVENTAIRE SE FAIT EN DEUX TEMPS. La commande SUPPRIME tout et le
  // serveur ne demande rien : le clic est irréversible, donc la confirmation est
  // ici.
  if (!state.wipe_asked) {
    if (SkinButton(core::Tr("Vider l'inventory"), art, full)) {
      state.wipe_asked = true;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("SUPPRIME tous les items de l'inventory. Irréversible, et le "
                   "serveur ne demande rien — la confirmation est ici."));
    }
    return;
  }
  const float half = (full.x - ImGui::GetStyle().ItemSpacing.x) * 0.5F;
  if (SkinButton(core::Tr("Confirmer"), art, ImVec2(half, 0.0F))) {
    action.order = StaffOrder::kWipeInventory;
    state.wipe_asked = false;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(half, 0.0F))) {
    state.wipe_asked = false;
  }
}

}  // namespace

// ⚠ UN SWITCH DE LITTÉRAUX, ET NON UNE TABLE INDEXÉE. `Tr` n'accepte qu'un
// littéral : une chaîne prise dans un tableau n'est dans aucun catalogue, et le
// contrôle des traductions le refuse — à raison, puisqu'elle ne serait jamais
// traduite et que rien ne le dirait.
const char* StaffJobFamilyLabel(StaffJobFamily family) {
  switch (family) {
    case StaffJobFamily::kNovice:
      return core::Tr("Novice / 1re classe");
    case StaffJobFamily::kSecond:
      return core::Tr("2e classe");
    case StaffJobFamily::kTransFirst:
      return core::Tr("Trans. 1re classe");
    case StaffJobFamily::kTransSecond:
      return core::Tr("Trans. 2e classe");
    case StaffJobFamily::kThird:
      return core::Tr("3e classe");
    case StaffJobFamily::kThirdTrans:
      return core::Tr("3e classe (trans.)");
    case StaffJobFamily::kFourth:
      return core::Tr("4e classe");
    case StaffJobFamily::kExpanded:
      return core::Tr("Classes étendues");
    case StaffJobFamily::kBabyFirst:
      return core::Tr("Baby / 1re classe");
    case StaffJobFamily::kBabySecond:
      return core::Tr("Baby 2e classe");
    case StaffJobFamily::kBabyThird:
      return core::Tr("Baby 3e classe");
    case StaffJobFamily::kBabyExpanded:
      return core::Tr("Baby étendues");
  }
  return core::Tr("Classes étendues");
}

float CharacterStaffPaneWidth(const SkinArt& art) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float square = ImGui::GetFrameHeight();
  // La rangée d'ajustement et celle des niveaux sont les deux candidates ; le
  // plancher couvre le reste.
  const float adjust = square * 2.0F + AmountFieldWidth() + style.ItemSpacing.x * 2.0F;
  const float labelled = LabelColumnWidth() + style.ItemSpacing.x + adjust;
  const std::array<const char*, 3> widest = {core::Tr("Vider l'inventory"),
                                             core::Tr("Vitesse normale"), core::Tr("État visuel")};
  const float content = std::max({adjust, labelled, SkinWidestButton(widest, art),
                                  SkinCheckBoxWidth(core::Tr("Ignoré des monstres"), art)});
  return std::max(kCharacterStaffPaneMinimum * Scale(),
                  std::ceil(content + style.WindowPadding.x * 2.0F + style.ScrollbarSize));
}

void DrawCharacterStaff(StaffPaneState& state, const StaffPaneView& view,
                        const CharacterStatRows& rows, const SkinArt& art, StaffAction& action) {
  const float top = ImGui::GetCursorPosY();

  DrawAttributeRows(state, rows, top, art, action);

  AlignTo(rows, top, rows.status_point);
  const std::array<const char*, 2> pair = {core::Tr("Reset"), core::Tr("Max")};
  const ImVec2 button(SkinWidestButton(pair, art), 0.0F);
  if (SkinButton(core::Tr("Reset"), art, button)) {
    action.order = StaffOrder::kResetStats;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Remet les six caractéristiques à leur base et rend tous les "
                 "points dépensés."));
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Max"), art, button)) {
    action.order = StaffOrder::kMaxStats;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Monte les six caractéristiques à leur maximum."));
  }

  AlignTo(rows, top, rows.first_derived);
  DrawJobPicker(state, view, art, action);
  DrawSpeed(state, view, art, action);
  DrawLevelsAndZeny(state, art, action);
  ImGui::Separator();
  DrawRefine(state, art, action);
  DrawBodyAndGear(state, view, art, action);
}

}  // namespace ro::ui
