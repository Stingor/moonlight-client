#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace ro::formats {

// La MUSIQUE de chaque carte, telle que le client la déclare.
//
// La table se lit dans l'installation par `vfs::ReadMapMusic`. Elle ne connaît
// que des cartes nommées en minuscules et sans extension ; c'est ce qui permet
// de l'interroger par un nom de carte tout court, dans une casse quelconque.
class BgmTable {
 public:
  BgmTable() = default;
  // Les cartes, en minuscules et sans extension, et leur musique.
  explicit BgmTable(std::map<std::string, std::string, std::less<>> tracks)
      : tracks_(std::move(tracks)) {}

  // Le fichier de musique d'une carte, relatif à la RACINE DU CLIENT et non à
  // l'archive : les musiques ne sont pas dans le GRF. Vide pour une carte que
  // la table ne nomme pas — elle se joue alors en silence.
  [[nodiscard]] std::string_view TrackFor(std::string_view map_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return tracks_.size(); }

  // Toutes les cartes nommées et leur musique : la carte en minuscules et sans
  // extension, le fichier tel que `TrackFor` le rend.
  [[nodiscard]] const std::map<std::string, std::string, std::less<>>& tracks() const noexcept {
    return tracks_;
  }

 private:
  // Par nom de carte en minuscules, sans extension.
  std::map<std::string, std::string, std::less<>> tracks_;
};

}  // namespace ro::formats
