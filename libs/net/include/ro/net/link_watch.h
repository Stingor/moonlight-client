#pragma once

#include <cstdint>
#include <optional>

namespace ro::net {

// LE CHIEN DE GARDE DE LA LIAISON : un réseau mort ne ferme rien.
//
// 🔴 UNE LIAISON COUPÉE EN ROUTE NE DIT RIEN. Sans FIN ni RST — un câble
// débranché, une box qui redémarre, un portable qui change de réseau —, la
// socket reste ouverte et muette : aucune lecture n'échoue, et la partie
// continue devant un monde figé. Seul le SILENCE trahit la panne.
//
// ⚠ ET LE SILENCE N'EST PAS UNE OPINION : le battement d'horloge part toutes
// les `kTickIntervalMs` tant que la liaison est ouverte, et le serveur répond à
// chacun. Trente secondes sans une seule trame, ce sont trois battements restés
// sans réponse — pas une carte calme où personne ne parle.
//
// ⚠ CE QUI GÈLE LA BOUCLE NE COMPTE PAS. Une fenêtre qu'on déplace ou qu'on
// réduit, un chargement de carte qui bloque : pendant ce temps on ne LIT pas,
// donc ne rien avoir lu ne prouve rien. Un écart entre deux images au-delà de
// `kLoopStallMs` fait repartir l'horloge du silence de zéro.
//
// LOGIQUE PURE : aucune horloge lue ici, aucune socket. L'appelant passe
// l'instant ; c'est ce qui rend chaque seuil essayable.

// Au-delà, la liaison est déclarée INSTABLE : un bandeau le dit, et il
// disparaît à la première trame.
inline constexpr std::uint32_t kLinkUnsteadyMs = 15000;
// Au-delà, la liaison est déclarée PERDUE.
inline constexpr std::uint32_t kLinkSilentLossMs = 30000;
// Un écart entre deux images plus long que celui-ci est un GEL de la boucle, et
// non un silence du serveur. Une image dure quelques dizaines de millisecondes ;
// deux secondes, c'est une fenêtre tenue ou un chargement.
inline constexpr std::uint32_t kLoopStallMs = 2000;

enum class LinkHealth : std::uint8_t {
  kSteady,
  kUnsteady,
  kLost,
};

class LinkWatch {
 public:
  // L'HORLOGE DU SILENCE REPART de cet instant : liaison neuve, changement de
  // serveur de zone, carte qu'on vient de charger.
  void Restart(std::uint32_t now_ms) noexcept;

  // UNE IMAGE DE LA BOUCLE, à noter AVANT la lecture de la liaison : c'est
  // l'écart avec la précédente qui dit si la boucle a gelé.
  void NoteLoop(std::uint32_t now_ms) noexcept;

  // UNE LECTURE QUI A RAPPORTÉ AU MOINS UNE TRAME, quelle qu'elle soit.
  void NoteReceived(std::uint32_t now_ms) noexcept;

  // ⚠ STABLE tant que rien n'a armé la garde : sans instant de départ, il n'y
  // a pas de silence à mesurer.
  [[nodiscard]] LinkHealth Health(std::uint32_t now_ms) const noexcept;

 private:
  // D'où se compte le silence : la dernière trame, ou le dernier départ.
  std::optional<std::uint32_t> heard_ms_;
  // La dernière image notée.
  std::optional<std::uint32_t> loop_ms_;
};

// LE PASSAGE VERS UN AUTRE SERVEUR DE ZONE A-T-IL TROP TARDÉ ?
//
// 🔴 L'ENTRÉE N'EXPIRE PAS D'ELLE-MÊME : un serveur qui accepte la connexion
// puis se tait la laisse en attente pour toujours, et la garde du silence ne
// compte pas pendant ce temps — il n'y a pas encore de liaison à écouter. Le
// même délai que le silence la borne : au-delà, c'est une perte.
[[nodiscard]] bool ZoneMoveOverdue(std::uint32_t started_ms, std::uint32_t now_ms) noexcept;

}  // namespace ro::net
