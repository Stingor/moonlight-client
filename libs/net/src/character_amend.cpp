// LES RETOUCHES D'UN PERSONNAGE DEPUIS LA TABLÉE : changer d'emplacement, et
// changer de nom.
//
// ⚠ MÊME CLASSE QUE `session.cpp`, AUTRE UNITÉ DE TRADUCTION : ce sont deux
// échanges complets — une demande, un accusé, une liste renvoyée — que la phase
// de personnages porte sans qu'ils touchent au reste de son déroulement.

#include <algorithm>
#include <string>
#include <utility>

#include "ro/net/refusal_text.h"
#include "ro/net/session.h"
#include "ro/proto/common_packets.h"

namespace ro::net {
namespace {

// LA RAISON QUI VEUT DIRE « DÉPLACÉ ». Tout le reste est un refus.
constexpr std::uint16_t kMoveAccepted = 0;
// LE RÉSULTAT QUI VEUT DIRE « RENOMMÉ » — y compris quand le nom demandé est
// déjà celui du personnage, que le serveur accepte sans rien décompter.
constexpr std::uint32_t kRenameAccepted = 0;

}  // namespace

void CharacterPhase::MoveCharacter(std::uint8_t from_slot, std::uint8_t to_slot) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  const int index = IndexOfSlot(characters_, from_slot);
  proto::ChReqChangeCharacterSlot request;
  request.slot_before = from_slot;
  request.slot_after = to_slot;
  // ⚠ LE SERVEUR NE LIT PAS CE CHAMP : il tient son propre compte. On y met ce
  // que la liste annonce.
  request.remaining =
      index < 0 ? 0
                : static_cast<std::uint16_t>(
                      std::max(characters_[static_cast<std::size_t>(index)].chr_slot_changeCnt, 0));
  link_.Send(proto::Encode(request));
  amend_step_ = AmendStep::kMove;
  amend_ = AmendState::kPending;
  amend_from_slot_ = from_slot;
  amend_to_slot_ = to_slot;
  amend_character_id_ = 0;
  amend_name_.clear();
  amend_refusal_text_.clear();
}

void CharacterPhase::RenameCharacter(std::uint32_t character_id, std::string_view new_name) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  proto::ChReqChangeCharname request;
  request.CID = character_id;
  request.new_name = std::string(new_name);
  link_.Send(proto::Encode(request));
  amend_step_ = AmendStep::kRename;
  amend_ = AmendState::kPending;
  amend_from_slot_ = -1;
  amend_to_slot_ = -1;
  amend_character_id_ = character_id;
  amend_name_ = std::string(new_name);
  amend_refusal_text_.clear();
}

void CharacterPhase::ForgetAmend() noexcept {
  amend_step_ = AmendStep::kNone;
  amend_ = AmendState::kIdle;
  amend_from_slot_ = -1;
  amend_to_slot_ = -1;
  amend_character_id_ = 0;
  amend_name_.clear();
  amend_refusal_text_.clear();
}

bool CharacterPhase::TookAmendAnswer(const Frame& frame) {
  if (frame.opcode == proto::HcAckChangeCharacterSlot::kOpcode) {
    proto::HcAckChangeCharacterSlot moved;
    if (!proto::Decode(frame.bytes, moved)) {
      error_ = core::Error{core::ErrorCode::kCorruptData, "accusé de déplacement illisible"};
      state_ = PhaseState::kFailed;
      return true;
    }
    if (moved.reason != kMoveAccepted) {
      amend_ = AmendState::kRefused;
      amend_refusal_text_ = MoveRefusalText(moved.reason);
      return true;
    }
    // 🔴 L'ACCUSÉ NE DIT NI QUI NI OÙ : on reporte NOTRE demande. Une place
    // occupée échange ses deux convives, comme le fait le serveur.
    const int from = IndexOfSlot(characters_, static_cast<std::uint8_t>(amend_from_slot_));
    const int to = IndexOfSlot(characters_, static_cast<std::uint8_t>(amend_to_slot_));
    if (to >= 0) {
      characters_[static_cast<std::size_t>(to)].CharNum =
          static_cast<std::uint8_t>(amend_from_slot_);
    }
    if (from >= 0) {
      proto::CharacterInfo& traveller = characters_[static_cast<std::size_t>(from)];
      traveller.CharNum = static_cast<std::uint8_t>(amend_to_slot_);
      // ⚠ CE QUI RESTE AU VOYAGEUR, compté par le serveur après le
      // déplacement.
      traveller.chr_slot_changeCnt = moved.moves;
    }
    amend_ = AmendState::kDone;
    return true;
  }
  if (frame.opcode == proto::HcAckChangeCharname::kOpcode) {
    proto::HcAckChangeCharname renamed;
    if (!proto::Decode(frame.bytes, renamed)) {
      error_ = core::Error{core::ErrorCode::kCorruptData, "accusé de renommage illisible"};
      state_ = PhaseState::kFailed;
      return true;
    }
    if (renamed.result != kRenameAccepted) {
      amend_ = AmendState::kRefused;
      amend_refusal_text_ = RenameRefusalText(renamed.result);
      return true;
    }
    if (proto::CharacterInfo* who = Seated(amend_character_id_); who != nullptr) {
      who->name = amend_name_;
      // ⚠ ZÉRO EN ATTENDANT LA LISTE : elle suit l'accusé, et dira s'il en
      // reste un autre.
      who->chr_name_changeCnt = 0;
    }
    amend_ = AmendState::kDone;
    return true;
  }
  return false;
}

}  // namespace ro::net
