#pragma once

#include <vector>

#include "ro/ui/broadcast_balloon.h"
#include "ro/ui/options_window.h"
#include "ro/ui/status_portrait.h"

#include "game/settings_file.h"
#include "options/status_portrait_options.h"

namespace moonclient {

// ── LES BULLES D'ANNONCE, dans leur propre section ──────────────────────────
//
// Ce que les deux bandeaux du haut de l'écran montrent, combien de temps, et où
// ils se posent. Voir `ro::ui::BroadcastBalloonLook`.
inline constexpr int kBalloonRowBase = 9200;

enum class BalloonRow : int {
  kShown = 0,
  kDurationMs,
  kMostPerFamily,
  kFade,
  kFadeMs,
  kFontScale,
  kWrapPx,
  kBackgroundColor,
  kBackgroundAlpha,
  kBorderColor,
  kAnnouncementsUnlocked,
  kAnnouncementsHome,
  kHuntsUnlocked,
  kHuntsHome,
  kCount,
};

// Les cadres d'aperçu des deux pages de bulles, juste sous la plage des réglages.
inline constexpr int kChatBubbleSampleRow = kBalloonRowBase - 2;
inline constexpr int kBroadcastBalloonSampleRow = kBalloonRowBase - 1;

static_assert(kPortraitPartRowBase +
                      static_cast<int>(ro::ui::kPortraitPartCount) * kPortraitFieldsPerPart <=
                  kChatBubbleSampleRow,
              "la plage des bulles d'annonce chevauche celle des cadres du portrait");

class BroadcastBalloonOptions {
 public:
  [[nodiscard]] const ro::ui::BroadcastBalloonLook& look() const noexcept { return look_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Les lignes de la section, et l'aperçu épinglé au-dessus d'elles.
  void Compose();
  [[nodiscard]] const std::vector<ro::ui::OptionRowView>& rows() const noexcept { return rows_; }
  [[nodiscard]] const std::vector<ro::ui::OptionRowView>& pinned_rows() const noexcept {
    return pinned_rows_;
  }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // Une poignée vient d'être posée : c'est le dessin qui a déplacé le bandeau.
  void NoteAnchor(bool hunts, const ro::ui::BalloonAnchorAsk& asked) noexcept;

 private:
  ro::ui::BroadcastBalloonLook look_;
  // ⚠ ICI ET NON SUR LA PILE : la section n'en garde qu'une vue.
  std::vector<ro::ui::OptionRowView> rows_;
  std::vector<ro::ui::OptionRowView> pinned_rows_;
};

}  // namespace moonclient
