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

#include "lobby/backdrop_session.h"
#include "lobby/session_drive.h"

namespace moonclient::lobby {

// CE QUE LA COQUILLE DOIT MONTRER. Une intention, pas un dessin : rien ici ne
// connaît ni fenêtre, ni carte, ni pixel.
enum class LobbyScreenKind : std::uint8_t {
  // Le formulaire. `message()` porte le motif du dernier refus, s'il y en a eu.
  kForm,
  // L'attente, formulaire tenu mais inerte.
  kConnecting,
  // ⚠ LE SERVEUR RÉCLAME LE CODE PIN. En production `pincode_enabled` vaut non,
  // donc cet écran n'est jamais demandé — mais le poste de test le réclame, et
  // une machine qui ne le prévoirait pas y attendrait pour rien.
  kPincode,
  // La scène de sélection : `characters()` porte la liste, vide comprise.
  kCharacters,
  // Le personnage est choisi ; la carte se monte.
  kEntering,
  // Fini. À la coquille de prendre la liaison et de passer la main à la partie.
  kHandoff,
};

// L'ARBITRE DU DÉMARRAGE : quel écran, quelle session, dans quel ordre.
//
// Il possède DEUX sessions qui ne se ressemblent pas — celle du décor, qui est
// une décoration dont l'échec est muet, et celle du joueur, dont chaque refus
// doit se dire. Les tenir ensemble est tout l'objet de cette classe : c'est ce
// qui permet de garantir l'ordre entre elles.
//
// 🔴 ET IL NE DESSINE RIEN. Cette cible ne se lie qu'à `ro_core` et `ro_net` ;
// l'éditeur de liens refuse tout ce qui toucherait la fenêtre. C'est ce qui la
// rend essayable sur un socket de bouclage, sans GPU — ce que n'est rien de ce
// qui est compilé dans l'exécutable.
class LobbyFlow {
 public:
  // ⚠ LE DÉCOR REÇOIT LE SERVEUR ICI, une fois pour toutes : il en a besoin à
  // chaque ouverture, et il en existe plus d'une. Voir `BackdropSession`, et ce
  // que le lui repasser à chaque fois a coûté.
  LobbyFlow(std::string host, std::uint16_t port)
      : host_(std::move(host)), port_(port), backdrop_(host_, port_) {}

  // Demande le décor vivant. Sans effet s'il a déjà été demandé.
  //
  // ⚠ UNE SEULE FOIS PAR LANCEMENT, et `BackdropSession` le garantit de
  // lui-même : une fois la session rendue, elle ne se rouvre pas. Un décor qui
  // se relancerait après un refus de connexion reprendrait la place que le
  // serveur n'accorde qu'une fois par adresse, à chaque essai manqué — et
  // changerait de ville sous le message d'erreur que le joueur est en train de
  // lire.
  void OpenBackdrop(std::uint32_t now_ms);

  // Demande une AUTRE ville. Voir `BackdropSession::Reroll` pour pourquoi
  // c'est la seule relance permise.
  void RelaunchBackdrop(std::uint32_t now_ms) { backdrop_.Relaunch(now_ms); }

  // LE JOUEUR A CHOISI UN AUTRE SERVEUR : la prochaine connexion y part, et le
  // décor, rendu, s'y rouvrira à la demande. Le motif du dernier refus
  // s'efface — il venait de l'autre serveur. Voir `BackdropSession::Retarget`.
  //
  // ⚠ SEULEMENT AU FORMULAIRE, sans demande en attente : une connexion lancée
  // ne change pas d'adresse en route. Rend faux quand il refuse.
  [[nodiscard]] bool Retarget(std::string host, std::uint16_t port);

  // Fait tourner les deux sessions. À appeler une fois par tour de boucle.
  void Advance(std::uint32_t now_ms);

  // Le joueur a validé le formulaire.
  //
  // Le mot de passe ne va nulle part ailleurs que dans la demande de connexion.
  void Submit(std::string account, std::string password);

  // LE RETOUR DEPUIS LA PARTIE : droit à la sélection de personnage, sans
  // formulaire ni mot de passe. Sans effet hors de `kForm`. Un échec ramène au
  // formulaire, motif en clair, comme tout échec après lui.
  void Resume(const Comeback& comeback);

  // LE RETOUR FORCÉ DEPUIS LA PARTIE : la liaison perdue, et pourquoi. Le
  // motif s'affiche sous le formulaire comme celui d'un refus, et s'efface de
  // la même façon à la tentative suivante. Sans effet hors de `kForm`.
  void Notify(std::string why);

  // De quoi revenir à la sélection plus tard. Voir `Comeback`.
  [[nodiscard]] Comeback comeback() const { return player_.comeback(); }

  // Donne à voir les octets que le serveur de zone envoie au JOUEUR — jamais
  // ceux du décor. Voir `SessionDrive::ObserveZone`.
  void ObserveZone(ro::net::ReceiveObserver observer) { player_.ObserveZone(std::move(observer)); }

  // Le joueur a choisi un emplacement. Sans effet hors de `kCharacters`.
  void ChooseCharacter(std::uint8_t slot);

  // Le joueur crée un personnage. Sans effet hors de `kCharacters`.
  //
  // ⚠ L'ÉCRAN NE CHANGE PAS : créer ne quitte pas la tablée. Le refus, lui, ne
  // revient PAS au formulaire — un nom déjà pris n'est pas une session perdue.
  void CreateCharacter(const ro::net::CharacterDraft& draft);

  [[nodiscard]] ro::net::CreationState creation() const noexcept { return player_.creation(); }
  [[nodiscard]] std::string creation_refusal() const { return player_.creation_refusal(); }
  [[nodiscard]] int created_slot() const noexcept { return player_.created_slot(); }
  void ForgetCreation() noexcept { player_.ForgetCreation(); }

  // LA SUPPRESSION, EN DEUX TEMPS. Sans effet hors de `kCharacters`, et — comme
  // la création — un refus n'y ramène PAS au formulaire : un personnage en
  // guilde reste à sa place, et le joueur va quitter sa guilde.
  void ReserveDeletion(std::uint32_t character_id);
  void CancelDeletion(std::uint32_t character_id);
  void ConfirmDeletion(std::uint32_t character_id, std::string_view secret);

  [[nodiscard]] ro::net::DeletionStep deletion_step() const noexcept {
    return player_.deletion_step();
  }
  [[nodiscard]] ro::net::DeletionState deletion() const noexcept { return player_.deletion(); }
  [[nodiscard]] std::uint32_t deletion_character_id() const noexcept {
    return player_.deletion_character_id();
  }
  [[nodiscard]] std::string deletion_refusal() const { return player_.deletion_refusal(); }
  void ForgetDeletion() noexcept { player_.ForgetDeletion(); }

  // LES RETOUCHES : déplacer, renommer. Sans effet hors de `kCharacters` ; un
  // refus reste sur la tablée, comme celui d'une suppression.
  void MoveCharacter(std::uint8_t from_slot, std::uint8_t to_slot);
  void RenameCharacter(std::uint32_t character_id, std::string_view new_name);

  [[nodiscard]] ro::net::AmendStep amend_step() const noexcept { return player_.amend_step(); }
  [[nodiscard]] ro::net::AmendState amend() const noexcept { return player_.amend(); }
  [[nodiscard]] int amend_target_slot() const noexcept { return player_.amend_target_slot(); }
  [[nodiscard]] std::string amend_refusal() const { return player_.amend_refusal(); }
  void ForgetAmend() noexcept { player_.ForgetAmend(); }

  // Répond au code PIN. Sans effet hors de `kPincode`.
  [[nodiscard]] ro::core::Result<void> SubmitPincode(std::string_view digits);

  // Revient au formulaire, en rendant la session en cours.
  //
  // ⚠ LA LISTE NE SURVIT PAS : elle venait d'une liaison qu'on referme. Y
  // revenir demandera une nouvelle connexion, et c'est la vérité du protocole
  // plutôt qu'une simplification.
  void Back();

  // REND LES DEUX SESSIONS, celle du joueur et celle du décor.
  //
  // 🔴 `Back` NE SUFFIT PAS pour fermer : il ramène au formulaire, donc il
  // GARDE le décor — c'est ce qu'on veut d'un retour. En partant, en revanche,
  // une session de décor que plus personne ne regarde tiendrait la seule place
  // que le serveur accorde à cette adresse.
  void Release() noexcept;

  [[nodiscard]] LobbyScreenKind screen() const noexcept { return screen_; }
  // Le motif du dernier refus, en clair, ou vide. Effacé dès qu'une nouvelle
  // tentative part : un message qui survit à son écran devient un mensonge.
  [[nodiscard]] const std::string& message() const noexcept { return message_; }
  [[nodiscard]] const BackdropSession& backdrop() const noexcept { return backdrop_; }
  // ⚠ MODIFIABLE POUR QUI LE DESSINE : composer une image mute le monde du
  // décor. Voir `BackdropSession::world`.
  [[nodiscard]] BackdropSession& backdrop() noexcept { return backdrop_; }
  [[nodiscard]] bool pincode_refused() const noexcept { return player_.pincode_refused(); }
  // Le serveur démarre : il n'a pas encore de carte à servir. L'écran est revenu
  // à la tablée, et le joueur peut redemander.
  [[nodiscard]] bool zone_unavailable() const noexcept { return player_.zone_unavailable(); }

  // La liste des personnages. ⚠ VIDE EST UNE RÉPONSE : c'est celle d'un compte
  // qui vient d'être créé, et l'écran doit alors proposer d'en créer un.
  [[nodiscard]] std::vector<ro::proto::CharacterInfo> characters() const {
    return player_.characters();
  }
  [[nodiscard]] std::uint8_t chosen_slot() const noexcept { return player_.chosen_slot(); }
  // ⚠ PAS `characters()[chosen_slot()]` : la liste est dense, les emplacements
  // ne le sont pas. Voir `ro::net::IndexOfSlot`.
  [[nodiscard]] const ro::proto::CharacterInfo* chosen_character() const {
    return player_.chosen_character();
  }
  // COMBIEN D'EMPLACEMENTS CE COMPTE PEUT REMPLIR, zéro tant qu'on ne le sait
  // pas. C'est ce qui décide combien de places libres la scène propose.
  [[nodiscard]] std::uint8_t producible_slots() const noexcept {
    return player_.producible_slots();
  }
  [[nodiscard]] const ro::net::LoginPhase::Account& account() const noexcept {
    return player_.account();
  }
  [[nodiscard]] const std::string& map_name() const noexcept { return player_.map_name(); }
  [[nodiscard]] const ro::net::MapPhase::Spawn& spawn() const noexcept { return player_.spawn(); }

  // La liaison de zone, une fois `kHandoff` atteint.
  [[nodiscard]] ro::net::Connection TakeLink() noexcept { return player_.TakeLink(); }

 private:
  // Ramène au formulaire en portant le motif. Tout échec après le formulaire y
  // revient : la liste venait d'une liaison qui n'existe plus.
  void FallBackToForm(std::string why);

  std::string host_;
  std::uint16_t port_ = 0;

  LobbyScreenKind screen_ = LobbyScreenKind::kForm;
  std::string message_;

  // La demande du joueur, mise de côté LE TEMPS D'UNE IMAGE. Voir `Advance`.
  std::optional<Credentials> pending_;
  // Ou le retour depuis la partie, mis de côté de la même façon.
  std::optional<Comeback> pending_comeback_;
  bool pending_held_a_turn_ = false;

  BackdropSession backdrop_;
  SessionDrive player_;
};

}  // namespace moonclient::lobby
