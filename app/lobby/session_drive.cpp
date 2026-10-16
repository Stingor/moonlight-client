#include "lobby/session_drive.h"

#include <string>
#include <utility>

namespace moonclient::lobby {
namespace {

using ro::net::PhaseState;

// Ce qu'on dit d'un acte refusé quand le serveur n'a nommé aucun motif.
std::string Fallback(std::string_view what, std::uint32_t code) {
  return std::string(what) + " refusée par le serveur (code " + std::to_string(code) + ")";
}

}  // namespace

// ⚠ « LA PHASE A-T-ELLE CASSÉ ? » ET NON « A-T-ELLE FINI ? ». La distinction
// n'est pas cosmétique : `CharacterPhase` ne rend `kDone` qu'à l'ADRESSE DE
// ZONE, c'est-à-dire longtemps après que la liste soit arrivée. Un pilote qui
// attendrait son verdict pour lire son étape n'afficherait jamais la liste.
template <typename Phase>
bool SessionDrive::Broke(PhaseState state, const Phase& phase, std::string_view what) {
  switch (state) {
    case PhaseState::kWaiting:
    case PhaseState::kDone:
      return false;
    case PhaseState::kRefused:
      // LE MOTIF EN CLAIR QUAND LA PHASE SAIT LE DIRE — voir
      // `ro/net/refusal_text.h`. Le code nu reste pour les tables qu'on ne
      // traduit pas encore.
      message_ = phase.refusal_text().empty() ? Fallback(what, phase.refusal_code())
                                              : std::string(what) + " : " + phase.refusal_text();
      stage_ = Stage::kRefused;
      return true;
    case PhaseState::kFailed:
      message_ = std::string(what) + " : " + phase.error().detail;
      stage_ = Stage::kFailed;
      return true;
  }
  message_ = "verdict inconnu";
  stage_ = Stage::kFailed;
  return true;
}

void SessionDrive::Begin(const Credentials& credentials) {
  // 🔴 PAS DE SECONDE SESSION PAR-DESSUS LA PREMIÈRE. Relancer sans rendre la
  // précédente laisserait une liaison ouverte que plus personne ne tient : le
  // serveur la garderait jusqu'à son propre délai, et sur le compte du décor
  // — plafonné à une session par adresse — la suivante serait refusée. Après un
  // échec, l'appelant appelle donc `Release` avant de réessayer.
  if (stage_ != Stage::kAsleep) {
    return;
  }
  auto opened = ro::net::LoginPhase::Start(credentials.host, credentials.port, credentials.account,
                                           credentials.password);
  if (!opened.has_value()) {
    message_ = "connexion : " + opened.error().detail;
    stage_ = Stage::kFailed;
    return;
  }
  login_.emplace(std::move(*opened));
  stage_ = Stage::kConnecting;
}

void SessionDrive::Resume(const Comeback& comeback) {
  if (stage_ != Stage::kAsleep) {
    return;
  }
  account_ = comeback.account;
  character_server_ = comeback.character_server;
  auto opened = ro::net::CharacterPhase::Start(character_server_, account_);
  if (!opened.has_value()) {
    message_ = "serveur de personnages : " + opened.error().detail;
    stage_ = Stage::kFailed;
    return;
  }
  characters_.emplace(std::move(*opened));
  stage_ = Stage::kAwaitingList;
}

void SessionDrive::Advance(std::uint32_t now_ms) {
  switch (stage_) {
    case Stage::kAsleep:
    case Stage::kInWorld:
    case Stage::kRefused:
    case Stage::kFailed:
      return;

    case Stage::kConnecting: {
      const PhaseState state = login_->Advance();
      if (Broke(state, *login_, "connexion") || state != PhaseState::kDone) {
        return;
      }
      if (login_->servers().empty()) {
        message_ = "le serveur n'annonce aucun serveur de personnages";
        stage_ = Stage::kFailed;
        return;
      }
      account_ = login_->account();
      // ⚠ LE PREMIER DE LA LISTE, comme le client d'origine quand il n'en voit
      // qu'un. Ce serveur n'en annonce qu'un : en montrer le choix serait une
      // question posée à un joueur qui n'a pas de quoi y répondre.
      character_server_ = login_->servers().front().address;
      auto opened = ro::net::CharacterPhase::Start(character_server_, account_);
      login_.reset();
      if (!opened.has_value()) {
        message_ = "serveur de personnages : " + opened.error().detail;
        stage_ = Stage::kFailed;
        return;
      }
      characters_.emplace(std::move(*opened));
      stage_ = Stage::kAwaitingList;
      return;
    }

    case Stage::kAwaitingList:
    case Stage::kAwaitingPincode:
    case Stage::kChoosing: {
      if (Broke(characters_->Advance(), *characters_, "liste des personnages")) {
        return;
      }
      // LE BATTEMENT QUI TIENT LA LIAISON pendant qu'on choisit. Le serveur
      // ferme une session dont il n'a rien reçu depuis `stall_time` secondes,
      // et un écran de sélection reste ouvert bien plus longtemps.
      characters_->Beat(now_ms);
      switch (characters_->step()) {
        case ro::net::CharacterPhase::Step::kAwaitingPincode:
          stage_ = Stage::kAwaitingPincode;
          return;
        case ro::net::CharacterPhase::Step::kAwaitingSelection:
          stage_ = Stage::kChoosing;
          return;
        default:
          // L'identifiant nu, la liste qui n'est pas encore là, le code en
          // cours de vérification : on attend, et l'étape ne bouge pas.
          return;
      }
    }

    case Stage::kEntering: {
      // DEUX ACTES SOUS UNE SEULE ÉTAPE, et c'est ce que le joueur voit : entre
      // le personnage choisi et la carte montée, il n'y a qu'une attente.
      if (world_.has_value()) {
        const PhaseState state = world_->Advance();
        if (Broke(state, *world_, "entrée sur la carte") || state != PhaseState::kDone) {
          return;
        }
        spawn_ = world_->spawn();
        link_ = world_->TakeLink();
        world_.reset();
        stage_ = Stage::kInWorld;
        return;
      }
      const PhaseState state = characters_->Advance();
      if (Broke(state, *characters_, "entrée en jeu") || state != PhaseState::kDone) {
        return;
      }
      map_name_ = characters_->handoff().map_name;
      // ⚠ L'HORLOGE PART D'ICI, et le serveur cale la sienne dessus. La couche
      // réseau n'en lit aucune : celle-ci vient de la boucle.
      entered_tick_ = now_ms;
      auto opened = ro::net::MapPhase::Start(characters_->handoff(), account_, entered_tick_);
      characters_.reset();
      if (!opened.has_value()) {
        message_ = "serveur de zone : " + opened.error().detail;
        stage_ = Stage::kFailed;
        return;
      }
      world_.emplace(std::move(*opened));
      // ⚠ AVANT LE PREMIER TOUR : l'accord d'entrée est la toute première
      // trame, et une capture qui commencerait après le manquerait.
      if (zone_observer_) {
        world_->SetReceiveObserver(zone_observer_);
      }
      return;
    }
  }
}

void SessionDrive::Choose(std::uint8_t slot) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  chosen_slot_ = slot;
  // ⚠ LA COPIE SE PREND ICI, avant que quoi que ce soit ne parte : voir
  // `chosen_`. Un emplacement qui ne correspond à personne ne pose rien, et
  // l'appelant le verra à son tour.
  if (const int index = ro::net::IndexOfSlot(characters_->characters(), slot); index >= 0) {
    chosen_ = characters_->characters()[static_cast<std::size_t>(index)];
  }
  characters_->Select(slot);
  stage_ = Stage::kEntering;
}

void SessionDrive::Create(const ro::net::CharacterDraft& draft) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  characters_->Create(draft);
}

ro::net::CreationState SessionDrive::creation() const noexcept {
  return characters_.has_value() ? characters_->creation() : ro::net::CreationState::kIdle;
}

std::string SessionDrive::creation_refusal() const {
  return characters_.has_value() ? characters_->creation_refusal_text() : std::string();
}

int SessionDrive::created_slot() const noexcept {
  return characters_.has_value() ? characters_->created_slot() : -1;
}

void SessionDrive::ForgetCreation() noexcept {
  if (characters_.has_value()) {
    characters_->ForgetCreation();
  }
}

void SessionDrive::ReserveDeletion(std::uint32_t character_id) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  characters_->ReserveDeletion(character_id);
}

void SessionDrive::CancelDeletion(std::uint32_t character_id) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  characters_->CancelDeletion(character_id);
}

void SessionDrive::ConfirmDeletion(std::uint32_t character_id, std::string_view secret) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  characters_->ConfirmDeletion(character_id, secret);
}

ro::net::DeletionStep SessionDrive::deletion_step() const noexcept {
  return characters_.has_value() ? characters_->deletion_step() : ro::net::DeletionStep::kNone;
}

ro::net::DeletionState SessionDrive::deletion() const noexcept {
  return characters_.has_value() ? characters_->deletion() : ro::net::DeletionState::kIdle;
}

std::uint32_t SessionDrive::deletion_character_id() const noexcept {
  return characters_.has_value() ? characters_->deletion_character_id() : 0;
}

std::string SessionDrive::deletion_refusal() const {
  return characters_.has_value() ? characters_->deletion_refusal_text() : std::string();
}

void SessionDrive::ForgetDeletion() noexcept {
  if (characters_.has_value()) {
    characters_->ForgetDeletion();
  }
}

void SessionDrive::MoveCharacter(std::uint8_t from_slot, std::uint8_t to_slot) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  characters_->MoveCharacter(from_slot, to_slot);
}

void SessionDrive::RenameCharacter(std::uint32_t character_id, std::string_view new_name) {
  if (stage_ != Stage::kChoosing) {
    return;
  }
  characters_->RenameCharacter(character_id, new_name);
}

ro::net::AmendStep SessionDrive::amend_step() const noexcept {
  return characters_.has_value() ? characters_->amend_step() : ro::net::AmendStep::kNone;
}

ro::net::AmendState SessionDrive::amend() const noexcept {
  return characters_.has_value() ? characters_->amend() : ro::net::AmendState::kIdle;
}

int SessionDrive::amend_target_slot() const noexcept {
  return characters_.has_value() ? characters_->amend_target_slot() : -1;
}

std::string SessionDrive::amend_refusal() const {
  return characters_.has_value() ? characters_->amend_refusal_text() : std::string();
}

void SessionDrive::ForgetAmend() noexcept {
  if (characters_.has_value()) {
    characters_->ForgetAmend();
  }
}

ro::core::Result<void> SessionDrive::SubmitPincode(std::string_view digits) {
  if (stage_ != Stage::kAwaitingPincode) {
    return {};
  }
  if (auto sent = characters_->SubmitPincode(digits); !sent.has_value()) {
    return std::unexpected(sent.error());
  }
  stage_ = Stage::kAwaitingList;
  return {};
}

bool SessionDrive::pincode_refused() const noexcept {
  return characters_.has_value() && characters_->pincode_refused();
}

std::uint8_t SessionDrive::producible_slots() const noexcept {
  return characters_.has_value() ? characters_->producible_slots() : 0;
}

std::vector<ro::proto::CharacterInfo> SessionDrive::characters() const {
  return characters_.has_value() ? characters_->characters()
                                 : std::vector<ro::proto::CharacterInfo>();
}

const ro::proto::CharacterInfo* SessionDrive::chosen_character() const {
  return chosen_.has_value() ? &*chosen_ : nullptr;
}

ro::net::Connection SessionDrive::TakeLink() noexcept { return std::move(link_); }

void SessionDrive::Release() noexcept {
  // 🔴 LES QUATRE PORTEURS DE LIAISON, ET PAS SEULEMENT LE DERNIER. Une session
  // se rend à n'importe quel acte — c'est même le cas ordinaire du décor, qu'on
  // referme à l'instant où le joueur se connecte —, et n'en fermer qu'un
  // laisserait la socket des trois autres ouverte jusqu'au délai du serveur.
  login_.reset();
  characters_.reset();
  world_.reset();
  link_.Close();
  stage_ = Stage::kAsleep;
  message_.clear();
  chosen_slot_ = 0;
  chosen_.reset();
  map_name_.clear();
  account_ = ro::net::LoginPhase::Account();
  character_server_ = ro::net::ServerAddress();
  spawn_ = ro::net::MapPhase::Spawn();
}

}  // namespace moonclient::lobby
