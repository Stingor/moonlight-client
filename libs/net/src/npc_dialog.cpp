#include "ro/net/npc_dialog.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/npc_dialog.h"
#include "ro/sim/world.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

using core::ErrorCode;

core::Error NoDialog() {
  return core::Error{ErrorCode::kInvalidArgument, "aucun dialogue de PNJ n'est ouvert"};
}

// LE MOT QUI DÉLIE UN SCRIPT, dans ses deux formes.
//
// 🔴 UN SCRIPT ARRÊTÉ SUR UN `select` N'ATTEND PAS UNE FERMETURE, IL ATTEND UN
// RANG. Lui envoyer CZ_CLOSE_DIALOG ne le termine pas : `sd->npc_id` reste posé,
// et le personnage ne lance plus aucune compétence jusqu'au changement de carte.
// C'est l'annulation convenue qui le débloque.
//
// ⚠ LES DEUX SORTIES PASSENT PAR ICI — le bouton « fermer » et le changement de
// carte —, et c'est ce qui garantit qu'elles choisissent pareil. Une seconde
// copie aurait divergé à la première correction.
core::Result<void> SendLeave(Connection& link, std::uint32_t npc_id, bool menu_pending) {
  if (menu_pending) {
    proto::NpcMenuChoice choice;
    choice.npc_id = npc_id;
    choice.choice = sim::kNpcMenuCancelled;
    return SendEncoded(link, proto::Encode(choice));
  }
  proto::CzCloseDialog packet;
  packet.GID = npc_id;
  link.Send(proto::Encode(packet));
  return {};
}

// Le « fermer » du serveur, dans ses DEUX branches.
//
// ⚠ IL NE FERME QUE SI RIEN N'EST ÉCRIT, ET C'EST LE CLIENT QUI EN DÉCIDE. Le
// serveur documente les deux au-dessus de `clif_scriptclose` : si la fenêtre de
// TEXTE est ouverte, retirer le « suivant » et poser un « fermer » ; SINON
// fermer tout, liste de choix comprise.
//
// Un agent de téléportation tombe dans la seconde branche — il n'offre qu'un
// menu, sans une ligne de texte —, et c'est précisément le cas où sa fenêtre
// restait ouverte après le voyage.
//
// ⚠ ET CE N'EST PAS LE CHANGEMENT DE CARTE QUI FERME. Le serveur garde le script
// vivant à travers un `warp` — `pc_setpos` ne touche pas à `npc_id` —, parce
// qu'un script peut téléporter puis continuer à parler.
FrameOutcome CloseOrOfferButton(sim::World& world, std::uint32_t npc_id, std::uint32_t now_ms) {
  if (!world.npc_dialog().has_value() || world.npc_dialog()->npc_id != npc_id) {
    return FrameOutcome::kIgnored;
  }
  if (!world.PromptNpcDialog(npc_id, sim::NpcPrompt::kClose, now_ms)) {
    return FrameOutcome::kIgnored;
  }
  // La page vient d'être publiée par le « fermer » lui-même : c'est APRÈS cette
  // publication qu'on sait s'il reste quelque chose d'écrit.
  if (world.npc_dialog()->lines.empty()) {
    world.CloseNpcDialog();
  }
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyNpcDialogFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcSayDialog::kOpcode:
      return Decoded<proto::ZcSayDialog>(frame, [&world, now_ms](const auto& packet) {
        // Le texte vient du JEU, donc de sa page de codes.
        world.SayNpcDialog(packet.NpcID, core::Utf8FromCp1252(packet.message), now_ms);
        return FrameOutcome::kApplied;
      });

    // ⚠ LE DIALOGUE SECONDAIRE PARLE PAREIL. Son seul champ de plus désigne la
    // fenêtre où le serveur voudrait écrire ; nous n'en tenons qu'une, et la
    // distinguer montrerait deux cadres pour une seule conversation.
    case proto::ZcSayDialog2::kOpcode:
      return Decoded<proto::ZcSayDialog2>(frame, [&world, now_ms](const auto& packet) {
        world.SayNpcDialog(packet.NpcID, core::Utf8FromCp1252(packet.message), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcWaitDialog::kOpcode:
      return Decoded<proto::ZcWaitDialog>(frame, [&world, now_ms](const auto& packet) {
        return world.PromptNpcDialog(packet.NpcID, sim::NpcPrompt::kNext, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcWaitDialog2::kOpcode:
      return Decoded<proto::ZcWaitDialog2>(frame, [&world, now_ms](const auto& packet) {
        return world.PromptNpcDialog(packet.NpcID, sim::NpcPrompt::kNext, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcCloseDialog::kOpcode:
      return Decoded<proto::ZcCloseDialog>(frame, [&world, now_ms](const auto& packet) {
        return CloseOrOfferButton(world, packet.npcId, now_ms);
      });

    case proto::ZcClearDialog::kOpcode:
      return Decoded<proto::ZcClearDialog>(frame, [&world, now_ms](const auto& packet) {
        if (!world.npc_dialog().has_value() || world.npc_dialog()->npc_id != packet.GID) {
          return FrameOutcome::kIgnored;
        }
        // 🔴 UN `clear` QUI SUIT UN « fermer » N'EST PAS CELUI D'UN SCRIPT :
        // c'est le SERVEUR qui met fin à la session. Son minuteur d'inactivité
        // envoie 0x00b6 puis 0x08d6, parce que 0x00b6 seul se contente
        // d'échanger le bouton et laisse la LISTE DE CHOIX à l'écran — le joueur
        // revenu de son absence cliquait dedans, sur un script déjà mort, et sa
        // réponse partait sur la session suivante. Donc l'éjection.
        //
        // La séquence ne peut pas naître d'un flux normal : `close;` termine le
        // script, rien n'est émis derrière, et le `clear;` d'une conversation
        // suivante arrive après le clic du joueur — clic qui a déjà refermé
        // celle-ci.
        if (world.npc_dialog()->prompt == sim::NpcPrompt::kClose) {
          world.CloseNpcDialog();
          return FrameOutcome::kApplied;
        }
        return world.ClearNpcDialogText(packet.GID, now_ms) ? FrameOutcome::kApplied
                                                            : FrameOutcome::kIgnored;
      });

    case proto::ZcMenuList::kOpcode:
      return Decoded<proto::ZcMenuList>(frame, [&world, now_ms](const auto& packet) {
        // ⚠ LE CORPS EST UNE CHAÎNE C, ET SON NUL EN FAIT PARTIE : le serveur
        // copie `strlen + 1` octets. Sans troncature, une dernière option vide
        // devient « \0 », donc non vide, et s'affiche en ligne blanche à laquelle
        // un rang est attribué.
        std::string menu = core::Utf8FromCp1252(packet.menu);
        if (const std::size_t nul = menu.find('\0'); nul != std::string::npos) {
          menu.resize(nul);
        }
        world.OfferNpcMenu(packet.npcId, sim::SplitNpcMenu(menu), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::ZcOpenEditdlg::kOpcode:
      return Decoded<proto::ZcOpenEditdlg>(frame, [&world, now_ms](const auto& packet) {
        // ⚠ UNE SAISIE OUVRE UN DIALOGUE À ELLE SEULE. Un script peut appeler
        // `input` sans avoir rien dit, et le client officiel montre alors un
        // champ sans texte au-dessus.
        if (!world.npc_dialog().has_value() || world.npc_dialog()->npc_id != packet.npcId) {
          world.SayNpcDialog(packet.npcId, std::string(), now_ms);
        }
        return world.PromptNpcDialog(packet.npcId, sim::NpcPrompt::kNumber, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    case proto::ZcOpenEditdlgstr::kOpcode:
      return Decoded<proto::ZcOpenEditdlgstr>(frame, [&world, now_ms](const auto& packet) {
        if (!world.npc_dialog().has_value() || world.npc_dialog()->npc_id != packet.npcId) {
          world.SayNpcDialog(packet.npcId, std::string(), now_ms);
        }
        return world.PromptNpcDialog(packet.npcId, sim::NpcPrompt::kText, now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> ContactNpc(Connection& link, std::uint32_t npc_id) {
  proto::CzContactnpc packet;
  packet.AID = npc_id;
  // Le serveur ne LIT PAS ce champ — `clif_parse_NpcClicked` n'en tire rien —,
  // et il n'y a donc rien à y mettre de significatif.
  packet.type = 0;
  link.Send(proto::Encode(packet));
  return {};
}

core::Result<void> AnswerNpcNext(Connection& link, sim::World& world) {
  if (!world.npc_dialog().has_value()) {
    return std::unexpected(NoDialog());
  }
  if (world.npc_dialog()->awaiting_reply) {
    return std::unexpected(core::Error{ErrorCode::kInvalidArgument,
                                       "une reponse est deja partie : le script avancerait deux "
                                       "fois"});
  }
  proto::NpcNextRequest request;
  request.npc_id = world.npc_dialog()->npc_id;
  if (auto sent = SendEncoded(link, proto::Encode(request)); !sent) {
    return sent;
  }
  world.AdvanceNpcDialog();
  return {};
}

core::Result<void> AnswerNpcMenu(Connection& link, sim::World& world, std::size_t option_index) {
  if (!world.npc_dialog().has_value()) {
    return std::unexpected(NoDialog());
  }
  const sim::NpcDialog& dialog = *world.npc_dialog();
  // ⚠ UNE LISTE NE SE RÉPOND QU'UNE FOIS. Le premier choix fait avancer le
  // script ; le second vise une liste qui n'a plus la même longueur, et un rang
  // au-delà de ce que le serveur compte fait ÉJECTER le joueur.
  if (dialog.prompt != sim::NpcPrompt::kMenu || dialog.menu_answered) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument, "aucun menu de PNJ n'attend de reponse"});
  }
  // 🔴 LE RANG N'EST PAS LA POSITION : voir `sim::NpcMenuOption`. Une entrée vide
  // du script ne compte pas pour le serveur, et lui envoyer la position absolue
  // dépasse son compte — donc l'éjection.
  const std::uint8_t rank = dialog.RankAt(option_index);
  if (rank == 0) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument,
                    "rang hors du menu : le serveur ejecterait le joueur plutot que de refuser"});
  }
  proto::NpcMenuChoice choice;
  choice.npc_id = dialog.npc_id;
  choice.choice = rank;
  if (auto sent = SendEncoded(link, proto::Encode(choice)); !sent) {
    return sent;
  }
  world.AnswerNpcMenu();
  return {};
}

core::Result<void> AnswerNpcNumber(Connection& link, sim::World& world, std::int32_t value) {
  if (!world.npc_dialog().has_value()) {
    return std::unexpected(NoDialog());
  }
  const sim::NpcDialog& dialog = *world.npc_dialog();
  if (dialog.prompt != sim::NpcPrompt::kNumber || dialog.awaiting_reply) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument, "aucun nombre n'est demande par le PNJ"});
  }
  proto::CzInputEditdlg packet;
  packet.GID = dialog.npc_id;
  packet.value = value;
  link.Send(proto::Encode(packet));
  world.SubmitNpcInput();
  return {};
}

core::Result<void> AnswerNpcText(Connection& link, sim::World& world, std::string_view text) {
  if (!world.npc_dialog().has_value()) {
    return std::unexpected(NoDialog());
  }
  const sim::NpcDialog& dialog = *world.npc_dialog();
  if (dialog.prompt != sim::NpcPrompt::kText || dialog.awaiting_reply) {
    return std::unexpected(
        core::Error{ErrorCode::kInvalidArgument, "aucun texte n'est demande par le PNJ"});
  }
  proto::CzInputEditdlgstr packet;
  packet.GID = static_cast<std::int32_t>(dialog.npc_id);
  // ⚠ LE TEXTE REPART DANS LA PAGE DE CODES DU JEU : ce qui est tapé ici est de
  // l'UTF-8, et le serveur range l'octet tel quel dans une variable de script.
  packet.value = core::Cp1252FromUtf8(text);
  if (packet.value.size() > kNpcTextAnswerMaxBytes) {
    packet.value.resize(kNpcTextAnswerMaxBytes);
  }
  // 🔴 LE NUL FINAL EST CE QUI REND UNE SAISIE VIDE EXPRIMABLE.
  // `clif_parse_NpcStringInput` calcule la longueur du corps et RETOURNE sans
  // rien faire si elle est nulle : un champ validé à vide laisserait le script
  // suspendu pour toujours. Un octet suffit à le distinguer d'un corps absent.
  packet.value.push_back('\0');
  link.Send(proto::Encode(packet));
  world.SubmitNpcInput();
  return {};
}

core::Result<void> LeaveNpcDialog(Connection& link, sim::World& world) {
  if (!world.npc_dialog().has_value()) {
    return std::unexpected(NoDialog());
  }
  if (auto sent = SayNpcFarewell(link, *NpcFarewellDue(world)); !sent) {
    return sent;
  }
  world.CloseNpcDialog();
  return {};
}

std::optional<NpcFarewell> NpcFarewellDue(const sim::World& world) {
  if (!world.npc_dialog().has_value()) {
    return std::nullopt;
  }
  const sim::NpcDialog& dialog = *world.npc_dialog();
  return NpcFarewell{
      .npc_id = dialog.npc_id,
      .menu_pending = dialog.prompt == sim::NpcPrompt::kMenu && !dialog.menu_answered};
}

core::Result<void> SayNpcFarewell(Connection& link, const NpcFarewell& due) {
  return SendLeave(link, due.npc_id, due.menu_pending);
}

}  // namespace ro::net
