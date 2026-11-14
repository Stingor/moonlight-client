#include "ro/ui/palette_editor.h"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "palette_doll.h"
#include "palette_layers.h"
#include "palette_outfits.h"
#include "palette_pickers.h"
#include "palette_pieces.h"
#include "palette_presets.h"
#include "palette_retouches.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// Le repli du texte, en largeurs de police. ⚠ DÉRIVÉ DE LA POLICE et non d'un
// nombre de pixels : la taille du texte est un réglage du joueur, et un repli
// en pixels serait faux dès qu'il la change. `TextWrapped` ne conviendrait pas
// non plus — il se replie sur la largeur de la FENÊTRE, qui n'existe pas
// encore dans une fenêtre auto-dimensionnée.
constexpr float kWrapInFonts = 24.0F;
// Le côté minimal du pantin, en points d'interface : un corps de deux pièces
// donnerait sinon une liste, donc un pantin, de deux lignes de haut.
constexpr float kDollMinPoints = 150.0F;

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

// Une bulle, même sur un bouton grisé : c'est là qu'on se demande pourquoi.
void Explain(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(text);
  }
}

// CE QUE LE SERVEUR A FAIT DE LA DEMANDE, à côté du bouton qui l'a faite.
//
// ⚠ « PARTAGÉ » SEULEMENT À L'ÉCHO : le serveur jette sans un mot ce qu'il
// refuse, et dire plus que ce qu'on sait serait un mensonge que le joueur
// relirait de travers le jour où son style ne parvient pas aux autres.
void DrawSendStatus(PaletteSendStatus send, const char* refusal = nullptr) {
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
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(said);
  if (send == PaletteSendStatus::kRefused && refusal != nullptr && ImGui::IsItemHovered()) {
    SkinTooltip(refusal);
  } else if (send == PaletteSendStatus::kRefused && ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Le serveur n'a pas renvoyé ton style : il ne l'a pas retenu. Une demande "
                 "trop rapprochée de la précédente, ou une liaison coupée, suffit. Attends "
                 "une seconde, puis réessaie."));
  }
}

// DÉFAIRE, REFAIRE : un pas par geste, Ctrl+Z et Ctrl+Y.
void DrawHistory(const PaletteEditorView& view, const SkinArt& art, PaletteEditorAction& action) {
  ImGui::BeginDisabled(!view.can_undo);
  if (SkinSmallButton(core::Tr("Défaire"), art)) {
    action.verb = PaletteEditorVerb::kUndo;
  }
  ImGui::EndDisabled();
  Explain(core::Tr("Défait ton dernier geste (Ctrl+Z)."));
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.can_redo);
  if (SkinSmallButton(core::Tr("Refaire"), art)) {
    action.verb = PaletteEditorVerb::kRedo;
  }
  ImGui::EndDisabled();
  Explain(core::Tr("Refait le geste défait (Ctrl+Y)."));
}

// LES RACCOURCIS DE L'HISTORIQUE, fenêtre au premier plan.
//
// ⚠ JAMAIS PENDANT UNE SAISIE : Ctrl+Z y défait le texte tapé.
void ReadHistoryKeys(PaletteEditorAction& action) {
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.KeyCtrl || io.WantTextInput ||
      !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
    action.verb = io.KeyShift ? PaletteEditorVerb::kRedo : PaletteEditorVerb::kUndo;
  } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
    action.verb = PaletteEditorVerb::kRedo;
  }
}

// L'INSPIRATION : des couleurs en harmonie sur les pièces non gardées, un
// bouton par harmonie.
void DrawInspiration(const SkinArt& art, PaletteEditorAction& action) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Inspiration :"));
  Explain(
      core::Tr("Tire des couleurs qui vont ensemble. Clic droit sur une pièce pour la garder "
               "telle quelle."));
  const auto offer = [&art, &action](const char* label, const char* tip, PaletteHarmony harmony) {
    ImGui::SameLine();
    if (SkinSmallButton(label, art)) {
      action.verb = PaletteEditorVerb::kInspire;
      action.harmony = harmony;
    }
    Explain(tip);
  };
  offer(core::Tr("Teintes voisines"),
        core::Tr("Des teintes proches les unes des autres, sur les pièces non gardées."),
        PaletteHarmony::kAnalogous);
  offer(core::Tr("Complémentaires"),
        core::Tr("Des teintes opposées sur le cercle, sur les pièces non gardées."),
        PaletteHarmony::kComplementary);
  offer(core::Tr("Triade"),
        core::Tr("Trois teintes à égale distance sur le cercle, sur les pièces non gardées."),
        PaletteHarmony::kTriadic);
}

// LES BOUTONS QUI DÉFONT : revenir, repartir de zéro, oublier, supprimer.
void DrawUndoButtons(const PaletteEditorView& view, const SkinArt& art,
                     PaletteEditorAction& action) {
  const bool on_body = view.tab == PaletteTabKind::kBody;
  ImGui::BeginDisabled(!view.has_edits);
  if (SkinButton(core::Tr("Revenir à mon style"), art)) {
    action.verb = PaletteEditorVerb::kBackToWorn;
  }
  ImGui::EndDisabled();
  Explain(view.has_edits ? core::Tr("Annule tes retouches en cours et revient à ce que ton "
                                    "personnage porte. Ctrl+Z les rend.")
                         : core::Tr("Rien à annuler : l'aperçu montre déjà ce que ton personnage "
                                    "porte."));

  // 🔴 SUR UNE RANGÉE : revenir, repartir de zéro, supprimer — trois gestes qui
  // défont, côte à côte.
  ImGui::SameLine();
  if (SkinButton(core::Tr("Repartir de zéro"), art)) {
    ImGui::OpenPopup("zero");
  }
  Explain(on_body ? core::Tr("Remet toutes les pièces à leur couleur d'origine, dans l'aperçu "
                             "seulement. Ta coupe et ta couleur de cheveux ne bougent pas.")
                  : core::Tr("Remet toutes les pièces de ce calque à leur couleur d'origine, "
                             "dans l'aperçu seulement."));
  if (Confirm("zero",
              core::Tr("Remettre tout l'aperçu à zéro ? Ctrl+Z le défait si tu changes d'avis."),
              core::Tr("Réinitialiser"), art)) {
    action.verb = PaletteEditorVerb::kBlankSlate;
  }

  // 🔴 « SUPPRIMER CE STYLE », SUR LE CORPS SEUL : c'est là qu'un style se
  // range. Un chapeau, une cape ont leur propre teinture, qui appartient à
  // l'item.
  if (!on_body) {
    return;
  }
  // 🔴 SANS LIAISON, IL NE PEUT RIEN : le serveur seul efface.
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.connected);
  // ⚠ IL OUBLIE CE CORPS SEUL QUAND CELA DIT AUTRE CHOSE QUE TOUT EFFACER : il
  // faut que CE corps ait son propre style, et qu'il en reste un autre
  // derrière. Sinon, son style est LE style, et le supprimer efface tout.
  const bool forgettable = view.body_has_own_style && view.body_style_count > 1;
  if (SkinButton(core::Tr("Supprimer ce style"), art)) {
    ImGui::OpenPopup(forgettable ? "oublier" : "supprimer");
  }
  Explain(forgettable
              ? core::Tr("Efface le style de CE corps seulement. Tes autres corps gardent le "
                         "leur, et celui-ci reprend ton style principal.")
              : core::Tr("Efface les couleurs de tous tes corps, de tes cheveux et de ta peau, "
                         "chez toi ET sur le serveur. Ta coupe et ta couleur de cheveux restent "
                         "celles que tu portes ; les teintures de tes items leur appartiennent "
                         "et restent."));
  if (Confirm("oublier",
              core::Tr("Oublier le style de ce corps ? Il reprendra ton style principal, "
                       "chez toi comme chez les autres. Tes autres corps ne changent pas."),
              core::Tr("Oublier"), art)) {
    action.verb = PaletteEditorVerb::kForgetBody;
  }
  if (Confirm("supprimer",
              core::Tr("Supprimer ton style, chez toi ET sur le serveur ? Tes corps, tes "
                       "cheveux et ta peau retrouvent leurs couleurs d'origine, pour toi comme "
                       "pour les autres ; ta coupe et ta couleur de cheveux restent, et tes "
                       "items gardent leur teinture. C'est définitif."),
              core::Tr("Supprimer"), art)) {
    action.verb = PaletteEditorVerb::kDeleteAll;
  }
  ImGui::EndDisabled();
}

// POURQUOI « VALIDER » EST GRISÉ, ou ce qu'il fera.
[[nodiscard]] const char* SendExplanation(const PaletteEditorView& view) {
  if (!view.connected) {
    return core::Tr("Hors ligne : rien ne peut partir.");
  }
  if (view.items_wait_atelier) {
    return core::Tr("Une teinture d'item se valide auprès du styliste.");
  }
  if (!view.has_something_to_send) {
    return core::Tr("Rien à valider : ton personnage porte déjà ce style.");
  }
  if (!view.can_send) {
    return core::Tr("Un instant : le serveur n'accepte qu'une demande par seconde.");
  }
  return core::Tr(
      "Applique ce style à ton personnage — jusqu'ici, rien n'a changé hors "
      "de cet aperçu. Les autres joueurs te verront ainsi.");
}

// VALIDER, ET EN FAIRE SON STYLE PRINCIPAL.
//
// 🔴 ON PRÉVIENT AVANT D'ÉVINCER : au plafond de corps habillés, valider un
// corps neuf efface le plus ancien sur le serveur, sans retour possible.
void DrawSendButtons(const PaletteEditorView& view, const SkinArt& art,
                     PaletteEditorAction& action) {
  const bool sendable = view.connected && view.has_something_to_send && view.can_send;
  ImGui::BeginDisabled(!sendable);
  if (SkinButton(core::Tr("Valider et partager mon style"), art)) {
    if (view.evicts_oldest) {
      ImGui::OpenPopup("evincer");
    } else {
      action.verb = PaletteEditorVerb::kApply;
    }
  }
  ImGui::EndDisabled();
  Explain(SendExplanation(view));
  ImGui::SameLine();
  DrawSendStatus(view.send);
  // ⚠ LES CALQUES ONT LEUR PROPRE VERDICT : le corps peut être partagé et un
  // chapeau refusé hors de l'atelier, et le joueur doit lire les deux.
  if (view.layers_send != PaletteSendStatus::kNone) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(core::Tr("Calques :"));
    ImGui::SameLine();
    DrawSendStatus(view.layers_send, view.layers_refusal);
  }

  // ⚠ SEULEMENT QUAND IL Y A UN AUTRE STYLE QUE CELUI-CI : le premier corps
  // validé devient le principal de lui-même. Et seulement sur le corps : un
  // chapeau n'est pas un style principal.
  const bool may_lead =
      view.tab == PaletteTabKind::kBody && view.body_style_count > 0 && !view.is_main;
  if (may_lead) {
    ImGui::BeginDisabled(!view.connected || !view.can_send);
    if (SkinButton(core::Tr("En faire mon style principal"), art)) {
      if (view.evicts_oldest) {
        ImGui::OpenPopup("evincer_principal");
      } else {
        action.verb = PaletteEditorVerb::kMakeMain;
      }
    }
    ImGui::EndDisabled();
    Explain(
        core::Tr("Valide ce style et le donne à tous tes corps qui n'ont pas le leur, "
                 "chez toi comme chez les autres."));
  }

  const char* evicts = core::Tr(
      "Tu as déjà habillé autant de corps que le serveur en garde. Valider "
      "celui-ci effacera le style du plus ancien — jamais ton style principal.");
  if (Confirm("evincer", evicts, core::Tr("Valider"), art)) {
    action.verb = PaletteEditorVerb::kApply;
  }
  if (Confirm("evincer_principal", evicts, core::Tr("Valider"), art)) {
    action.verb = PaletteEditorVerb::kMakeMain;
  }
}

// À QUEL CORPS CE RÉGLAGE APPARTIENT, et le choix d'en régler un autre.
//
// ⚠ LE CORPS NE PEUT PAS ÊTRE NOMMÉ : son nom est le chemin de son sprite, en
// coréen. On dit donc ce qui se DÉCIDE, pas ce qui se porte.
void DrawBodyLines(const PaletteEditorView& view) {
  if (view.is_main) {
    ImGui::TextUnformatted(core::Tr("Ce corps porte ton style principal."));
  } else if (view.body_has_own_style) {
    char line[160] = {};
    std::snprintf(line, sizeof(line), core::Tr("Ce corps a son propre style (%d au total)."),
                  view.body_style_count);
    ImGui::TextUnformatted(line);
    Explain(
        core::Tr("Les couleurs d'un corps ne veulent rien dire sur un autre : chaque "
                 "monture, chaque costume a donc son propre style. Valider ne change que "
                 "celui-ci."));
  } else if (view.body_style_count > 0) {
    ImGui::TextUnformatted(core::Tr("Ce corps reprend ton style principal."));
    Explain(
        core::Tr("Tu n'as pas encore habillé ce corps : il reprend ton style principal, au "
                 "plus proche. Valide pour lui donner le sien, sans toucher aux autres."));
  }
}

// Un réglage d'effet en cours diffère de ce que l'effet porte.
[[nodiscard]] bool EffectsDiffer(const PaletteEffectsState& state, const PaletteEffectsView& view) {
  return std::ranges::any_of(view.rows, [&state](const PaletteEffectRow& row) {
    return EffectDraftOf(state, row) != row.worn;
  });
}

// CE QUE DIT UN ONGLET SANS PIÈCE : la raison du calque, s'il en a une.
[[nodiscard]] const char* EmptyTabText(const PaletteEditorView& view) {
  if (view.tab == PaletteTabKind::kBody) {
    return core::Tr("Aucune pièce de ce corps ne se règle.");
  }
  const PaletteTab* tab = OpenTab(view);
  if (tab != nullptr && tab->blocked != PaletteTabBlock::kNone) {
    return TabBlockReason(tab->blocked);
  }
  return core::Tr("Aucune pièce de ce calque ne se règle.");
}

// LE PANTIN, LA LISTE ET LEURS COMMANDES. Rend vrai quand un nuancier est
// ouvert.
[[nodiscard]] bool DrawPiecesBlock(PaletteEditorState& state, const PaletteEditorView& view,
                                   const SkinArt& art, PaletteEditorAction& action) {
  // La hauteur de la liste se CALCULE au lieu de se mesurer après coup : le
  // pantin vient en premier, et il lui faut cette hauteur pour être carré.
  const float row = ImGui::GetFrameHeight();
  // L'intitulé du reste prend une rangée de plus.
  const auto rows = static_cast<float>(view.ramps.size() + (view.rest_first >= 0 ? 1U : 0U));
  const float list_height = rows * row + (rows - 1.0F) * ImGui::GetStyle().ItemSpacing.y;
  // Le pantin est carré, à GAUCHE de la liste — et pas l'inverse. Un nuancier
  // s'ouvre SOUS sa pastille : avec les pièces à gauche, il retomberait pile
  // sur le pantin et masquerait ce qu'on cherche justement à regarder.
  ImGui::BeginGroup();
  DrawPaletteDoll(state.doll, std::max(list_height, kDollMinPoints * Scale()), art.look(),
                  /*pipette=*/true);
  ImGui::EndGroup();
  // 🔴 LA PIPETTE CLIQUÉE OUVRE LE NUANCIER de la pièce visée, dans la même
  // image : la liste se dessine juste après.
  //
  // ⚠ UN CLIC SUR UN AUTRE CALQUE OUVRE D'ABORD SON ONGLET : la pièce attend
  // l'image où la liste sera la sienne.
  // 🔴 EN RETOUCHE EXACTE, LE CLIC VISE UNE COULEUR DE LA PALETTE, et non la
  // pièce qui la contient.
  if (state.retouch_mode && state.doll.clicked) {
    if (view.pick_index > 0) {
      state.retouch_index = view.pick_index;
      state.retouch_opening = true;
    }
  } else if (state.doll.clicked && view.pipette.found && view.pipette.tab != view.tab) {
    state.tab = view.pipette.tab;
    state.tab_asked = true;
    state.open_piece = view.pipette.skin ? -1 : view.pipette.piece;
    if (view.pipette.skin) {
      state.skin_open = view.pipette.tab;
    }
  } else if (state.doll.clicked && view.pipette.found && view.pipette.skin) {
    // 🔴 LA PEAU N'EST PAS UNE PIÈCE : la pipette ouvre le nuancier de la ligne
    // de peau, qui se dessine sous la liste.
    state.skin_open = view.tab;
  } else if (state.doll.clicked && view.pipette_ramp >= 0) {
    state.open_piece = view.pipette_ramp;
  }

  ImGui::SameLine();
  const PiecesAnswer pieces = DrawPalettePieces(state, view, art, action);
  // CE QUI PULSE SUR LE PANTIN : la ligne survolée si le curseur est dans la
  // liste, sinon la pièce que la pipette désigne sur le pantin.
  state.hovered_ramp = pieces.hovered >= 0 ? pieces.hovered : view.pipette_ramp;
  DrawPaletteDollControls(state.doll, art, view.has_edits,
                          [&view, &art, &action] { DrawHistory(view, art, action); });
  return pieces.picker_open;
}

}  // namespace

void ClosePaletteEditor(PaletteEditorState& state) { state.close_asked = state.open; }

void TogglePaletteEditor(PaletteEditorState& state) {
  if (state.open) {
    ClosePaletteEditor(state);
  } else {
    state.open = true;
  }
}

PaletteEditorAction DrawPaletteEditor(PaletteEditorState& state, const PaletteEditorView& view,
                                      const SkinArt& art) {
  PaletteEditorAction action;
  // 🔴 REMIS À « AUCUNE » AVANT TOUTE SORTIE : c'est la grille OUVERTE qui se
  // redéclare en se dessinant, et un chemin de sortie anticipée la laisserait
  // ouverte pour toute la session, avec ses vignettes.
  state.picker.open = PaletteEditorPicker::kNone;
  state.hovered_preset = -1;
  state.doll.before = false;
  state.skin_focus = false;
  if (!state.open) {
    state.close_asked = false;
    return action;
  }

  static WindowName name(kPaletteEditorTitle, kPaletteEditorId);
  bool open = true;
  // ⚠ SANS REDIMENSIONNEMENT : la fenêtre suit son contenu.
  PoseBugReportButton(state.bug_report);
  const bool begun =
      BeginSkinnedWindow(name.Get(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  action.report_bug = begun && state.bug_report && TitleActionClicked(0);
  // 🔴 UN STYLE NON VALIDÉ SE PERD À LA FERMETURE : on le dit avant. Rien
  // d'autre n'est parti, et l'aperçu ne survit pas à la fenêtre.
  const bool unsent = view.has_something_to_send ||
                      (view.effects != nullptr && EffectsDiffer(state.effects, *view.effects));
  const bool closing = !open || std::exchange(state.close_asked, false);
  if (closing && !unsent) {
    state.open = false;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }
  if (closing && unsent) {
    ImGui::OpenPopup("fermer_sans_valider");
  }
  if (Confirm("fermer_sans_valider",
              core::Tr("Ton style n'est pas validé : fermer l'éditeur le perd. Personne ne l'a "
                       "vu, et ton personnage garde ce qu'il porte."),
              core::Tr("Fermer sans valider"), art)) {
    state.open = false;
  }

  // ── LE TRAIT EN COURS ─────────────────────────────────────────────────────
  //
  // 🔴 UN APPUI, UN TRAIT : ce qu'on choisit bouton tenu — un glissement dans
  // le nuancier — ne fait qu'un pas d'historique.
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ++state.stroke;
  }
  action.stroke = ImGui::IsMouseDown(ImGuiMouseButton_Left) ? state.stroke : 0;
  ReadHistoryKeys(action);

  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
  ImGui::TextUnformatted(core::Tr("Choisis une pièce, puis règle sa couleur."));
  ImGui::PopTextWrapPos();
  DrawLayerChoice(state, view, art, action);
  // L'ONGLET DES EFFETS n'a ni pièces, ni historique, ni codes : son corps est le
  // sien. Voir `ro/ui/palette_effects.h`. Le pantin y joue les effets, teints de
  // ce qu'on règle.
  //
  // 🔴 SANS PIPETTE NI LOUPE : il n'y a pas de pièce à désigner, et un clic à
  // travers l'aura ouvrirait la pièce du personnage qu'elle recouvre.
  //
  // LA MÊME DISPOSITION QUE LES PIÈCES : le pantin à gauche, aussi haut que le
  // panneau, et ses réglages dessous, sur toute la largeur.
  if (view.tab == PaletteTabKind::kEffects && view.effects != nullptr) {
    DrawPaletteDoll(state.doll, std::max(kDollMinPoints * Scale(), state.effects.panel_height),
                    art.look(), /*pipette=*/false);
    state.hovered_ramp = -1;
    ImGui::SameLine();
    ImGui::BeginGroup();
    action.effects = DrawPaletteEffects(state.effects, *view.effects, art);
    ImGui::EndGroup();
    state.effects.panel_height = ImGui::GetItemRectSize().y;
    DrawPaletteDollControls(state.doll, art, EffectsDiffer(state.effects, *view.effects));
    EndSkinnedWindow();
    return action;
  }
  const bool on_body = view.tab == PaletteTabKind::kBody;

  if (view.hue_classes) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
    ImGui::TextUnformatted(
        core::Tr("Cet item se règle par familles de teinte : chaque pastille teint toutes "
                 "ses couleurs de cette famille."));
    ImGui::PopTextWrapPos();
  }
  if (on_body) {
    DrawBodyLines(view);
  }
  ImGui::Separator();

  // 🔴 SANS PIÈCE, ON LE DIT — ET LE RESTE DEMEURE : la coupe, la couleur de
  // cheveux, la teinte de base, les préréglages et les codes valent encore,
  // et « Supprimer » doit rester atteignable.
  bool picker_open = false;
  if (view.ramps.empty()) {
    ImGui::TextDisabled("%s", EmptyTabText(view));
  } else {
    picker_open = DrawPiecesBlock(state, view, art, action);
  }
  picker_open = DrawPaletteRetouches(state, view, art, action) || picker_open;
  DrawLayerNotes(view, art, action);
  if (action.open_effects) {
    state.tab = PaletteTabKind::kEffects;
  }
  picker_open = DrawSkinLine(state, view, art, action) || picker_open;

  // ⚠ SANS PIÈCE, PAS DE PANTIN NI DE SES COMMANDES : l'historique se tient
  // alors seul.
  if (view.ramps.empty()) {
    DrawHistory(view, art, action);
  } else {
    DrawInspiration(art, action);
  }
  ImGui::Separator();
  // 🔴 TOUT CE QUI SUIT PASSE SOUS LE NUANCIER quand il est ouvert : on le
  // neutralise plutôt que de le déplacer.
  if (picker_open) {
    ImGui::BeginDisabled();
  }

  // 🔴 LA COUPE ET LA COULEUR VONT AVEC LES CHEVEUX, la teinte de base avec le
  // corps ; sans onglet des cheveux, le corps reprend les trois.
  const bool hair_tab = std::ranges::any_of(
      view.tabs, [](const PaletteTab& tab) { return tab.kind == PaletteTabKind::kHair; });
  const ChoiceButtons choices{.hair = view.tab == PaletteTabKind::kHair || (on_body && !hair_tab),
                              .body_palette = on_body};
  if (view.bounds.usable() && (choices.hair || choices.body_palette)) {
    DrawChoiceButtons(state, view, art, action, choices);
    ImGui::Separator();
  }
  DrawUndoButtons(view, art, action);
  ImGui::Separator();
  DrawSendButtons(view, art, action);
  // 🔴 LES PRÉRÉGLAGES SUR LE CORPS ET SUR LA TÊTE : un préréglage range la
  // tenue du personnage, le corps comme la tête. La tête garde en plus son
  // code de calque.
  const bool on_head = view.tab == PaletteTabKind::kHair;
  if (on_body || on_head) {
    DrawPalettePresets(state, view, art, action);
    DrawPaletteOutfits(state, view, art, action);
  }
  if (!on_body) {
    DrawLayerCode(view, art, action);
    if (view.code_note != nullptr) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
      ImGui::TextDisabled("%s", view.code_note);
      ImGui::PopTextWrapPos();
    }
  }

  if (picker_open) {
    ImGui::EndDisabled();
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
