// LA PHRASE D'UNE TRAME DE PAROLE, ET SON AUTEUR. Voir l'en-tête.

#include "ro/net/chat_author.h"

#include <cstddef>
#include <utility>

#include "ro/core/text.h"

namespace ro::net {

AuthoredText ReadAuthoredText(const Frame& frame, std::string_view message) {
  AuthoredText read;
  proto::ChatAuthorNotice notice;
  if (frame.preamble.empty() || !proto::Decode(frame.preamble, notice) ||
      notice.fixed.next_opcode != frame.opcode) {
    read.text = core::Utf8FromWire(message);
    return read;
  }
  read.channel_type = notice.fixed.channel_type;
  // ⚠ LA TÊTE NE SE CROIT QUE SI LE NOM EST LÀ OÙ ELLE LE DIT : une tête nulle
  // dit que le serveur n'a pas reconnu la forme de la phrase.
  const std::string_view name = notice.fixed.name;
  const std::size_t head = notice.fixed.header_len;
  const std::size_t name_at = notice.fixed.name_offset;
  if (head == 0 || head > message.size() || name_at > head || head - name_at < name.size() ||
      message.substr(name_at, name.size()) != name) {
    read.text = core::Utf8FromWire(message);
    return read;
  }
  const std::string lead = core::Utf8FromWire(message.substr(0, name_at));
  std::string shown_name = core::Utf8FromWire(name);
  const std::string trail =
      core::Utf8FromWire(message.substr(name_at + name.size(), head - name_at - name.size()));
  const std::string said = core::Utf8FromWire(message.substr(head));

  sim::ChatAuthor author;
  author.account_id = notice.fixed.account_id;
  author.char_id = notice.fixed.char_id;
  author.name_offset = static_cast<std::uint32_t>(lead.size());
  author.name_length = static_cast<std::uint32_t>(shown_name.size());
  author.said_offset = static_cast<std::uint32_t>(lead.size() + shown_name.size() + trail.size());
  read.text = lead + shown_name + trail + said;
  author.name = std::move(shown_name);
  read.author = std::move(author);
  return read;
}

}  // namespace ro::net
