#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace ro::voice {

// ── LA FILE D'UN LOCUTEUR : l'ordre des paquets, et ce qui manque ───────────
//
// Les paquets d'un locuteur arrivent en retard, en désordre, ou pas du tout.
// La file les range par numéro et en rend un toutes les 20 ms, après en avoir
// accumulé assez pour qu'un paquet un peu lent arrive encore à temps.

// Combien de trames attendre avant de jouer : 60 ms de marge contre la gigue.
inline constexpr std::size_t kSpeakerPrimeFrames = 3;
// Combien de trames manquées d'affilée closent une prise de parole : au-delà,
// il s'est tu, et la reprise attendra de nouveau sa marge.
inline constexpr std::size_t kSpeakerMostMissedFrames = 10;
// Combien de trames la file garde au plus : au-delà, elle a pris du retard et
// jette les plus anciennes pour rattraper le direct.
inline constexpr std::size_t kSpeakerMostQueuedFrames = 25;

enum class SpeakerStep : std::uint8_t {
  kPacket,  // le paquet attendu
  kLost,    // le paquet attendu manque, d'autres le suivent : à reconstruire
  kSilent,  // rien à jouer
};

struct SpeakerFrame {
  SpeakerStep step = SpeakerStep::kSilent;
  std::vector<std::uint8_t> packet;  // pour `kPacket`
  // POUR `kLost`, le paquet qui suit, s'il est là : il porte de quoi refaire
  // la trame perdue. Il reste dans la file.
  std::vector<std::uint8_t> next;
};

class SpeakerQueue {
 public:
  void Push(std::uint16_t sequence, std::span<const std::uint8_t> packet);
  // La trame des 20 ms qui viennent.
  [[nodiscard]] SpeakerFrame Pop();
  // Vrai entre la marge accumulée et le silence qui clôt la prise de parole.
  [[nodiscard]] bool playing() const noexcept { return playing_; }

 private:
  // Le numéro du fil n'a que 16 bits : il se déroule ici sur 64, d'après le
  // dernier reçu, pour que l'ordre survive au retour à zéro.
  [[nodiscard]] std::int64_t Unwrap(std::uint16_t sequence);

  std::map<std::int64_t, std::vector<std::uint8_t>> queued_;
  std::optional<std::int64_t> last_received_;
  std::uint16_t last_sequence_ = 0;
  std::int64_t next_ = 0;
  std::int64_t last_played_ = 0;
  bool playing_ = false;
  std::size_t missed_ = 0;
};

}  // namespace ro::voice
