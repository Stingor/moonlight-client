#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/talk_mute.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// CE QUE LE SERVEUR DIT, par opposition à ce qu'il fait au monde.
//
// La parole, les bulles au-dessus des têtes, les annonces, l'heure et le
// battement de cœur. Aucun de ces paquets ne déplace un corps, n'en change
// l'apparence ni n'en pose l'état : ils portent ce que le serveur ÉCRIT.
//
// Ils vivent à part de `ApplyFrame` pour la même raison que les compétences :
// ils forment une famille, et `world_update.cpp` touchait son plafond de
// lignes. Un fichier qui ne porte qu'un sujet se relit.
//
// Même contrat que `ApplyFrame` : un acteur inconnu n'est pas inventé, et
// `now_ms` est l'horloge de l'APPELANT.
[[nodiscard]] FrameOutcome ApplyNoticeFrame(const Frame& frame, sim::World& world,
                                            std::uint32_t now_ms);

// CE QUE LE JOUEUR NE VEUT PAS ENTENDRE D'UNE ANNONCE. Chaque partie peut
// manquer : rien ne se tait alors, et rien ne se masque.
struct BroadcastFilter {
  // Ce nom, en UTF-8, est-il ignoré ? La voix « micc » d'un nom ignoré se tait
  // ENTIÈRE. ⚠ LE NOM SE COMPARE OCTET POUR OCTET : la casse compte.
  std::function<bool(std::string_view)> ignores;
  // Les mots masqués dans la voix « micc » et dans toute annonce colorée.
  const sim::BannedWords* banned = nullptr;
};

// LES DEUX ANNONCES (`ZC_BROADCAST`, `ZC_BROADCAST2`), filtrées ; `kUnhandled`
// pour toute autre trame.
//
// ⚠ À APPELER AVANT `ApplyFrame` quand on tient le filtre : sans lui, le monde
// les lit quand même, mais n'en tait ni n'en masque rien.
//
// ⚠ « TOUT BLOQUER » NE TAIT PAS UNE VOIX : seul un nom ignoré la fait taire.
[[nodiscard]] FrameOutcome ApplyBroadcastFrame(const Frame& frame, sim::World& world,
                                               const BroadcastFilter& filter, std::uint32_t now_ms);

// LA PAROLE DES JOUEURS — publique, son propre écho, groupe, guilde, clan —, ses
// mots interdits masqués ; `kUnhandled` pour toute autre trame. `banned` nul :
// rien ne se masque.
//
// ⚠ À APPELER AVANT `ApplyFrame` quand on tient la liste : le monde lit ces
// paquets aussi, mais sans elle.
//
// ⚠ LE CHUCHOTEMENT N'EN EST PAS : il paraît tel quel dans la conversation, et
// seule sa fenêtre 1:1 le masque.
//
// `mute` tait la ligne de la parole publique des autres — sa bulle reste —, de
// la guilde et du clan : voir `sim::TalkMute`.
[[nodiscard]] FrameOutcome ApplySpokenFrame(const Frame& frame, sim::World& world,
                                            const sim::BannedWords* banned, std::uint32_t now_ms,
                                            sim::TalkMute mute = {});

// LE MOTIF D'UNE ÉVICTION (`SC_NOTIFY_BAN`), ou rien si cette trame n'en est pas
// une. Le serveur coupe la liaison aussitôt après : c'est à `app` de dire la
// perte avec son motif — voir `BanText` et `EvictionText`.
[[nodiscard]] std::optional<std::uint8_t> EvictionOf(const Frame& frame);

}  // namespace ro::net
