#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "ro/core/error.h"

namespace ro::net {

// Le descripteur natif. Windows et POSIX ne s'accordent ni sur le type ni sur
// la valeur d'invalidité ; tout le reste du client ignore la différence.
#if defined(_WIN32)
using NativeSocket = std::uintptr_t;
#else
using NativeSocket = int;
#endif

// Vaut INVALID_SOCKET sur Windows et -1 sur POSIX : la même écriture donne les
// deux, parce que l'un est le motif de bits de l'autre.
inline constexpr NativeSocket kInvalidSocket = static_cast<NativeSocket>(-1);

// Combien de temps attendre l'établissement d'une connexion.
//
// Sans limite, une tentative vers un hôte injoignable rend la main au bout du
// délai du système — une vingtaine de secondes pendant lesquelles le client
// paraît figé, sans rien afficher.
inline constexpr int kConnectTimeoutMs = 5000;

// Ce qu'une lecture a trouvé.
enum class ReceiveState : std::uint8_t {
  // Des octets sont là.
  kBytes,
  // Rien pour l'instant. Ce n'est PAS une erreur : c'est l'état ordinaire d'un
  // socket non bloquant entre deux paquets.
  kWouldBlock,
  // Le pair a fermé proprement. À distinguer d'une absence d'octets : dans un
  // cas la session continue, dans l'autre elle est finie.
  kClosed,
};

struct ReceiveResult {
  ReceiveState state = ReceiveState::kWouldBlock;
  // Octets écrits dans le tampon fourni. Nul sauf si l'état dit kBytes.
  std::size_t bytes = 0;
};

// Un socket TCP client, fermé à la destruction.
//
// Non bloquant après connexion : la boucle de jeu ne peut pas se permettre
// d'attendre le réseau, et un socket bloquant impose soit un fil dédié soit une
// image perdue à chaque paquet.
class TcpSocket {
 public:
  TcpSocket() = default;
  ~TcpSocket();

  // Déplaçable, non copiable : deux copies fermeraient deux fois le même
  // descripteur, et le second appel pourrait fermer un descripteur réattribué
  // entre-temps à un autre usage.
  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;
  TcpSocket(TcpSocket&& other) noexcept;
  TcpSocket& operator=(TcpSocket&& other) noexcept;

  // Résout l'hôte et se connecte, dans la limite du délai donné.
  //
  // L'hôte peut être un nom ou une adresse, en IPv4 comme en IPv6 : la
  // résolution rend les candidats dans l'ordre de préférence du système et ils
  // sont essayés l'un après l'autre.
  [[nodiscard]] static core::Result<TcpSocket> Connect(std::string_view host, std::uint16_t port,
                                                       int timeout_ms = kConnectTimeoutMs);

  // Lit ce qui est disponible, sans jamais attendre.
  [[nodiscard]] core::Result<ReceiveResult> Receive(std::span<std::uint8_t> into);

  // Envoie ce qu'il peut, et rend combien il a pris.
  //
  // ⚠ Le compte rendu peut être INFÉRIEUR à ce qui était demandé, y compris
  // nul : le tampon d'émission du système est de taille finie. L'appelant doit
  // garder le reste et le représenter plus tard, sinon le flux perd des octets
  // au milieu d'une trame et la session se désynchronise.
  [[nodiscard]] core::Result<std::size_t> Send(std::span<const std::uint8_t> bytes);

  void Close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return handle_ != kInvalidSocket; }

  // Le descripteur natif, pour les tests qui doivent l'observer.
  [[nodiscard]] NativeSocket handle() const noexcept { return handle_; }

 private:
  friend class TcpListener;

  explicit TcpSocket(NativeSocket handle) noexcept : handle_(handle) {}

  NativeSocket handle_ = kInvalidSocket;
};

// UNE ÉCOUTE TCP SUR LA BOUCLE LOCALE, fermée à la destruction.
//
// 🔴 127.0.0.1 ET RIEN D'AUTRE : elle sert la console de mise au point, qu'un
// outil de la même machine interroge. Écouter sur toutes les interfaces
// ouvrirait l'état du client au réseau.
//
// Non bloquante, comme le socket client : la boucle de jeu demande à chaque
// image s'il y a quelqu'un, et n'attend jamais.
class TcpListener {
 public:
  TcpListener() = default;
  ~TcpListener();

  TcpListener(const TcpListener&) = delete;
  TcpListener& operator=(const TcpListener&) = delete;
  TcpListener(TcpListener&& other) noexcept;
  TcpListener& operator=(TcpListener&& other) noexcept;

  // Écoute sur 127.0.0.1:`port`. Zéro laisse le système choisir : voir `port()`.
  [[nodiscard]] static core::Result<TcpListener> ListenLocal(std::uint16_t port);

  // Une connexion en attente, déjà non bloquante, ou rien — sans attendre.
  [[nodiscard]] core::Result<std::optional<TcpSocket>> Accept();

  // Le port réellement écouté.
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

  void Close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return handle_ != kInvalidSocket; }

 private:
  NativeSocket handle_ = kInvalidSocket;
  std::uint16_t port_ = 0;
};

}  // namespace ro::net
