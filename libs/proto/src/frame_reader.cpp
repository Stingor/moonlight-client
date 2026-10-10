#include "ro/proto/frame_reader.h"

#include <algorithm>

namespace ro::proto {
namespace {

// Au-delà de ce gaspillage en tête, le tampon se replie.
//
// Sans repli, l'indice de lecture n'avancerait que vers l'avant et le tampon
// grandirait indéfiniment sur une session longue — un client de jeu en tient
// plusieurs heures. Replier À CHAQUE trame recopierait le reste à chaque fois ;
// attendre un seuil amortit ce coût sur beaucoup de trames.
constexpr std::size_t kCompactionThreshold = 64 * 1024;

}  // namespace

void FrameReader::Append(std::span<const std::uint8_t> bytes) {
  buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

void FrameReader::Remember(std::uint16_t opcode, std::size_t size) {
  recent_[recent_next_] = Recent{opcode, size};
  recent_next_ = (recent_next_ + 1) % kRecentCapacity;
  recent_count_ = std::min(recent_count_ + 1, kRecentCapacity);
}

std::vector<FrameReader::Recent> FrameReader::recent_frames() const {
  std::vector<Recent> ordered;
  ordered.reserve(recent_count_);
  // Le plus ancien se trouve juste APRÈS le prochain emplacement à écrire, une
  // fois le tampon plein ; avant cela il est en tête.
  const std::size_t first = recent_count_ < kRecentCapacity ? 0 : recent_next_ % kRecentCapacity;
  for (std::size_t step = 0; step < recent_count_; ++step) {
    ordered.push_back(recent_[(first + step) % kRecentCapacity]);
  }
  return ordered;
}

void FrameReader::Compact() {
  if (read_offset_ == 0) {
    return;
  }
  buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(read_offset_));
  read_offset_ = 0;
}

std::vector<std::uint8_t> FrameReader::TakeRaw() {
  if (raw_expected_ == 0 || pending() < raw_expected_) {
    return {};
  }
  const std::span<const std::uint8_t> raw = std::span(buffer_).subspan(read_offset_, raw_expected_);
  std::vector<std::uint8_t> taken(raw.begin(), raw.end());
  read_offset_ += raw_expected_;
  raw_expected_ = 0;
  if (read_offset_ == buffer_.size()) {
    Compact();
  }
  return taken;
}

FrameReader::Taken FrameReader::Take() {
  if (raw_expected_ > 0) {
    // Les octets attendus sont en tête : découper par-dessus rendrait une trame
    // inventée à partir d'un contenu qui n'en est pas une.
    return Taken();
  }
  const std::span<const std::uint8_t> pending_bytes = std::span(buffer_).subspan(read_offset_);
  const FrameResult result = NextFrame(pending_bytes);

  Taken taken;
  taken.status = result.status;
  taken.opcode = result.opcode;
  if (result.status != FrameStatus::kComplete) {
    // Rien n'est consommé, y compris sur une erreur : le lecteur ne sait pas de
    // combien avancer, et l'appelant doit fermer plutôt que reprendre.
    return taken;
  }

  const std::span<const std::uint8_t> frame = pending_bytes.first(result.size);
  taken.frame.assign(frame.begin(), frame.end());
  read_offset_ += result.size;
  Remember(result.opcode, result.size);

  if (read_offset_ >= kCompactionThreshold) {
    Compact();
  } else if (read_offset_ == buffer_.size()) {
    // Tout est consommé : le repli est gratuit, et il rend le tampon vide au
    // lieu de le laisser croître jusqu'au seuil.
    Compact();
  }
  return taken;
}

}  // namespace ro::proto
