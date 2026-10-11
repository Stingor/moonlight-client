#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/ui/mvp_tracker_window.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA PAGE « CARNET DE CHASSE MVP » ────────────────────────────────────────
//
// Les réglages du carnet, dans la mise en page des pages de ce client.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`. La plage
// est chaînée à celle des sons de sprite dans `option_pages.h`, comme les
// autres : deux pages sur la même plage se volent leurs lignes.
inline constexpr int kMvpTrackerRowBase = 7700;

enum class MvpTrackerRow : int {
  kEnabled = 0,
  kOpen,
  kAlertSound,
  kAlertLead,
  kShowSprites,
  kAnimateSprites,
  kShowFilter,
  kShowTombs,
  kLinesNote,
  kLineSprite,
  kLineText,
  kLineBackground,
  kPutBackAll,
  kShowDelay,
  kAlertTone,
  kAlertListen,
  kAlertFile,
  kAlertTrouble,
  kCount,
};

[[nodiscard]] constexpr int MvpTrackerRowId(MvpTrackerRow row) {
  return kMvpTrackerRowBase + static_cast<int>(row);
}

// LE SON DE L'ALERTE. Les valeurs sont celles du fichier de réglages.
enum class MvpAlertTone : std::uint8_t { kSystem = 0, kNewMessage = 1, kButton = 2, kCustom = 3 };

// LE SON QU'UNE VALEUR DU FICHIER DÉSIGNE. ⚠ HORS PLAGE, C'EST LE DÉFAUT, et non
// la borne la plus proche : un 7 venu d'un fichier abîmé ne doit pas choisir le
// fichier personnel.
[[nodiscard]] constexpr MvpAlertTone MvpAlertToneOf(int stored) noexcept {
  return stored >= static_cast<int>(MvpAlertTone::kSystem) &&
                 stored <= static_cast<int>(MvpAlertTone::kCustom)
             ? static_cast<MvpAlertTone>(stored)
             : MvpAlertTone::kSystem;
}
inline constexpr std::size_t kMvpAlertToneCount =
    static_cast<std::size_t>(MvpAlertTone::kCustom) + 1;

// CE QUI A EMPÊCHÉ LE DERNIER SON DE L'ALERTE DE SONNER, que la page écrit sous
// le réglage en cause. `kNone` : le son a joué, ou rien n'a été tenté depuis le
// dernier changement de choix.
enum class MvpAlertTrouble : std::uint8_t {
  kNone,
  // Les bruitages du client sont coupés.
  kMuted,
  // Le fichier personnel n'est pas nommé.
  kNameEmpty,
  // Le nom sort de `savedata\` : absolu, ou remontant d'un dossier.
  kNameRefused,
  // Le fichier manque : sous `savedata\`, ou dans l'installation pour un son
  // du jeu.
  kFileMissing,
  // Le fichier se lit et ne se décode pas.
  kFileUnreadable,
};

// Les bornes du préavis d'alerte, en minutes.
inline constexpr int kMvpAlertLeadLeastMin = 0;
inline constexpr int kMvpAlertLeadMostMin = 60;

struct MvpTrackerSettings {
  // L'INTERRUPTEUR MAÎTRE, et le seul : éteint, le carnet ne reçoit, ne montre
  // et n'envoie rien, et aucune porte ne le rallume.
  bool enabled = false;
  bool alert_sound = true;
  // Un `MvpAlertTone`, gardé en entier comme il est lu du fichier.
  int alert_tone = 0;
  // LE FICHIER PERSONNEL, relatif à `savedata\`, en UTF-8 : lu sous
  // `MvpAlertTone::kCustom` seulement.
  std::string alert_wav;
  int alert_lead_min = 5;
  bool show_tombs = true;
  ro::ui::MvpTrackerLook look;
};

class MvpTrackerOptions {
 public:
  // UNE COPIE NORMALISÉE : le préavis ramené dans [`kMvpAlertLeadLeastMin`,
  // `kMvpAlertLeadMostMin`], le son par `MvpAlertToneOf`. La borne s'applique à
  // la LECTURE : une valeur hors bornes venue du fichier, ou d'un profil posé,
  // n'atteint jamais la détection.
  [[nodiscard]] MvpTrackerSettings settings() const noexcept;
  [[nodiscard]] const ro::ui::MvpTrackerLook& look() const noexcept { return settings_.look; }
  // Le clic droit de l'en-tête de la table a montré ou masqué « Délai » : rend
  // vrai si le réglage change.
  [[nodiscard]] bool SetShowDelay(bool shown) noexcept;
  // ⚠ POSÉS PAR LA PARTIE AVANT LA COMPOSITION : cette page ne connaît ni les
  // raccourcis ni le carnet. La touche vient des réglages, le nombre de lignes
  // détachées de l'étage du carnet.
  void NoteOpenKey(std::string label) { open_key_ = std::move(label); }
  void NoteLineCount(int count) noexcept { line_count_ = count; }
  // LE DERNIER ÉCHEC DU SON DE L'ALERTE DIT AU JOURNAL, ou vide : le même ne se
  // redit pas à chaque alerte. Voir `MvpAlertSoundSources::reported`. ⚠ VIDÉ
  // QUAND LE SON OU LE FICHIER CHANGE : un choix neuf mérite son propre mot.
  [[nodiscard]] std::string& alert_failure_reported() noexcept { return alert_failure_reported_; }
  // CE QUI A EMPÊCHÉ LE DERNIER SON DE SONNER. Voir `MvpAlertSoundSources::trouble`.
  // ⚠ REMIS À RIEN QUAND LE SON OU LE FICHIER CHANGE : la raison ne vaut que
  // pour le choix qui l'a produite.
  [[nodiscard]] MvpAlertTrouble& alert_trouble() noexcept { return alert_trouble_; }
  // LES BRUITAGES SONT-ILS AUDIBLES, posé à chaque image avant la composition :
  // rallumés, la raison « coupés » s'efface sans attendre un son.
  void NoteSoundAudible(bool audible) noexcept;
  // LE FICHIER PERSONNEL QUE LE MOTEUR TIENT. Voir `MvpAlertSoundSources::custom_held`.
  [[nodiscard]] std::string& alert_sound_held() noexcept { return alert_sound_held_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }
  // Rend vrai quand la ligne était l'une des siennes. ⚠ « OUVRIR LE CARNET »,
  // « TOUT REMETTRE AU CARNET » ET « ÉCOUTER » EN SONT, mais ne changent aucun
  // réglage : c'est la partie qui les joue.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  // ⚠ ALLUMÉ D'USINE, à la différence de `MvpTrackerSettings` : c'est la
  // disposition que le joueur livre.
  MvpTrackerSettings settings_{.enabled = true};
  std::string open_key_;
  int line_count_ = 0;
  // Ce que les lignes pointent, gardé le temps qu'elles vivent : « ou la touche
  // Ctrl+M », « Tout remettre au carnet (3) ».
  std::string open_hint_;
  std::string put_back_label_;
  std::string put_back_confirm_;
  std::string alert_failure_reported_;
  MvpAlertTrouble alert_trouble_ = MvpAlertTrouble::kNone;
  std::string alert_sound_held_;
  // Les noms des sons, traduits à la composition : la liste déroulante les
  // pointe.
  std::array<const char*, kMvpAlertToneCount> tone_names_{};
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
