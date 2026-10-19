#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/session.h"
#include "ro/proto/common_packets.h"

namespace moonclient::lobby {

// Combien de temps une attente de réponse du serveur peut durer avant d'être
// déclarée perdue : connexion, liste des personnages, entrée sur la carte.
//
// 🔴 UN SERVEUR EN COURS DE DÉMARRAGE ACCEPTE LA LIAISON ET NE RÉPOND PAS
// ENCORE. Rien ne casse alors la socket, et sans borne l'écran d'entrée en jeu
// reste noir pour toujours.
inline constexpr std::uint32_t kStageTimeoutMs = 20'000;

// Ce qu'il faut pour se présenter à un serveur.
//
// Le mot de passe s'arrête à `ro::net::LoginPhase` : rien de ce qui est ici ne
// le conserve au-delà de l'envoi, et rien ne le journalise.
struct Credentials {
  std::string host;
  std::uint16_t port = 0;
  std::string account;
  std::string password;
};

// DE QUOI REVENIR À LA SÉLECTION DE PERSONNAGE SANS REPASSER PAR LE LOGIN : le
// serveur de personnages joint à la connexion, et les jetons qu'il a remis.
//
// ⚠ PAS DE MOT DE PASSE : le serveur de personnages reconnaît les jetons, que
// le serveur de carte lui a rendus au moment du retour. Voir
// `ro/net/character_select_return.h`.
struct Comeback {
  ro::net::ServerAddress character_server;
  ro::net::LoginPhase::Account account;

  [[nodiscard]] bool usable() const noexcept { return account.account_id != 0; }
};

// LES TROIS ACTES DE LA SESSION, JOUÉS SANS BLOQUER LA BOUCLE.
//
// Une image doit se dessiner entre deux tours : un écran de connexion qui se
// fige cinq secondes sur un serveur éteint est le premier que le joueur verra.
//
// ⚠ ELLE NE LIT AUCUNE HORLOGE, comme la couche qu'elle pilote : `Advance`
// reçoit `now_ms` de l'appelant. C'est ce qui permet de l'éprouver sans attendre
// les délais qu'elle mesure.
//
// ⚠ `Begin` BLOQUE LE TEMPS D'OUVRIR LA LIAISON, et on ne peut pas l'éviter ici.
// Mesuré en production : 1 ms sur une adresse déjà résolue, 25 ms au premier
// appel, mais **5,2 s sur un port fermé** — c'est-à-dire exactement quand le
// joueur a besoin d'un message. À l'appelant de dessiner « connexion… » AVANT
// d'appeler, et c'est ce que `LobbyFlow` garantit.
// Les deux ouvertures suivantes, elles, ne coûtent rien de plus : le serveur
// donne ses adresses en chiffres, donc aucune résolution de nom n'a lieu.
class SessionDrive {
 public:
  // Où en est la session. L'appelant en a besoin pour savoir ce qu'il doit
  // montrer, et quand il peut choisir.
  enum class Stage : std::uint8_t {
    // Rien n'a été demandé.
    kAsleep,
    // La demande de connexion est partie.
    kConnecting,
    // Le serveur de personnages a été joint ; sa liste arrive.
    kAwaitingList,
    // Le serveur réclame le code PIN. ⚠ La liste est DÉJÀ là : il l'envoie
    // avant de demander. En production `pincode_enabled` vaut non, donc ce
    // passage n'est jamais emprunté — mais il existe sur le poste de test, et
    // une phase qui ne le traiterait pas y attendrait pour rien.
    kAwaitingPincode,
    // La liste est là : à l'appelant de choisir.
    kChoosing,
    // Le personnage est choisi ; la carte se demande.
    kEntering,
    // La liaison est prête et le monde arrive dessus. À l'appelant de la
    // prendre par `TakeLink`.
    kInWorld,
    // Le serveur a dit non. `message()` le dit en clair.
    kRefused,
    // Panne, ou lien tombé. `message()` le dit.
    kFailed,
  };

  // Ouvre la liaison et envoie la demande de connexion.
  //
  // Sans effet si une session est déjà en route : relancer par-dessus laisserait
  // une liaison ouverte que plus personne ne tient.
  void Begin(const Credentials& credentials);

  // REPREND AU SERVEUR DE PERSONNAGES, jetons en main : le retour depuis la
  // partie. Même règle que `Begin` — sans effet si une session est en route.
  void Resume(const Comeback& comeback);

  // Donne à voir les octets reçus du serveur de zone, dès la première trame.
  // Vide, rien n'est observé. Voir `ro::net::MapPhase::SetReceiveObserver`.
  void ObserveZone(ro::net::ReceiveObserver observer) { zone_observer_ = std::move(observer); }

  // De quoi y revenir plus tard. ⚠ À PRENDRE AVANT `Release`, qui l'efface.
  [[nodiscard]] Comeback comeback() const {
    return Comeback{.character_server = character_server_, .account = account_};
  }

  // Fait avancer l'acte en cours. À appeler à chaque tour de boucle.
  void Advance(std::uint32_t now_ms);

  // Demande l'entrée en jeu avec le personnage de cet emplacement.
  // Sans effet hors de `kChoosing`.
  void Choose(std::uint8_t slot);

  // Demande la création d'un personnage. Sans effet hors de `kChoosing`.
  //
  // ⚠ L'ÉTAPE NE CHANGE PAS : créer ne quitte pas la tablée, réussite comprise.
  // Le nouveau venu s'y assoit, et le joueur choisit ensuite s'il entre avec
  // lui.
  void Create(const ro::net::CharacterDraft& draft);

  // Où en est la dernière création, et pourquoi elle a échoué. Voir
  // `ro::net::CreationState`.
  [[nodiscard]] ro::net::CreationState creation() const noexcept;
  [[nodiscard]] std::string creation_refusal() const;
  [[nodiscard]] int created_slot() const noexcept;
  void ForgetCreation() noexcept;

  // LA SUPPRESSION, EN DEUX TEMPS. Sans effet hors de `kChoosing`. Voir
  // `ro::net::DeletionStep`.
  void ReserveDeletion(std::uint32_t character_id);
  void CancelDeletion(std::uint32_t character_id);
  void ConfirmDeletion(std::uint32_t character_id, std::string_view secret);

  [[nodiscard]] ro::net::DeletionStep deletion_step() const noexcept;
  [[nodiscard]] ro::net::DeletionState deletion() const noexcept;
  [[nodiscard]] std::uint32_t deletion_character_id() const noexcept;
  [[nodiscard]] std::string deletion_refusal() const;
  void ForgetDeletion() noexcept;

  // LES RETOUCHES : déplacer, renommer. Sans effet hors de `kChoosing`. Voir
  // `ro::net::AmendStep`.
  void MoveCharacter(std::uint8_t from_slot, std::uint8_t to_slot);
  void RenameCharacter(std::uint32_t character_id, std::string_view new_name);

  [[nodiscard]] ro::net::AmendStep amend_step() const noexcept;
  [[nodiscard]] ro::net::AmendState amend() const noexcept;
  [[nodiscard]] int amend_target_slot() const noexcept;
  [[nodiscard]] std::string amend_refusal() const;
  void ForgetAmend() noexcept;

  // Répond à la demande de code PIN. Sans effet hors de `kAwaitingPincode`.
  // Rend une erreur, sans rien émettre, si le code n'a pas quatre chiffres.
  [[nodiscard]] ro::core::Result<void> SubmitPincode(std::string_view digits);

  // Rend la liaison et remet tout à zéro. Le geste est VOLONTAIRE : rien n'en
  // sort par `kFailed`, et l'appelant n'a donc rien à montrer.
  //
  // Idempotent : appelable sans se demander si c'est déjà fait.
  void Release() noexcept;

  [[nodiscard]] Stage stage() const noexcept { return stage_; }
  // Ce qui s'est passé, en clair, quand l'étape est `kRefused` ou `kFailed`.
  [[nodiscard]] const std::string& message() const noexcept { return message_; }
  // Vrai quand le serveur a répondu à l'entrée en jeu qu'aucune carte n'est
  // encore servie : l'étape est revenue à `kChoosing`. S'efface au choix suivant.
  [[nodiscard]] bool zone_unavailable() const noexcept { return zone_unavailable_; }
  // Vrai quand le serveur vient de rejeter le dernier code PIN proposé.
  [[nodiscard]] bool pincode_refused() const noexcept;

  // Combien d'emplacements ce compte peut remplir. Voir
  // `ro::net::CharacterPhase::producible_slots`.
  [[nodiscard]] std::uint8_t producible_slots() const noexcept;

  // La liste des personnages, vide tant qu'elle n'est pas arrivée.
  //
  // ⚠ VIDE N'EST PAS « PAS ENCORE » : un compte qui vient d'être créé n'a aucun
  // personnage, et c'est `kChoosing` qui dit que la liste est là. Voir le témoin
  // `list_received_` de `ro::net::CharacterPhase`.
  [[nodiscard]] std::vector<ro::proto::CharacterInfo> characters() const;

  // L'EMPLACEMENT choisi, une fois `Choose` appelé.
  //
  // 🔴 UN EMPLACEMENT, PAS UN RANG DE LISTE, et ce n'est pas une nuance : la
  // liste est DENSE, les emplacements ne le sont pas. Voir
  // `ro::net::IndexOfSlot`. Pour retrouver le personnage, employer
  // `chosen_character` — qui existe précisément pour qu'on n'ait pas à y
  // penser.
  [[nodiscard]] std::uint8_t chosen_slot() const noexcept { return chosen_slot_; }

  // LE PERSONNAGE CHOISI, ou nul tant qu'il n'y en a pas.
  //
  // Son nom, son apparence, ses niveaux et sa bourse ne viennent que de la
  // liste : le serveur de zone n'annonce jamais le joueur à lui-même.
  //
  // ⚠ IL POINTE DANS LA LISTE DU PILOTE, donc jusqu'à `Release` — c'est-à-dire
  // au-delà de la passation, qui est le seul moment où quelqu'un le demande.
  [[nodiscard]] const ro::proto::CharacterInfo* chosen_character() const;

  // Ce que la poignée de main a obtenu, une fois `kInWorld` atteint.
  [[nodiscard]] const ro::net::LoginPhase::Account& account() const noexcept { return account_; }
  [[nodiscard]] const std::string& map_name() const noexcept { return map_name_; }
  [[nodiscard]] const ro::net::MapPhase::Spawn& spawn() const noexcept { return spawn_; }

  // La liaison de zone, une fois `kInWorld` atteint.
  //
  // ⚠ Le pilote s'en dessaisit : deux objets qui croiraient lire le même flux
  // en perdraient chacun la moitié.
  [[nodiscard]] ro::net::Connection TakeLink() noexcept;

 private:
  // Le verdict d'une phase a-t-il CASSÉ la séquence ? Pose alors l'étape et le
  // message. Voir sa définition pour la raison de cette formulation.
  template <typename Phase>
  bool Broke(ro::net::PhaseState state, const Phase& phase, std::string_view what);

  // Une étape d'attente a-t-elle dépassé `kStageTimeoutMs` ? Pose alors l'étape
  // `kFailed` et son message. Voir sa définition.
  bool TimedOut(std::uint32_t now_ms);

  Stage stage_ = Stage::kAsleep;
  // L'étape d'attente que l'horloge ci-dessous mesure, et l'instant où l'appelant
  // l'a vue pour la première fois : `Begin`, `Resume` et `Choose` ne reçoivent
  // pas d'horloge, c'est donc `Advance` qui date l'arrivée.
  bool zone_unavailable_ = false;
  Stage timed_stage_ = Stage::kAsleep;
  std::uint32_t timed_since_ms_ = 0;
  std::string message_;
  // L'horloge du client au moment de la demande d'entrée en carte. Le serveur
  // s'en sert pour caler la sienne ; nous ne la lisons pas.
  std::uint32_t entered_tick_ = 0;
  std::uint8_t chosen_slot_ = 0;

  // LE PERSONNAGE CHOISI, EN COPIE, ET C'EST LA COPIE QUI COMPTE.
  //
  // 🔴 LA LISTE MEURT AVANT QU'ON S'EN SERVE. Le passage au serveur de zone
  // détruit la phase des personnages — `characters_.reset()`, dans `Advance` —,
  // or c'est EXACTEMENT à cet instant que l'appelant demande qui l'on a choisi,
  // pour composer le monde de départ. Une réponse qui pointait dans la liste
  // rendait donc toujours nul, et l'écran se refermait sans un mot.
  //
  // ⚠ PRISE AU MOMENT DU CHOIX, pas au passage : c'est le seul instant où l'on
  // tient à la fois la liste et l'emplacement voulu.
  std::optional<ro::proto::CharacterInfo> chosen_;

  std::optional<ro::net::LoginPhase> login_;
  std::optional<ro::net::CharacterPhase> characters_;
  std::optional<ro::net::MapPhase> world_;
  ro::net::ReceiveObserver zone_observer_;

  ro::net::LoginPhase::Account account_;
  // Le serveur de personnages joint à la connexion : celui où l'on revient.
  ro::net::ServerAddress character_server_;
  std::string map_name_;
  ro::net::MapPhase::Spawn spawn_;
  ro::net::Connection link_;
};

}  // namespace moonclient::lobby
