#include "ro/net/session.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <string>

#include "ro/core/ascii.h"
#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/core/text.h"
#include "ro/net/map_name.h"
#include "ro/net/refusal_text.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/packet_lengths.h"
#include "ro/proto/pincode.h"

namespace ro::net {
namespace {

using core::Error;
using core::ErrorCode;

// Le type de client annoncé. Le serveur de connexion le range et le transmet au
// serveur de personnages sans jamais s'en servir pour décider quoi que ce soit.
constexpr std::uint8_t kClientType = 0x0A;

// La demande d'entrée sur le serveur de personnages.
//
// ⚠ Écrite à la main, et elle le restera : le serveur n'a PAS de structure pour
// elle. Il la lit champ par champ à coups de décalages dans
// chclif_parse_reqtoconnect, et un générateur ne peut pas produire ce qui
// n'existe pas. L'ordre des champs est recopié de cette fonction, et la taille
// totale est recoupée avec la table avant tout envoi.
constexpr std::uint16_t kEnterOpcode = 0x0065;
constexpr std::size_t kEnterSize = 17;

// La réponse à la demande d'entrée : un identifiant de compte NU, sans opcode.
// Le serveur de personnages ET celui de zone répondent ainsi.
constexpr std::size_t kBareAccountIdSize = 4;

// CE QUE « ACCEPTÉ » VAUT DANS LES DEUX PREMIÈRES ÉTAPES DE LA SUPPRESSION.
//
// ⚠ UN, ET NON ZÉRO. Ces deux réponses portent un RÉSULTAT et non un code
// d'erreur : zéro y veut dire « déjà programmée » à la réservation, et rien du
// tout à l'annulation. Lire un succès dans le zéro habituel effacerait
// l'échéance du personnage sur un refus.
constexpr std::int32_t kDeleteReserveAccepted = 1;
constexpr std::int32_t kDeleteCancelAccepted = 1;

// L'ÉCHÉANCE D'UNE SUPPRESSION, en date Unix, depuis ce que porte le fil.
//
// 🔴 LE FIL NE PORTE PAS UNE DATE MAIS UN DÉLAI RESTANT, compté à l'envoi :
// `delete_date - time(nullptr)`, dans la liste comme dans l'accusé de
// réservation. Il devient NÉGATIF une fois l'heure passée. Lu comme une date,
// il donnerait « jamais programmée » à un personnage dont la suppression est
// mûre — plus de rouge, plus d'« Annuler ». On l'ancre donc à la réception :
// une seconde plus tard, il ne voudrait déjà plus rien dire.
[[nodiscard]] std::int32_t DeleteDeadlineFrom(std::int32_t remaining_seconds) {
  const auto now = static_cast<std::int64_t>(std::time(nullptr));
  return static_cast<std::int32_t>(now + remaining_seconds);
}

// Le refus du serveur de zone. Il emploie la forme ANCIENNE du refus de
// connexion — opcode puis un octet de code — là où le serveur de connexion
// emploie la nouvelle, plus longue et sous un autre opcode.
constexpr std::uint16_t kMapRefusalOpcode = 0x006A;

// Les deux octets de l'opcode, en tête de chaque trame.
constexpr std::size_t kOpcodeBytes = 2;

std::vector<std::uint8_t> BuildEnterRequest(const LoginPhase::Account& account) {
  std::vector<std::uint8_t> request;
  request.reserve(kEnterSize);
  core::ByteWriter writer(request);
  writer.U16(kEnterOpcode);
  writer.U32(account.account_id);
  writer.U32(account.login_id1);
  writer.U32(account.login_id2);
  // Deux octets que le serveur saute : ils existent dans la trame, mais sa
  // fonction de lecture ne les nomme pas.
  writer.U16(0);
  writer.U8(account.sex);
  return request;
}

// Traduit ce qu'une liaison vient de rendre en verdict de phase.
//
// Une fermeture par le serveur pendant une phase N'EST PAS un déroulement
// normal : chacune se termine sur un paquet, jamais sur un silence.
PhaseState LinkVerdict(LinkStatus status, const Connection& link, Error& error) {
  switch (status) {
    case LinkStatus::kOpen:
      return PhaseState::kWaiting;
    case LinkStatus::kClosedByPeer:
      error = Error{ErrorCode::kIoFailure, "le serveur a fermé avant de répondre"};
      return PhaseState::kFailed;
    case LinkStatus::kProtocolError:
    case LinkStatus::kIoError:
      error = link.error();
      return PhaseState::kFailed;
  }
  error = Error{ErrorCode::kIoFailure, "état de liaison inconnu"};
  return PhaseState::kFailed;
}

// L'AVIS D'ÉVICTION, QUE LES TROIS SERVEURS ENVOIENT.
//
// Il ne demande rien et n'attend pas de réponse : le serveur appelle `set_eof`
// dans la foulée. Rendre faux dit seulement que ce n'était pas lui.
//
// 🔴 SANS CE TRAITEMENT, la phase attendait une réponse qui n'arrivait plus,
// puis concluait à une fermeture muette — et le seul mot du serveur sur la
// raison, le seul qu'il ait pris la peine d'envoyer, était jeté.
bool TookBanNotice(const Frame& frame, std::uint32_t& refusal_code, std::string& refusal_text) {
  if (frame.opcode != proto::ScNotifyBan::kOpcode) {
    return false;
  }
  proto::ScNotifyBan banned;
  if (!proto::Decode(frame.bytes, banned)) {
    // ⚠ INDÉCHIFFRABLE N'EST PAS « PAS À MOI » : l'opcode était le bon, donc la
    // session est finie de toute façon. Le dire sans motif vaut mieux que de
    // laisser la phase attendre.
    refusal_code = 0;
    refusal_text = "le serveur a mis fin à la session, dans une trame illisible";
    return true;
  }
  refusal_code = banned.result;
  refusal_text = BanText(banned.result);
  return true;
}

Error UndecodableFrame(std::uint16_t opcode) {
  std::array<char, 48> text = {};
  std::snprintf(text.data(), text.size(), "trame 0x%04x illisible", static_cast<unsigned>(opcode));
  return Error{ErrorCode::kCorruptData, text.data()};
}

}  // namespace

std::string ServerAddress::host() const {
  std::array<char, 16> text = {};
  std::snprintf(text.data(), text.size(), "%u.%u.%u.%u", wire_ip & 0xFF, (wire_ip >> 8) & 0xFF,
                (wire_ip >> 16) & 0xFF, (wire_ip >> 24) & 0xFF);
  return text.data();
}

core::Result<LoginPhase> LoginPhase::Start(std::string_view host, std::uint16_t port,
                                           std::string_view account, std::string_view password) {
  auto link = Connection::Open(host, port);
  if (!link.has_value()) {
    return std::unexpected(link.error());
  }

  proto::CaLogin request;
  request.version = static_cast<std::uint32_t>(proto::kPacketVersion);
  request.username = std::string(account);
  request.password = std::string(password);
  request.clienttype = kClientType;

  LoginPhase phase(std::move(*link));
  phase.link_.Send(proto::Encode(request));
  return phase;
}

PhaseState LoginPhase::Advance() {
  if (state_ != PhaseState::kWaiting) {
    return state_;
  }

  Connection::Polled polled = link_.Poll();
  for (const Frame& frame : polled.frames) {
    if (frame.opcode == proto::AcAcceptLogin::kOpcode) {
      proto::AcAcceptLogin accepted;
      if (!proto::Decode(frame.bytes, accepted)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      account_.account_id = accepted.AID;
      account_.login_id1 = accepted.login_id1;
      account_.login_id2 = accepted.login_id2;
      account_.sex = accepted.sex;
      account_.web_token = accepted.token;
      account_.world_name = accepted.char_servers.empty()
                                ? std::string()
                                : core::Utf8FromCp1252(accepted.char_servers.front().name);
      servers_.clear();
      servers_.reserve(accepted.char_servers.size());
      for (const proto::AcAcceptLoginSub& server : accepted.char_servers) {
        servers_.push_back(
            CharacterServer{server.name, ServerAddress{server.ip, server.port}, server.users});
      }
      state_ = PhaseState::kDone;
      return state_;
    }
    if (frame.opcode == proto::AcRefuseLogin::kOpcode) {
      proto::AcRefuseLogin refused;
      if (!proto::Decode(frame.bytes, refused)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      refusal_code_ = refused.error;
      refusal_text_ = LoginRefusalText(refused.error, refused.unblock_time);
      state_ = PhaseState::kRefused;
      return state_;
    }
    if (TookBanNotice(frame, refusal_code_, refusal_text_)) {
      state_ = PhaseState::kRefused;
      return state_;
    }
    // Tout le reste est ignoré : le serveur de connexion peut envoyer des
    // paquets dont cette phase n'a pas besoin, et les refuser fermerait une
    // session parfaitement valide.
  }

  state_ = LinkVerdict(polled.status, link_, error_);
  return state_;
}

core::Result<CharacterPhase> CharacterPhase::Start(const ServerAddress& server,
                                                   const LoginPhase::Account& account) {
  // Le recoupement qui garde la seule disposition écrite à la main de ce
  // fichier : la table vient du serveur, la taille ci-dessus vient de sa
  // fonction de lecture. Un désaccord signifierait que l'une des deux a bougé.
  const auto declared = proto::PacketLength(kEnterOpcode);
  if (!declared.has_value() || static_cast<std::size_t>(*declared) != kEnterSize) {
    return core::Fail(ErrorCode::kCorruptData,
                      "la demande d'entrée ne fait pas la taille que la table annonce");
  }

  auto link = Connection::Open(server.host(), server.port);
  if (!link.has_value()) {
    return std::unexpected(link.error());
  }

  CharacterPhase phase(std::move(*link));
  phase.link_.Send(BuildEnterRequest(account));
  // La réponse arrive AVANT toute trame, et n'en est pas une.
  phase.link_.ExpectRaw(kBareAccountIdSize);
  return phase;
}

void CharacterPhase::Select(std::uint8_t slot) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  proto::ChSelectChar request;
  request.slot = slot;
  link_.Send(proto::Encode(request));
  step_ = Step::kAwaitingZoneAddress;
}

void CharacterPhase::Create(const CharacterDraft& draft) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  proto::ChMakeChar request;
  request.name = draft.name;
  request.slot = draft.slot;
  request.hair_color = draft.hair_palette;
  request.hair_style = draft.hair_style;
  // ⚠ LE NOVICE, ET RIEN D'AUTRE. Le serveur n'accepte que deux classes de
  // départ (`allowed_job_flag`, `char.cpp:1691`), et l'on ne sait pas laquelle
  // il autorise en production. Le client d'origine n'envoie que
  // celle-ci ; proposer l'autre sans savoir ferait refuser sans le dire.
  request.job = 0;
  request.sex = draft.sex;
  link_.Send(proto::Encode(request));
  creation_ = CreationState::kPending;
  creation_refusal_code_ = 0;
  creation_refusal_text_.clear();
  created_slot_ = -1;
}

void CharacterPhase::ReserveDeletion(std::uint32_t character_id) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  proto::ChDeleteChar3Reserved request;
  request.CID = character_id;
  link_.Send(proto::Encode(request));
  deletion_step_ = DeletionStep::kReserve;
  deletion_ = DeletionState::kPending;
  deletion_character_id_ = character_id;
  deletion_refusal_text_.clear();
}

void CharacterPhase::CancelDeletion(std::uint32_t character_id) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  proto::ChDeleteChar3Cancel request;
  request.CID = character_id;
  link_.Send(proto::Encode(request));
  deletion_step_ = DeletionStep::kCancel;
  deletion_ = DeletionState::kPending;
  deletion_character_id_ = character_id;
  deletion_refusal_text_.clear();
}

void CharacterPhase::ConfirmDeletion(std::uint32_t character_id, std::string_view secret) {
  if (step_ != Step::kAwaitingSelection) {
    return;
  }
  proto::ChDeleteChar request;
  request.CID = character_id;
  request.key = std::string(secret);
  link_.Send(proto::Encode(request));
  deletion_step_ = DeletionStep::kConfirm;
  deletion_ = DeletionState::kPending;
  deletion_character_id_ = character_id;
  deletion_refusal_text_.clear();
}

void CharacterPhase::ForgetDeletion() noexcept {
  deletion_step_ = DeletionStep::kNone;
  deletion_ = DeletionState::kIdle;
  deletion_character_id_ = 0;
  deletion_refusal_text_.clear();
}

void CharacterPhase::ForgetCreation() noexcept {
  creation_ = CreationState::kIdle;
  creation_refusal_code_ = 0;
  creation_refusal_text_.clear();
  created_slot_ = -1;
}

core::Result<void> CharacterPhase::SubmitPincode(std::string_view digits) {
  if (step_ != Step::kAwaitingPincode) {
    return {};
  }

  // Le brouillage a lieu AVANT toute émission : un code mal formé se refuse
  // ici, où l'on peut le dire, plutôt que sur le fil, où le serveur se
  // contenterait de fermer.
  auto obfuscated = proto::ObfuscatePincode(pincode_seed_, digits);
  if (!obfuscated.has_value()) {
    return std::unexpected(obfuscated.error());
  }

  proto::ChSecondPasswdAck answer;
  answer.AID = account_id_;
  answer.pin = *obfuscated;
  link_.Send(proto::Encode(answer));
  pincode_refused_ = false;
  step_ = Step::kCheckingPincode;
  return {};
}

void CharacterPhase::Beat(std::uint32_t now_ms) {
  // Rien à tenir en vie tant que la phase n'a pas d'identifiant à présenter, ni
  // une fois qu'elle s'est prononcée : la liaison part alors à l'acte suivant,
  // ou elle est morte.
  if (state_ != PhaseState::kWaiting || step_ == Step::kAwaitingAccountId) {
    return;
  }
  if (!last_beat_ms_.has_value()) {
    last_beat_ms_ = now_ms;
    return;
  }
  if (now_ms - *last_beat_ms_ < kCharacterPingIntervalMs) {
    return;
  }
  last_beat_ms_ = now_ms;
  proto::Ping beat;
  beat.AID = account_id_;
  link_.Send(proto::Encode(beat));
}

bool CharacterPhase::HandlePincodeState(std::uint16_t state) {
  // Les valeurs sont celles de enum pincode_state, dans src/char/char.hpp.
  // Depuis PACKETVER 20180124, « accepté » et « désactivé » partagent le zéro :
  // le bouton qui les distinguait a disparu du client officiel.
  constexpr std::uint16_t kPincodeAccepted = 0;
  constexpr std::uint16_t kPincodeAsk = 1;
  constexpr std::uint16_t kPincodeNotSet = 2;
  constexpr std::uint16_t kPincodeExpired = 3;
  constexpr std::uint16_t kPincodeNew = 4;
  constexpr std::uint16_t kPincodeIllegal = 5;
  constexpr std::uint16_t kPincodeWrong = 8;

  switch (state) {
    case kPincodeAccepted:
      // La liste est déjà arrivée : le serveur l'envoie AVANT de parler du
      // code. Revenir à l'attente de la liste ferait attendre pour rien.
      //
      // 🔴 ON LIT LE TÉMOIN, PAS LE NOMBRE. Un compte qui vient d'être créé
      // reçoit une liste VIDE, indiscernable d'une liste absente si l'on compte
      // ses personnages : la phase repartait alors attendre ce qu'elle tenait
      // déjà, jusqu'au délai, et l'appelant annonçait « le serveur n'a pas
      // répondu » d'un serveur qui avait répondu. C'est le cas du joueur neuf.
      step_ = list_received_ ? Step::kAwaitingSelection : Step::kAwaitingCharacterList;
      return true;
    case kPincodeAsk:
      pincode_refused_ = false;
      step_ = Step::kAwaitingPincode;
      return true;
    case kPincodeWrong:
      // Le serveur a joint une graine NEUVE : la réponse suivante doit être
      // brouillée avec elle, ce dont s'occupe l'appelant de cette fonction.
      pincode_refused_ = true;
      step_ = Step::kAwaitingPincode;
      return true;
    case kPincodeNotSet:
    case kPincodeNew:
      error_ = Error{ErrorCode::kUnsupportedVersion,
                     "ce compte n'a pas encore de code PIN ; en créer un demande un échange "
                     "que ce client ne sait pas mener"};
      return false;
    case kPincodeExpired:
      error_ = Error{ErrorCode::kUnsupportedVersion,
                     "le code PIN de ce compte a expiré ; le changer demande un échange que "
                     "ce client ne sait pas mener"};
      return false;
    case kPincodeIllegal:
      error_ = Error{ErrorCode::kCorruptData, "le serveur a jugé le code PIN interdit"};
      return false;
    default:
      error_ = Error{ErrorCode::kUnsupportedVersion,
                     "état de code PIN inconnu : " + std::to_string(state)};
      return false;
  }
}

proto::CharacterInfo* CharacterPhase::Seated(std::uint32_t character_id) noexcept {
  for (proto::CharacterInfo& who : characters_) {
    if (who.GID == character_id) {
      return &who;
    }
  }
  return nullptr;
}

int IndexOfSlot(std::span<const proto::CharacterInfo> characters, std::uint8_t slot) noexcept {
  for (std::size_t index = 0; index < characters.size(); ++index) {
    if (characters[index].CharNum == slot) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

PhaseState CharacterPhase::Advance() {
  if (state_ != PhaseState::kWaiting) {
    return state_;
  }

  Connection::Polled polled = link_.Poll();
  if (polled.raw.size() == kBareAccountIdSize) {
    core::ByteReader reader(polled.raw);
    account_id_ = reader.U32();
    step_ = Step::kAwaitingCharacterList;
  }

  for (const Frame& frame : polled.frames) {
    if (frame.opcode == proto::HcAcceptEnter::kOpcode) {
      proto::HcAcceptEnter list;
      if (!proto::Decode(frame.bytes, list)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      characters_ = list.characters;
      // ⚠ ZÉRO RESTE ZÉRO : c'est « pas programmée », et non « mûre à
      // l'instant ».
      for (proto::CharacterInfo& who : characters_) {
        if (who.DelRevDate != 0) {
          who.DelRevDate = DeleteDeadlineFrom(who.DelRevDate);
        }
      }
      list_received_ = true;
      step_ = Step::kAwaitingSelection;
      continue;
    }
    if (frame.opcode == proto::HcDeleteChar3Reserved::kOpcode) {
      proto::HcDeleteChar3Reserved reserved;
      if (!proto::Decode(frame.bytes, reserved)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      if (reserved.result == kDeleteReserveAccepted) {
        // 🔴 L'ÉCHÉANCE SE POSE ICI, dans notre copie de la liste : le serveur
        // ne la renvoie PAS. Il répond par un verdict et un délai, et le
        // personnage garde en base son `delete_date` — que seule une nouvelle
        // liste montrerait, et il n'en envoie pas.
        if (proto::CharacterInfo* who = Seated(reserved.CID); who != nullptr) {
          who->DelRevDate = DeleteDeadlineFrom(static_cast<std::int32_t>(reserved.date));
        }
        deletion_ = DeletionState::kDone;
      } else {
        deletion_ = DeletionState::kRefused;
        deletion_refusal_text_ = DeleteReserveRefusalText(reserved.result);
      }
      continue;
    }
    if (frame.opcode == proto::HcDeleteChar3Cancel::kOpcode) {
      proto::HcDeleteChar3Cancel cancelled;
      if (!proto::Decode(frame.bytes, cancelled)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      if (cancelled.result == kDeleteCancelAccepted) {
        if (proto::CharacterInfo* who = Seated(cancelled.CID); who != nullptr) {
          who->DelRevDate = 0;
        }
        deletion_ = DeletionState::kDone;
      } else {
        deletion_ = DeletionState::kRefused;
        deletion_refusal_text_ = DeleteCancelRefusalText(cancelled.result);
      }
      continue;
    }
    if (frame.opcode == proto::HcAcceptDeletechar::kOpcode) {
      // 🔴 CETTE RÉPONSE EST VIDE : deux octets, pas d'identifiant. C'est à
      // nous de savoir QUI l'on vient de supprimer — d'où
      // `deletion_character_id_` — et de le retirer de la liste, que le
      // serveur ne renvoie pas davantage qu'aux deux autres étapes.
      std::erase_if(characters_, [this](const proto::CharacterInfo& who) {
        return who.GID == deletion_character_id_;
      });
      deletion_ = DeletionState::kDone;
      continue;
    }
    if (frame.opcode == proto::HcRefuseDeletechar::kOpcode) {
      proto::HcRefuseDeletechar refused;
      if (!proto::Decode(frame.bytes, refused)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      deletion_ = DeletionState::kRefused;
      deletion_refusal_text_ = DeleteRefusalText(refused.error);
      continue;
    }
    if (TookAmendAnswer(frame)) {
      if (state_ == PhaseState::kFailed) {
        return state_;
      }
      continue;
    }
    if (frame.opcode == proto::HcAcceptMakechar::kOpcode) {
      proto::HcAcceptMakechar made;
      if (!proto::Decode(frame.bytes, made)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      // 🔴 LE SERVEUR NE RENVOIE PAS LA LISTE, il envoie le personnage SEUL.
      // Sans cet ajout la tablée resterait telle qu'elle était, et le joueur
      // croirait sa création perdue — puis en relancerait une, que le serveur
      // refuserait pour un emplacement déjà pris.
      created_slot_ = made.character.CharNum;
      characters_.push_back(std::move(made.character));
      creation_ = CreationState::kCreated;
      continue;
    }
    if (frame.opcode == proto::HcRefuseMakechar::kOpcode) {
      proto::HcRefuseMakechar refused;
      if (!proto::Decode(frame.bytes, refused)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      // 🔴 CE REFUS NE MET PAS FIN À LA PHASE. Le joueur reste devant sa
      // tablée et essaie un autre nom ; couper la liaison le déconnecterait
      // pour une faute de frappe. Voir `CreationState`.
      creation_refusal_code_ = refused.error;
      creation_refusal_text_ = MakeCharRefusalText(refused.error);
      creation_ = CreationState::kRefused;
      continue;
    }
    if (frame.opcode == proto::HcAcceptEnter2::kOpcode) {
      proto::HcAcceptEnter2 room;
      if (!proto::Decode(frame.bytes, room)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      // ⚠ IL PRÉCÈDE LA LISTE, et c'est le seul paquet qui dise combien
      // d'emplacements ce compte peut REMPLIR — la liste, elle, ne porte que
      // ceux qui le sont déjà.
      producible_slots_ = room.producible;
      total_slots_ = room.total;
      continue;
    }
    if (frame.opcode == proto::HcNotifyZonesvr::kOpcode) {
      proto::HcNotifyZonesvr handoff;
      if (!proto::Decode(frame.bytes, handoff)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      handoff_.character_id = handoff.CID;
      handoff_.map_name = MapBaseName(handoff.mapname);
      handoff_.address = ServerAddress{handoff.ip, handoff.port};
      step_ = Step::kDone;
      state_ = PhaseState::kDone;
      return state_;
    }
    if (frame.opcode == proto::HcRefuseEnter::kOpcode) {
      proto::HcRefuseEnter refused;
      if (!proto::Decode(frame.bytes, refused)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      refusal_code_ = refused.error;
      state_ = PhaseState::kRefused;
      return state_;
    }
    if (frame.opcode == proto::HcNotifyAccessibleMapname::kOpcode) {
      // Aucun serveur de carte n'est prêt : la liaison reste ouverte et le
      // joueur peut redemander. Sans cette réponse lue, l'attente de l'adresse
      // de zone ne finirait jamais.
      if (step_ == Step::kAwaitingZoneAddress) {
        step_ = Step::kAwaitingSelection;
        zone_unavailable_ = true;
      }
      continue;
    }
    if (TookBanNotice(frame, refusal_code_, refusal_text_)) {
      state_ = PhaseState::kRefused;
      return state_;
    }
    if (frame.opcode == proto::HcSecondPasswdLogin::kOpcode) {
      proto::HcSecondPasswdLogin pincode;
      if (!proto::Decode(frame.bytes, pincode)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      pincode_seed_ = pincode.seed;
      if (!HandlePincodeState(pincode.result)) {
        state_ = PhaseState::kFailed;
        return state_;
      }
      continue;
    }
    // Le reste est ignoré : la notification de liste et les paquets d'agrément
    // n'apprennent rien à cette phase.
  }

  state_ = LinkVerdict(polled.status, link_, error_);
  return state_;
}

std::optional<std::size_t> ZoneStreamPreamble(std::span<const std::uint8_t> head) {
  if (head.size() < kBareAccountIdSize + kOpcodeBytes) {
    return std::nullopt;
  }
  const auto opcode_after_id = static_cast<std::uint16_t>(
      head[kBareAccountIdSize] | (static_cast<std::uint16_t>(head[kBareAccountIdSize + 1]) << 8));
  return opcode_after_id == proto::ZcAcceptEnter::kOpcode ? kBareAccountIdSize : 0;
}

bool MapPhase::DecideBareAccountId() {
  const std::optional<std::size_t> preamble = ZoneStreamPreamble(link_.pending_bytes());
  if (!preamble.has_value()) {
    // Pas de quoi trancher. ATTENDRE : découper maintenant lirait un opcode
    // dans un numéro de compte, et le flux serait perdu.
    return false;
  }
  if (*preamble != 0) {
    link_.ExpectRaw(*preamble);
  }
  return true;
}

core::Result<MapPhase> MapPhase::Start(const CharacterPhase::Handoff& handoff,
                                       const LoginPhase::Account& account,
                                       std::uint32_t client_tick) {
  proto::EnterMapRequest request;
  request.account_id = account.account_id;
  request.character_id = handoff.character_id;
  request.login_id1 = account.login_id1;
  request.client_tick = client_tick;
  request.sex = account.sex;

  // La demande est encodée AVANT d'ouvrir la liaison : son encodage recoupe sa
  // disposition avec ce que le serveur déclare lire, et un désaccord vaut mieux
  // découvert avant d'avoir ouvert un socket.
  auto frame = proto::Encode(request);
  if (!frame.has_value()) {
    return std::unexpected(frame.error());
  }

  auto link = Connection::Open(handoff.address.host(), handoff.address.port);
  if (!link.has_value()) {
    return std::unexpected(link.error());
  }

  MapPhase phase(std::move(*link));
  phase.link_.Send(*frame);
  // ⚠ Contrairement au serveur de personnages, celui de zone ne fait PAS
  // toujours précéder sa réponse d'un identifiant nu : il refuse AVANT de
  // l'envoyer. On ne peut donc pas le réserver d'avance.
  return phase;
}

PhaseState MapPhase::Advance() {
  if (state_ != PhaseState::kWaiting) {
    return state_;
  }

  // Recevoir, REGARDER, puis découper. C'est le seul endroit du client où
  // l'ordre compte : décider si les premiers octets portent une trame demande
  // de les voir avant que le découpeur ne s'en saisisse.
  link_.Receive();
  if (step_ == Step::kAwaitingAccountId && !DecideBareAccountId()) {
    // Trop tôt pour savoir si ces octets portent une trame. On ne découpe rien
    // et on repassera : les octets restent dans le tampon.
    state_ = LinkVerdict(link_.received_status(), link_, error_);
    return state_;
  }

  Connection::Polled polled = link_.Extract();
  if (polled.raw.size() == kBareAccountIdSize) {
    core::ByteReader reader(polled.raw);
    account_id_ = reader.U32();
    step_ = Step::kAwaitingAccept;
  }

  for (std::size_t rank = 0; rank < polled.frames.size(); ++rank) {
    Frame& frame = polled.frames[rank];
    if (frame.opcode == proto::ZcAcceptEnter::kOpcode) {
      proto::ZcAcceptEnter accepted;
      if (!proto::Decode(frame.bytes, accepted)) {
        error_ = UndecodableFrame(frame.opcode);
        state_ = PhaseState::kFailed;
        return state_;
      }
      spawn_.start_tick = accepted.startTime;
      spawn_.packed_position = accepted.posDir;

      // Sans cet avis, le serveur n'envoie jamais le monde : ni les unités, ni
      // l'état du personnage. Deux octets qui décident de tout le reste.
      auto loaded = proto::Encode(proto::MapLoadedNotice());
      if (!loaded.has_value()) {
        error_ = loaded.error();
        state_ = PhaseState::kFailed;
        return state_;
      }
      link_.Send(*loaded);
      // 🔴 CE QUI ENTOURE L'ACCORD REVIENT AU JEU : la liste d'amis, entre
      // autres, le suit dans la même foulée, et le serveur ne la renvoie plus.
      held_.insert(
          held_.end(),
          std::make_move_iterator(polled.frames.begin() + static_cast<std::ptrdiff_t>(rank) + 1),
          std::make_move_iterator(polled.frames.end()));
      link_.Requeue(std::move(held_));
      held_.clear();
      step_ = Step::kInGame;
      state_ = PhaseState::kDone;
      return state_;
    }
    if (frame.opcode == kMapRefusalOpcode) {
      // Le serveur de zone refuse avec la forme ANCIENNE du refus : opcode puis
      // un octet de code, dans une trame que sa table déclare bien plus longue.
      core::ByteReader reader(frame.bytes);
      reader.Skip(kOpcodeBytes);
      refusal_code_ = reader.U8();
      state_ = PhaseState::kRefused;
      return state_;
    }
    if (TookBanNotice(frame, refusal_code_, refusal_text_)) {
      state_ = PhaseState::kRefused;
      return state_;
    }
    // Le reste attend l'accord : le serveur envoie des paquets d'état avant
    // lui, et ils reviennent au jeu une fois l'entrée acceptée.
    held_.push_back(std::move(frame));
  }

  state_ = LinkVerdict(polled.status, link_, error_);
  return state_;
}

}  // namespace ro::net
