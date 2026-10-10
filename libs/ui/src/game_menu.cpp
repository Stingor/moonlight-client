#include "ro/ui/game_menu.h"

#include <array>

#include "ro/ui/chat_macros.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/staff_tools.h"
#include "ro/ui/window_name.h"

namespace ro::ui {
namespace {

// ⚠ TOUS LES BOUTONS À LA MÊME LARGEUR, celle du plus long — et mesurée DANS LA
// LANGUE COURANTE. Un menu dont les boutons ont chacun leur largeur se lit comme
// une liste de choses sans rapport ; et une largeur figée sur le français
// laisserait sortir un libellé anglais.
//
// ⚠ ET TOUS LES BOUTONS COMPTENT DANS LA MESURE, présents ou non : sans cela, le
// menu changerait de largeur en changeant de compte — ou en mourant —, ce qui
// ferait passer un droit ou un état pour un défaut d'affichage.
//
// ⚠ LES LIBELLÉS SONT ÉCRITS EN TOUTES LETTRES, et non repris des constantes de
// titre : le contrôle des traductions ne sait lire que des littéraux, et une
// chaîne construite n'entre dans aucun catalogue. C'est le même texte que le
// titre des fenêtres, à dessein — un bouton doit nommer ce qu'il ouvre.
[[nodiscard]] float MenuWidth(const SkinArt& art) {
  const std::array<const char*, 9> labels = {core::Tr("Retour au jeu"),
                                             core::Tr("Retour à mon point de sauvegarde"),
                                             core::Tr("Sélection du personnage"),
                                             core::Tr("Options"),
                                             core::Tr("Configuration des raccourcis"),
                                             core::Tr("Macros de chat"),
                                             core::Tr("Staff Tools"),
                                             core::Tr("Résurrection"),
                                             core::Tr("Quitter le jeu")};
  return SkinWidestButton(labels, art);
}

// LA LISTE DU PERSONNAGE VIVANT.
[[nodiscard]] GameMenuChoice DrawAlive(GameMenuState& state, const SkinArt& art,
                                       const GameMenuFacts& facts, ImVec2 size) {
  GameMenuChoice chosen = GameMenuChoice::kNothing;
  if (SkinButton(core::Tr("Retour au jeu"), art, size)) {
    chosen = GameMenuChoice::kBackToGame;
  }
  // 🔴 LE RETOUR AU POINT DE SAUVEGARDE D'UN VIVANT EST UNE COMMANDE, pas un
  // paquet : rien dans le protocole ne téléporte un personnage debout. Il
  // disparaît donc avec la liaison, quand il n'y a plus personne pour
  // l'exécuter.
  if (facts.online) {
    if (SkinButton(core::Tr("Retour à mon point de sauvegarde"), art, size)) {
      chosen = GameMenuChoice::kSavePointCommand;
    }
    // ⚠ LA COMMANDE EXACTE EN INFOBULLE, et elle ne se traduit pas : le joueur
    // voit ce qui part, et peut la retaper lui-même.
    if (ImGui::IsItemHovered()) {
      SkinTooltip("@load");
    }
    // LE RETOUR À LA TABLÉE, sans quitter le client ni retaper le mot de
    // passe. ⚠ EN LIGNE SEULEMENT, comme le précédent : c'est le serveur qui
    // l'accorde — et qui le refuse à un personnage qui vient de combattre.
    if (SkinButton(core::Tr("Sélection du personnage"), art, size)) {
      chosen = GameMenuChoice::kCharacterSelect;
    }
  }
  if (SkinButton(core::Tr("Options"), art, size)) {
    chosen = GameMenuChoice::kOptions;
  }
  // ⚠ APRÈS LES OPTIONS DU JEU, et c'est l'ordre qui le dit : ce panneau-ci
  // n'existe que dans ce client. Le mettre en premier ferait passer nos ajouts
  // pour le réglage ordinaire du jeu.
  if (SkinButton(core::Tr("Configuration des raccourcis"), art, size)) {
    chosen = GameMenuChoice::kShortcuts;
  }
  // LES DIX MACROS DE CHAT, juste après les touches : ce sont les mêmes
  // touches, vues de l'autre bout.
  if (SkinButton(core::Tr("Macros de chat"), art, size)) {
    chosen = GameMenuChoice::kChatMacros;
  }
  // LES COULEURS, après les macros : c'est de l'apparence, pas du clavier.
  if (SkinButton(core::Tr("Couleurs du personnage"), art, size)) {
    chosen = GameMenuChoice::kStyleEditor;
  }
  // L'ÉTABLI DU STAFF, et il n'apparaît que pour qui y a droit. ⚠ ABSENT PLUTÔT
  // QUE GRISÉ, à rebours du reste de ce client : un bouton grisé annonce à tout
  // le monde une fenêtre qu'il ne verra jamais, et invite à chercher comment
  // l'atteindre. Ce qui manque ici n'est pas un réglage, c'est un droit.
  if (facts.staff && SkinButton(core::Tr("Staff Tools"), art, size)) {
    chosen = GameMenuChoice::kStaffTools;
  }
  if (SkinButton(core::Tr("Quitter le jeu"), art, size)) {
    chosen = GameMenuChoice::kQuit;
  }
  static_cast<void>(state);
  return chosen;
}

// LA LISTE DU PERSONNAGE MORT : deux gestes, trois avec un jeton.
[[nodiscard]] GameMenuChoice DrawDead(const SkinArt& art, const GameMenuFacts& facts, ImVec2 size) {
  GameMenuChoice chosen = GameMenuChoice::kNothing;
  // ⚠ EN TÊTE, et c'est l'ordre du client : se relever sur place est ce qu'on
  // vient chercher quand on en a le moyen.
  if (facts.can_resurrect && SkinButton(core::Tr("Résurrection"), art, size)) {
    chosen = GameMenuChoice::kResurrect;
  }
  // ⚠ CELUI-CI DEMANDE CONFIRMATION, à rebours de celui du vivant : il fait
  // perdre l'endroit où l'on est mort, et c'est souvent ce qu'on voulait
  // garder le temps qu'un soigneur arrive.
  if (SkinButton(core::Tr("Retour au point de sauvegarde"), art, size)) {
    AskModal(kGameMenuRespawnId);
  }
  if (SkinButton(core::Tr("Retour au jeu"), art, size)) {
    chosen = GameMenuChoice::kBackToGame;
  }
  return chosen;
}

}  // namespace

void FollowDeath(GameMenuState& state, bool dead) {
  // ⚠ ON NE RÉCLAME QUE CE QU'ON OUVRE. Mourir menu déjà ouvert ne nous en rend
  // pas propriétaire : il était au joueur avant, il le reste après.
  if (dead && !state.dead_last_frame && !state.open) {
    state.open = true;
    state.opened_by_death = true;
  }
  // Refermé — par la croix, par Échap, par un bouton qui mène ailleurs : il
  // n'est plus à nous, et un retour à la vie n'aura plus rien à reprendre.
  if (!state.open) {
    state.opened_by_death = false;
  }
  if (!dead && state.dead_last_frame && state.opened_by_death) {
    state.open = false;
    state.opened_by_death = false;
  }
  state.dead_last_frame = dead;
}

GameMenuChoice DrawGameMenu(GameMenuState& state, const SkinArt& art, const GameMenuFacts& facts) {
  // 🔴 AVANT LE DÉPART ANTICIPÉ : c'est précisément quand le menu est FERMÉ que
  // la mort a quelque chose à faire. Le placer après ne l'ouvrirait jamais.
  FollowDeath(state, facts.dead);
  if (!state.open) {
    return GameMenuChoice::kNothing;
  }
  static WindowName name(kGameMenuTitle, kGameMenuId);

  const float width = MenuWidth(art);
  ImGui::SetNextWindowSize(ImVec2(width + ImGui::GetStyle().WindowPadding.x * 2.0F, 0.0F),
                           ImGuiCond_Always);
  // ⚠ AU CENTRE À LA PREMIÈRE OUVERTURE, et pas au coin : c'est un menu qu'on
  // ouvre pour en sortir, il doit tomber sous les yeux.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));

  GameMenuChoice chosen = GameMenuChoice::kNothing;
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_NoResize)) {
    EndSkinnedWindow();
    return chosen;
  }

  const ImVec2 size(width, 0.0F);
  chosen = facts.dead ? DrawDead(art, facts, size) : DrawAlive(state, art, facts, size);

  // La question du retour au point de sauvegarde. ⚠ DESSINÉE DANS LES DEUX
  // DISPOSITIONS : le joueur peut être ressuscité pendant qu'elle est ouverte,
  // et une boîte qui disparaîtrait alors laisserait sa réponse sans destinataire.
  //
  // ⚠ LE TEXTE EST CELUI DU CLIENT, pas une paraphrase : « Do you really want
  // to go back to your savepoint? ». C'est sa traduction qu'on montre, et non un raccourci de
  // notre cru — un joueur qui connaît la question doit la reconnaître.
  switch (DrawModalBox(kGameMenuRespawnId,
                       core::Tr("Voulez-vous vraiment revenir à votre point de sauvegarde ?"),
                       core::Tr("OK"), core::Tr("Annuler"), art)) {
    case ModalAnswer::kYes:
      chosen = GameMenuChoice::kRespawn;
      break;
    case ModalAnswer::kNo:
    case ModalAnswer::kWaiting:
      break;
  }

  EndSkinnedWindow();
  // La croix de la barre de titre vaut « retour au jeu » : fermer le menu, c'est
  // reprendre la partie.
  if (!state.open && chosen == GameMenuChoice::kNothing) {
    chosen = GameMenuChoice::kBackToGame;
  }
  return chosen;
}

}  // namespace ro::ui
