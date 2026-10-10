#pragma once

#include <cstdint>

namespace moonclient {

// L'ÉCRITURE D'UN COURRIER VUE DE SA FENÊTRE : quand elle s'ouvre, quand elle
// garde la saisie, quand elle se ferme, et ce que chaque événement oblige à dire
// au serveur.
//
// 🔴 APRÈS UN ENVOI RÉUSSI, LE SERVEUR LAISSE L'ÉCRITURE OUVERTE — le joueur ne
// peut plus marcher ni utiliser d'item — tant que le client n'émet pas
// l'annulation. La réussite ferme donc la fenêtre ET émet l'annulation.
//
// 🔴 APRÈS UN ÉCHEC, L'ÉTAT DES PIÈCES JOINTES EST AMBIGU : selon le refus, le
// serveur les a rendues au sac sans le dire, ou les a laissées jointes. La
// session se remet donc à zéro — annulation, puis réouverture adressée au même
// destinataire —, et l'accord de cette réouverture GARDE la saisie : seules les
// pièces jointes repartent de zéro.
//
// 🔴 APRÈS UN SILENCE, LE COURRIER A PU PARTIR. La réouverture garde le texte,
// mais remet le zeny à zéro et le dit : renvoyer tel quel pourrait envoyer
// deux fois le même argent.
//
// Pure : aucune trame, aucune horloge propre. La boucle exécute les ordres.

// Le silence au bout duquel une ouverture demandée, ou la réouverture qui suit
// un échec, est tenue pour restée sans réponse.
inline constexpr std::uint32_t kMailWriteAskSilenceMs = 5000;

// CE QUE LA BOUCLE DOIT FAIRE après un événement.
struct MailWriteOrders {
  // Émettre l'annulation de l'écriture.
  bool emit_cancel = false;
  // Puis redemander une écriture, adressée au destinataire saisi.
  bool emit_reopen = false;
  // Ouvrir la fenêtre sur une saisie vierge.
  bool open_fresh = false;
  // La réouverture est accordée : la saisie reste, les pièces jointes sont à
  // refaire si l'envoi en portait.
  bool keep_typing = false;
  bool redo_attachments = false;
  // La réouverture suit un envoi resté sans réponse : le zeny saisi revient à
  // zéro, et le joueur vérifie son inventory et sa bourse avant de renvoyer.
  bool outcome_unknown = false;
  // Fermer la fenêtre.
  bool close_window = false;
  // La réouverture a été refusée, ou n'a pas eu de réponse : le courrier n'est
  // pas parti, et la saisie se perd avec la fenêtre.
  bool reopen_failed = false;
};

class MailWriteFlow {
 public:
  // La fenêtre d'écriture se montre.
  [[nodiscard]] bool shown() const noexcept { return shown_; }
  // L'échec d'un envoi a remis la session à zéro ; l'accord n'est pas venu.
  [[nodiscard]] bool reopening() const noexcept { return reopening_; }
  // Une ouverture est demandée, et ni l'accord ni le refus ne sont venus.
  [[nodiscard]] bool asking() const noexcept { return asking_; }

  // « Écrire », « Répondre », « Envoyer un courrier… » : la demande part.
  void NoteAsked(std::uint32_t now_ms) noexcept;

  // L'accord du serveur.
  [[nodiscard]] MailWriteOrders TellGranted() noexcept;
  // Le refus du serveur : coffre ouvert, échange en cours, écriture déjà
  // ouverte.
  [[nodiscard]] MailWriteOrders TellRefused() noexcept;

  // Le résultat d'un envoi, ou le silence. `carried_attachments` : l'envoi
  // portait des pièces jointes.
  [[nodiscard]] MailWriteOrders TellSendResult(std::uint8_t code, bool carried_attachments,
                                               std::uint32_t now_ms) noexcept;

  // La croix, Échap ou « Annuler ».
  [[nodiscard]] MailWriteOrders NoteClosedByPlayer() noexcept;

  // Le serveur a fermé l'écriture de lui-même — un changement de carte. Rien
  // n'est à émettre.
  [[nodiscard]] MailWriteOrders NoteServerDropped() noexcept;

  // Le temps passe : une demande sans réponse se relâche.
  [[nodiscard]] MailWriteOrders Tick(std::uint32_t now_ms) noexcept;

 private:
  bool shown_ = false;
  bool reopening_ = false;
  bool asking_ = false;
  // Un accord qui arriverait encore n'a plus de fenêtre à ouvrir : il
  // s'annule aussitôt.
  bool discard_next_grant_ = false;
  bool redo_attachments_ = false;
  bool outcome_unknown_ = false;
  std::uint32_t asked_ms_ = 0;
};

}  // namespace moonclient
