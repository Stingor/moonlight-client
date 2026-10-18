// L'étage du carnet de chasse MVP : les commandes envoyées, les résultats que
// le serveur leur rend, et ce qu'on en dit dans le chat.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/mvp_tracker.h"
#include "ro/sim/mvp_tracker.h"
#include "ro/ui/mvp_tracker_window.h"

#include "mvp/mvp_tracker_stage.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::net::MvpCommand;

constexpr auto kResultOk = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kOk);
constexpr auto kResultNoInvite = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kNoInvite);
constexpr auto kResultObservationKept =
    static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kObservationKept);

// LE SERVEUR RÉPOND À CHAQUE COMMANDE, succès compris, sauf à l'instantané et
// au favori, qui n'en reçoivent aucun.
[[nodiscard]] bool GetsResult(MvpCommand command) {
  return command != MvpCommand::kSnapshot && command != MvpCommand::kFavorite;
}

// La phrase d'un refus, déjà traduite ; un code inconnu se dit par son numéro.
[[nodiscard]] std::string RefusalText(std::uint8_t code) {
  if (const char* known = ro::ui::MvpResultText(code); known != nullptr) {
    return known;
  }
  return ro::core::Printed(Tr("Refusé par le serveur (code %u)."), static_cast<unsigned>(code));
}

// LA LIGNE DU CHAT d'une invitation ou d'une saisie.
//
// ⚠ UNE SAISIE ACCEPTÉE N'EST « AJOUTÉE AU CARNET » QUE D'UN SERVEUR QUI PARLE
// LA FORME ÉTENDUE À CETTE SESSION (`extended`) : il dit aussi la saisie sans
// effet. L'autre répond OK même à celle qu'il ignore, faute de précision, et
// cela même quand il étiquette le résultat : elle n'y est que « transmise ».
// Sans effet, rien n'a été refusé : la ligne le dit tel quel.
[[nodiscard]] std::string NoticeOf(MvpCommand command, std::string_view subject, std::uint8_t code,
                                   bool extended) {
  const std::string name(subject);
  std::string text;
  if (code == kResultOk) {
    if (command == MvpCommand::kInvite) {
      text = ro::core::Printed(Tr("Carnet MVP : invitation envoyée à %s."), name.c_str());
    } else if (extended) {
      text = ro::core::Printed(Tr("Carnet MVP : heure de mort de %s ajoutée au carnet du groupe."),
                               name.c_str());
    } else {
      text = ro::core::Printed(Tr("Carnet MVP : heure de mort de %s transmise au groupe."),
                               name.c_str());
    }
    return text;
  }
  if (code == kResultObservationKept && command == MvpCommand::kReportManual) {
    return ro::core::Printed(
        Tr("Carnet MVP : saisie de %s sans effet — le groupe tient déjà une heure au "
           "moins aussi précise."),
        name.c_str());
  }
  const std::string reason = RefusalText(code);
  if (command == MvpCommand::kInvite) {
    text = ro::core::Printed(Tr("Carnet MVP : invitation à %s refusée — %s"), name.c_str(),
                             reason.c_str());
  } else {
    text = ro::core::Printed(Tr("Carnet MVP : saisie de %s refusée — %s"), name.c_str(),
                             reason.c_str());
  }
  return text;
}

}  // namespace

bool MvpTrackerStage::Send(ro::net::Connection& link, MvpCommand command, std::uint32_t a,
                           std::uint32_t b, std::string_view text_utf8, std::string subject) {
  // 🔴 HORS VÉRIFICATION, LE SERVEUR JETTE LA COMMANDE SANS RÉPONDRE : partie,
  // elle attendrait dans la file une réponse qui ne viendra jamais. Et carnet
  // éteint, rien ne part.
  if (!verified_ || !enabled_) {
    return false;
  }
  // TOUTE COMMANDE PORTE SON IDENTIFIANT, jamais nul — zéro dit « aucun » sur
  // le fil. Le serveur qui le comprend le rend avec le résultat ; l'autre ne le
  // voit pas.
  ++last_request_id_;
  if (last_request_id_ == 0) {
    ++last_request_id_;
  }
  ro::net::RequestMvpCommand(link, command, a, b, text_utf8, last_request_id_);
  if (!GetsResult(command)) {
    return true;
  }
  // Bornée, la file oublie la plus ancienne ; datée, elle oublie celle dont la
  // réponse tarde au-delà de la patience — voir `SettleResult`.
  if (awaited_.size() >= kMvpMostAwaitedResults) {
    awaited_.pop_front();
  }
  awaited_.push_back(AwaitedResult{.command = command,
                                   .subject = std::move(subject),
                                   .closed_at_send = !window_shown_,
                                   .sent_ms = now_ms_,
                                   .request_id = last_request_id_});
  return true;
}

void MvpTrackerStage::SettleResult(std::uint8_t code, std::uint32_t now_ms,
                                   const std::optional<ro::sim::MvpResultTag>& tag) {
  result_in_modal_ = false;
  // UNE RÉPONSE PERDUE NE DÉCALE PAS LES SUIVANTES : la commande qui l'attend
  // depuis plus que `kMvpLostAnswerMs` n'en recevra plus, et sort de la file
  // avant l'appariement.
  while (!awaited_.empty() && now_ms - awaited_.front().sent_ms > kMvpLostAnswerMs) {
    awaited_.pop_front();
  }
  // Une réponse que rien n'attend — une file vidée par une nouvelle partie ou
  // une liaison perdue — ne s'apparie à rien.
  if (awaited_.empty()) {
    return;
  }
  // 🔴 L'ÉTIQUETTE DIT LA COMMANDE. Le serveur répond dans l'ordre d'envoi :
  // celles parties avant elle n'auront plus de réponse, et sortent de la file.
  // Une étiquette qui ne désigne rien d'attendu ne s'apparie à rien ; un écho
  // de commande qui diffère se journalise, et le résultat retombe sur l'ordre
  // d'envoi.
  // Sans étiquette — un serveur qui ne la rend pas —, l'ordre d'envoi apparie.
  if (tag.has_value()) {
    const auto tagged = std::ranges::find(awaited_, tag->request_id, &AwaitedResult::request_id);
    if (tagged == awaited_.end()) {
      ro::core::LogInfo("carnet MVP : résultat {} de la commande {} que rien n'attend", code,
                        tag->request_id);
      return;
    }
    if (static_cast<std::uint8_t>(tagged->command) == tag->command) {
      awaited_.erase(awaited_.begin(), tagged);
    } else {
      ro::core::LogWarning(
          "carnet MVP : le résultat de la commande {} dit la commande {}, envoyée {} ; "
          "appariement par l'ordre d'envoi",
          tag->request_id, tag->command, static_cast<std::uint8_t>(tagged->command));
    }
  }
  const AwaitedResult settled = std::move(awaited_.front());
  awaited_.pop_front();
  const MvpCommand command = settled.command;

  // L'INVITATION ATTENDAIT CE RÉSULTAT : acceptée ou refusée, elle s'efface ;
  // refusée par le serveur, elle reste, avec la raison, que la modale dit. ⚠
  // AUCUNE TRAME NE L'EFFACE APRÈS UN SUCCÈS : c'est au client de le faire.
  if (command == MvpCommand::kAcceptInvite || command == MvpCommand::kDeclineInvite) {
    result_in_modal_ = true;
    invite_answer_sent_ = false;
    if (code == kResultOk) {
      book_.ClearInvitation();
      invite_refusal_ = 0;
      invite_gone_ = false;
      // Accepter, c'est vouloir voir ce que le groupe sait.
      if (command == MvpCommand::kAcceptInvite) {
        open_asked_ = true;
      }
    } else {
      invite_refusal_ = code;
      invite_gone_ = code == kResultNoInvite;
    }
    return;
  }

  // LE BANDEAU ROUGE NE SE VOIT QUE CARNET OUVERT : fermé au geste ou à la
  // réponse, le résultat se dit dans le chat.
  if ((command == MvpCommand::kInvite || command == MvpCommand::kReportManual) &&
      (settled.closed_at_send || !window_shown_)) {
    notices_.push_back(NoticeOf(command, settled.subject, code, book_.extensions_seen()));
  }
}

void MvpTrackerStage::NoteNotSent(MvpCommand command, std::string_view subject,
                                  const char* reason) {
  const std::string name(subject);
  std::string text;
  if (command == MvpCommand::kInvite) {
    text = ro::core::Printed(Tr("Carnet MVP : invitation à %s non envoyée — %s"), name.c_str(),
                             reason);
  } else {
    text =
        ro::core::Printed(Tr("Carnet MVP : saisie de %s non envoyée — %s"), name.c_str(), reason);
  }
  notices_.emplace_back(text);
}

void MvpTrackerStage::NoteGesturesNotSent(const char* reason) {
  notices_.emplace_back(ro::core::Printed(Tr("Carnet MVP : rien n'est parti — %s"), reason));
}

std::vector<std::string> MvpTrackerStage::TakeNotices() { return std::exchange(notices_, {}); }

}  // namespace moonclient
