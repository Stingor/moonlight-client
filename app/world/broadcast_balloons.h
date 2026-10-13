#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

#include "ro/sim/broadcast_balloon.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/broadcast_balloon.h"

#include "chat/chat_item_link.h"
#include "chat/chat_rich_decor.h"

namespace moonclient {

// LES BULLES D'ANNONCE, CÔTÉ ÉTAT VIVANT.
//
// Le monde dépose ce qui doit se montrer (`sim::BalloonNews`) ; ce module le
// prend, tient chaque famille à son plafond, laisse vieillir puis s'éteindre,
// et rend aux deux bandeaux ce qu'ils ont à peindre. Les règles — plafond,
// extinction, fondu — sont des fonctions pures, plus bas.

// Combien de bulles d'une famille restent à l'écran. La chasse n'en garde
// qu'une, chacune remplaçant la précédente ; les autres suivent le réglage.
[[nodiscard]] std::size_t MostBalloonsOf(ro::sim::BalloonFamily family,
                                         const ro::ui::BroadcastBalloonLook& look);

// Une bulle reçue à `heard_ms` est-elle éteinte à `now_ms` ?
[[nodiscard]] bool BalloonExpired(std::uint32_t heard_ms, std::uint32_t now_ms,
                                  const ro::ui::BroadcastBalloonLook& look);

// SON OPACITÉ, de zéro à un : pleine, puis décroissante sur la fin de sa vie
// quand le fondu est réglé.
[[nodiscard]] float BalloonAlpha(std::uint32_t heard_ms, std::uint32_t now_ms,
                                 const ro::ui::BroadcastBalloonLook& look);

// Ce qu'il faut pour décorer les lignes comme le fil.
struct BalloonDecor {
  ChatDecorTables tables;
  ItemIconLookup item_icon;
};

class BroadcastBalloons {
 public:
  // Prend ce que le monde a déposé depuis le dernier passage. ⚠ DANS LE
  // POMPAGE RÉSEAU, pas dans le dessin : une image sautée ne doit rien perdre.
  void Notice(ro::sim::World& world, const ro::ui::BroadcastBalloonLook& look);

  // Purge ce qui est éteint, décore, et compose ce qu'il y a à peindre.
  //
  // `preview_announcements` et `preview_hunts` posent une bulle d'EXEMPLE sur
  // un bandeau vide : placer une ancre suppose de voir ce qu'on place.
  void Compose(std::uint32_t now_ms, const ro::ui::BroadcastBalloonLook& look,
               const BalloonDecor& decor, bool preview_announcements, bool preview_hunts);

  // ⚠ LES VUES SONT PRISES APRÈS `Compose`, sur des tampons qui ne bougent plus.
  [[nodiscard]] std::span<const ro::ui::BalloonView> announcements() const noexcept {
    return announcement_views_;
  }
  [[nodiscard]] std::span<const ro::ui::BalloonView> hunts() const noexcept { return hunt_views_; }

 private:
  // Les bulles tenues, par famille, de la plus ancienne à la plus récente.
  std::array<std::deque<ro::sim::BalloonNotice>, ro::sim::kBalloonFamilyCount> held_;
  // Les bulles d'exemple de l'image, quand une ancre se place sur un bandeau vide.
  std::vector<ro::sim::BalloonNotice> previews_;

  // LES TAMPONS QUE LES VUES DÉSIGNENT, remplis en ENTIER avant qu'une seule
  // vue ne soit prise : une vue posée pendant le remplissage mourrait à la
  // première réallocation.
  std::vector<DecoratedChatLine> decorated_;
  std::vector<ro::ui::BalloonLineView> line_views_;
  std::vector<ro::ui::BalloonView> announcement_views_;
  std::vector<ro::ui::BalloonView> hunt_views_;
};

// CE QUE LES DEUX BANDEAUX PEIGNENT À CETTE IMAGE, et ce qu'ils rendent.
//
// ⚠ LES POIGNÉES DURENT D'UNE IMAGE À L'AUTRE : la structure vit avec les
// fenêtres, et seules les vues et le réglage se reposent à chaque image.
struct BalloonBoards {
  std::span<const ro::ui::BalloonView> announcements;
  std::span<const ro::ui::BalloonView> hunts;
  // Nul : rien ne se peint, pas même les poignées.
  const ro::ui::BroadcastBalloonLook* look = nullptr;
  ro::ui::BalloonAnchorGrip announcement_grip;
  ro::ui::BalloonAnchorGrip hunt_grip;
  // OÙ UNE POIGNÉE VIENT D'ÊTRE POSÉE, lu à l'image suivante.
  ro::ui::BalloonAnchorAsk announcement_ask;
  ro::ui::BalloonAnchorAsk hunt_ask;
};

// Peint les deux bandeaux. ⚠ DANS L'IMAGE D'IMGUI, hors de toute fenêtre, et
// avant elles : les bulles se peignent sur la liste de fond.
void DrawBalloonBoards(BalloonBoards& boards, const ro::ui::ChatRichArt* rich);

}  // namespace moonclient
