#include "ro/net/socket.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// clang-format off
// L'ordre est SÉMANTIQUE : <windows.h> inclut la version 1 de Winsock si elle
// n'est pas déjà là, et ses déclarations contredisent celles de la version 2.
// Les trier alphabétiquement casse la compilation.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
// clang-format on
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ro::net {
namespace {

using core::Error;
using core::ErrorCode;
using core::Fail;
using core::Result;

// Le port tient sur cinq chiffres ; getaddrinfo l'attend sous forme de texte.
constexpr std::size_t kPortTextSize = 6;

#if defined(_WIN32)

// Winsock exige une initialisation par processus.
//
// Ce n'est pas un singleton de commodité : c'est l'API du système qui est
// globale. La cacher derrière un objet que l'appelant devrait construire
// n'ajouterait qu'une manière d'oublier de le faire.
void EnsureSubsystem() {
  static std::once_flag once;
  std::call_once(once, [] {
    WSADATA data = WSADATA();
    WSAStartup(MAKEWORD(2, 2), &data);
  });
}

int LastError() { return WSAGetLastError(); }

bool WouldBlock(int code) { return code == WSAEWOULDBLOCK; }

// Le message du système, converti en UTF-8.
//
// La variante ANSI de FormatMessage rend le texte dans la page de codes locale ;
// sur une machine française, « connexion refusée » y perd ses accents une fois
// versé dans un journal UTF-8. La variante large puis une conversion explicite
// donnent le message tel qu'il est.
std::string DescribeError(int code) {
  LPWSTR wide = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<LPWSTR>(&wide), 0, nullptr);
  std::string text;
  if (length != 0 && wide != nullptr) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), nullptr, 0,
                                          nullptr, nullptr);
    if (bytes > 0) {
      text.resize(static_cast<std::size_t>(bytes));
      WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(length), text.data(), bytes, nullptr,
                          nullptr);
    }
  }
  if (wide != nullptr) {
    LocalFree(wide);
  }
  // Le système termine ses messages par un retour à la ligne, qui couperait la
  // ligne de journal en deux.
  while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
    text.pop_back();
  }
  std::array<char, 32> numeric = {};
  std::snprintf(numeric.data(), numeric.size(), " (%d)", code);
  return text + numeric.data();
}

void CloseNative(NativeSocket handle) { closesocket(static_cast<SOCKET>(handle)); }

bool SetNonBlocking(NativeSocket handle) {
  u_long enabled = 1;
  return ioctlsocket(static_cast<SOCKET>(handle), FIONBIO, &enabled) == 0;
}

// Windows signale « connexion en cours » par un code qui lui est propre.
bool ConnectInProgress(int code) { return code == WSAEWOULDBLOCK; }

#else

void EnsureSubsystem() {}

int LastError() { return errno; }

// ⚠ UN APPEL INTERROMPU PAR UN SIGNAL (`EINTR`) N'EST PAS UNE PANNE : rien
// n'a été lu ni écrit, et la prochaine image réessaie, comme pour un tampon
// vide ou plein. Le traiter en erreur fermait la liaison au premier signal.
bool WouldBlock(int code) { return code == EAGAIN || code == EWOULDBLOCK || code == EINTR; }

std::string DescribeError(int code) {
  std::array<char, 32> numeric = {};
  std::snprintf(numeric.data(), numeric.size(), " (%d)", code);
  return std::string(std::strerror(code)) + numeric.data();
}

void CloseNative(NativeSocket handle) { ::close(handle); }

bool SetNonBlocking(NativeSocket handle) {
  const int flags = fcntl(handle, F_GETFL, 0);
  return flags != -1 && fcntl(handle, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool ConnectInProgress(int code) { return code == EINPROGRESS; }

#endif

// Attend que la connexion aboutisse, et dit si elle a abouti.
//
// Un socket non bloquant rend la main avant que la poignée de main soit finie ;
// c'est select qui dit quand elle l'est, et SO_ERROR qui dit comment. Sans cette
// seconde question, un socket devenu inscriptible PARCE QUE la connexion a
// échoué serait pris pour un succès, et l'échec n'apparaîtrait qu'au premier
// envoi.
Result<void> AwaitConnection(NativeSocket handle, int timeout_ms) {
  fd_set writable;
  FD_ZERO(&writable);
  timeval limit = timeval();
  limit.tv_sec = timeout_ms / 1000;
  limit.tv_usec = (timeout_ms % 1000) * 1000;

#if defined(_WIN32)
  FD_SET(static_cast<SOCKET>(handle), &writable);
  // Winsock ignore ce premier argument : ses ensembles portent les descripteurs
  // eux-mêmes, pas un intervalle. Le calculer y produirait une conversion
  // dépassant la taille d'un int sur un descripteur haut.
  const int ready = select(0, nullptr, &writable, nullptr, &limit);
#else
  FD_SET(handle, &writable);
  const int ready = select(handle + 1, nullptr, &writable, nullptr, &limit);
#endif
  if (ready == 0) {
    return Fail(ErrorCode::kIoFailure, "délai de connexion dépassé");
  }
  if (ready < 0) {
    return Fail(ErrorCode::kIoFailure, "attente de connexion : " + DescribeError(LastError()));
  }

  int pending = 0;
  socklen_t size = static_cast<socklen_t>(sizeof(pending));
#if defined(_WIN32)
  const int queried = getsockopt(static_cast<SOCKET>(handle), SOL_SOCKET, SO_ERROR,
                                 reinterpret_cast<char*>(&pending), &size);
#else
  const int queried = getsockopt(handle, SOL_SOCKET, SO_ERROR, &pending, &size);
#endif
  if (queried != 0) {
    return Fail(ErrorCode::kIoFailure, "état de connexion : " + DescribeError(LastError()));
  }
  if (pending != 0) {
    return Fail(ErrorCode::kIoFailure, "connexion refusée : " + DescribeError(pending));
  }
  return {};
}

// Désactive l'algorithme de Nagle.
//
// Il retient les petits envois pour les regrouper. Un client de jeu n'envoie
// presque que des petits paquets, et chacun retenu est un aller-retour de
// latence ajouté à une action du joueur.
void DisableNagle(NativeSocket handle) {
  int enabled = 1;
#if defined(_WIN32)
  setsockopt(static_cast<SOCKET>(handle), IPPROTO_TCP, TCP_NODELAY,
             reinterpret_cast<const char*>(&enabled), sizeof(enabled));
#else
  setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
#endif
}

}  // namespace

TcpSocket::~TcpSocket() { Close(); }

TcpListener::~TcpListener() { Close(); }

TcpListener::TcpListener(TcpListener&& other) noexcept
    : handle_(other.handle_), port_(other.port_) {
  other.handle_ = kInvalidSocket;
}

TcpListener& TcpListener::operator=(TcpListener&& other) noexcept {
  if (this != &other) {
    Close();
    handle_ = other.handle_;
    port_ = other.port_;
    other.handle_ = kInvalidSocket;
  }
  return *this;
}

void TcpListener::Close() noexcept {
  if (handle_ != kInvalidSocket) {
    CloseNative(handle_);
    handle_ = kInvalidSocket;
  }
}

Result<TcpListener> TcpListener::ListenLocal(std::uint16_t port) {
  EnsureSubsystem();
  const auto handle = static_cast<NativeSocket>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  if (handle == kInvalidSocket) {
    return Fail(ErrorCode::kIoFailure, "création de l'écoute : " + DescribeError(LastError()));
  }
  TcpListener listener;
  listener.handle_ = handle;
  sockaddr_in local = sockaddr_in();
  local.sin_family = AF_INET;
  local.sin_port = htons(port);
  local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#if defined(_WIN32)
  const int bound = ::bind(static_cast<SOCKET>(handle), reinterpret_cast<const sockaddr*>(&local),
                           static_cast<int>(sizeof(local)));
  const int listening = bound == 0 ? ::listen(static_cast<SOCKET>(handle), SOMAXCONN) : -1;
#else
  // Relancer le client ne doit pas attendre que l'ancienne écoute expire.
  int reuse = 1;
  setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  const int bound = ::bind(handle, reinterpret_cast<const sockaddr*>(&local), sizeof(local));
  const int listening = bound == 0 ? ::listen(handle, SOMAXCONN) : -1;
#endif
  if (listening != 0) {
    return Fail(ErrorCode::kIoFailure,
                "écoute du port " + std::to_string(port) + " : " + DescribeError(LastError()));
  }
  if (!SetNonBlocking(handle)) {
    return Fail(ErrorCode::kIoFailure, "écoute non bloquante : " + DescribeError(LastError()));
  }
  sockaddr_in actual = sockaddr_in();
  socklen_t size = static_cast<socklen_t>(sizeof(actual));
#if defined(_WIN32)
  getsockname(static_cast<SOCKET>(handle), reinterpret_cast<sockaddr*>(&actual), &size);
#else
  getsockname(handle, reinterpret_cast<sockaddr*>(&actual), &size);
#endif
  listener.port_ = ntohs(actual.sin_port);
  return listener;
}

Result<std::optional<TcpSocket>> TcpListener::Accept() {
  if (!is_open()) {
    return Fail(ErrorCode::kIoFailure, "accueil sur une écoute fermée");
  }
#if defined(_WIN32)
  const auto handle =
      static_cast<NativeSocket>(::accept(static_cast<SOCKET>(handle_), nullptr, nullptr));
#else
  const auto handle = static_cast<NativeSocket>(::accept(handle_, nullptr, nullptr));
#endif
  if (handle == kInvalidSocket) {
    const int code = LastError();
    if (WouldBlock(code)) {
      return std::optional<TcpSocket>();
    }
    return Fail(ErrorCode::kIoFailure, "accueil : " + DescribeError(code));
  }
  TcpSocket accepted(handle);
  if (!SetNonBlocking(handle)) {
    return Fail(ErrorCode::kIoFailure, "accueil non bloquant : " + DescribeError(LastError()));
  }
  DisableNagle(handle);
  return std::optional<TcpSocket>(std::move(accepted));
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : handle_(other.handle_) {
  other.handle_ = kInvalidSocket;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
  if (this != &other) {
    Close();
    handle_ = other.handle_;
    other.handle_ = kInvalidSocket;
  }
  return *this;
}

void TcpSocket::Close() noexcept {
  if (handle_ != kInvalidSocket) {
    CloseNative(handle_);
    handle_ = kInvalidSocket;
  }
}

Result<TcpSocket> TcpSocket::Connect(std::string_view host, std::uint16_t port, int timeout_ms) {
  EnsureSubsystem();

  const std::string host_text(host);
  std::array<char, kPortTextSize> port_text = {};
  std::snprintf(port_text.data(), port_text.size(), "%u", static_cast<unsigned>(port));

  addrinfo hints = addrinfo();
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* candidates = nullptr;
  const int resolved = getaddrinfo(host_text.c_str(), port_text.data(), &hints, &candidates);
  if (resolved != 0 || candidates == nullptr) {
    return Fail(ErrorCode::kIoFailure, "hôte introuvable : " + host_text);
  }

  // Le dernier échec, gardé pour le rapporter si AUCUN candidat n'aboutit :
  // « aucune adresse n'a répondu » ne dit pas pourquoi, « connexion refusée »
  // si.
  Error last = Error{ErrorCode::kIoFailure, "aucune adresse pour " + host_text};
  for (addrinfo* candidate = candidates; candidate != nullptr; candidate = candidate->ai_next) {
    const NativeSocket handle = static_cast<NativeSocket>(
        socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol));
    if (handle == kInvalidSocket) {
      last = Error{ErrorCode::kIoFailure, "création du socket : " + DescribeError(LastError())};
      continue;
    }
    TcpSocket attempt(handle);

    if (!SetNonBlocking(handle)) {
      last = Error{ErrorCode::kIoFailure, "socket non bloquant : " + DescribeError(LastError())};
      continue;
    }

#if defined(_WIN32)
    const int started = ::connect(static_cast<SOCKET>(handle), candidate->ai_addr,
                                  static_cast<int>(candidate->ai_addrlen));
#else
    const int started = ::connect(handle, candidate->ai_addr, candidate->ai_addrlen);
#endif
    if (started != 0) {
      const int code = LastError();
      if (!ConnectInProgress(code)) {
        last = Error{ErrorCode::kIoFailure, "connexion : " + DescribeError(code)};
        continue;
      }
      Result<void> awaited = AwaitConnection(handle, timeout_ms);
      if (!awaited.has_value()) {
        last = awaited.error();
        continue;
      }
    }

    DisableNagle(handle);
    freeaddrinfo(candidates);
    return attempt;
  }

  freeaddrinfo(candidates);
  return std::unexpected(last);
}

Result<ReceiveResult> TcpSocket::Receive(std::span<std::uint8_t> into) {
  if (!is_open()) {
    return Fail(ErrorCode::kIoFailure, "lecture sur un socket fermé");
  }
  if (into.empty()) {
    return ReceiveResult{ReceiveState::kBytes, 0};
  }

#if defined(_WIN32)
  const int read = recv(static_cast<SOCKET>(handle_), reinterpret_cast<char*>(into.data()),
                        static_cast<int>(into.size()), 0);
#else
  const auto read = ::recv(handle_, into.data(), into.size(), 0);
#endif
  if (read > 0) {
    return ReceiveResult{ReceiveState::kBytes, static_cast<std::size_t>(read)};
  }
  // Zéro octet lu sur un socket EN FLUX signifie que le pair a fermé, jamais
  // « rien à lire » : cette dernière situation passe par une erreur de blocage.
  if (read == 0) {
    return ReceiveResult{ReceiveState::kClosed, 0};
  }
  const int code = LastError();
  if (WouldBlock(code)) {
    return ReceiveResult{ReceiveState::kWouldBlock, 0};
  }
  return Fail(ErrorCode::kIoFailure, "lecture : " + DescribeError(code));
}

Result<std::size_t> TcpSocket::Send(std::span<const std::uint8_t> bytes) {
  if (!is_open()) {
    return Fail(ErrorCode::kIoFailure, "écriture sur un socket fermé");
  }
  if (bytes.empty()) {
    return std::size_t{0};
  }

#if defined(_WIN32)
  const int written =
      send(static_cast<SOCKET>(handle_), reinterpret_cast<const char*>(bytes.data()),
           static_cast<int>(bytes.size()), 0);
#else
  // 🔴 `MSG_NOSIGNAL` : sans lui, écrire sur une liaison que le pair vient de
  // fermer lève SIGPIPE, qui TUE le processus au lieu de rendre une erreur.
  const auto written = ::send(handle_, bytes.data(), bytes.size(), MSG_NOSIGNAL);
#endif
  if (written >= 0) {
    return static_cast<std::size_t>(written);
  }
  const int code = LastError();
  if (WouldBlock(code)) {
    // Le tampon d'émission du système est plein ; l'appelant gardera son reste.
    return std::size_t{0};
  }
  return Fail(ErrorCode::kIoFailure, "écriture : " + DescribeError(code));
}

}  // namespace ro::net
