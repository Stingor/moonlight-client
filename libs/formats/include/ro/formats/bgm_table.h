#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace ro::formats {

// La MUSIQUE de chaque carte, telle que le client la déclare.
//
// Le fichier est du texte, une ligne par carte :
//
//   prontera.rsw#bgm\\08.mp3#
//
// Trois pièges, tous les trois muets si on les manque. Les commentaires
// commencent par deux barres obliques, et servent aussi à DÉSACTIVER une
// entrée — une carte peut ainsi paraître deux fois, dont une commentée. Le
// chemin y est écrit avec des antislashs DOUBLÉS, comme dans une chaîne de
// programme. Et la clé porte l'extension .rsw, que personne n'emploie pour
// nommer une carte ailleurs.
// Où le client range cette table.
inline constexpr std::string_view kBgmTablePath = "data\\mp3nametable.txt";

class BgmTable {
 public:
  // Lit la table. N'échoue pas : une ligne qu'on ne comprend pas est ignorée,
  // et une table vide vaut mieux qu'un client qui refuse de démarrer parce
  // qu'un serveur a écrit une ligne de travers.
  [[nodiscard]] static BgmTable Parse(std::span<const std::uint8_t> bytes);

  // Le fichier de musique d'une carte, relatif à la RACINE DU CLIENT et non à
  // l'archive : les musiques ne sont pas dans le GRF. Vide pour une carte que
  // la table ne nomme pas — elle se joue alors en silence.
  [[nodiscard]] std::string_view TrackFor(std::string_view map_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return tracks_.size(); }

 private:
  // Par nom de carte en minuscules, sans extension.
  std::map<std::string, std::string, std::less<>> tracks_;
};

}  // namespace ro::formats
