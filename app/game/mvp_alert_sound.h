#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "ro/audio/audio_engine.h"
#include "ro/vfs/vfs.h"

#include "game/interface_sound.h"
#include "options/mvp_tracker_options.h"

namespace moonclient {

// CE QUE LE SON DE L'ALERTE D'UN FAVORI DU CARNET DE CHASSE MVP DEMANDE : le
// moteur et ses deux sources de sons — le GRF et le disque de l'installation —,
// puis le son choisi, un `MvpAlertTone` gardé en entier.
struct MvpAlertSoundSources {
  ro::audio::AudioEngine& audio;
  ro::vfs::Vfs& vfs;
  const std::filesystem::path& client_root;
  int tone = 0;
  // Le fichier personnel, relatif à `savedata\` : lu sous `kCustom` seulement.
  std::string_view custom_file;
  // LE DERNIER ÉCHEC DIT AU JOURNAL, ou nul pour tout dire. Un échec identique
  // ne se redit pas ; un son qui joue de nouveau l'efface.
  std::string* reported = nullptr;
  // LE NOM SOUS LEQUEL LE MOTEUR TIENT LE FICHIER PERSONNEL joué en dernier,
  // ou vide ; nul pour n'en rien retenir. Dès que l'alerte joue autre chose —
  // un autre fichier, le même sous une autre casse, un son du jeu —, celui-là
  // s'oublie : le joueur qui change de fichier ne garde pas l'ancien décodé.
  std::string* custom_held = nullptr;
  // CE QUI A EMPÊCHÉ CE SON DE SONNER, réécrit à chaque appel — `kNone` s'il a
  // joué —, ou nul. À la différence de `reported`, il se pose aussi quand la
  // même raison se répète : c'est ce que la page montre.
  MvpAlertTrouble* trouble = nullptr;
};

// Ce qu'a fait un appel, pour qui veut le savoir — l'essai.
enum class MvpAlertSoundOutcome : std::uint8_t {
  kPlayed,
  kSystemBeep,
  // Les bruits du client sont coupés : rien n'est lu, et rien n'a échoué.
  kMuted,
  // Muet, et le journal vient de dire pourquoi.
  kSilentReported,
  // Muet pour la même raison que la fois d'avant : rien de redit.
  kSilentAlreadyReported,
};

// LE SON DE L'ALERTE. Le son du système ignore le volume du client ; les
// autres passent par le moteur, à son volume. ⚠ UN SON QUI NE PEUT PAS
// SONNER NE FAIT PAS TOMBER L'ALERTE : elle reste écrite à l'écran, et l'échec
// se journalise une fois.
//
// ⚠ DANS UNE UNITÉ À PART : l'en-tête de Windows y apporte ses macros `min` et
// `max`.
MvpAlertSoundOutcome PlayMvpAlertSound(const MvpAlertSoundSources& from);

// LE FICHIER D'UN CHOIX, sans rien lire : place `kNowhere` pour le son du
// système quand il existe, et pour un fichier personnel refusé. ⚠ SANS BIP
// SYSTÈME — hors de Windows —, le son du système devient « Nouveau message ».
// Une valeur hors plage est le son du système.
[[nodiscard]] InterfaceSoundFile MvpAlertToneFile(int tone, bool system_beep_available,
                                                  std::string_view custom_file);

}  // namespace moonclient
