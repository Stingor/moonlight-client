#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/proto/framing.h"

namespace ro::proto {

// Accumule les octets d'un flux et en tire des trames entières.
//
// C'est TOUT ce que le fil réseau a le droit de faire : recevoir, découper,
// remettre. Il ne décode rien, ne touche à aucun état de jeu, et n'appelle
// personne. Il n'y a donc aucune règle à retenir sur ce qui est sûr d'être
// touché depuis quel fil — c'est ainsi qu'on supprime une famille de bugs au
// lieu de la gérer.
class FrameReader {
 public:
  // Ce qu'une tentative de retrait a donné.
  struct Taken {
    FrameStatus status = FrameStatus::kIncomplete;
    std::uint16_t opcode = 0;
    // La trame entière, opcode compris. Vide sauf si le statut dit complète.
    std::vector<std::uint8_t> frame;
  };

  // Verse des octets reçus du réseau.
  void Append(std::span<const std::uint8_t> bytes);

  // Réserve les `count` prochains octets : ils ne portent PAS de trame.
  //
  // Tout le flux n'est pas encadré. Le serveur de personnages répond à la
  // demande d'entrée par un identifiant de compte NU — quatre octets sans
  // opcode. Les découper reviendrait à lire un opcode dans un numéro de compte,
  // puis à consommer un nombre d'octets arbitraire : la session serait perdue
  // dès son deuxième paquet.
  void ExpectRaw(std::size_t count) noexcept { raw_expected_ += count; }

  // Octets bruts encore attendus. Nul en régime normal.
  [[nodiscard]] std::size_t expected_raw() const noexcept { return raw_expected_; }

  // Retire les octets réservés dès qu'ils sont TOUS arrivés ; vide sinon.
  [[nodiscard]] std::vector<std::uint8_t> TakeRaw();

  // Retire la prochaine trame complète.
  //
  // ⚠ Rend « incomplète » tant que des octets bruts sont attendus : ils sont en
  // tête du flux, et découper par-dessus rendrait une trame inventée.
  //
  // ⚠ Sur un opcode inconnu ou une trame corrompue, le lecteur N'AVANCE PAS :
  // il ne sait pas de combien, et deviner reviendrait à chercher un opcode au
  // milieu d'un corps de paquet. L'appelant doit fermer la session ; rappeler
  // Take rendra indéfiniment le même verdict, ce qui vaut mieux qu'un lecteur
  // qui se croit reparti.
  [[nodiscard]] Taken Take();

  // Les octets reçus et pas encore consommés, sans les consommer.
  //
  // Sert à lever une ambiguïté que le protocole laisse : le serveur de zone
  // fait précéder son acceptation d'un identifiant NU, mais pas son refus. Rien
  // dans les quatre premiers octets ne dit lequel des deux arrive ; ce qui les
  // SUIT le dit.
  [[nodiscard]] std::span<const std::uint8_t> pending_bytes() const noexcept {
    return std::span(buffer_).subspan(read_offset_);
  }

  // Octets reçus mais pas encore consommés.
  [[nodiscard]] std::size_t pending() const noexcept { return buffer_.size() - read_offset_; }

  // Une trame déjà découpée, telle qu'on s'en souvient.
  struct Recent {
    std::uint16_t opcode = 0;
    std::size_t size = 0;
  };

  // Les dernières trames découpées, de la plus ANCIENNE à la plus récente.
  //
  // Un opcode inconnu ne se lit presque jamais comme la faute du paquet qui le
  // porte : il se lit comme la faute du PRÉCÉDENT, dont la longueur annoncée
  // n'était pas celle que le serveur a écrite, si bien que le découpage
  // recommence au milieu d'un corps. Sans ce souvenir, il n'y a rien pour
  // désigner ce précédent, et le seul recours est de recapturer le flux.
  [[nodiscard]] std::vector<Recent> recent_frames() const;

 private:
  void Compact();
  void Remember(std::uint16_t opcode, std::size_t size);

  // Assez pour remonter à la cause sans peser sur une session de plusieurs
  // heures : le coupable est le paquet juste avant, et ses voisins situent la
  // scène.
  static constexpr std::size_t kRecentCapacity = 8;
  std::array<Recent, kRecentCapacity> recent_{};
  std::size_t recent_count_ = 0;
  std::size_t recent_next_ = 0;

  std::vector<std::uint8_t> buffer_;
  // Où commence la partie non consommée. Avancer un indice plutôt que d'effacer
  // en tête évite de recopier tout le reste à chaque trame.
  std::size_t read_offset_ = 0;
  // Octets en tête qui ne portent pas de trame.
  std::size_t raw_expected_ = 0;
};

}  // namespace ro::proto
