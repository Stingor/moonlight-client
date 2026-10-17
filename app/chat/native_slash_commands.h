#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace moonclient {

// LE CLIENT DU JOUEUR CONNAÎT-IL CETTE COMMANDE ?
//
// Il en connaît quelques centaines, qu'il traite lui-même. Une ligne en « / »
// qu'il ne connaît PAS part au serveur comme une parole ordinaire, barre
// oblique comprise, et tout le monde la lit. Nous faisons pareil : une commande
// que ni lui ni nous ne connaissons se dit, une commande qu'il connaît et que
// nous n'avons pas encore reprise se tait.
//
// ⚠ L'INCONNUE SE DIT À VUE : c'est une parole comme une autre, que le joueur
// voit revenir dans sa conversation — une faute de frappe ne part pas en
// silence. La connue ne part pas du tout, et une ligne grise le dit.
//
// 🔴 LA LISTE EST DONC UNE BARRIÈRE, PAS UN INVENTAIRE : un nom qui y manque
// fait partir en public ce que le joueur croyait confier au client — un
// message de guilde, un nom d'échange, un volume. Elle porte tous les noms que
// le client compare et qu'un clavier latin écrit — alias en frappe coréenne
// compris (« /rjfo ») —, même ceux que nous traitons avant d'arriver ici : une
// prise qui recule ne doit pas ouvrir de fuite.
//
// Seul le premier mot compte, et la casse latine ne compte pas.
[[nodiscard]] bool NativeKnowsSlashCommand(std::string_view line);

// Les noms eux-mêmes, dans l'ordre des octets UTF-8 : de quoi vérifier, nom par
// nom, qu'aucun ne part en public.
[[nodiscard]] std::span<const std::string_view> NativeSlashCommandNames();

// LA LIGNE QUI DIT AU JOUEUR QU'UNE COMMANDE CONNUE S'EST TUE : rien n'est
// parti, et il le sait. Traduite, la commande tapée en queue.
[[nodiscard]] std::string SilencedCommandNotice(std::string_view line);

// Sa teinte, en `0xRRGGBB` : un gris discret, qui ne se confond avec aucune
// parole ni avec l'écho jaune d'un réglage.
inline constexpr std::uint32_t kSilencedCommandColor = 0xA0A0A0;

}  // namespace moonclient
