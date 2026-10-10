#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "ro/net/world_update.h"

namespace ro::net {

// Ce que les trames reçues sont devenues, opcode par opcode.
//
// POURQUOI. Un opcode que le traducteur ne traite pas rend `kUnhandled`, et ce
// verdict ne va NULLE PART : le jeu ne montre rien et ne se plaint de rien. Le
// joueur n'a alors qu'une impression — « ça ne marche pas » —, sans pouvoir
// dire si le paquet est arrivé, s'il a été refusé par le décodeur, ou si le
// serveur ne l'a jamais envoyé. Le compteur tranche ces trois cas.
//
// Il ne lit aucune horloge et n'écrit nulle part : un rejeu headless le remplit
// exactement comme la boucle de jeu, et la mise en forme est le travail de
// l'appelant.
//
// ⚠ Il compte par OPCODE, pas par cause. Un opcode absent de la table de
// traduction et un opcode traité dont ce cas-là ne l'est pas — un type d'acte
// inconnu, un paramètre autre que la vitesse — comptent tous deux sous leur
// opcode. C'est assez pour savoir OÙ regarder ; il faut lire le code pour
// savoir quoi y écrire.
class FrameTally {
 public:
  // Le relevé d'un opcode. Les quatre compteurs PARTITIONNENT ses trames :
  // leur somme est le nombre de fois où il est arrivé.
  struct Entry {
    std::uint16_t opcode = 0;
    std::uint32_t applied = 0;
    std::uint32_t ignored = 0;
    std::uint32_t unhandled = 0;
    std::uint32_t undecodable = 0;

    // Les trames qui n'ont RIEN donné : ni traduites, ni décodables. Ce sont
    // les seules qui décrivent du travail restant.
    [[nodiscard]] std::uint32_t missing() const noexcept { return unhandled + undecodable; }
    [[nodiscard]] std::uint32_t total() const noexcept { return applied + ignored + missing(); }
  };

  // Range le verdict qu'`ApplyFrame` vient de rendre sur cette trame.
  void Record(std::uint16_t opcode, FrameOutcome outcome);

  // UNE TRAME RÉCENTE, pour la console de mise au point : son rang d'arrivée,
  // son verdict, sa taille et ses premiers octets — assez pour y chercher
  // l'identifiant d'un acteur, qui se tient presque toujours en tête.
  struct Recent {
    std::uint32_t serial = 0;
    std::uint16_t opcode = 0;
    FrameOutcome outcome = FrameOutcome::kApplied;
    std::uint32_t size = 0;
    std::vector<std::uint8_t> head;
  };
  static constexpr std::size_t kRecentKept = 1024;
  static constexpr std::size_t kRecentHeadBytes = 64;

  // Range le verdict ET garde la trame parmi les récentes.
  void Record(const Frame& frame, FrameOutcome outcome);

  // Les dernières trames, de la plus ancienne à la plus récente.
  [[nodiscard]] const std::deque<Recent>& recent() const noexcept { return recent_; }

  // Les opcodes dont au moins une trame s'est perdue, du plus fréquent au moins
  // fréquent, et à égalité par opcode croissant. L'ordre est celui du TRAVAIL :
  // ce qui manque le plus souvent se répare en premier.
  [[nodiscard]] std::vector<Entry> Missing() const;

  // Tout ce qui est passé, par opcode croissant.
  [[nodiscard]] std::vector<Entry> All() const;

  // Le nombre de trames rangées, et celles qui n'ont rien donné.
  [[nodiscard]] std::uint32_t frames() const noexcept { return frames_; }
  [[nodiscard]] std::uint32_t missing_frames() const noexcept { return missing_frames_; }

  void Clear();

 private:
  // Triée par opcode : c'est ce qui donne un ordre stable à `All()` sans avoir
  // à trier à chaque consultation.
  std::map<std::uint16_t, Entry> by_opcode_;
  std::uint32_t frames_ = 0;
  std::uint32_t missing_frames_ = 0;
  std::deque<Recent> recent_;
};

// Combien d'opcodes tiennent sur une ligne du relevé.
inline constexpr std::size_t kTallyOpcodesPerLine = 3;

// Le relevé en français : une ligne de résumé, puis les opcodes perdus groupés
// par ligne.
//
// `max_lines` est ce que l'affichage peut montrer — le journal du chat n'en
// tient que huit. Ce qui déborde n'est pas tu : la dernière ligne dit combien
// d'opcodes elle n'a pas nommés. La liste complète, elle, se lit dans le
// journal, que `All()` nourrit sans plafond.
//
// Rend toujours au moins la ligne de résumé, et jamais plus de `max_lines`.
[[nodiscard]] std::vector<std::string> DescribedFrameTally(const FrameTally& tally,
                                                           std::size_t max_lines);

// « 0x0284 » : la forme sous laquelle tous les relevés du projet nomment un
// opcode, donc celle qui se recopie telle quelle dans une recherche.
[[nodiscard]] std::string HexOpcode(std::uint16_t opcode);

}  // namespace ro::net
