#include "ro/ui/palette_editor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "palette_doll.h"
#include "palette_pickers.h"
#include "palette_presets.h"

namespace ro::ui {
namespace {

// Le repli du texte, en largeurs de police. ⚠ DÉRIVÉ DE LA POLICE et non d'un
// nombre de pixels : la taille du texte est un réglage du joueur, et un repli
// en pixels serait faux dès qu'il la change. `TextWrapped` ne conviendrait pas
// non plus — il se replie sur la largeur de la FENÊTRE, qui n'existe pas
// encore dans une fenêtre auto-dimensionnée.
constexpr float kWrapInFonts = 24.0F;

// 🔴 SOUS CETTE CLARTÉ, UNE PIÈCE NE PEUT PLUS ÊTRE ÉCLAIRCIE, et on l'écrit.
// Deux cent vingt-quatre sur deux cent cinquante-cinq — au-dessus, le plafond
// ne se remarque pas. Sur les 421 corps, cela désigne
// environ une pièce sur six, assez rare pour que la mention veuille dire
// quelque chose, assez fréquent pour couvrir les cas signalés.
constexpr int kShadowCeiling = 224;
constexpr int kFullValue = 255;

// L'ambre dont s'allume la pièce que la pipette désigne, la même que celle du
// cadre d'une case choisie.
constexpr ImU32 kPipetteColor = IM_COL32(255, 209, 110, 255);

// ── UNE PIÈCE ───────────────────────────────────────────────────────────────

// Ce qu'une rangée de pièce a relevé.
struct PieceAnswer {
  // La couleur désignée dans le nuancier, ou rien.
  std::optional<std::array<std::uint8_t, 3>> picked;
  // La souris est sur la ligne — pastille ou nom, les deux posent la même
  // question.
  bool hovered = false;
  // Sa croix vient d'être cliquée.
  bool reset = false;
};

[[nodiscard]] PieceAnswer DrawPiece(std::size_t rank, ImU32 color, int ceiling,
                                    const render::RampAdjust& adjust, const SkinArt& art,
                                    bool& picker_open, bool pointed) {
  ImGui::PushID(static_cast<int>(rank));
  PieceAnswer answer;

  std::array<float, 3> rgb = {static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xFFU) / 255.0F,
                              static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xFFU) / 255.0F,
                              static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xFFU) / 255.0F};
  bool open = false;
  // `##` : la pastille sans libellé. Le nom suit, sur la même ligne.
  if (SkinColorSwatch("##pastille", rgb, art, &open)) {
    const auto to_byte = [](float channel) {
      return static_cast<std::uint8_t>(std::clamp(std::lround(channel * 255.0F), 0L, 255L));
    };
    answer.picked = std::array<std::uint8_t, 3>{to_byte(rgb[0]), to_byte(rgb[1]), to_byte(rgb[2])};
  }
  picker_open = picker_open || open;
  // ⚠ C'EST LA PASTILLE QUI RÉPOND ICI : sans libellé à montrer, elle est le
  // dernier élément soumis — voir `SkinColorSwatch`.
  answer.hovered = ImGui::IsItemHovered();
  // 🔴 SUR LA MÊME LIGNE QUE LA PASTILLE, et c'est ce qui rend la fenêtre
  // compacte : sans cet appel, chaque pièce prend DEUX rangées — huit pièces
  // en font seize —, la liste double de hauteur et le pantin, dont le côté
  // vaut la hauteur CALCULÉE de la liste, se retrouve deux fois trop petit
  // dans son cadre.
  ImGui::SameLine();

  const bool in_shadow = ceiling < kShadowCeiling;
  char label[64] = {};
  if (in_shadow) {
    std::snprintf(label, sizeof(label), "%s %zu %s", core::Tr("Pièce"), rank + 1,
                  core::Tr("(ombre)"));
  } else {
    std::snprintf(label, sizeof(label), "%s %zu", core::Tr("Pièce"), rank + 1);
  }
  // 🔴 `AlignTextToFramePadding`, ET SURTOUT PAS UN CURSEUR POSÉ À LA MAIN.
  // Repositionner le curseur en ordonnée après le texte laisse l'abscisse où
  // elle est et annule l'avance de ligne : les huit pièces se dessineraient
  // TOUTES au même endroit, empilées, et la fenêtre paraîtrait n'en avoir
  // qu'une.
  ImGui::AlignTextToFramePadding();
  // LA PIÈCE QUE LA PIPETTE DÉSIGNE S'ALLUME : c'est la réponse à « quelle
  // ligne commande cette manche ? », et elle doit sauter aux yeux sans qu'on
  // quitte le pantin du regard.
  if (pointed) {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kPipetteColor), "%s", label);
  } else {
    ImGui::TextUnformatted(label);
  }
  if (ImGui::IsItemHovered()) {
    answer.hovered = true;
    // ⚠ L'EXPLICATION NE S'AFFICHE QUE POUR LES PIÈCES CONCERNÉES : une bulle
    // qui dirait « pas de limite » sur les autres noierait celle qui compte.
    if (in_shadow) {
      char text[256] = {};
      std::snprintf(text, sizeof(text),
                    core::Tr("Zone d'ombre du sprite : la clarté de cette pièce plafonne à "
                             "%d %%. Les réglages gardent le modelé du dégradé, donc une "
                             "pièce peinte sombre le reste — tu peux changer sa teinte, pas "
                             "l'éclairer."),
                    (ceiling * 100) / kFullValue);
      SkinTooltip(text);
    }
  }

  // ── REMISE À ZÉRO, PAR PIÈCE ──────────────────────────────────────────────
  //
  // 🔴 LE BOUTON N'APPARAÎT QUE SI LA PIÈCE A ÉTÉ RETOUCHÉE. C'est ce qui le
  // rend lisible sans icône dédiée : une croix posée sur toutes les lignes se
  // lirait « supprimer la pièce », alors qu'à côté d'une pièce visiblement
  // modifiée elle ne peut vouloir dire qu'« annuler ça ». Elle montre du même
  // coup, d'un coup d'œil, ce qu'on a touché.
  //
  // « x » et non une flèche de retour : la police de l'interface ne couvre pas
  // les icônes, une flèche Unicode n'y serait qu'un rectangle vide.
  if (!adjust.neutral()) {
    ImGui::SameLine();
    answer.reset = SkinSmallButton("x", art);
    if (ImGui::IsItemHovered()) {
      answer.hovered = true;
      SkinTooltip(core::Tr("Réinitialiser cette pièce"));
    }
  }
  ImGui::PopID();
  return answer;
}

// Une confirmation : le texte, le bouton qui agit, celui qui renonce.
[[nodiscard]] bool Confirm(const char* id, const char* question, const char* confirm,
                           const SkinArt& art) {
  bool done = false;
  if (!BeginSkinPopup(id)) {
    return done;
  }
  ImGui::PushTextWrapPos(ImGui::GetFontSize() * kWrapInFonts);
  ImGui::TextUnformatted(question);
  ImGui::PopTextWrapPos();
  ImGui::Separator();
  if (SkinButton(confirm, art)) {
    done = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
  return done;
}

// CE QUE LE SERVEUR A FAIT DE LA DEMANDE, à côté du bouton qui l'a faite.
//
// ⚠ « PARTAGÉ » SEULEMENT À L'ÉCHO : le serveur jette sans un mot ce qu'il
// refuse, et dire plus que ce qu'on sait serait un mensonge que le joueur
// relirait de travers le jour où son style ne parvient pas aux autres.
void DrawSendStatus(PaletteSendStatus send) {
  const char* said = nullptr;
  switch (send) {
    case PaletteSendStatus::kNone:
      return;
    case PaletteSendStatus::kWaiting:
      said = core::Tr("Envoi…");
      break;
    case PaletteSendStatus::kShared:
      said = core::Tr("Partagé.");
      break;
    case PaletteSendStatus::kRefused:
      said = core::Tr("Non retenu, réessaie.");
      break;
  }
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(said);
  if (send == PaletteSendStatus::kRefused && ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Le serveur n'a pas renvoyé ton style : il ne l'a pas retenu. Une demande "
                 "trop rapprochée de la précédente, ou une liaison coupée, suffit. Attends "
                 "une seconde, puis réessaie."));
  }
}

// LES BOUTONS QUI DÉFONT : revenir, repartir de zéro, oublier, supprimer.
void DrawUndoButtons(const PaletteEditorView& view, const SkinArt& art,
                     PaletteEditorAction& action) {
  ImGui::BeginDisabled(!view.has_edits);
  if (SkinButton(core::Tr("Revenir à mon style"), art)) {
    action.verb = PaletteEditorVerb::kBackToWorn;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    SkinTooltip(view.has_edits
                    ? core::Tr("Annule tes retouches en cours et revient à ce que ton "
                               "personnage porte.")
                    : core::Tr("Rien à annuler : l'aperçu montre déjà ce que ton personnage "
                               "porte."));
  }

  // 🔴 EMPILÉS, ET NON SUR UNE RANGÉE : quatre libellés côte à côte imposent à
  // la fenêtre une largeur que rien d'autre ne demande — le double de celle de
  // son contenu — et cette largeur ENFLE À LA TRADUCTION, où chaque libellé
  // peut s'allonger. Empilés, ils tiennent dans toutes les langues.
  if (SkinButton(core::Tr("Repartir de zéro"), art)) {
    ImGui::OpenPopup("zero");
  }
  if (Confirm("zero",
              core::Tr("Remettre tout l'aperçu à zéro ? Tes réglages en cours seront perdus."),
              core::Tr("Réinitialiser"), art)) {
    action.verb = PaletteEditorVerb::kBlankSlate;
  }

  // 🔴 SANS LIAISON, NI L'UN NI L'AUTRE NE PEUT RIEN : le serveur seul efface,
  // et un bouton cliquable laisserait croire qu'il a reçu la demande.
  ImGui::BeginDisabled(!view.connected);
  // ⚠ LE GESTE FIN N'APPARAÎT QUE QUAND IL DIT AUTRE CHOSE QUE LE SUIVANT :
  // il faut que CE corps ait son propre style, et qu'il en reste un autre
  // derrière. Sinon « oublier celui-ci » et « tout supprimer » font la même
  // chose, sous deux libellés dont le plus obscur laisse croire à un troisième
  // geste.
  const bool forgettable = view.body_has_own_style && view.body_style_count > 1;
  if (forgettable) {
    if (SkinButton(core::Tr("Oublier le style de ce corps"), art)) {
      ImGui::OpenPopup("oublier");
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("Efface le style de CE corps seulement. Tes autres corps gardent le "
                   "leur, et celui-ci reprend ton style principal."));
    }
    if (Confirm("oublier",
                core::Tr("Oublier le style de ce corps ? Il reprendra ton style principal, "
                         "chez toi comme chez les autres. Tes autres corps ne changent pas."),
                core::Tr("Oublier"), art)) {
      action.verb = PaletteEditorVerb::kForgetBody;
    }
  }

  if (SkinButton(core::Tr("Supprimer mon style"), art)) {
    ImGui::OpenPopup("supprimer");
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Efface ta palette, chez toi ET sur le serveur. Ton personnage "
                 "retrouve l'apparence qu'il avait avant, pour toi comme pour les "
                 "autres. Ce n'est PAS un retour à ta dernière couleur partagée : "
                 "celle-ci est perdue."));
  }
  if (Confirm("supprimer",
              core::Tr("Supprimer ton style, chez toi ET sur le serveur ? Ton personnage "
                       "retrouve son apparence d'avant, pour toi comme pour les autres. "
                       "C'est définitif."),
              core::Tr("Supprimer"), art)) {
    action.verb = PaletteEditorVerb::kDeleteAll;
  }
  ImGui::EndDisabled();
}

}  // namespace

PaletteEditorAction DrawPaletteEditor(PaletteEditorState& state, const PaletteEditorView& view,
                                      const SkinArt& art) {
  PaletteEditorAction action;
  // 🔴 REMIS À « AUCUNE » AVANT TOUTE SORTIE, ET PAS AU MOMENT DE DESSINER LES
  // BOUTONS. C'est la grille OUVERTE qui se redéclare en se dessinant ; poser
  // la remise à zéro plus bas laissait les chemins de sortie anticipée — une
  // fenêtre fermée, un corps dont aucune pièce ne se règle — garder une grille
  // ouverte pour toute la session, avec ses vignettes.
  state.picker.open = PaletteEditorPicker::kNone;
  if (!state.open) {
    return action;
  }

  static WindowName name(kPaletteEditorTitle, kPaletteEditorId);
  bool open = true;
  // ⚠ SANS REDIMENSIONNEMENT : la fenêtre suit son contenu. Une largeur
  // imposée serait fausse dès qu'on change de langue ou de taille de police,
  // qui sont l'une et l'autre des réglages du joueur.
  PoseBugReportButton(state.bug_report);
  const bool begun =
      BeginSkinnedWindow(name.Get(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  action.report_bug = begun && state.bug_report && TitleActionClicked(0);
  if (!open) {
    state.open = false;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }

  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
  ImGui::TextUnformatted(core::Tr("Choisissez une pièce, puis réglez sa couleur."));
  ImGui::PopTextWrapPos();

  // ── CE QUE LES RÉGLAGES ATTEIGNENT VRAIMENT ───────────────────────────────
  if (view.coverage_percent >= 0) {
    char line[128] = {};
    std::snprintf(line, sizeof(line), core::Tr("Réglable : %d%% du corps"), view.coverage_percent);
    ImGui::TextUnformatted(line);
  }

  // ── À QUEL CORPS CE RÉGLAGE APPARTIENT ────────────────────────────────────
  //
  // ⚠ LE CORPS NE PEUT PAS ÊTRE NOMMÉ : son nom est le chemin de son sprite,
  // en coréen. On dit donc ce qui se DÉCIDE, pas ce qui se porte.
  if (view.body_has_own_style) {
    char line[160] = {};
    std::snprintf(line, sizeof(line), core::Tr("Ce corps a son propre style (%d au total)."),
                  view.body_style_count);
    ImGui::TextUnformatted(line);
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("Les couleurs d'un corps ne veulent rien dire sur un autre : chaque "
                   "monture, chaque costume a donc son propre style. Valider ne change que "
                   "celui-ci."));
    }
  } else if (view.body_style_count > 0) {
    ImGui::TextUnformatted(core::Tr("Ce corps reprend ton style principal."));
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("Tu n'as pas encore habillé ce corps : il reprend ton style principal, au "
                   "plus proche. Valide pour lui donner le sien, sans toucher aux autres."));
    }
  }
  ImGui::Separator();

  if (view.ramps.empty()) {
    // ⚠ ON LE DIT, ON N'ALIGNE PAS DES COMMANDES QUI NE COMMANDENT RIEN.
    ImGui::TextDisabled("%s", core::Tr("Aucune pièce de ce corps ne se règle."));
    EndSkinnedWindow();
    return action;
  }

  // La hauteur de la liste se CALCULE au lieu de se mesurer après coup : le
  // pantin vient en premier, et il lui faut cette hauteur pour être carré.
  const float row = ImGui::GetFrameHeight();
  const auto rows = static_cast<float>(view.ramps.size());
  const float list_height = rows * row + (rows - 1.0F) * ImGui::GetStyle().ItemSpacing.y;
  // Le pantin est carré, à GAUCHE de la liste — et pas l'inverse. Un nuancier
  // s'ouvre SOUS sa pastille : avec les pièces à gauche, il retomberait pile
  // sur le pantin et masquerait ce qu'on cherche justement à regarder en
  // choisissant une couleur.
  DrawPaletteDoll(state.doll, list_height, art.look());
  ImGui::SameLine();

  int hovered_ramp = -1;
  bool picker_open = false;
  ImGui::BeginGroup();
  for (std::size_t rank = 0; rank < view.ramps.size() && rank < render::kMaxRamps; ++rank) {
    const ImU32 color = rank < view.colors.size() ? view.colors[rank] : IM_COL32_WHITE;
    const int ceiling = rank < view.ceilings.size() ? view.ceilings[rank] : kFullValue;
    const bool pointed = view.pipette_ramp == static_cast<int>(rank);
    const PieceAnswer answer =
        DrawPiece(rank, color, ceiling, view.recipe.ramps[rank], art, picker_open, pointed);
    if (answer.picked.has_value()) {
      action.picked_ramp = static_cast<int>(rank);
      action.picked_color = *answer.picked;
    }
    if (answer.reset) {
      action.reset_ramp = static_cast<int>(rank);
    }
    if (answer.hovered) {
      hovered_ramp = static_cast<int>(rank);
    }
  }
  ImGui::EndGroup();
  // CE QUI PULSE SUR LE PANTIN : la ligne survolée si le curseur est dans la
  // liste, sinon la pièce que la pipette désigne sur le pantin. Les deux
  // gestes posent la même question par les deux bouts, et allument donc la
  // même chose.
  state.hovered_ramp = hovered_ramp >= 0 ? hovered_ramp : view.pipette_ramp;

  ImGui::Separator();
  // 🔴 TOUT CE QUI SUIT PASSE SOUS LE NUANCIER quand il est ouvert. On le
  // neutralise plutôt que de le déplacer : un sélecteur qui surgit loin de sa
  // pastille est déroutant, alors qu'un réglage grisé pendant qu'on choisit
  // une couleur se comprend tout seul.
  if (picker_open) {
    ImGui::BeginDisabled();
  }

  if (view.bounds.usable()) {
    DrawChoiceButtons(state, view, art, action);
    ImGui::Separator();
  }

  DrawUndoButtons(view, art, action);

  ImGui::Separator();
  // 🔴 SANS LIAISON, VALIDER NE PEUT RIEN : le bouton se grise plutôt que de
  // laisser croire que le serveur a reçu quelque chose.
  ImGui::BeginDisabled(!view.connected);
  if (SkinButton(core::Tr("Valider et partager mon style"), art)) {
    action.verb = PaletteEditorVerb::kApply;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Applique ce style à ton personnage — jusqu'ici, rien n'a changé hors "
                 "de cet aperçu. Les autres joueurs te verront ainsi."));
  }
  DrawSendStatus(view.send);

  DrawPalettePresets(state, view, art, action);

  if (picker_open) {
    ImGui::EndDisabled();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
