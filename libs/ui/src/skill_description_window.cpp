// LA DESCRIPTION D'UNE COMPÉTENCE : son en-tête, et ses trois onglets — le
// texte de la table, la cadence réelle et les dégâts estimés, que le serveur
// calcule sur le personnage.

#include <algorithm>
#include <cstdio>
#include <string>

#include "ro/ui/bug_report_window.h"
#include "ro/ui/description_window.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "description_odds.h"

namespace ro::ui {
namespace {

// ── CE QUE LA FENÊTRE D'UNE COMPÉTENCE PREND ────────────────────────────────
//
// Sa taille d'ouverture et son plancher. Plus étroite que
// celle d'un objet : une description de compétence est écrite en lignes COURTES,
// pré-coupées par la table, et l'élargir ne remplit que du vide.
constexpr float kSkillFirstWidthPixels = 520.0F;
constexpr float kSkillFirstHeightPixels = 360.0F;
constexpr float kSkillLeastWidthPixels = 500.0F;
constexpr float kSkillLeastHeightPixels = 300.0F;

// L'icône d'une compétence fait vingt-quatre pixels, et on la montre au DOUBLE
// EXACT : c'est la mesure de la grille du grimoire, et pour la même raison —
// tout autre facteur double un pixel sur deux ou sur trois, et l'icône vient
// inégale quel que soit le filtre.
constexpr float kSkillIconSidePixels = 48.0F;

// ── CE QUE LES ONGLETS TECHNIQUES ONT BESOIN DE SAVOIR ──────────────────────
constexpr double kMillisecondsPerSecond = 1000.0;

// Une durée du serveur, en secondes, telle que `%f` l'attend : un `double`.
[[nodiscard]] double SecondsOf(int duration_ms) {
  return static_cast<double>(duration_ms) / kMillisecondsPerSecond;
}
// Assez pour un numéro de monstre à cinq chiffres et ses flèches.
constexpr float kDamageTargetFieldPixels = 120.0F;
// LES VERDICTS DU SERVEUR, sous leurs noms. ⚠ ZÉRO EST « L'ESTIMATION VAUT » :
// c'est le seul cas où les chiffres se lisent.
constexpr int kDamageNotOffensive = 1;
constexpr int kDamageTargetUnknown = 2;
// CE QUE LE COUP EST : `BF_WEAPON` et `BF_MAGIC` du serveur, le reste est divers.
constexpr int kDamagePhysical = 1;
constexpr int kDamageMagical = 2;

// ── L'ONGLET « INFOS TECHNIQUES » : LA TABLE DE CAST, RANG PAR RANG ─────────
//
// 🔴 CES CHIFFRES NE SONT DANS AUCUNE TABLE DU CLIENT, et ne peuvent pas y être :
// ils se CALCULENT sur l'équipement et les états du personnage, et changent
// d'une minute à l'autre. Le serveur les envoie sur demande.
//
// ⚠ LA COLONNE « CAST » EST LE TEMPS VARIABLE, celui qui tombe avec la DEX et
// l'INT et que l'interruption coupe. C'est celui qu'on vient vérifier — le fixe,
// lui, ne bouge que sous certains états.
void DrawCastTab(const SkillDescriptionView& view, const SkinArt& art,
                 SkillDescriptionAction& action) {
  // ⚠ DEMANDÉE À CHAQUE IMAGE OÙ L'ONGLET EST OUVERT : c'est `app` qui décide de
  // ne pas harceler le serveur — lui seul sait ce qu'il a déjà.
  action.want_cast = view.skill_id;
  if (SkinSmallButton(core::Tr("Rafraîchir"), art)) {
    action.refresh = true;
  }
  if (view.cast.empty()) {
    // ⚠ « EN ATTENTE » ET NON « AUCUNE DONNÉE » : la réponse met un aller-retour
    // à venir, et écrire « aucune » ferait conclure à un trou du serveur.
    ImGui::TextDisabled("%s", core::Tr("En attente du serveur..."));
    return;
  }
  constexpr auto kFlags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("cast", 4, kFlags)) {
    ImGui::TableSetupColumn(core::Tr("Niv"));
    ImGui::TableSetupColumn(core::Tr("Cast"));
    ImGui::TableSetupColumn(core::Tr("Cooldown"));
    ImGui::TableSetupColumn(core::Tr("Délai"));
    ImGui::TableHeadersRow();
    for (std::size_t rank = 0; rank < view.cast.size(); ++rank) {
      const SkillCastLevelView& level = view.cast[rank];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%d", static_cast<int>(rank) + 1);
      ImGui::TableNextColumn();
      ImGui::Text("%.2fs", SecondsOf(level.cast_variable_ms));
      ImGui::TableNextColumn();
      ImGui::Text("%.2fs", SecondsOf(level.cooldown_ms));
      ImGui::TableNextColumn();
      ImGui::Text("%.2fs", SecondsOf(level.after_delay_ms));
    }
    ImGui::EndTable();
  }
  ImGui::TextDisabled("%s", core::Tr("Valeurs effectives : tes stats, ton équipement, tes états."));
}

// ── L'ONGLET « DÉGÂTS » : CE QUE LA COMPÉTENCE FERAIT, ICI, MAINTENANT ──────
//
// 🔴 TROIS CIBLES, ET LE ZÉRO N'EST PAS « AUCUNE » : zéro est le mannequin
// NEUTRE sans défense — les dégâts bruts, avant toute réduction —, le miroir est
// soi-même, et tout le reste est un vrai monstre avec sa défense, son élément,
// sa race et sa taille.
void DrawDamageTab(SkillDescriptionWindowState& state, const SkillDescriptionView& view,
                   const SkinArt& art, SkillDescriptionAction& action) {
  // ⚠ LA CASE D'IMGUI, comme le champ et la table de cette fenêtre : l'habillage
  // du jeu n'a pas de case à cocher, et en peindre une ici la rendrait seule de
  // son espèce.
  if (ImGui::Checkbox(core::Tr("Contre moi-même (PvP)"), &state.damage_self)) {
    // ⚠ CHANGER DE CIBLE INVALIDE CE QU'ON MONTRE : l'estimation d'avant a été
    // faite contre autre chose, et la garder à l'écran la ferait lire comme la
    // nouvelle.
    action.refresh = true;
  }
  ImGui::BeginDisabled(state.damage_self);
  ImGui::SetNextItemWidth(kDamageTargetFieldPixels);
  ImGui::InputInt(core::Tr("Cible : ID monstre (0 = neutre)"), &state.damage_target, 0, 0);
  ImGui::EndDisabled();
  state.damage_target = std::max(state.damage_target, 0);
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Calculer"), art)) {
    action.refresh = true;
  }

  action.want_damage = view.skill_id;
  action.damage_target =
      state.damage_self ? kSkillDamageAgainstSelf : static_cast<std::uint32_t>(state.damage_target);

  if (!view.damage.told) {
    ImGui::TextDisabled("%s", core::Tr("Calcul en cours..."));
    return;
  }
  const SkillDamageView& told = view.damage;
  if (told.status == kDamageNotOffensive) {
    ImGui::TextDisabled("%s", core::Tr("Sort non offensif : aucun dégât."));
    return;
  }
  if (told.status == kDamageTargetUnknown) {
    ImGui::TextDisabled("%s", core::Tr("Monstre introuvable : cet identifiant n'existe pas."));
    return;
  }
  if (told.status != 0) {
    ImGui::TextDisabled("%s", core::Tr("Estimation indisponible."));
    return;
  }
  if (told.target_mob_id == kSkillDamageAgainstSelf) {
    ImGui::TextDisabled("%s", core::Tr("Cible : toi-même (miroir du duel)"));
  } else if (told.target_mob_id == 0) {
    ImGui::TextDisabled("%s", core::Tr("Cible : neutre sans défense (dégâts bruts)"));
  } else if (!told.target_name.empty()) {
    ImGui::Text(core::Tr("Cible : %.*s (n° %u)"), static_cast<int>(told.target_name.size()),
                told.target_name.data(), told.target_mob_id);
  } else {
    ImGui::Text(core::Tr("Cible : monstre n° %u"), told.target_mob_id);
  }
  // ⚠ CE QUE LE COUP EST AUX YEUX DU SERVEUR, et il ne s'en déduit pas : une
  // compétence d'arme et une compétence magique ne se réduisent pas du tout de
  // la même façon chez la cible.
  const char* kind = told.attack_kind == kDamagePhysical  ? core::Tr("physique")
                     : told.attack_kind == kDamageMagical ? core::Tr("magique")
                                                          : core::Tr("divers");
  ImGui::Text(core::Tr("Niveau %d - %s"), told.level, kind);
  ImGui::Text("%lld - %lld", static_cast<long long>(told.least), static_cast<long long>(told.most));
  ImGui::SameLine();
  ImGui::Text(core::Tr("(moyenne %lld)"), static_cast<long long>(told.average));
  if (told.hits > 1) {
    ImGui::SameLine();
    ImGui::Text(core::Tr("x%d coups"), told.hits);
  }
  ImGui::TextDisabled("%s",
                      core::Tr("Inclut ton équipement et tes états. Neutre : avant défense."));
}

}  // namespace

void AskSkillDescription(SkillDescriptionWindowState& state, std::uint16_t skill_id) {
  if (skill_id == 0) {
    return;
  }
  if (state.open && state.skill_id == skill_id) {
    state.open = false;
    return;
  }
  state.skill_id = skill_id;
  state.open = true;
  state.place_at_mouse = true;
  state.raise = true;
}

SkillDescriptionAction DrawSkillDescriptionWindow(SkillDescriptionWindowState& state,
                                                  const SkillDescriptionView& view,
                                                  const SkinArt& art) {
  SkillDescriptionAction action;
  if (!state.open || state.skill_id == 0) {
    state.was_open = false;
    return action;
  }

  // ── LE TITRE : LE NOM, ET CE QUE LA COMPÉTENCE COÛTE ──────────────────────
  //
  // 🔴 LE COÛT EST DANS LA BARRE DE TITRE, comme au client : c'est le chiffre
  // qu'on vient vérifier, et le chercher au milieu d'un paragraphe le ferait
  // manquer. Le client écrit « Gloria (Sp : 20) » — ces mots-là, cette casse-là.
  //
  // ⚠ RECOMPOSÉ SEULEMENT QUAND IL LE FAUT, comme celui d'un objet : refait à
  // chaque image, il coûterait une allocation par image pour un texte qui ne
  // bouge qu'au changement de compétence ou de langue.
  const std::size_t language = core::LanguageSerial();
  if (state.title.empty() || view.name != state.title_for || language != state.title_language ||
      view.sp_cost != state.title_sp_cost) {
    state.title_for = std::string(view.name);
    state.title_language = language;
    state.title_sp_cost = view.sp_cost;
    state.title = view.name.empty() ? std::string(core::Tr("Description")) : state.title_for;
    if (view.sp_cost > 0) {
      // ⚠ L'ESPACE DE TÊTE EST POSÉE ICI, PAS DANS LE CATALOGUE : celui-ci ROGNE
      // les espaces de bord d'une clé, et l'entrée deviendrait introuvable à
      // l'exécution — le titre perdrait son coût dans toutes les langues, sans
      // qu'aucun contrôle ne le dise. Même raison que « - Broken » au titre d'un item.
      char cost[32] = {};
      std::snprintf(cost, sizeof(cost), core::Tr("(Sp : %d)"), view.sp_cost);
      state.title += ' ';
      state.title += cost;
    }
    state.title += "###";
    state.title += kSkillDescriptionWindowId;
  }

  PlaceDescription(state.place_at_mouse, state.was_open, state.placement);
  // ⚠ À CHAQUE DEMANDE, PAS SEULEMENT À L'OUVERTURE : décrire une AUTRE
  // compétence pendant qu'elle est ouverte n'offre aucun front montant, et son
  // contenu changerait sans qu'elle remonte. C'est le cas le plus déroutant —
  // on clique, et rien ne paraît se passer.
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(kSkillFirstWidthPixels, kSkillFirstHeightPixels),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kSkillLeastWidthPixels, kSkillLeastHeightPixels),
                                      ImVec2(FLT_MAX, FLT_MAX));
  PoseBugReportButton(state.bug_report);
  const bool body_shown =
      BeginSkinnedWindow(state.title.c_str(), &state.open, art, SkinnedTitle::kDescription,
                         ImGuiWindowFlags_NoCollapse);
  KeepDescriptionOnScreen();
  action.report_bug = state.bug_report && TitleActionClicked(0);
  if (!body_shown) {
    EndSkinnedWindow();
    return action;
  }

  // ── L'EN-TÊTE : L'ICÔNE, ET LES DEUX LIGNES QUE LA TABLE LUI DONNE ────────
  std::span<const std::string> lines = view.lines;
  if (view.icon.usable()) {
    ImGui::Image(view.icon.id, ImVec2(kSkillIconSidePixels, kSkillIconSidePixels));
    ImGui::SameLine(0.0F, kPictureGapPixels);
  }
  ImGui::BeginGroup();
  const std::size_t header = std::min(kSkillDescriptionHeaderLines, lines.size());
  if (header > 0) {
    RichText top =
        LayOutRichTextHere(lines.subspan(0, header), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                           ImGui::GetContentRegionAvail().x);
    KeepReadable(top, art.look().paper);
    static_cast<void>(DrawRichText("##entete_competence", top,
                                   ImGui::GetTextLineHeightWithSpacing(), state.text_select));
    lines = lines.subspan(header);
  } else {
    // ⚠ REPLI QUAND LA TABLE NE DIT RIEN : le numéro reste la seule façon de
    // retrouver une compétence ailleurs, et il ne doit pas disparaître.
    ImGui::TextDisabled(core::Tr("ID : %u"), static_cast<unsigned>(view.skill_id));
  }
  ImGui::EndGroup();

  // ── TROIS ONGLETS ─────────────────────────────────────────────────────────
  //
  // 🔴 LE TEXTE DU JEU D'UN CÔTÉ, LES CHIFFRES DU SERVEUR DE L'AUTRE, et ce
  // n'est pas un rangement de commodité : la description est écrite une fois pour
  // toutes dans une table de l'installation, le cast et les dégâts se CALCULENT
  // sur l'équipement et les états du personnage. Deux natures, deux onglets — et
  // les seconds ne se demandent que lorsqu'on les regarde.
  ImGui::Separator();
  if (BeginSkinTabBar("onglets_competence", art)) {
    if (ImGui::BeginTabItem(core::Tr("Description"))) {
      state.tab = 0;
      if (lines.empty()) {
        ImGui::TextDisabled("%s", core::Tr("(pas de description)"));
      } else {
        // Les couleurs de la table, choisies pour un papier blanc, lisibles sur
        // celui de l'habillage.
        RichText laid = LayOutRichTextHere(lines, ImGui::GetColorU32(ImGuiCol_Text),
                                           ImGui::GetContentRegionAvail().x);
        KeepReadable(laid, art.look().paper);
        const RichTextAction followed =
            DrawRichText("##description_competence", laid, ImGui::GetTextLineHeightWithSpacing(),
                         state.text_select);
        if (followed.followed >= 0 && followed.followed < static_cast<int>(laid.links.size())) {
          // ⚠ LA CHARGE SE RECOPIE : la disposition meurt à la fin de cette
          // image, et l'appelant lira l'action après.
          const RichLink& link = laid.links[static_cast<std::size_t>(followed.followed)];
          action.followed = link.payload;
          action.followed_kind = link.kind;
        }
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(core::Tr("Infos techniques"))) {
      state.tab = 1;
      DrawCastTab(view, art, action);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(core::Tr("Dégâts"))) {
      state.tab = 2;
      DrawDamageTab(state, view, art, action);
      ImGui::EndTabItem();
    }
    EndSkinTabBar();
  }

  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
