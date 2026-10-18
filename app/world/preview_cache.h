#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace moonclient {

// LES APERÇUS QU'ON GARDE : une image par fichier, faite à la demande, sous un
// BUDGET PAR IMAGE, et `capacity` au plus, les moins récemment demandées jetées
// d'abord.
//
// 🔴 UN BUDGET, parce que faire un aperçu coûte de une à des dizaines de
// millisecondes : survoler vite une liste ou ouvrir une grille en ferait des
// centaines d'un coup, et l'interface gèlerait. Les demandes qui attendent se
// refont aux images suivantes, dans l'ordre où l'appelant les pose — les plus
// pressées d'abord.
//
// Le budget est un temps, en millisecondes, mesuré par l'horloge que
// l'appelant donne : on fait des aperçus tant que ceux de l'image ont coûté
// moins que lui. ⚠ LE PREMIER EST TOUJOURS PERMIS, même plus long que tout le
// budget : sans lui, un aperçu trop cher ne se ferait jamais. Un budget nul
// — et `Get` sans horloge — permet donc exactement UN aperçu par image.
//
// ⚠ UN ÉCHEC SE GARDE COMME UNE IMAGE : un fichier qui ne se lit pas ne se
// relit pas à chaque image du survol.
//
// `Picture` est ce que l'appelant montre : une texture dans le jeu, un entier
// dans un essai. Le cache ne sait pas la faire : `Get` reçoit la fabrique.
template <typename Picture>
class PreviewCache {
 public:
  explicit PreviewCache(std::size_t capacity, double budget_ms = 0.0)
      : capacity_(capacity), budget_ms_(budget_ms) {}
  // ⚠ PAS DE COPIE : l'index désigne les éléments de SA liste.
  PreviewCache(const PreviewCache&) = delete;
  PreviewCache& operator=(const PreviewCache&) = delete;
  PreviewCache(PreviewCache&&) = default;
  PreviewCache& operator=(PreviewCache&&) = default;
  ~PreviewCache() = default;

  // Ce que montre la demande d'un fichier : l'image, l'échec, ou rien encore.
  struct Shown {
    const Picture* picture = nullptr;
    bool failed = false;

    [[nodiscard]] bool pending() const noexcept { return picture == nullptr && !failed; }
  };

  // 🔴 À APPELER UNE FOIS PAR IMAGE : c'est ce qui rend le budget.
  void BeginFrame() noexcept {
    made_this_frame_ = 0;
    spent_this_frame_ms_ = 0.0;
  }

  // L'aperçu de `file`. `make(file)` rend un `std::optional<Picture>`, vide
  // pour un échec ; il n'est appelé que pour un fichier inconnu, et tant que le
  // budget de l'image le permet. `clock_ms()` rend l'heure en millisecondes.
  //
  // ⚠ UN APERÇU RENDU RESTE VALIDE jusqu'au prochain `Get` d'un fichier
  // inconnu, qui peut le jeter.
  template <typename Make, typename Clock>
  Shown Get(const std::string& file, Make&& make, Clock&& clock_ms) {
    if (const auto known = by_file_.find(file); known != by_file_.end()) {
      entries_.splice(entries_.begin(), entries_, known->second);
      return ShownOf(entries_.front());
    }
    if (capacity_ == 0 || (made_this_frame_ > 0 && spent_this_frame_ms_ >= budget_ms_)) {
      return {};
    }
    const double started_ms = clock_ms();
    Entry fresh{.file = file, .picture = std::forward<Make>(make)(file)};
    spent_this_frame_ms_ += clock_ms() - started_ms;
    ++made_this_frame_;
    while (entries_.size() >= capacity_) {
      by_file_.erase(entries_.back().file);
      entries_.pop_back();
    }
    entries_.push_front(std::move(fresh));
    by_file_.emplace(file, entries_.begin());
    return ShownOf(entries_.front());
  }

  // La même chose sans horloge : un aperçu neuf par image au plus.
  template <typename Make>
  Shown Get(const std::string& file, Make&& make) {
    return Get(file, std::forward<Make>(make), [] { return 0.0; });
  }

  // Jette tout. ⚠ ENTRE DEUX IMAGES : l'interface tient les aperçus rendus.
  void Clear() {
    by_file_.clear();
    entries_.clear();
  }

  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  // Les aperçus faits depuis `BeginFrame`.
  [[nodiscard]] std::size_t made_this_frame() const noexcept { return made_this_frame_; }

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
  double budget_ms_;
  // Les plus récemment demandés d'abord.
  std::list<Entry> entries_;
  // Chaque entrée par son fichier ; une liste ne bouge pas ses éléments, et
  // l'index reste juste à travers les `splice`.
  std::unordered_map<std::string, typename std::list<Entry>::iterator> by_file_;
  std::size_t made_this_frame_ = 0;
  double spent_this_frame_ms_ = 0.0;
};

}  // namespace moonclient
