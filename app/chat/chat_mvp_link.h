#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/sim/chat_line.h"
#include "ro/sim/mvp_tracker.h"

namespace moonclient {

// ── LE LIEN D'UN RESPAWN DE MVP ─────────────────────────────────────────────
//
//   `<MVPL>mob:carte:mort:exact:d1:d2:tx:ty:libellé</MVPL>`
//
// Une balise À NOUS, de chat seulement, lue aussi par le relais Discord et le
// site : son format n'est pas libre. Huit champs séparés par ':', puis le
// libellé EN DERNIER, qui peut porter des ':'.
//
// 🔴 C'EST UN FAIT DATÉ, PAS UNE DÉSIGNATION : la mort, l'instant du retour et
// la tombe voyagent tels que l'expéditeur les a vus. Le libellé lisible se
// compose chez le LECTEUR, dans son fuseau et dans sa langue.

inline constexpr std::string_view kMvpLinkOpen = "<MVPL>";
inline constexpr std::string_view kMvpLinkClose = "</MVPL>";

struct MvpLink {
  // Le monstre désigné, non nul même pour un créneau scripté.
  std::uint32_t monster_id = 0;
  // Le nom interne de la carte.
  std::string map;
  // L'heure de mort, en secondes UNIX ; zéro : inconnue.
  std::int64_t kill_time_s = 0;
  // L'instant exact du retour, en secondes UNIX ; zéro : seule une fenêtre se
  // déduit.
  std::int64_t exact_respawn_s = 0;
  // Le retour au plus tôt après la mort, et l'amplitude du tirage, en minutes.
  std::uint16_t delay1_min = 0;
  std::uint16_t delay2_min = 0;
  // La cellule de la tombe.
  std::int16_t tomb_x = ro::sim::kMvpNoTomb;
  std::int16_t tomb_y = ro::sim::kMvpNoTomb;
  // Le NOM du MVP, en UTF-8.
  std::string name;
};

// La balise. VIDE si la carte ou le nom est vide, ou si un '<' ou un '>' se
// trouve n'importe où dans le corps : il fermerait la balise d'un autre.
[[nodiscard]] std::string BuildMvpLink(const MvpLink& link);

// CE QUE LA LECTURE D'UNE BALISE A DONNÉ.
enum class MvpLinkRead : std::uint8_t {
  // Lue : `out` est rempli, `after` désigne l'octet qui suit la fermante.
  kRead,
  // Le texte ne commence pas par l'ouvrante.
  kNoTag,
  // Pas de fermante après l'ouvrante : `after` vaut la taille du texte.
  // L'ouvrante n'est alors que du texte, et ce qui la suit se lit encore.
  kUnclosed,
  // Une fermante, mais un corps qui n'apprend rien (champ manquant, carte ou
  // libellé vide, ni mort ni instant) : `after` et `body` sont posés, et la
  // ligne survit en texte.
  kUnreadable,
};

// Relit une balise qui COMMENCE à `text[0]`. `out`, `after` et `body` peuvent
// être nuls.
[[nodiscard]] MvpLinkRead ParseMvpLink(std::string_view text, MvpLink* out, std::size_t* after,
                                       std::string_view* body = nullptr);
// La position de la prochaine ouvrante à partir de `from`, ou `npos`.
[[nodiscard]] std::size_t FindMvpLink(std::string_view text, std::size_t from);

// Le libellé lu par tous, composé CHEZ LE LECTEUR (son fuseau, sa langue). Une
// tombe connue s'y cite avant le crochet final, « , tombe en x,y » : la forme
// du relais Discord et du site, qui lisent la même balise.
[[nodiscard]] std::string MvpLinkLabel(const MvpLink& link);

// LA VIE D'UN LIEN « MORT À » SANS LOI, en secondes après la mort. La plus
// longue loi des scripts du serveur est de 87 000 000 ms, environ 24 h 10 :
// au-delà, une mort sans loi ne dit plus rien d'un retour.
inline constexpr std::int64_t kMvpLawlessLinkLifetimeS =
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(25)).count();
// La fenêtre est-elle échue à `server_now_s` ? Un instant exact l'est une fois
// passé, une fenêtre une fois refermée, et un lien « mort à » sans loi
// `kMvpLawlessLinkLifetimeS` après la mort. Sans aucune heure, jamais.
// `catalogue`, facultatif, est celui du lecteur : il prête sa loi à un lien
// « mort à » qui n'en porte pas.
[[nodiscard]] bool MvpLinkPassed(const MvpLink& link, std::int64_t server_now_s,
                                 const ro::sim::MvpTrackerBook* catalogue = nullptr);
// « HH:MM », l'heure murale LOCALE d'un instant UNIX ; vide si la conversion
// échoue.
[[nodiscard]] std::string MvpClockText(std::int64_t unix_s);

// L'EXPÉDITEUR D'UNE LIGNE, pour l'import : le texte avant " :" s'il tient en
// `kMvpSenderMostWireBytes` octets DU FIL, pour les seuls types qui portent un
// locuteur (parole publique, groupe, guilde, clan) ; vide sinon.
//
// ⚠ LA BORNE SE MESURE DANS L'ENCODAGE DU FIL, pas en UTF-8 : un nom de
// personnage est un identifiant CP1252, un caractère par octet. Mesurée sur
// l'UTF-8, elle ferait perdre son expéditeur à un pseudo accentué.
inline constexpr std::size_t kMvpSenderMostWireBytes = 24;
// Une ligne de salon, « Nom : texte » : sa tête porte jusqu'à 40 octets du fil.
inline constexpr std::size_t kMvpRoomSenderMostWireBytes = 40;
[[nodiscard]] std::string MvpSenderOf(ro::sim::ChatType type, std::string_view text);
// L'EXPÉDITEUR D'UNE LIGNE DU FIL : celui de `MvpSenderOf`, sauf qu'une ligne du
// SERVEUR n'en a jamais. ⚠ LE SERVEUR ÉCRIT AU JOUEUR SOUS LA CASE « CHAT
// PUBLIC » : sans cette garde, « Autolootid : … » se lirait comme un pseudo.
[[nodiscard]] std::string MvpSenderOfLine(ro::sim::ChatKind kind, ro::sim::ChatType type,
                                          std::string_view text);
[[nodiscard]] std::string MvpRoomSenderOf(std::string_view text);
// UN CHUCHOTEMENT SE RÉSOUT PAR SON SENS, jamais par son en-tête : reçu, c'est
// le pair, son auteur ; écho de notre envoi ou envoi non remis, c'est NOUS
// (`own_name`, vide s'il n'est pas connu) ; toute autre ligne, personne.
[[nodiscard]] std::string MvpWhisperSenderOf(const ro::sim::WhisperFacts& whisper,
                                             std::string_view own_name);
// LE MÊME CHUCHOTEMENT, RELU DANS LE JOURNAL : reçu, c'est encore le pair. Mais
// l'écho d'un envoi n'y a PAS D'AUTEUR CONNU : le journal est celui de toute
// l'installation, tous personnages confondus, et il n'écrit pas qui a envoyé.
// Le personnage en jeu n'est pas forcément celui-là.
[[nodiscard]] std::string MvpPastWhisperSenderOf(const ro::sim::WhisperFacts& whisper);

}  // namespace moonclient
