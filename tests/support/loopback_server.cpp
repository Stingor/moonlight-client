#include "support/loopback_server.h"

#include <array>
#include <cstddef>
#include <mutex>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off
// L'ordre est SÉMANTIQUE : <windows.h> inclut la version 1 de Winsock si elle
// n'est pas déjà là, et ses déclarations contredisent celles de la version 2.
#include <winsock2.h>
#include <ws2tcpip.h>
// clang-format on
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ro::test {
namespace {

constexpr std::uintptr_t kInvalid = static_cast<std::uintptr_t>(-1);

// Combien de connexions le système garde en attente d'acceptation. Le test n'en
// ouvre qu'une à la fois.
constexpr int kBacklog = 1;

// Au-delà, un test qui attend une réponse qui ne viendra pas doit échouer
// plutôt que suspendre la suite entière.
constexpr int kWaitMs = 2000;

// Le descripteur, dans le type qu'attend l'API du système. Le reste du fichier
// n'a plus à connaître la différence.
#if defined(_WIN32)
using Native = SOCKET;
#else
using Native = int;
#endif

Native AsNative(std::uintptr_t handle) { return static_cast<Native>(handle); }

#if defined(_WIN32)
void EnsureSubsystem() {
  static std::once_flag once;
  std::call_once(once, [] {
    WSADATA data = WSADATA();
    WSAStartup(MAKEWORD(2, 2), &data);
  });
}
void CloseNative(std::uintptr_t handle) { closesocket(AsNative(handle)); }
int AsLength(std::size_t count) { return static_cast<int>(count); }
char* AsBuffer(std::uint8_t* bytes) { return reinterpret_cast<char*>(bytes); }
const char* AsBuffer(const std::uint8_t* bytes) { return reinterpret_cast<const char*>(bytes); }
const char* AsOption(const int* value) { return reinterpret_cast<const char*>(value); }
#else
void EnsureSubsystem() {}
void CloseNative(std::uintptr_t handle) { ::close(AsNative(handle)); }
std::size_t AsLength(std::size_t count) { return count; }
std::uint8_t* AsBuffer(std::uint8_t* bytes) { return bytes; }
const std::uint8_t* AsBuffer(const std::uint8_t* bytes) { return bytes; }
const int* AsOption(const int* value) { return value; }
#endif

// Attend que le descripteur ait quelque chose à lire, ou que le délai passe.
// Le seau dans lequel `ClientHungUp` jette ce qui restait. Sa taille n'a
// d'importance que pour le nombre de tours : rien de ce qu'il attrape n'est lu.
constexpr std::size_t kDrainBytes = 256;

bool WaitReadable(std::uintptr_t handle, int timeout_ms) {
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(AsNative(handle), &readable);
  timeval limit = timeval();
  limit.tv_sec = timeout_ms / 1000;
  limit.tv_usec = (timeout_ms % 1000) * 1000;
#if defined(_WIN32)
  // Winsock ignore ce premier argument : ses ensembles portent les
  // descripteurs eux-mêmes, pas un intervalle.
  return select(0, &readable, nullptr, nullptr, &limit) > 0;
#else
  return select(AsNative(handle) + 1, &readable, nullptr, nullptr, &limit) > 0;
#endif
}

}  // namespace

bool LimitSendBuffer(std::uintptr_t socket, int bytes) {
  return setsockopt(AsNative(socket), SOL_SOCKET, SO_SNDBUF, AsOption(&bytes), sizeof(bytes)) == 0;
}

LoopbackServer::LoopbackServer(int receive_buffer_bytes) {
  EnsureSubsystem();
  const std::uintptr_t listener = static_cast<std::uintptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  if (listener == kInvalid) {
    return;
  }

  // Réglé AVANT listen : la fenêtre est annoncée pendant la poignée de main, et
  // les sockets acceptés héritent du réglage. La rétrécir après coup laisserait
  // le client émettre à pleine vitesse le temps d'un aller-retour.
  if (receive_buffer_bytes > 0) {
    setsockopt(AsNative(listener), SOL_SOCKET, SO_RCVBUF, AsOption(&receive_buffer_bytes),
               sizeof(receive_buffer_bytes));
  }

  sockaddr_in address = sockaddr_in();
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  // Port zéro : le système en attribue un libre, relu juste après. Un numéro
  // fixé d'avance ferait échouer le test dès qu'un autre processus l'occupe.
  address.sin_port = 0;

  sockaddr_in bound = sockaddr_in();
  socklen_t size = static_cast<socklen_t>(sizeof(bound));
  const bool ready =
      bind(AsNative(listener), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
      getsockname(AsNative(listener), reinterpret_cast<sockaddr*>(&bound), &size) == 0 &&
      listen(AsNative(listener), kBacklog) == 0;
  if (!ready) {
    CloseNative(listener);
    return;
  }

  listener_ = listener;
  receive_buffer_bytes_ = receive_buffer_bytes;
  port_ = ntohs(bound.sin_port);
}

LoopbackServer::~LoopbackServer() {
  CloseClient();
  if (listener_ != kInvalid) {
    CloseNative(listener_);
    listener_ = kInvalid;
  }
}

bool LoopbackServer::listening() const noexcept { return listener_ != kInvalid && port_ != 0; }

bool LoopbackServer::Accept() {
  if (listener_ == kInvalid || !WaitReadable(listener_, kWaitMs)) {
    return false;
  }
  const std::uintptr_t client =
      static_cast<std::uintptr_t>(accept(AsNative(listener_), nullptr, nullptr));
  if (client == kInvalid) {
    return false;
  }
  CloseClient();
  client_ = client;
  // Le réglage du socket d'écoute est censé être hérité ; Windows ne l'applique
  // pas toujours, et une fenêtre restée large laisse passer des mégaoctets là
  // où le banc en veut quelques milliers.
  if (receive_buffer_bytes_ > 0) {
    setsockopt(AsNative(client_), SOL_SOCKET, SO_RCVBUF, AsOption(&receive_buffer_bytes_),
               sizeof(receive_buffer_bytes_));
  }
  return true;
}

bool LoopbackServer::Send(std::span<const std::uint8_t> bytes) {
  if (client_ == kInvalid) {
    return false;
  }
  std::size_t sent = 0;
  while (sent < bytes.size()) {
    const auto written =
        ::send(AsNative(client_), AsBuffer(bytes.data() + sent), AsLength(bytes.size() - sent), 0);
    if (written <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(written);
  }
  return true;
}

std::vector<std::uint8_t> LoopbackServer::Receive(std::size_t count, int timeout_ms) {
  std::vector<std::uint8_t> bytes;
  if (client_ == kInvalid) {
    return bytes;
  }
  bytes.resize(count);
  std::size_t filled = 0;
  while (filled < count) {
    if (!WaitReadable(client_, timeout_ms)) {
      break;
    }
    const auto read =
        ::recv(AsNative(client_), AsBuffer(bytes.data() + filled), AsLength(count - filled), 0);
    if (read <= 0) {
      break;
    }
    filled += static_cast<std::size_t>(read);
  }
  bytes.resize(filled);
  return bytes;
}

bool LoopbackServer::ClientHungUp(int timeout_ms) {
  if (client_ == kInvalid) {
    return true;
  }
  // 🔴 ON VIDE CE QUI RESTE AVANT DE CONCLURE, et lorgner ne suffisait pas :
  // des octets en attente ne prouvent RIEN. Un client qui écrit puis raccroche
  // est le cas ORDINAIRE — le décor envoie son « carte chargée » (0x7D) avant
  // de rendre sa session —, et la première version de cette fonction lisait
  // cet octet-là, concluait « il est encore là » et rendait faux sur une
  // liaison bel et bien fermée. Ce qu'on cherche est la FIN du flux, qui se
  // trouve DERRIÈRE ce qui reste à lire.
  //
  // ⚠ ELLE CONSOMME DONC : ce qui n'avait pas été lu est jeté. C'est un banc
  // d'essai, et l'essai qui appelle ceci a fini de lire.
  std::array<std::uint8_t, kDrainBytes> thrown = {};
  while (WaitReadable(client_, timeout_ms)) {
    const auto read =
        ::recv(AsNative(client_), AsBuffer(thrown.data()), AsLength(thrown.size()), 0);
    // Zéro : fermeture ordonnée. Négatif : coupure. Les deux valent « parti ».
    if (read <= 0) {
      return true;
    }
  }
  // Plus rien à lire, et la socket n'a pas rendu sa fin : le client est là.
  return false;
}

void LoopbackServer::CloseClient() noexcept {
  if (client_ != kInvalid) {
    CloseNative(client_);
    client_ = kInvalid;
  }
}

}  // namespace ro::test
