#include "ro/proto/show_script_packets.h"

#include <utility>

#include "ro/core/byte_reader.h"

namespace ro::proto {

bool Decode(std::span<const std::uint8_t> frame, ShowScriptNotice& packet) {
  if (frame.size() < ShowScriptNotice::kHeaderSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != ShowScriptNotice::kOpcode) {
    return false;
  }
  const std::uint16_t declared = reader.U16();
  if (declared != frame.size()) {
    return false;
  }
  ShowScriptNotice read;
  read.actor_id = reader.U32();
  if (!reader.ok()) {
    return false;
  }
  // Le texte court jusqu'à son zéro, absent ou non selon le serveur.
  const std::span<const std::uint8_t> tail = frame.subspan(ShowScriptNotice::kHeaderSize);
  read.message.assign(reinterpret_cast<const char*>(tail.data()), tail.size());
  if (const std::size_t end = read.message.find('\0'); end != std::string::npos) {
    read.message.resize(end);
  }
  packet = std::move(read);
  return true;
}

}  // namespace ro::proto
