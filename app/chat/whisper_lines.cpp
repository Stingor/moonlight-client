#include "chat/whisper_lines.h"

#include <array>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/vfs/whisper_words.h"

namespace moonclient {
namespace {

using ro::sim::WhisperRelation;
using ro::sim::WhisperSide;

// Les pièces des en-têtes du fil, telles que le client d'origine les écrit.
constexpr std::string_view kFriendOpen = "[ Friend ";
constexpr std::string_view kMemberOpen = "[ Member ";
constexpr std::string_view kStaffOpen = "[ ";
constexpr std::string_view kFromOpen = "( From ";
constexpr std::string_view kToOpen = "( To ";
constexpr std::string_view kToBracketOpen = "[ To ";
constexpr std::string_view kBracketClose = " ] : ";
constexpr std::string_view kParenClose = " ) : ";
// La conversation, et le refus : « Nom : », « (Nom) ».
constexpr std::string_view kSpeakerSeparator = " : ";
constexpr std::string_view kRefusedOpen = "(";
constexpr std::string_view kRefusedClose = ") ";

// LES FORMES DU FIL, telles qu'une ligne relue se reconnaît : celle du membre
// de l'équipe, la moins marquée, après les crochets qui annoncent la leur.
struct LogHeadForm {
  std::string_view open;
  std::string_view close;
  WhisperSide side = WhisperSide::kNone;
  WhisperRelation relation = WhisperRelation::kStranger;
};
constexpr std::array<LogHeadForm, 6> kLogHeadForms = {{
    {kFriendOpen, kBracketClose, WhisperSide::kReceived, WhisperRelation::kFriend},
    {kMemberOpen, kBracketClose, WhisperSide::kReceived, WhisperRelation::kGuildMember},
    {kToBracketOpen, kBracketClose, WhisperSide::kSent, WhisperRelation::kFriend},
    {kStaffOpen, kBracketClose, WhisperSide::kReceived, WhisperRelation::kStaff},
    {kFromOpen, kParenClose, WhisperSide::kReceived, WhisperRelation::kStranger},
    {kToOpen, kParenClose, WhisperSide::kSent, WhisperRelation::kStranger},
}};

// Un en-tête qui encadre le pseudo.
[[nodiscard]] WhisperHead Framed(std::string_view open, std::string_view name,
                                 std::string_view close) {
  WhisperHead head;
  head.text.reserve(open.size() + name.size() + close.size());
  head.text.append(open);
  head.name_offset = head.text.size();
  head.name_length = name.size();
  head.text.append(name);
  head.text.append(close);
  return head;
}

// L'EN-TÊTE D'UNE LIGNE REÇUE, dans le fil : la relation choisit, l'ami
// d'abord — c'est l'ordre où le client pose ses questions.
[[nodiscard]] WhisperHead ReceivedInLog(const ro::sim::WhisperFacts& facts) {
  switch (facts.relation) {
    case WhisperRelation::kFriend:
      return Framed(kFriendOpen, facts.peer_name, kBracketClose);
    case WhisperRelation::kStaff:
      return Framed(kStaffOpen, facts.peer_name, kBracketClose);
    case WhisperRelation::kGuildMember:
      return Framed(kMemberOpen, facts.peer_name, kBracketClose);
    case WhisperRelation::kStranger:
      break;
  }
  return Framed(kFromOpen, facts.peer_name, kParenClose);
}

// L'ÉCHO DE NOTRE ENVOI, dans le fil : des crochets pour un ami ou un membre de
// la guilde, des parenthèses pour les autres.
[[nodiscard]] WhisperHead SentInLog(const ro::sim::WhisperFacts& facts) {
  const bool bracketed =
      facts.relation == WhisperRelation::kFriend || facts.relation == WhisperRelation::kGuildMember;
  return bracketed ? Framed(kToBracketOpen, facts.peer_name, kBracketClose)
                   : Framed(kToOpen, facts.peer_name, kParenClose);
}

// L'en-tête, avant qu'on décide si le pseudo se clique.
[[nodiscard]] WhisperHead HeadOf(const ro::sim::WhisperFacts& facts, WhisperSurface surface,
                                 std::string_view own_name) {
  switch (facts.side) {
    case WhisperSide::kNone:
      return {};
    case WhisperSide::kUndelivered:
      // Le refus se lit pareil partout : il nomme celui qu'on n'a pas atteint.
      if (facts.peer_name.empty()) {
        return {};
      }
      return Framed(kRefusedOpen, facts.peer_name, kRefusedClose);
    case WhisperSide::kReceived:
      if (surface == WhisperSurface::kConversation) {
        return Framed({}, facts.peer_name, kSpeakerSeparator);
      }
      return ReceivedInLog(facts);
    case WhisperSide::kSent:
      if (surface == WhisperSurface::kConversation) {
        // ⚠ NOTRE NOM N'EST PAS UN LIEN : on ne s'invite pas soi-même.
        WhisperHead head;
        head.text.append(own_name);
        head.text.append(kSpeakerSeparator);
        return head;
      }
      return SentInLog(facts);
  }
  return {};
}

}  // namespace

WhisperHead ComposeWhisperHead(const ro::sim::WhisperFacts& facts, WhisperSurface surface,
                               std::string_view own_name) {
  WhisperHead head = HeadOf(facts, surface, own_name);
  // ⚠ ON NE SE CHUCHOTE PAS À SOI-MÊME PAR UN LIEN : ce qu'on s'est chuchoté
  // nomme le joueur, et son menu ne proposerait que des gestes absurdes.
  if (!own_name.empty() && facts.peer_name == own_name) {
    head.name_length = 0;
  }
  return head;
}

std::string_view WhisperSaid(const ro::sim::ChatLine& line, const ro::vfs::ItemWords* words) {
  if (line.whisper.side != WhisperSide::kUndelivered) {
    return line.text;
  }
  std::string_view said;
  if (words != nullptr) {
    switch (line.whisper.refusal) {
      case ro::sim::WhisperRefusal::kOffline:
        said = ro::vfs::WhisperText(*words, ro::vfs::WhisperWord::kNotConnected);
        break;
      case ro::sim::WhisperRefusal::kIgnored:
        said = ro::vfs::WhisperText(*words, ro::vfs::WhisperWord::kIgnored);
        break;
      case ro::sim::WhisperRefusal::kIgnoresAll:
        said = ro::vfs::WhisperText(*words, ro::vfs::WhisperWord::kIgnoresAll);
        break;
      case ro::sim::WhisperRefusal::kNone:
      default:
        break;
    }
  }
  // ⚠ UN CODE QUE LE CLIENT NE CONNAÎT PAS SE DIT QUAND MÊME, comme un refus
  // avant que les mots ne soient posés : se taire laisserait le joueur devant
  // un chuchotement qui n'a visiblement rien fait.
  return said.empty() ? std::string_view(ro::core::Tr("Ce chuchotement n'est pas parvenu.")) : said;
}

std::string WhisperLogText(const ro::sim::ChatLine& line, const ro::vfs::ItemWords* words) {
  if (line.whisper.side == WhisperSide::kNone) {
    return line.text;
  }
  // ⚠ SANS NOTRE NOM : la forme du fil ne le porte pas.
  WhisperHead head = ComposeWhisperHead(line.whisper, WhisperSurface::kLog, {});
  head.text.append(WhisperSaid(line, words));
  return std::move(head.text);
}

std::string StaffBalloonText(const ro::sim::ChatLine& line, bool shown_apart) {
  ro::sim::WhisperFacts facts = line.whisper;
  // ⚠ PRISE PAR UNE CONVERSATION, LA LIGNE N'A PAS D'EN-TÊTE DE FIL : la bulle
  // garde la forme de l'équipe, même pour un ami.
  if (shown_apart) {
    facts.relation = WhisperRelation::kStaff;
  }
  WhisperHead head = ComposeWhisperHead(facts, WhisperSurface::kLog, {});
  head.text.append(line.text);
  return std::move(head.text);
}

ro::sim::WhisperFacts WhisperFactsOfLogText(std::string_view text) {
  for (const LogHeadForm& form : kLogHeadForms) {
    if (!text.starts_with(form.open)) {
      continue;
    }
    const std::size_t close = text.find(form.close, form.open.size());
    if (close == std::string_view::npos || close == form.open.size()) {
      continue;
    }
    return ro::sim::WhisperFacts{
        .side = form.side,
        .peer_name = std::string(text.substr(form.open.size(), close - form.open.size())),
        .relation = form.relation};
  }
  return {};
}

std::uint32_t ConversationColor(WhisperSide side) {
  switch (side) {
    case WhisperSide::kSent:
      return kConversationSentColor;
    case WhisperSide::kUndelivered:
      return kConversationUndeliveredColor;
    case WhisperSide::kNone:
    case WhisperSide::kReceived:
      break;
  }
  return kConversationReceivedColor;
}

}  // namespace moonclient
