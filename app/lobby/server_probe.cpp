#include "lobby/server_probe.h"

#include <thread>
#include <utility>

#include "ro/net/socket.h"

namespace moonclient::lobby {

bool TcpServerProbe::Answers(std::string_view host, std::uint16_t port) {
  return ro::net::TcpSocket::Connect(host, port, timeout_ms_).has_value();
}

ProbeInFlight ProbeInFlight::Launch(std::shared_ptr<ServerProbe> probe, std::string host,
                                    std::uint16_t port) {
  auto verdict = std::make_shared<Verdict>();
  std::thread([probe = std::move(probe), host = std::move(host), port, verdict] {
    const bool answered = probe->Answers(host, port);
    const std::scoped_lock settle(verdict->guard);
    verdict->answered = answered;
    verdict->settled = true;
    verdict->settled_signal.notify_all();
  }).detach();
  return ProbeInFlight(std::move(verdict));
}

bool ProbeInFlight::Detected(std::chrono::milliseconds wait) const {
  std::unique_lock settle(verdict_->guard);
  verdict_->settled_signal.wait_for(settle, wait, [this] { return verdict_->settled; });
  return verdict_->settled && verdict_->answered;
}

}  // namespace moonclient::lobby
