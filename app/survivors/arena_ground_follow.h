#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_plan.h"

namespace moonclient {

// ── LE RECENTRAGE DU SOL GÉNÉRÉ (ADR 0134, T4) ─────────────────────────────
//
// La fenêtre dessinée (160 × 160 cases) suit le héros par pas entiers d'un
// morceau. Quand il s'écarte de son centre de plus d'un morceau sur un axe, la
// fenêtre avance d'autant de morceaux qu'il faut sur cet axe : le décalage est
// un multiple exact de 32 cases, donc de 16 cubes, et rien du sol ne change
// d'aspect — deux fenêtres qui se recouvrent rendent les mêmes cubes.
//
// 🔴 L'HYSTÉRÉSIS EST D'UN MORCEAU : après un pas, le héros est à moins d'un
// morceau du nouveau centre, du côté où il marchait ; il lui faut revenir de
// plus d'un morceau en arrière pour que la fenêtre recule. Un aller-retour sur
// la frontière ne recentre donc rien.

// L'écart au centre qui déclenche le recentrage, et son pas.
inline constexpr std::int32_t kGroundRecentreCells = ro::sim::kTerrainChunkCells;
// LA FENÊTRE D'AVANCE se bâtit dès que le héros s'écarte de la moitié de ce
// pas : il lui reste un demi-morceau à marcher pendant qu'elle se bâtit.
inline constexpr std::int32_t kGroundAheadCells = kGroundRecentreCells / 2;
// CE QUE L'IMAGE VOIT DU SOL au plus grand recul du 21:9, vers l'ouest ou
// l'est (le plus long des quatre côtés, relevé par `survivors render`), et la
// marge gardée avant le bord.
inline constexpr std::int32_t kGroundWidestSightCells = 30;
inline constexpr std::int32_t kGroundEdgeMarginCells = 6;
// AU-DELÀ DE CET ÉCART, LE RECENTRAGE N'ATTEND PLUS : ni que les effets se
// déplacent sans trace, ni que le fil de travail ait fini — la fenêtre se bâtit
// sur place s'il le faut. En deçà, le bord n'est pas encore en vue.
inline constexpr std::int32_t kGroundForceCells =
    kGroundWindowCells / 2 - kGroundWidestSightCells - kGroundEdgeMarginCells;
static_assert(kGroundForceCells > kGroundRecentreCells);
// LA DESCENTE D'ENTRÉE VOIT AU PLUS CECI du héros, de chaque côté
// (`ArenaCamera::BoundDescent`) : la fenêtre du départ (`GroundWindowAround`)
// laisse deux morceaux entiers entre le morceau du héros et chaque bord, soit
// au moins la demi-fenêtre moins un demi-morceau ; la marge reste avant le
// bord. Le héros ne marche pas pendant la descente.
inline constexpr std::int32_t kGroundDescentReachCells =
    (kGroundWindowCells - ro::sim::kTerrainChunkCells) / 2 - kGroundEdgeMarginCells;
static_assert(kGroundDescentReachCells > kGroundWidestSightCells);

// L'écart du héros (cases continues) au centre de la fenêtre `origin`.
[[nodiscard]] glm::vec2 GroundOffsetFromCentre(GroundWindowOrigin origin, glm::vec2 hero_cell);

// LE COIN DE LA FENÊTRE QUI SUIT LE HÉROS : `shown` quand il est à au plus un
// morceau du centre sur chaque axe, sinon `shown` avancé du nombre de morceaux
// qui l'y ramène, sur l'axe ou les axes dépassés. Bornée au monde.
[[nodiscard]] GroundWindowOrigin GroundWindowFollowing(GroundWindowOrigin shown,
                                                       glm::vec2 hero_cell);

// LA FENÊTRE D'AVANCE : celle que le héros atteindra s'il continue, dès qu'il
// a fait la moitié du chemin sur un axe ; rien tant qu'il reste près du
// centre.
[[nodiscard]] std::optional<GroundWindowOrigin> GroundWindowAhead(GroundWindowOrigin shown,
                                                                  glm::vec2 hero_cell);

// Vrai quand le héros est assez loin du centre pour que le bord approche.
[[nodiscard]] bool GroundRecentreForced(GroundWindowOrigin shown, glm::vec2 hero_cell);

// CE QUE DEVIENT UN POINT DU MONDE DESSINÉ quand le coin passe de `from` à
// `to` : la même case du monde se compte depuis un autre coin. L'est est l'axe
// des x, le nord celui des z décroissants ; l'altitude ne change pas.
[[nodiscard]] glm::vec3 GroundWorldShift(GroundWindowOrigin from, GroundWindowOrigin to);

// LA FENÊTRE D'AVANCE PRÉPARÉE `prepared` SERT-ELLE ENCORE ? Oui quand c'est
// celle d'avance ; oui encore tant que le héros reste du côté où elle avance,
// et n'a pas fait la moitié du chemin sur l'autre axe. C'est l'hystérésis de
// l'avance : elle naît à 16 cases du centre et ne meurt qu'au retour en deçà
// du centre, si bien qu'un héros qui piétine à 16 cases ne la rebâtit pas.
[[nodiscard]] bool GroundAheadStillServes(GroundWindowOrigin shown, GroundWindowOrigin prepared,
                                          glm::vec2 hero_cell);

// CE QUE DEVIENT LA FENÊTRE PRÉPARÉE `prepared` à cette image, la fenêtre
// `shown` montée et le héros en `hero_cell`. Quand il faut une autre fenêtre
// que `shown`, seule celle-là vaut : une autre s'abandonne, pour que la voulue
// se demande ; celle-là se monte quand son sol est téléversé (`uploaded`) et
// que le recentrage peut se faire — les effets se déplacent sans trace
// (`shifts_without_trace`), ou le bord approche —, et se garde sinon. Quand
// `shown` suffit, la fenêtre d'avance se garde tant qu'elle sert
// (`GroundAheadStillServes`) et s'abandonne ensuite.
enum class GroundPreparedFate { kKeep, kDrop, kMount };
[[nodiscard]] GroundPreparedFate GroundPreparedFateOf(GroundWindowOrigin shown,
                                                      GroundWindowOrigin prepared,
                                                      glm::vec2 hero_cell, bool uploaded,
                                                      bool shifts_without_trace);

// LE SOL D'UNE FENÊTRE SE TÉLÉVERSE PAR TRANCHES de cette taille, une par
// image : sommets et indices du sol arrondi font quelque seize méga-octets, et
// une tranche de deux se copie en une fraction de milliseconde.
inline constexpr std::size_t kGroundUploadSliceBytes = std::size_t{2} * 1024 * 1024;

// COMMENT UNE FENÊTRE SE BÂTIT : sur ce fil (essais, banc), ou sur un fil de
// travail, la part pure seule — plan, GND, GAT, RSW, maillage, atlas, lumière,
// blocs de profondeur du sol et poses cuites du décor (`PrepareSceneUpload`).
enum class GroundBuildThread { kCaller, kWorker };

// ── LA FILE DES FENÊTRES BÂTIES ─────────────────────────────────────────────
//
// Au plus `kMostQueuedWindows` fenêtres, prêtes ou en cours. Une fenêtre en
// cours ne s'abandonne jamais : son fil la finit, et elle sort de la file
// quand elle est prête. Abandonner un `std::future` d'`std::async` ATTENDRAIT
// sa fin sur le fil principal.
inline constexpr std::size_t kMostQueuedWindows = 3;

template <typename Window>
class GroundWindowQueue {
 public:
  using Build = std::function<ro::core::Result<Window>()>;

  explicit GroundWindowQueue(GroundBuildThread thread) : thread_(thread) {}

  // Demande la fenêtre `origin`. Rien quand elle est déjà demandée, ou que la
  // file est pleine : la demande se refera à l'image suivante.
  void Want(GroundWindowOrigin origin, Build build) {
    if (Holds(origin) || entries_.size() >= kMostQueuedWindows) return;
    Entry entry{.origin = origin, .built = {}};
    if (thread_ == GroundBuildThread::kWorker) {
      entry.built = std::async(std::launch::async, std::move(build));
    } else {
      std::promise<ro::core::Result<Window>> done;
      done.set_value(build());
      entry.built = done.get_future();
    }
    entries_.push_back(std::move(entry));
  }

  [[nodiscard]] bool Holds(GroundWindowOrigin origin) const {
    return std::ranges::any_of(entries_,
                               [origin](const Entry& entry) { return entry.origin == origin; });
  }

  // LA FENÊTRE `origin` si elle est prête, sortie de la file ; `wait` attend
  // qu'elle le soit. Rien si elle n'est pas demandée.
  [[nodiscard]] std::optional<ro::core::Result<Window>> Take(GroundWindowOrigin origin, bool wait) {
    const auto found = std::ranges::find(entries_, origin, &Entry::origin);
    if (found == entries_.end() || (!wait && !Ready(*found))) return std::nullopt;
    ro::core::Result<Window> window = found->built.get();
    entries_.erase(found);
    return window;
  }

  // Sort de la file les fenêtres PRÊTES qui ne sont pas dans `keep`.
  void KeepOnly(std::initializer_list<GroundWindowOrigin> keep) {
    std::erase_if(entries_, [keep](const Entry& entry) {
      return Ready(entry) && std::ranges::find(keep, entry.origin) == keep.end();
    });
  }

  // UNE FENÊTRE QUI NE SERT PLUS se libère sur le fil de travail : sa mémoire
  // part sans que l'image l'attende.
  void Retire(Window window) {
    std::erase_if(retired_, [](const std::future<void>& gone) {
      return gone.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    if (thread_ == GroundBuildThread::kWorker) {
      retired_.push_back(std::async(std::launch::async, [gone = std::move(window)]() mutable {
        const Window freed = std::move(gone);
      }));
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

 private:
  struct Entry {
    GroundWindowOrigin origin;
    std::future<ro::core::Result<Window>> built;
  };

  [[nodiscard]] static bool Ready(const Entry& entry) {
    return entry.built.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
  }

  GroundBuildThread thread_;
  std::vector<Entry> entries_;
  std::vector<std::future<void>> retired_;
};

// ── LE SUIVI : quelle fenêtre bâtir, laquelle préparer ─────────────────────
//
// Une fenêtre bâtie se PRÉPARE ensuite sur le fil principal — son sol se
// téléverse par tranches, une par image (`kGroundUploadSliceBytes`) — puis se
// MONTE en une image. La fenêtre d'avance se prépare dès qu'elle est bâtie : le
// héros la trouve prête en passant la frontière.
template <typename Window>
class GroundFollow {
 public:
  using Build = std::function<ro::core::Result<Window>(GroundWindowOrigin)>;

  GroundFollow(Build build, GroundBuildThread thread) : build_(std::move(build)), queue_(thread) {}

  // UNE FENÊTRE BÂTIE, à préparer.
  struct Built {
    GroundWindowOrigin origin;
    Window window;
  };

  // UNE IMAGE, la fenêtre `shown` montée et le héros en `hero_cell` : demande
  // la fenêtre qu'il lui faut, ou à défaut celle d'avance, et la rend quand elle
  // est bâtie. `wait` attend celle qu'il faut si elle est en cours.
  [[nodiscard]] std::optional<Built> Next(GroundWindowOrigin shown, glm::vec2 hero_cell,
                                          bool wait) {
    const GroundWindowOrigin wanted = GroundWindowFollowing(shown, hero_cell);
    if (wanted != shown) return Take(wanted, wait);
    const std::optional<GroundWindowOrigin> ahead = GroundWindowAhead(shown, hero_cell);
    if (!ahead.has_value()) {
      queue_.KeepOnly({});
      return std::nullopt;
    }
    return Take(*ahead, false);
  }

  // La fenêtre démontée, ou abandonnée, libérée hors du fil principal.
  void Retire(Window window) { queue_.Retire(std::move(window)); }

  [[nodiscard]] std::size_t queued() const noexcept { return queue_.size(); }

 private:
  [[nodiscard]] std::optional<Built> Take(GroundWindowOrigin origin, bool wait) {
    queue_.KeepOnly({origin});
    // UNE FENÊTRE REFUSÉE ne se redemande pas à chaque image : le refus est
    // dit une fois, et l'image garde la fenêtre montée.
    if (refused_ == origin) return std::nullopt;
    queue_.Want(origin, [build = build_, origin] { return build(origin); });
    std::optional<ro::core::Result<Window>> taken = queue_.Take(origin, wait);
    if (!taken.has_value()) return std::nullopt;
    if (!taken->has_value()) {
      ro::core::LogWarning("sol généré : fenêtre ({}, {}) refusée — {}", origin.x_cells,
                           origin.y_cells, ro::core::Describe(taken->error()));
      refused_ = origin;
      return std::nullopt;
    }
    return Built{.origin = origin, .window = std::move(**taken)};
  }

  Build build_;
  GroundWindowQueue<Window> queue_;
  std::optional<GroundWindowOrigin> refused_;
};

}  // namespace moonclient
