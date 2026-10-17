#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace moonclient::lobby {

// COMBIEN DE TEMPS LE SERVEUR TEST A POUR RÉPONDRE.
//
// ⚠ COURT, PARCE QU'IL EST LOCAL : un serveur sur la machine accepte en moins
// d'une milliseconde. Au-delà, il n'est pas là — et sous Windows, une connexion
// vers un port local fermé ne se refuse qu'après plusieurs relances de deux
// secondes au total, qu'on n'attend pas.
inline constexpr int kTestServerProbeMs = 250;

// SAVOIR SI UN SERVEUR ÉCOUTE, sans rien lui dire.
//
// Derrière une interface pour que les règles qui en dépendent s'éprouvent sans
// réseau.
class ServerProbe {
 public:
  virtual ~ServerProbe() = default;
  // Vrai si une connexion TCP s'établit dans le délai. Elle se ferme aussitôt,
  // sans qu'un octet soit parti.
  [[nodiscard]] virtual bool Answers(std::string_view host, std::uint16_t port) = 0;
};

// LA SONDE TCP : une connexion ouverte puis refermée.
class TcpServerProbe final : public ServerProbe {
 public:
  explicit TcpServerProbe(int timeout_ms = kTestServerProbeMs) : timeout_ms_(timeout_ms) {}
  [[nodiscard]] bool Answers(std::string_view host, std::uint16_t port) override;

 private:
  int timeout_ms_;
};

// UNE SONDE LANCÉE EN FOND, et son verdict quand on le demande.
//
// 🔴 ELLE NE RETIENT PAS LE LANCEMENT. Partie avant la fenêtre, elle a fini
// bien avant l'écran de connexion — le patch et les archives prennent plus que
// son délai. Le verdict se demande là, borné : une sonde qui n'a pas encore
// conclu vaut « absent ».
//
// ⚠ SON FIL EST DÉTACHÉ : un hôte qui tarde à se résoudre ne doit pas retenir
// la fermeture du client. Il ne tient que ce qu'il partage avec cet objet.
class ProbeInFlight {
 public:
  [[nodiscard]] static ProbeInFlight Launch(std::shared_ptr<ServerProbe> probe, std::string host,
                                            std::uint16_t port);

  // Vrai si le serveur a répondu, en attendant au plus `wait` qu'elle conclue.
  [[nodiscard]] bool Detected(std::chrono::milliseconds wait) const;

 private:
  struct Verdict {
    std::mutex guard;
    std::condition_variable settled_signal;
    bool settled = false;
    bool answered = false;
  };

  explicit ProbeInFlight(std::shared_ptr<Verdict> verdict) : verdict_(std::move(verdict)) {}

  std::shared_ptr<Verdict> verdict_;
};

}  // namespace moonclient::lobby
