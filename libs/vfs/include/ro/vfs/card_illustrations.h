#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// L'ILLUSTRATION D'UNE CARTE : le grand dessin que le client montre à sa place.
//
// 🔴 CE N'EST PAS LE NOM DE RESSOURCE DE L'OBJET, et c'est mesuré : la carte
// Poring (4001) a « 이름없는카드 » — « carte sans nom » — pour ressource, et
// TOUTES les cartes portent la même : c'est le dos de carte générique de la case
// de sac. Son illustration, elle, se nomme « 포링카드 », et seule cette table le
// dit. Composer le chemin sur la ressource montrerait le même dos mille cinq
// cents fois.
//
// ⚠ ET C'EST AUSSI ELLE QUI DIT QUI EST UNE CARTE. Un objet qu'elle ignore n'en
// est pas une. Le client natif le marque autrement — sa fiche « nil » a « sorry »
// pour ressource — et charger celle-là poserait le carton « now printing » sur
// toute boîte et tout costume. Ici l'absence suffit.
struct CardIllustrations {
  // ⚠ EN CP949, comme toute ressource : c'est un nom de fichier d'archive, et le
  // transcoder ferait chercher ce qui n'existe nulle part.
  std::unordered_map<std::uint32_t, std::string> by_id;

  // Le nom de fichier de l'illustration, ou rien pour ce qui n'est pas une carte.
  [[nodiscard]] std::string_view Find(std::uint32_t item_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_id.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id.empty(); }
};

// ⚠ `num2…` ET NON `card…` : le fichier se nomme par sa clé numérique, et
// `cardillustnametable` — qui semble le nom évident — n'existe pas.
inline constexpr std::string_view kCardIllustrationTablePath = "data\\num2cardillustnametable.txt";

// Lit la table. ABSENTE, elle rend une table vide plutôt qu'une erreur : les
// cartes montreront alors leur image de collection, comme les autres objets.
[[nodiscard]] core::Result<CardIllustrations> ReadCardIllustrations(Vfs& vfs);

}  // namespace ro::vfs
