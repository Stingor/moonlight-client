#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ro/sim/damage_meter.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/pick_precision_options.h"

namespace moonclient {

// ── LA PAGE « COMPTEUR DE DÉGÂTS » ───────────────────────────────────────────
//
// Les réglages du panneau, et ceux du palmarès.
inline constexpr int kDamageMeterRowBase = 7100;

// ⚠ L'ORDRE EST CELUI DES RANGÉES À L'ÉCRAN, et il entre dans les identifiants.
enum class DamageMeterRow : int {
  kIntro = 0,
  kShown,
  kLocked,
  kRecords,
  kTextColor,
  kPlotColor,
  kOpacity,
  kSlotMs,
  kWindowSecs,
  kCombatTimeout,
  kResetFight,
  kForgetRecords,
  kAreaInChat,
  // CE QUE PORTENT LE TITRE DE LA FENÊTRE ET LA LANGUETTE DE L'ONGLET.
  kCaptionRate,
  kCaptionPeak,
  kCaptionAverage,
  kCaptionTotal,
  kCount,
};

static_assert(kPickPrecisionRowBase + static_cast<int>(PickPrecisionRow::kCount) <=
                  kDamageMeterRowBase,
              "la plage du compteur de dégâts chevauche celle de la précision du ciblage");

[[nodiscard]] constexpr int DamageMeterRowId(DamageMeterRow row) {
  return kDamageMeterRowBase + static_cast<int>(row);
}

// Les bornes du panneau.
inline constexpr int kFinestSlotMs = 50;
inline constexpr int kCoarsestSlotMs = 2000;
inline constexpr int kShortestWindowSecs = 1;
inline constexpr int kLongestWindowSecs = 30;
inline constexpr int kShortestCombatTimeoutSecs = 1;
inline constexpr int kLongestCombatTimeoutSecs = 15;

class DamageMeterOptions {
 public:
  // L'apparence et les bascules, opacité bornée.
  [[nodiscard]] ro::ui::DamageMeterLook look() const noexcept;
  // La mécanique, chaque champ borné : un fichier retouché à la main peut
  // porter n'importe quoi.
  [[nodiscard]] ro::sim::DamageMeter::Settings meter() const noexcept;
  // Écrire chaque cumul de sort de zone dans la conversation.
  [[nodiscard]] bool area_in_chat() const noexcept { return area_in_chat_; }

  // CE QUE LE COMPTEUR MONTRE, choisi dans sa barre d'outils — bornés.
  [[nodiscard]] ro::ui::DamageMeterMode mode() const noexcept;
  [[nodiscard]] ro::ui::DamageMeterSegment segment() const noexcept;
  [[nodiscard]] ro::ui::DamageMeterPane pane() const noexcept;
  void SetMode(ro::ui::DamageMeterMode mode) noexcept { mode_ = static_cast<int>(mode); }
  void SetSegment(ro::ui::DamageMeterSegment segment) noexcept {
    segment_ = static_cast<int>(segment);
  }
  void SetPane(ro::ui::DamageMeterPane pane) noexcept { pane_ = static_cast<int>(pane); }

  // CE QUE PORTENT LE TITRE ET LA LANGUETTE : le rythme, le pic, la moyenne,
  // le total — le rythme seul par défaut.
  struct Caption {
    bool rate = true;
    bool peak = false;
    bool average = false;
    bool total = false;
  };
  [[nodiscard]] const Caption& caption() const noexcept { return caption_; }

  // CE QUE LA FENÊTRE ELLE-MÊME A CHANGÉ — sa croix, son menu, son chevron.
  void SetShown(bool shown) noexcept { look_.shown = shown; }
  void SetLocked(bool locked) noexcept { look_.locked = locked; }
  void SetRecords(bool records) noexcept { look_.records = records; }

  // Les deux gestes de la page, que la partie exécute : ce n'est pas elle qui
  // tient le compteur. Chacun se prend une fois.
  [[nodiscard]] bool TakeResetFight() noexcept;
  [[nodiscard]] bool TakeForgetRecords() noexcept;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  // ⚠ CACHÉ ET VERROUILLÉ PAR DÉFAUT : on l'allume quand on veut mesurer. Le
  // palmarès, lui, est un volet qu'on choisit d'ouvrir.
  ro::ui::DamageMeterLook look_{.shown = false, .locked = true, .records = false};
  int slot_ms_ = 200;
  int window_secs_ = 10;
  int combat_timeout_secs_ = 5;
  bool area_in_chat_ = true;
  int mode_ = 0;
  int segment_ = 0;
  int pane_ = 0;
  Caption caption_;
  bool reset_fight_asked_ = false;
  bool forget_records_asked_ = false;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
