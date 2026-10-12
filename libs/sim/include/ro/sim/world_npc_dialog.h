#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/sim/npc_dialog.h"

namespace ro::sim {

// LE DIALOGUE DE PNJ EN COURS, et ce que le serveur et le joueur en font.
//
// 🔴 C'EST UNE BASE DE `World`, ET C'EST LA FRONTIÈRE LA PLUS NETTE DU MONDE :
// ces méthodes ne regardent aucun acteur, ne déplacent rien et ne lisent aucune
// case. Le modèle qu'elles manipulent est dans `ro/sim/npc_dialog.h`.
class WorldNpcDialog {
 public:
  // LE DIALOGUE EN COURS, s'il y en a un. Il n'appartient à aucun acteur : le
  // PNJ peut disparaître de la vue sans que sa fenêtre se ferme, et le serveur
  // en ouvre même au nom d'un PNJ FEINT qui n'est nulle part.
  [[nodiscard]] const std::optional<NpcDialog>& npc_dialog() const noexcept { return npc_dialog_; }

  // Ce que le PNJ dit de plus. Ouvre le dialogue s'il n'y en avait pas.
  //
  // ⚠ LA LIGNE NE S'AFFICHE PAS TOUT DE SUITE : elle rejoint la page en
  // RÉCEPTION, publiée d'un seul tenant par le paquet qui la termine. Voir
  // `NpcDialog::pending_lines`.
  //
  // ⚠ UN AUTRE PNJ REMPLACE LE PRÉCÉDENT. Deux dialogues ne coexistent pas : le
  // serveur n'en tient qu'un par joueur, et en garder deux montrerait une
  // fenêtre que plus rien ne ferme.
  void SayNpcDialog(std::uint32_t npc_id, std::string line, std::uint32_t now_ms);

  // Ce que le dialogue attend désormais, et la PUBLICATION de la page reçue :
  // le paquet qui pose un bouton est celui qui la termine. Rend faux s'il n'y a
  // rien à quoi l'attacher — le serveur envoie « fermer » à la fin d'un script
  // même quand rien n'est ouvert, et ce n'est pas une anomalie.
  bool PromptNpcDialog(std::uint32_t npc_id, NpcPrompt prompt, std::uint32_t now_ms);

  // Un menu de choix. Il OUVRE un dialogue au besoin — un script peut appeler
  // `select` sans avoir rien dit — et PUBLIE la page, comme tout terminateur.
  void OfferNpcMenu(std::uint32_t npc_id, std::vector<NpcMenuOption> options, std::uint32_t now_ms);

  // Le `clear` d'un script : la prochaine ligne remplacera la page.
  //
  // 🔴 IL NE PUBLIE RIEN, ET SURTOUT PAS. `clear` est le PREMIER paquet de la
  // réponse du serveur : y publier ferait tomber le menu de la page précédente à
  // cet instant, le texte s'étalerait dans la place libérée pendant toute la
  // rafale, puis se ferait raboter à l'arrivée du nouveau menu. Deux
  // réarrangements au lieu de zéro.
  //
  // ⚠ ET IL N'EFFACE PAS NON PLUS TOUT DE SUITE : vider ici laisse la fenêtre
  // sans texte ni bouton le temps que le `mes` suivant arrive — un clignotement
  // à chaque écran un peu bavard. Écart assumé avec le client officiel, qui vide
  // aussitôt : un `clear` non suivi d'un `mes` est introuvable dans les scripts,
  // et l'ancienne page est de toute façon plus lisible qu'un cadre vide.
  //
  // Rend faux si le paquet ne désigne pas le dialogue en cours.
  bool ClearNpcDialogText(std::uint32_t npc_id, std::uint32_t now_ms);

  // LE FILET DE LA MISE EN ATTENTE : une page dont le terminateur n'arrive
  // jamais finit par s'afficher — un script qui écrit puis dort, ou qui s'arrête
  // sur `end`. Rend vrai s'il a publié quelque chose. Voir `kNpcPageIdleMs`.
  bool CommitNpcPageIfIdle(std::uint32_t now_ms);

  // Le joueur a cliqué « suivant » : la prochaine page remplacera celle-ci, et
  // en attendant la réponse la page reste affichée, INERTE.
  void AdvanceNpcDialog();

  // Le joueur a répondu au menu. La liste RESTE affichée, inerte, jusqu'à ce que
  // la réponse arrive : la vider tout de suite escamote la question et fait
  // sauter le pied de fenêtre. Une seconde réponse à la même liste est dès lors
  // impossible — voir `NpcDialog::menu_answered`.
  void AnswerNpcMenu();

  // Le joueur a soumis une saisie. Le champ reste en place, inerte.
  //
  // ⚠ IL N'ARME PAS L'EFFACEMENT, contrairement au « suivant » : le serveur ne
  // documente cet effacement qu'au-dessus de ZC_WAIT_DIALOG, et l'armer ici
  // ferait disparaître la question à laquelle on vient de répondre.
  void SubmitNpcInput();

  // Le dialogue se ferme, quelle qu'en soit la raison.
  void CloseNpcDialog();

 private:
  std::optional<NpcDialog> npc_dialog_;
};

}  // namespace ro::sim
