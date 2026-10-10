#include "game/whisper_stage.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/chat_target.h"
#include "ro/net/friends.h"
#include "ro/net/guild.h"
#include "ro/net/party.h"
#include "ro/net/whisper.h"
#include "ro/net/whisper_blocks.h"

#include "chat/chat_line_preview.h"
#include "chat/chat_mvp_link.h"
#include "chat/chat_navi_search_link.h"
#include "game/interface_sound.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// LE TITRE DE LA FENÊTRE, celui du client : « With », deux espaces, le nom.
constexpr std::string_view kTitleOpen = "With  ";
// La guilde du correspondant, quand on la connaît, après deux espaces.
constexpr std::string_view kGuildOpen = "  [";
constexpr std::string_view kGuildClose = "]";
// LA MARQUE D'UN NON-LU, en tête du titre : la fenêtre a reçu sans être
// regardée.
constexpr std::string_view kUnreadMark = "* ";

// LA GUILDE DU CORRESPONDANT, si un joueur de ce nom est en vue et que le
// serveur nous a dit sous quelle bannière il marche. ⚠ AUCUNE QUESTION NE
// PART : le serveur ne répond qu'aux acteurs de la même carte, et le titre se
// passe très bien d'elle.
[[nodiscard]] std::string_view GuildOf(const ro::sim::World& world, std::string_view peer) {
  for (const auto& [id, actor] : world.actors()) {
    if (actor.kind == ro::sim::ActorKind::kPlayer && actor.name == peer && actor.card.has_value()) {
      return actor.card->banner;
    }
  }
  return {};
}

// 🔴 UNE PHRASE TROP LONGUE NE PART PAS, ici comme dans la barre : le serveur la
// jetterait sans un mot. Elle revient dans la saisie de sa fenêtre, ses liens
// toujours posés, pour être raccourcie, et le fil dit de combien elle dépasse.
// Rend vrai si elle est retenue ; sans personnage, la place est inconnue et rien
// ne se retient.
//
// 🔴 C'EST LA LIGNE QUI PART QUI SE MESURE, chaque libellé repris sous sa
// balise : c'est elle que le serveur compte, et un lien d'objet pèse bien plus
// que son nom.
[[nodiscard]] bool HoldOverlongWhisper(ro::ui::WhisperWindowState& state, const std::string& peer,
                                       const std::string& said, const std::string& outgoing,
                                       std::vector<PendingChatLink>* links,
                                       const WhisperContext& context) {
  const std::string_view speaker = context.world.player_name();
  if (speaker.empty()) {
    return false;
  }
  // ⚠ LA CIBLE EST FIGÉE : la place se mesure sur l'intention, sans relire de
  // sigil dans la phrase.
  const ChatWireRoom room = ChatIntentRoomOf(
      ro::net::ChatIntent{
          .target = ro::net::ChatTarget::kWhisper, .recipient = peer, .text = outgoing},
      ChatWireSpeaker{.name = speaker, .channels = context.world.chat_channels()});
  std::optional<std::string> held =
      HoldIfOverlong(state.input, said, room, /*speaker_known=*/true, links);
  if (!held.has_value()) {
    return false;
  }
  state.wants_focus = true;
  context.world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(*held), context.now_ms,
                           kHeldChatColor);
  return true;
}

// LES LIENS QU'UNE PHRASE PARTIE A EMPORTÉS quittent sa saisie : ceux que la
// résolution a pris, un à la fois et dans le même ordre (`ResolveChatLinks`).
//
// ⚠ PAS TOUS : un lien posé pendant que la phrase attendait son envoi est dans
// la saisie, pas dans la phrase, et il doit y rester.
void DropSentLinks(std::string_view said, std::vector<PendingChatLink>* posed) {
  std::vector<PendingChatLink> kept;
  std::size_t at = 0;
  for (PendingChatLink& link : *posed) {
    const std::size_t found =
        link.shown.empty() ? std::string_view::npos : said.find(link.shown, at);
    if (found != std::string_view::npos) {
      at = found + link.shown.size();
      continue;
    }
    kept.push_back(std::move(link));
  }
  posed->swap(kept);
}

// UNE PHRASE DE CE CORRESPONDANT A-T-ELLE ÉTÉ VALIDÉE SANS ÊTRE ENCORE PARTIE ?
// Elle part à l'image suivante, quand l'étape lit ce que les fenêtres ont rendu.
[[nodiscard]] bool AwaitsSending(const WhisperWindows& windows, std::string_view peer) {
  return std::ranges::any_of(windows.answers, [peer](const auto& entry) {
    return entry.first == peer && entry.second.said.has_value();
  });
}

// CE QUE LA SAISIE PORTERA, dépôt en attente compris : une ligne retenue pour sa
// longueur y revient par lui, avec ses liens.
[[nodiscard]] std::string CarriedBy(const ro::ui::ChatInputState& input) {
  std::string carried = input.text.data();
  carried += input.pending_insert;
  return carried;
}

// LA PASTILLE D'UN LIEN POSÉ. Son jeton est son rang parmi les liens de SA
// saisie ; le libellé est donné à part, recopié par l'appelant.
[[nodiscard]] ro::ui::ChatInputLink ChipOf(const PendingChatLink& posed, std::string_view label,
                                           std::size_t rank, const WhisperContext& context) {
  ro::ui::ChatInputLink chip{.label = label,
                             .kind = posed.kind,
                             .item_id = posed.item_id,
                             .token = static_cast<std::uint32_t>(rank)};
  // UN RESPAWN POSÉ SE RELIT DE SA BALISE : son menu sait ce que celui du fil
  // sait. Une recherche de même, qui sait si elle a un lieu.
  if (MvpLink read; posed.kind == ro::ui::ChatLinkKind::kMvp && context.judge_mvp &&
                    ParseMvpLink(posed.wire, &read, nullptr) == MvpLinkRead::kRead) {
    chip.mvp = context.judge_mvp(read);
  }
  if (NaviSearchLink read;
      posed.kind == ro::ui::ChatLinkKind::kNaviSearch &&
      ParseNaviSearchLink(posed.wire, &read, nullptr) == NaviSearchLinkRead::kRead) {
    chip.navi = NaviSearchFactsOf(read);
  }
  return chip;
}

}  // namespace

void ArmWhisper(ro::ui::ChatInputState& bar, std::string_view name) {
  if (name.empty()) {
    return;
  }
  std::array<char, ro::ui::kChatWhisperNameSize>& box = bar.whisper_to;
  box.fill('\0');
  std::copy_n(name.begin(), std::min(name.size(), box.size() - 1), box.begin());
  bar.unfolded = true;
  bar.focus_on_whisper = false;
  bar.wants_focus = true;
}

void PlayWhisperAlarm(ro::audio::AudioEngine& audio, ro::vfs::Vfs& vfs,
                      const std::filesystem::path& client_root, bool alarm) {
  if (!audio.sound_audible()) {
    return;
  }
  // L'ARRIVÉE D'UN MESSAGE D'ABORD ; le son de bouton seulement quand l'alarme
  // est demandée et que le premier n'a pas pu sonner. Un fichier d'arrivée
  // absent ne se journalise pas : seul un fichier présent mais illisible le
  // mérite.
  const ro::core::Result<void> messenger =
      PlayInterfaceSound(audio, vfs, client_root, MessengerSoundFile());
  if (messenger) {
    return;
  }
  if (messenger.error().code != ro::core::ErrorCode::kNotFound) {
    ro::core::LogWarning("son : {}", ro::core::Describe(messenger.error()));
  }
  if (!alarm) {
    return;
  }
  if (const ro::core::Result<void> button =
          PlayInterfaceSound(audio, vfs, client_root, ButtonSoundFile());
      !button) {
    ro::core::LogWarning("son : {}", ro::core::Describe(button.error()));
  }
}

void WhisperStage::StartSession(std::uint64_t last_seq) {
  desk_.Clear(last_seq);
  outbox_.Clear();
  blocks_.Clear();
  titles_.clear();
  links_.clear();
  links_peer_.clear();
  chips_.clear();
  chip_labels_.clear();
}

ro::net::FrameOutcome WhisperStage::ApplyFrame(const ro::net::Frame& frame, ro::sim::World& world,
                                               const ro::sim::FriendList& friends,
                                               const ro::sim::GuildBook& guild,
                                               std::uint32_t now_ms) {
  const ro::net::FrameOutcome whispered = ro::net::ApplyWhisperFrame(
      frame, world,
      ro::net::WhisperCircle{.friends = &friends, .guild = &guild, .outbox = &outbox_}, now_ms);
  if (whispered != ro::net::FrameOutcome::kUnhandled) {
    return whispered;
  }
  return ro::net::ApplyWhisperBlockFrame(frame, blocks_, world, now_ms);
}

void WhisperStage::Open(std::string_view peer) {
  if (!peer.empty() && desk_.Open(peer) == nullptr) {
    refused_open_ = true;
  }
}

void WhisperStage::FollowPlayer(std::string_view name, ro::ui::ChatLinkGesture gesture,
                                ro::net::Connection& link, ro::ui::ChatInputState& bar) {
  switch (gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      ArmWhisper(bar, name);
      return;
    case ro::ui::ChatLinkGesture::kWhisperWindow:
      Open(name);
      return;
    case ro::ui::ChatLinkGesture::kCopyCode:
      ImGui::SetClipboardText(std::string(name).c_str());
      return;
    case ro::ui::ChatLinkGesture::kInviteParty:
      if (link.is_open()) {
        ReportRefusal(ro::net::RequestPartyInvite(link, name));
      }
      return;
    case ro::ui::ChatLinkGesture::kInviteGuild:
      if (link.is_open()) {
        ReportRefusal(ro::net::RequestGuildInvite(link, name));
      }
      return;
    case ro::ui::ChatLinkGesture::kAddFriend:
      if (link.is_open()) {
        ReportRefusal(ro::net::RequestFriend(link, name));
      }
      return;
    default:
      return;
  }
}

int WhisperStage::Advance(WhisperWindows& windows, const WhisperContext& context) {
  std::vector<std::pair<std::string, ro::ui::WhisperWindowAction>> answers =
      std::exchange(windows.answers, {});
  // ⚠ LES FERMETURES D'ABORD : une conversation refermée à l'image d'avant ne
  // doit plus prendre au fil la ligne qui arrive à celle-ci. ⚠ MAIS LA PHRASE
  // VALIDÉE DANS LA MÊME IMAGE PART QUAND MÊME : on l'a écrite. ⚠ ET TROP LONGUE,
  // ELLE GARDE LA FENÊTRE OUVERTE : la refermer perdrait la phrase retenue.
  for (auto& [peer, answer] : answers) {
    if (!answer.closed) {
      continue;
    }
    if (answer.said.has_value()) {
      const bool held = Settle(windows, peer, *answer.said, context);
      answer.said.reset();
      if (held) {
        windows.states[peer].open = true;
        continue;
      }
    }
    desk_.Close(peer);
  }
  // CE QUE PORTENT LES SAISIES : une conversation où l'on écrit ne cède pas sa
  // place au plafond. ⚠ DÉPÔT COMPRIS : une phrase retenue y attend son retour.
  for (const auto& [peer, state] : windows.states) {
    desk_.NoteDraft(peer, state.input.text.front() != '\0' || !state.input.pending_insert.empty());
  }
  const WhisperDesk::Absorbed absorbed =
      desk_.Absorb(context.world.chat_lines(), context.setup, context.world.player_name());
  for (auto& [peer, answer] : answers) {
    if (answer.closed) {
      continue;
    }
    if (answer.focused) {
      desk_.Seen(peer);
    }
    if (answer.input_active) {
      links_peer_ = peer;
    }
    if (answer.said.has_value()) {
      static_cast<void>(Settle(windows, peer, *answer.said, context));
    }
  }

  // LES ÉTATS SUIVENT LES CONVERSATIONS : une fenêtre refermée oublie sa
  // saisie et ses liens, une fenêtre neuve en reçoit une.
  std::erase_if(windows.states,
                [this](const auto& entry) { return desk_.Find(entry.first) == nullptr; });
  PruneLinks(windows);
  for (const Conversation& conversation : desk_.conversations()) {
    ro::ui::WhisperWindowState& state = windows.states[conversation.peer];
    if (Conversation* raised = desk_.Find(conversation.peer);
        raised != nullptr && std::exchange(raised->raise, false)) {
      state.raise = true;
    }
  }
  // UNE OUVERTURE REFUSÉE SE DIT : sans quoi le geste semblerait sans effet.
  if (std::exchange(refused_open_, false)) {
    context.world.RecordChat(ro::sim::ChatKind::kServer, 0,
                             Tr("Toutes les conversations 1:1 ont un non-lu ou une phrase en "
                                "cours : fermez-en une pour en ouvrir une autre."),
                             context.now_ms);
  }
  Compose(windows, context);
  return absorbed.opened;
}

bool WhisperStage::Settle(WhisperWindows& windows, const std::string& peer, const std::string& said,
                          const WhisperContext& context) {
  // ⚠ CE QUI PART N'EST PAS CE QUI EST TAPÉ : chaque libellé de lien y reprend
  // sa balise.
  std::vector<PendingChatLink>& posed = links_[peer];
  std::string outgoing = ResolveChatLinks(said, posed);
  if (HoldOverlongWhisper(windows.states[peer], peer, said, outgoing, &posed, context)) {
    return true;
  }
  DropSentLinks(said, &posed);
  Send(peer, std::move(outgoing), context);
  return false;
}

void WhisperStage::Send(std::string_view peer, std::string text, const WhisperContext& context) {
  if (!context.link.is_open() || text.empty()) {
    return;
  }
  // 🔴 LA CIBLE EST FIGÉE : la phrase part à ce correspondant, telle quelle —
  // un « % » en tête n'en fait pas une parole de groupe.
  const ro::net::ChatIntent intent{.target = ro::net::ChatTarget::kWhisper,
                                   .recipient = std::string(peer),
                                   .text = std::move(text)};
  const ro::core::Result<void> sent =
      ro::net::SendChat(context.link, context.world.player_name(), intent);
  if (sent) {
    static_cast<void>(ro::net::NoteWhisperSent(outbox_, intent, context.now_ms));
  }
  ReportRefusal(sent);
}

void WhisperStage::Compose(WhisperWindows& windows, const WhisperContext& context) {
  const std::span<const Conversation> conversations = desk_.conversations();
  // ⚠ TOUS LES TITRES D'ABORD, LES VUES ENSUITE : les vues pointent dedans, et
  // un vecteur qui grandit déplacerait ce qu'elles regardent.
  titles_.clear();
  titles_.reserve(conversations.size() * 2);
  for (const Conversation& conversation : conversations) {
    std::string title;
    if (conversation.unread) {
      title.append(kUnreadMark);
    }
    title.append(kTitleOpen);
    title.append(conversation.peer);
    if (const std::string_view guild = GuildOf(context.world, conversation.peer); !guild.empty()) {
      title.append(kGuildOpen);
      title.append(guild);
      title.append(kGuildClose);
    }
    titles_.push_back(std::move(title));
    // LE NOM AUSSI, recopié : la vue le garde jusqu'au dessin, et un geste qui
    // ouvrirait une conversation entre-temps déplacerait celui de la table.
    titles_.push_back(conversation.peer);
  }
  // LES LIBELLÉS DES PASTILLES, de même : tous recopiés avant qu'une pastille
  // ne les regarde.
  chip_labels_.clear();
  std::size_t chip_count = 0;
  for (const auto& [peer, posed] : links_) {
    chip_count += posed.size();
  }
  chip_labels_.reserve(chip_count);
  chips_.resize(conversations.size());
  for (std::size_t rank = 0; rank < conversations.size(); ++rank) {
    std::vector<ro::ui::ChatInputLink>& chips = chips_[rank];
    chips.clear();
    const auto posed = links_.find(conversations[rank].peer);
    if (posed == links_.end()) {
      continue;
    }
    for (std::size_t at = 0; at < posed->second.size(); ++at) {
      chip_labels_.push_back(posed->second[at].shown);
      chips.push_back(ChipOf(posed->second[at], chip_labels_.back(), at, context));
    }
  }
  windows.views.clear();
  for (std::size_t rank = 0; rank < conversations.size(); ++rank) {
    windows.views.push_back(ro::ui::WhisperWindowView{.peer = titles_[rank * 2 + 1],
                                                      .title = titles_[rank * 2],
                                                      .cascade = static_cast<int>(rank),
                                                      .can_send = context.link.is_open(),
                                                      .input_links = chips_[rank]});
  }
}

void WhisperStage::PruneLinks(const WhisperWindows& windows) {
  // ⚠ CONTRE CE QUE LA SAISIE PORTERA, COMME LA BARRE : contre le texte seul,
  // les liens d'une phrase retenue disparaîtraient et partiraient en texte brut.
  for (auto posed = links_.begin(); posed != links_.end();) {
    const auto state = windows.states.find(posed->first);
    if (state != windows.states.end()) {
      PruneChatLinks(CarriedBy(state->second.input), &posed->second);
    }
    posed = state == windows.states.end() || posed->second.empty() ? links_.erase(posed)
                                                                   : std::next(posed);
  }
  // Une saisie refermée ne prend plus rien : le lien suivant va à la barre.
  if (!links_peer_.empty() && !windows.states.contains(links_peer_)) {
    links_peer_.clear();
  }
}

bool WhisperStage::TakesLinks() const noexcept { return !links_peer_.empty(); }

void WhisperStage::ReleaseLinks() noexcept { links_peer_.clear(); }

bool WhisperStage::PlaceLink(PendingChatLink link, WhisperWindows& windows) {
  const auto state = windows.states.find(links_peer_);
  // ⚠ UNE FENÊTRE FERMÉE NE PREND RIEN : le lien atterrirait dans un champ que
  // personne ne voit.
  if (!TakesLinks() || state == windows.states.end() || !state->second.open) {
    return false;
  }
  std::vector<PendingChatLink>& posed = links_[links_peer_];
  // 🔴 UNE PHRASE VALIDÉE À CETTE IMAGE N'EST PAS ENCORE PARTIE : elle part à la
  // suivante, et le champ qu'elle a quitté ne porte plus ses libellés. La pose
  // élague alors une COPIE pour compter sa place — élaguer les liens eux-mêmes
  // ferait partir ceux de la phrase en texte brut.
  std::vector<PendingChatLink> counted;
  std::vector<PendingChatLink>* pruned = &posed;
  if (AwaitsSending(windows, links_peer_)) {
    counted = posed;
    pruned = &counted;
  }
  // ⚠ AU BOUT DE CE QUE LA SAISIE PORTERA, dépôt en attente compris : voir
  // `PlaceChatLinkLabel`.
  if (!PlaceChatLinkLabel(link.shown, state->second.input, pruned)) {
    return false;
  }
  posed.push_back(std::move(link));
  // La main revient au clavier : on vient de poser un mot dans une phrase. Le
  // champ qui le reprend fait passer sa fenêtre devant.
  state->second.wants_focus = true;
  return true;
}

const std::vector<PendingChatLink>* WhisperStage::PendingOf(std::string_view peer) const {
  const auto posed = links_.find(peer);
  return posed != links_.end() ? &posed->second : nullptr;
}

}  // namespace moonclient
