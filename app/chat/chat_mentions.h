#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"
#include "ro/sim/chat_mention.h"

namespace moonclient {

// L'écart le plus court entre deux alertes de mention, en millisecondes : une
// rafale de lignes qui nous nomment ne sonne qu'une fois.
inline constexpr std::uint32_t kMentionRingGapMs = 2000;

// LES LIGNES QUI NOUS NOMMENT, ARRIVÉES DEPUIS LA DERNIÈRE IMAGE : faut-il
// alerter le joueur — un son, la barre des tâches qui clignote ?
//
// ⚠ CHAQUE LIGNE SE JUGE UNE FOIS, à son arrivée : la veille retient les rangs
// de celles qui nous nomment, et le fil les relit sans rechercher un mot. Des
// règles neuves — un autre nom, d'autres mots-clés, une autre partie —
// rejugent le tampon entier, sans rien faire sonner.
class ChatMentionWatch {
 public:
  // Une partie commence : ce qui a été dit avant ne sonne pas.
  void StartSession(std::uint64_t last_seq) noexcept;
  // Vrai : une ligne neuve nous nomme, et la dernière alerte a plus de
  // `kMentionRingGapMs`.
  //
  // ⚠ LES LIGNES SE CONSOMMENT MÊME QUAND L'ALERTE SE TAIT : une mention tombée
  // pendant l'écart ne sonne pas après coup, à contretemps.
  [[nodiscard]] bool Advance(std::span<const ro::sim::ChatLine> chat,
                             const ro::sim::MentionRules& rules, std::uint32_t now_ms);

  // LES RÈGLES DE L'IMAGE : notre nom, notre compte, et les mots-clés du joueur
  // (« a;b »). Les mots ne se redécoupent que quand leur texte change.
  //
  // ⚠ LES RÈGLES RENDUES REGARDENT CETTE VEILLE : elles valent jusqu'à l'appel
  // suivant, et le fil les lit pendant toute l'image.
  [[nodiscard]] const ro::sim::MentionRules& RulesFor(std::string_view own_name,
                                                      std::uint32_t own_id,
                                                      std::string_view typed_words);

  // Les rangs des lignes du tampon qui nous nomment, croissants. ⚠ VALABLES
  // JUSQU'AU PROCHAIN `Advance`.
  [[nodiscard]] std::span<const std::uint64_t> naming_seqs() const noexcept { return naming_seqs_; }

 private:
  std::uint64_t seen_through_ = 0;
  std::uint32_t rang_at_ms_ = 0;
  // Une alerte a déjà sonné cette partie : sans lui, `rang_at_ms_` nul
  // retiendrait la première pendant les deux premières secondes de l'horloge.
  bool rang_ = false;
  // Les règles ont changé depuis le dernier `Advance` : le tampon se rejuge.
  bool rejudge_ = true;
  std::string own_name_;
  std::uint32_t own_id_ = 0;
  std::string typed_words_;
  std::vector<std::string> words_;
  ro::sim::MentionRules rules_;
  // ⚠ BORNÉS PAR LE TAMPON : un rang que le monde a oublié s'en va.
  std::vector<std::uint64_t> naming_seqs_;
};

}  // namespace moonclient
