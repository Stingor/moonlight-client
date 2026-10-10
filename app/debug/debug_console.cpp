#include "debug/debug_console.h"

#include <array>
#include <span>
#include <utility>

#include "ro/core/log.h"

#include "debug/debug_request.h"

namespace moonclient {
namespace {

// Ce qu'une lecture prend au plus : une requête tient sur quelques dizaines
// d'octets.
constexpr std::size_t kReadChunkBytes = 4096;
// Une ligne plus longue n'est pas une requête : l'interlocuteur se trompe de
// protocole, et on le renvoie plutôt que d'accumuler sans fin.
constexpr std::size_t kLongestLineBytes = 4096;
// Les requêtes servies au plus par image : la boucle de jeu passe d'abord.
constexpr int kRequestsPerFrame = 8;

}  // namespace

std::optional<std::uint16_t> DebugConsole::port() const noexcept {
  if (!listener_.is_open()) {
    return std::nullopt;
  }
  return listener_.port();
}

void DebugConsole::Open() {
  if (listener_.is_open() || gave_up_) {
    return;
  }
  ro::core::Result<ro::net::TcpListener> opened =
      ro::net::TcpListener::ListenLocal(kDebugConsolePort);
  if (!opened.has_value()) {
    failure_ = ro::core::Describe(opened.error());
    gave_up_ = true;
    ro::core::LogWarning("console de mise au point : {}", failure_);
    return;
  }
  listener_ = std::move(*opened);
  failure_.clear();
  ro::core::LogInfo("console de mise au point : écoute sur 127.0.0.1:{}", listener_.port());
}

void DebugConsole::Shut() {
  if (listener_.is_open()) {
    ro::core::LogInfo("console de mise au point : éteinte");
  }
  client_.Close();
  listener_.Close();
  received_.clear();
  pending_.clear();
  gave_up_ = false;
}

void DebugConsole::Read() {
  std::array<std::uint8_t, kReadChunkBytes> chunk{};
  const ro::core::Result<ro::net::ReceiveResult> read = client_.Receive(chunk);
  if (!read.has_value() || read->state == ro::net::ReceiveState::kClosed) {
    client_.Close();
    received_.clear();
    pending_.clear();
    return;
  }
  if (read->state == ro::net::ReceiveState::kBytes) {
    received_.append(reinterpret_cast<const char*>(chunk.data()), read->bytes);
  }
  if (received_.size() > kLongestLineBytes && received_.find('\n') == std::string::npos) {
    ro::core::LogWarning("console de mise au point : ligne trop longue, interlocuteur renvoyé");
    client_.Close();
    received_.clear();
    pending_.clear();
  }
}

void DebugConsole::Answer(const DebugSources& sources) {
  for (int served = 0; served < kRequestsPerFrame; ++served) {
    const std::size_t end = received_.find('\n');
    if (end == std::string::npos) {
      return;
    }
    const std::string line = received_.substr(0, end);
    received_.erase(0, end + 1);
    const std::optional<DebugRequest> request = ParseDebugRequest(line);
    pending_ +=
        request.has_value() ? AnswerDebugRequest(*request, sources) : AnswerUnreadableLine();
    pending_ += '\n';
  }
}

void DebugConsole::Flush() {
  if (pending_.empty()) {
    return;
  }
  const ro::core::Result<std::size_t> sent = client_.Send(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(pending_.data()), pending_.size()));
  if (!sent.has_value()) {
    client_.Close();
    received_.clear();
    pending_.clear();
    return;
  }
  // Le reste attend l'image suivante : le tampon du système était plein.
  pending_.erase(0, *sent);
}

void DebugConsole::Serve(bool enabled, const DebugSources& sources) {
  if (!enabled) {
    Shut();
    return;
  }
  Open();
  if (!listener_.is_open()) {
    return;
  }
  ro::core::Result<std::optional<ro::net::TcpSocket>> accepted = listener_.Accept();
  if (accepted.has_value() && accepted->has_value()) {
    // UN SEUL INTERLOCUTEUR : le nouveau remplace l'ancien.
    client_ = std::move(**accepted);
    received_.clear();
    pending_.clear();
    ro::core::LogInfo("console de mise au point : interlocuteur branché");
  }
  if (!client_.is_open()) {
    return;
  }
  Read();
  if (!client_.is_open()) {
    return;
  }
  Answer(sources);
  Flush();
}

}  // namespace moonclient
