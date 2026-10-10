#pragma once

#include <cstdint>
#include <unordered_map>

#include "ro/render/icon_texture.h"
#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES VIGNETTES DE L'ALBUM DE CARTES.
//
// Neuf cents pochettes, chacune avec l'illustration de sa carte. Le cache des
// icônes (`ItemIcons`) garde chaque illustration EN PLEINE TAILLE et pour
// toujours : c'est juste pour la carte qu'on survole, et ruineux pour un album
// qu'on feuillette. D'où celui-ci, qui ne partage rien avec lui :
//
//   · l'illustration est RÉDUITE à la taille exacte de la pochette avant d'être
//     téléversée (voir `card_thumbnail_pixels.h`) ;
//   · le cache est BORNÉ, et c'est la vignette la moins récemment servie qui
//     part — jamais une vignette servie à l'image en cours, que le rendu n'a
//     pas encore lue ;
//   · le chargement est ÉTALÉ : quelques vignettes par image, les autres
//     attendent la suivante. Tourner une page ne fige donc jamais l'écran, la
//     page se remplit sous les yeux.
class CardThumbnails {
 public:
  CardThumbnails(vfs::Vfs& vfs, const vfs::CardIllustrations* cards);

  // UNE FOIS PAR IMAGE, avant toute demande : rouvre le budget de chargement.
  void BeginFrame();

  // OUBLIE TOUT : le skin a changé. Les vignettes se redemandent à chaque image.
  void Clear() { entries_.clear(); }

  // Ce qu'une demande rend.
  struct Thumb {
    // Nul quand il n'y a rien à dessiner — voir `pending`.
    const IconTexture* texture = nullptr;
    // Vrai tant que la vignette attend son tour : l'appelant dessine un repli
    // et REDEMANDE à l'image suivante. Faux avec une texture nulle : la carte
    // n'a pas d'illustration, et c'est retenu.
    bool pending = false;
  };

  // La vignette de cette carte, réduite pour tenir dans (box_width, box_height)
  // PIXELS. `silhouette` = la version grise d'une pochette scellée. La clé est
  // (carte, silhouette, boîte) : un changement d'échelle d'interface change la
  // boîte, et les anciennes vignettes s'évincent d'elles-mêmes.
  [[nodiscard]] Thumb Get(std::uint32_t card_id, bool silhouette, int box_width, int box_height);

 private:
  struct Entry {
    IconTexture texture;
    bool loaded = false;
    bool missing = false;
    std::uint64_t last_frame = 0;
    std::uint64_t last_use = 0;
  };

  void Load(Entry& entry, std::uint32_t card_id, bool silhouette, int box_width, int box_height);
  void EvictBeyondBound();

  vfs::Vfs* vfs_ = nullptr;
  const vfs::CardIllustrations* cards_ = nullptr;
  std::unordered_map<std::uint64_t, Entry> entries_;
  std::uint64_t frame_ = 0;
  std::uint64_t uses_ = 0;
  int budget_ = 0;
};

}  // namespace ro::render
