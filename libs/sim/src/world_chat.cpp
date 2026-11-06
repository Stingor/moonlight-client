// LA CONVERSATION DU MONDE : ce qui entre dans le fil, ce qu'il évince, et la
// retenue qui le laisse intact tant que les fenêtres le regardent.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/world.h"

namespace ro::sim {

void World::RecordWhisper(WhisperFacts facts, std::string text, std::uint32_t said_ms) {
  RecordChat(ChatKind::kWhisper, 0, std::move(text), said_ms);
  (chat_held_ ? chat_waiting_ : chat_lines_).back().whisper = std::move(facts);
}

void World::ShowSpeech(std::uint32_t speaker_id, std::string text, std::uint32_t said_ms,
                       std::uint32_t bubble_bgr) {
  if (const auto found = actors_.find(speaker_id); found != actors_.end()) {
    found->second.chat_text = std::move(text);
    found->second.chat_since_ms = said_ms;
    found->second.chat_bgr = bubble_bgr;
  }
}

void World::RecordChat(ChatKind kind, ChatType type, std::uint32_t speaker_id, std::string text,
                       std::uint32_t said_ms, std::uint32_t color,
                       std::optional<ChatAuthor> author) {
  // La bulle n'attend pas : elle ne vit pas dans le fil.
  ShowSpeech(speaker_id, text, said_ms);
  ChatLine line{.kind = kind,
                .type = type,
                .speaker_id = speaker_id,
                .text = std::move(text),
                .said_ms = said_ms,
                .color = color,
                .author = std::move(author)};
  if (chat_held_) {
    chat_waiting_.push_back(std::move(line));
    return;
  }
  AppendChatLine(std::move(line));
}

void World::AppendChatLine(ChatLine line) {
  // ÉVICTION PAR GENRE : les plus anciennes de CE genre-là s'en vont.
  //
  // ⚠ DEUX AU PLUS PAR LIGNE ARRIVÉE quand la borne est dépassée, et non une :
  // une seule retirée pour une ajoutée laisserait le compte où il est, et une
  // borne abaissée ne prendrait jamais effet. Deux, et non tout l'excédent :
  // le trop-plein s'écoule au fil des lignes, sans vider d'un coup ce que le
  // joueur est en train de lire.
  constexpr int kMostEvictedPerLine = 2;
  const ChatKind kind = line.kind;
  const auto of_kind = static_cast<int>(std::ranges::count_if(
      chat_lines_, [kind](const ChatLine& kept) { return kept.kind == kind; }));
  int evicted = std::min(of_kind - kept_per_kind_ + 1, kMostEvictedPerLine);
  if (evicted > 0) {
    std::erase_if(chat_lines_, [kind, &evicted](const ChatLine& kept) {
      if (evicted == 0 || kept.kind != kind) {
        return false;
      }
      --evicted;
      return true;
    });
  }
  // Le rang part à UN : une marque de vidage à zéro ne désigne aucune ligne.
  ++chat_lines_seen_;
  line.seq = chat_lines_seen_;
  chat_lines_.push_back(std::move(line));
}

void World::HoldChat() noexcept { chat_held_ = true; }

void World::ReleaseChat() {
  chat_held_ = false;
  for (ChatLine& line : chat_waiting_) {
    AppendChatLine(std::move(line));
  }
  chat_waiting_.clear();
}

}  // namespace ro::sim
