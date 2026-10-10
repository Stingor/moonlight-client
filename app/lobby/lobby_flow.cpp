#include "lobby/lobby_flow.h"

#include <utility>

#include "ro/core/log.h"

namespace moonclient::lobby {

void LobbyFlow::OpenBackdrop(std::uint32_t now_ms) { backdrop_.Open(now_ms); }

void LobbyFlow::Submit(std::string account, std::string password) {
  if (screen_ != LobbyScreenKind::kForm) {
    return;
  }
  message_.clear();
  pending_.emplace(Credentials{.host = host_,
                               .port = port_,
                               .account = std::move(account),
                               .password = std::move(password)});
  pending_held_a_turn_ = false;
  screen_ = LobbyScreenKind::kConnecting;
}

void LobbyFlow::Resume(const Comeback& comeback) {
  if (screen_ != LobbyScreenKind::kForm || !comeback.usable()) {
    return;
  }
  message_.clear();
  pending_comeback_ = comeback;
  pending_held_a_turn_ = false;
  screen_ = LobbyScreenKind::kConnecting;
}

void LobbyFlow::FallBackToForm(std::string why) {
  player_.Release();
  message_ = std::move(why);
  screen_ = LobbyScreenKind::kForm;
}

void LobbyFlow::Advance(std::uint32_t now_ms) {
  backdrop_.Advance(now_ms);

  if (pending_.has_value() || pending_comeback_.has_value()) {
    // 🔴 UN TOUR DE BOUCLE ENTIER SÉPARE L'INTENTION DE L'APPEL QUI BLOQUE, et
    // ce tour est ce que le joueur voit.
    //
    // `SessionDrive::Begin` ouvre une socket : mesuré en production, 1 ms sur
    // une adresse résolue — mais **5,2 s sur un port fermé**, c'est-à-dire
    // quand le serveur est éteint, c'est-à-dire précisément quand il faut le
    // dire. Le formulaire est dessiné au moment où le bouton est pressé ; sans
    // ce tour d'attente, l'appel partirait avant qu'aucune image ne porte
    // « connexion… », et l'écran resterait figé cinq secondes sur un
    // formulaire qui a l'air de ne rien faire.
    if (!pending_held_a_turn_) {
      pending_held_a_turn_ = true;
      return;
    }
    std::optional<Credentials> credentials = std::move(pending_);
    std::optional<Comeback> comeback = std::move(pending_comeback_);
    pending_.reset();
    pending_comeback_.reset();

    // 🔴 LE DÉCOR SE REND À L'INSTANT OÙ LE JOUEUR SE CONNECTE — jamais quand
    // sa connexion RÉUSSIT. C'est ce que garde cette ligne, et c'est ce que son
    // essai éprouve : le rendre plus tard laisserait une session de décor
    // vivante derrière chaque tentative manquée, plus un décor mais une session
    // gratuite que personne ne regarde, sur la seule place que le serveur
    // accorde à cette adresse.
    //
    // ⚠ ET AVANT `Begin`, qui peut retenir la boucle 5,2 s sur un port fermé :
    // rendre après, c'est tenir la session pendant tout ce temps, à chaque
    // essai. L'ordre de ces deux lignes-là n'est en revanche pas observable du
    // dehors — deux sockets indépendantes —, donc aucun essai ne le fixe.
    backdrop_.Release();
    // LE RETOUR DEPUIS LA PARTIE reprend au serveur de personnages : même
    // ordre, même tour d'attente — la socket s'ouvre là aussi.
    if (comeback.has_value()) {
      player_.Resume(*comeback);
    } else {
      player_.Begin(*credentials);
    }
  }

  player_.Advance(now_ms);

  switch (player_.stage()) {
    case SessionDrive::Stage::kAsleep:
    case SessionDrive::Stage::kConnecting:
    case SessionDrive::Stage::kAwaitingList:
      return;
    case SessionDrive::Stage::kAwaitingPincode:
      screen_ = LobbyScreenKind::kPincode;
      return;
    case SessionDrive::Stage::kChoosing:
      screen_ = LobbyScreenKind::kCharacters;
      return;
    case SessionDrive::Stage::kEntering:
      screen_ = LobbyScreenKind::kEntering;
      return;
    case SessionDrive::Stage::kInWorld:
      screen_ = LobbyScreenKind::kHandoff;
      return;
    case SessionDrive::Stage::kRefused:
    case SessionDrive::Stage::kFailed:
      // ⚠ TOUT ÉCHEC REVIENT AU FORMULAIRE, y compris celui qui survient APRÈS
      // la liste. Ce n'est pas un raccourci : la liste venait d'une liaison
      // qui n'existe plus, et la remontrer laisserait le joueur cliquer sur des
      // personnages que plus rien ne relie au serveur.
      ro::core::LogWarning("connexion : {}", player_.message());
      FallBackToForm(player_.message());
      return;
  }
}

void LobbyFlow::ChooseCharacter(std::uint8_t slot) {
  if (screen_ != LobbyScreenKind::kCharacters) {
    return;
  }
  player_.Choose(slot);
  screen_ = LobbyScreenKind::kEntering;
}

void LobbyFlow::CreateCharacter(const ro::net::CharacterDraft& draft) {
  if (screen_ != LobbyScreenKind::kCharacters) {
    return;
  }
  // ⚠ L'ÉCRAN NE BOUGE PAS. Créer se fait DEPUIS la tablée, et la réponse — le
  // nouveau venu assis, ou le motif du refus — s'y lit. Passer par un écran
  // d'attente ferait clignoter la salle pour un aller-retour de quelques
  // millisecondes.
  player_.Create(draft);
}

void LobbyFlow::ReserveDeletion(std::uint32_t character_id) {
  if (screen_ != LobbyScreenKind::kCharacters) {
    return;
  }
  player_.ReserveDeletion(character_id);
}

void LobbyFlow::CancelDeletion(std::uint32_t character_id) {
  if (screen_ != LobbyScreenKind::kCharacters) {
    return;
  }
  player_.CancelDeletion(character_id);
}

void LobbyFlow::ConfirmDeletion(std::uint32_t character_id, std::string_view secret) {
  if (screen_ != LobbyScreenKind::kCharacters) {
    return;
  }
  player_.ConfirmDeletion(character_id, secret);
}

ro::core::Result<void> LobbyFlow::SubmitPincode(std::string_view digits) {
  if (screen_ != LobbyScreenKind::kPincode) {
    return {};
  }
  if (auto sent = player_.SubmitPincode(digits); !sent.has_value()) {
    return std::unexpected(sent.error());
  }
  screen_ = LobbyScreenKind::kConnecting;
  return {};
}

void LobbyFlow::Release() noexcept {
  pending_.reset();
  pending_comeback_.reset();
  backdrop_.Release();
  player_.Release();
  message_.clear();
  screen_ = LobbyScreenKind::kForm;
}

void LobbyFlow::Back() {
  if (screen_ == LobbyScreenKind::kForm || screen_ == LobbyScreenKind::kHandoff) {
    return;
  }
  // La demande en attente part avec le reste : un retour pendant le tour
  // d'attente ne doit pas voir la connexion s'ouvrir juste après.
  pending_.reset();
  pending_comeback_.reset();
  player_.Release();
  message_.clear();
  screen_ = LobbyScreenKind::kForm;
}

}  // namespace moonclient::lobby
