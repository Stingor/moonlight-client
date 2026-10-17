#pragma once

#include <cstdint>
#include <span>

namespace ro::ui {

// « VOIR DANS L'ALBUM » : l'entrée que le menu de toute carte porte, où qu'elle
// se montre — le sac, le coffre, un lien du chat, une ligne de la base.
//
// Son clic ouvre l'album de cartes et en tourne les pages jusqu'à la pochette
// de cette carte. Une pochette scellée se montre aussi : sa silhouette dit ce
// qui reste à débloquer.
//
// ⚠ `ui` NE SAIT NI CE QU'EST UNE CARTE NI CE QUE L'ALBUM CONTIENT : `app`
// compose ces faits une fois par image, et le menu ne fait que les lire.

// CE QUE LE MENU D'UN OBJET SAIT DE L'ALBUM.
struct CardAlbumLinkFacts {
  // L'album s'ouvre-t-il dans cette partie ? Faux, l'entrée ne paraît nulle part.
  bool available = false;
  // LES CARTES que les tables de l'installation connaissent, triées par
  // identifiant.
  std::span<const std::uint32_t> card_ids;
  // LES POCHETTES du catalogue du serveur, triées par identifiant.
  std::span<const std::uint32_t> pocket_ids;
  // 🔴 LE SERVEUR A-T-IL DÉJÀ DONNÉ SON CATALOGUE ? Tant qu'il ne l'a pas fait,
  // l'absence d'une pochette ne prouve rien, et l'entrée reste offerte.
  bool pockets_known = false;
};

// CE QUE L'ENTRÉE DEVIENT pour un objet donné.
enum class CardAlbumEntry : std::uint8_t {
  // Ce n'est pas une carte, ou l'album n'est pas là : pas d'entrée.
  kAbsent,
  kOffered,
  // Une carte que le catalogue ne range nulle part : grisée, et dite au survol.
  kNoPocket,
};

// L'entrée pour `item_id`. `known_card` : l'appelant sait déjà que c'en est une
// — le serveur l'a dit d'un objet du sac —, même si les tables l'ignorent.
[[nodiscard]] CardAlbumEntry CardAlbumEntryOf(const CardAlbumLinkFacts& facts,
                                              std::uint32_t item_id, bool known_card = false);

// Pose l'entrée dans le menu ouvert, rien pour `kAbsent`. Rend vrai au clic.
[[nodiscard]] bool SeeInAlbumMenuItem(CardAlbumEntry entry);

}  // namespace ro::ui
