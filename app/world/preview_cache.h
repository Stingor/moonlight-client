#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <utility>

namespace moonclient {

// LES APERÇUS QU'ON GARDE : une image par fichier, faite à la demande, au plus
// UNE NEUVE PAR IMAGE, et `capacity` au plus, les moins récemment demandées
// jetées d'abord.
//
// 🔴 UNE PAR IMAGE, parce que faire un aperçu coûte des dizaines de
// millisecondes : survoler vite une liste en ferait une par ligne traversée,
// et l'interface gèlerait. La demande qui attend se refait à l'image suivante.
//
// ⚠ UN ÉCHEC SE GARDE COMME UNE IMAGE : un fichier qui ne se lit pas ne se
// relit pas à chaque image du survol.
//
// `Picture` est ce que l'appelant montre : une texture dans le jeu, un entier
// dans un essai. Le cache ne sait pas la faire : `Get` reçoit la fabrique.
template <typename Picture>
class PreviewCache {
 public:
  explicit PreviewCache(std::size_t capacity) : capacity_(capacity) {}

  // Ce que montre la demande d'un fichier : l'image, l'échec, ou rien encore.
  struct Shown {
    const Picture* picture = nullptr;
    bool failed = false;

    [[nodiscard]] bool pending() const noexcept { return picture == nullptr && !failed; }
  };

  // 🔴 À APPELER UNE FOIS PAR IMAGE : c'est ce qui rend le droit d'en faire une.
  void BeginFrame() noexcept { made_this_frame_ = false; }

  // L'aperçu de `file`. `make(file)` rend un `std::optional<Picture>`, vide
  // pour un échec ; il n'est appelé que pour un fichier inconnu, et une fois
  // par image au plus.
  //
  // ⚠ UN APERÇU RENDU RESTE VALIDE jusqu'au prochain `Get` d'un fichier
  // inconnu, qui peut le jeter.
  template <typename Make>
  Shown Get(const std::string& file, Make&& make) {
    for (auto entry = entries_.begin(); entry != entries_.end(); ++entry) {
      if (entry->file == file) {
        entries_.splice(entries_.begin(), entries_, entry);
        return ShownOf(entries_.front());
      }
    }
    if (made_this_frame_ || capacity_ == 0) {
      return {};
    }
    made_this_frame_ = true;
    Entry fresh{.file = file, .picture = std::forward<Make>(make)(file)};
    while (entries_.size() >= capacity_) {
      entries_.pop_back();
    }
    entries_.push_front(std::move(fresh));
    return ShownOf(entries_.front());
  }

  // Jette tout. ⚠ ENTRE DEUX IMAGES : l'interface tient les aperçus rendus.
  void Clear() { entries_.clear(); }

  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

 private:
  struct Entry {
    std::string file;
    // Vide : le fichier n'a pas pu se faire.
    std::optional<Picture> picture;
  };

  [[nodiscard]] static Shown ShownOf(const Entry& entry) {
    return entry.picture.has_value() ? Shown{.picture = &*entry.picture, .failed = false}
                                     : Shown{.picture = nullptr, .failed = true};
  }

  std::size_t capacity_;
  // Les plus récemment demandés d'abord.
  std::list<Entry> entries_;
  bool made_this_frame_ = false;
};

}  // namespace moonclient
