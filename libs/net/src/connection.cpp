#include "ro/net/connection.h"

#include <array>
#include <cstdio>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// Taille d'une lecture. Elle n'a pas à contenir une trame entière — le lecteur
// recolle ce qui traverse deux lectures — mais une lecture par octet coûterait
// un appel système par octet.
constexpr std::size_t kReceiveChunkBytes = 16 * 1024;

// Combien de lectures au plus par tour de boucle.
//
// La boucle s'arrête d'elle-même dès que le socket n'a plus rien ; ce plafond
// garantit qu'elle s'arrête AUSSI face à un pair qui écrit aussi vite qu'on
// lit. Sans lui, un tour de boucle pourrait ne jamais rendre la main, et le
// client se figerait sans qu'aucune erreur ne soit signalée.
constexpr int kMaxReceiveCallsPerPoll = 64;

// Au-delà de ce gaspillage en tête, le tampon d'émission se replie. Même raison
// que pour le tampon de réception : un indice qui n'avance que vers l'avant
// laisse la mémoire croître pendant toute une session.
constexpr std::size_t kOutputCompactionThreshold = 64 * 1024;

// L'opcode tel qu'il est écrit partout ailleurs — dans les sources du serveur,
// dans la table du client, dans les relevés. Le donner en décimal obligerait à
// le convertir avant de pouvoir chercher où que ce soit.
std::string HexOpcode(std::uint16_t opcode) {
  std::array<char, 8> text = {};
  std::snprintf(text.data(), text.size(), "0x%04x", static_cast<unsigned>(opcode));
  return text.data();
}

// Ce que le découpage a rendu juste avant de buter, en clair.
//
// Le coupable d'un opcode inconnu est presque toujours le paquet PRÉCÉDENT :
// sa longueur annoncée n'était pas celle que le serveur a écrite, et le
// découpage a repris au milieu de son corps. Nommer ce précédent avec sa
// taille suffit à le confronter à la table.
std::string DescribeRecent(const std::vector<proto::FrameReader::Recent>& recent) {
  if (recent.empty()) {
    return {};
  }
  std::string text = " ; trames précédentes :";
  for (const proto::FrameReader::Recent& frame : recent) {
    text += " " + HexOpcode(frame.opcode) + "(" + std::to_string(frame.size) + ")";
  }
  return text;
}

// L'opcode de la trame qu'un auteur de parole décrit, ou `std::nullopt` s'il ne
// se lit pas.
std::optional<std::uint16_t> AnnouncedOpcode(std::span<const std::uint8_t> author) {
  proto::ZcMoonlightChatAuthor fixed;
  if (!proto::Decode(author, fixed)) {
    return std::nullopt;
  }
  return fixed.next_opcode;
}

}  // namespace

core::Result<Connection> Connection::Open(std::string_view host, std::uint16_t port,
                                          int timeout_ms) {
  auto socket = TcpSocket::Connect(host, port, timeout_ms);
  if (!socket.has_value()) {
    return std::unexpected(socket.error());
  }
  return Connection(std::move(*socket));
}

void Connection::Close() noexcept { socket_.Close(); }

bool Connection::Flush() {
  while (sent_offset_ < outgoing_.size()) {
    const std::span<const std::uint8_t> rest = std::span(outgoing_).subspan(sent_offset_);
    const auto written = socket_.Send(rest);
    if (!written.has_value()) {
      error_ = written.error();
      return false;
    }
    if (*written == 0) {
      // Le tampon du système est plein : le reste attendra le prochain tour.
      break;
    }
    sent_offset_ += *written;
  }

  if (sent_offset_ == outgoing_.size()) {
    outgoing_.clear();
    sent_offset_ = 0;
  } else if (sent_offset_ >= kOutputCompactionThreshold) {
    outgoing_.erase(outgoing_.begin(),
                    outgoing_.begin() + static_cast<std::ptrdiff_t>(sent_offset_));
    sent_offset_ = 0;
  }
  return true;
}

void Connection::Send(std::span<const std::uint8_t> bytes) {
  // ⚠ UNE LIAISON FERMÉE NE SE ROUVRE PAS : ce qu'on lui confierait ne partirait
  // jamais, et s'entasserait pour toute la séance.
  if (!socket_.is_open()) {
    return;
  }
  if (send_gate_ && !send_gate_(bytes)) {
    return;
  }
  if (send_observer_) {
    send_observer_(bytes);
  }
  outgoing_.insert(outgoing_.end(), bytes.begin(), bytes.end());
  static_cast<void>(Flush());
}

void Connection::Receive() {
  received_ = LinkStatus::kOpen;
  if (!socket_.is_open()) {
    received_ = LinkStatus::kIoError;
    return;
  }
  if (!Flush()) {
    received_ = LinkStatus::kIoError;
    Close();
    return;
  }

  std::array<std::uint8_t, kReceiveChunkBytes> chunk = {};
  for (int call = 0; call < kMaxReceiveCallsPerPoll; ++call) {
    const auto received = socket_.Receive(chunk);
    if (!received.has_value()) {
      error_ = received.error();
      received_ = LinkStatus::kIoError;
      break;
    }
    if (received->state == ReceiveState::kWouldBlock) {
      break;
    }
    if (received->state == ReceiveState::kClosed) {
      received_ = LinkStatus::kClosedByPeer;
      break;
    }
    const std::span<const std::uint8_t> fresh = std::span(chunk).first(received->bytes);
    if (observer_) {
      observer_(fresh);
    }
    reader_.Append(fresh);
  }
}

void Connection::Requeue(std::vector<Frame> frames) {
  requeued_.insert(requeued_.end(), std::make_move_iterator(frames.begin()),
                   std::make_move_iterator(frames.end()));
}

Connection::Polled Connection::Extract() {
  Polled polled;
  polled.status = received_;

  // Les octets non encadrés passent AVANT le découpage : ils sont en tête du
  // flux, et le découpeur refuse de travailler tant qu'ils sont là.
  polled.raw = reader_.TakeRaw();

  // Les trames rendues sont plus anciennes que tout ce que le tampon porte.
  polled.frames = std::move(requeued_);
  requeued_.clear();

  // Le découpage a lieu même quand la liaison vient de tomber : ce que le
  // serveur a envoyé avant de fermer explique souvent pourquoi il a fermé.
  while (true) {
    auto taken = reader_.Take();
    if (taken.status == proto::FrameStatus::kComplete) {
      if (taken.opcode == proto::ZcMoonlightChatAuthor::kOpcode) {
        // Un second auteur remplace le premier : seul le dernier décrit la
        // trame qui vient.
        pending_author_ = std::move(taken.frame);
        continue;
      }
      Frame frame{taken.opcode, std::move(taken.frame)};
      if (!pending_author_.empty()) {
        if (AnnouncedOpcode(pending_author_) == frame.opcode) {
          frame.preamble = std::move(pending_author_);
        }
        pending_author_.clear();
      }
      polled.frames.push_back(std::move(frame));
      continue;
    }
    if (taken.status == proto::FrameStatus::kIncomplete) {
      break;
    }
    // Opcode inconnu ou trame impossible : le lecteur n'avancera plus, et
    // deviner de combien reviendrait à chercher un opcode au milieu d'un corps
    // de paquet. La session est perdue.
    error_ = core::Error{
        core::ErrorCode::kCorruptData,
        (taken.status == proto::FrameStatus::kUnknownOpcode ? "opcode inconnu : "
                                                            : "longueur impossible pour ") +
            HexOpcode(taken.opcode) + DescribeRecent(reader_.recent_frames())};
    polled.status = LinkStatus::kProtocolError;
    break;
  }

  if (polled.status != LinkStatus::kOpen) {
    Close();
  }
  return polled;
}

}  // namespace ro::net
