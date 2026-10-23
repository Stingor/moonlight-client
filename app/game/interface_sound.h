#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "ro/audio/audio_engine.h"
#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// ── LES SONS DE L'INTERFACE ─────────────────────────────────────────────────
//
// Une alarme de chuchotement, l'arrivée d'un membre au salon, l'alerte d'un
// favori du carnet : des sons que l'interface joue d'elle-même, hors de toute
// entité et de toute carte. Ils passent par le moteur, au centre, dans la
// famille `SoundChannel::kInterface`, et se taisent quand les bruits sont
// coupés.
//
// ⚠ CETTE FAMILLE N'A PAS DE CURSEUR À ELLE : elle suit celui des effets et
// des compétences, sous le volume général. Voir `GameOption::kEffectSoundVolume`.

// OÙ SE LIT UN SON D'INTERFACE.
enum class InterfaceSoundPlace : std::uint8_t {
  // Aucun fichier : le son du système, ou un chemin refusé.
  kNowhere = 0,
  // SUR LE DISQUE, sous la racine de l'installation, hors de toute archive —
  // comme les musiques.
  kInstallation,
  // DANS LES ARCHIVES, sous le dossier des sons du jeu.
  kSoundArchive,
};

struct InterfaceSoundFile {
  InterfaceSoundPlace place = InterfaceSoundPlace::kNowhere;
  // ⚠ RELATIF, et ce n'est pas le même départ selon la place : la racine de
  // l'installation, séparateurs « / », pour `kInstallation` ; le dossier des
  // sons du jeu pour `kSoundArchive`, sous le nom que le reste du client donne
  // déjà à ce son.
  std::string path;

  [[nodiscard]] bool operator==(const InterfaceSoundFile&) const = default;
};

// L'arrivée d'un message, sur le disque de l'installation.
[[nodiscard]] InterfaceSoundFile MessengerSoundFile();
// Le son de bouton du jeu, dans les archives.
[[nodiscard]] InterfaceSoundFile ButtonSoundFile();
// UN FICHIER DU JOUEUR, nommé relativement à `savedata\`.
//
// ⚠ REFUSÉ — place `kNowhere` — quand le nom est vide, absolu, ou remonte
// d'un dossier : le réglage désigne un fichier POSÉ DANS `savedata\`, et
// rien d'autre du disque.
[[nodiscard]] InterfaceSoundFile SaveFolderSoundFile(std::string_view typed);

// JOUE UN SON D'INTERFACE, lu à sa première fois puis retenu par le moteur.
// ⚠ UN FICHIER DU DISQUE SE RELIT quand sa taille ou sa date d'écriture
// changent : le joueur qui remplace son fichier entend le nouveau, et le
// moteur oublie l'ancien.
//
// Rend une erreur quand le fichier manque (`kNotFound`), qu'il ne se décode
// pas, ou qu'aucun fichier n'est désigné. ⚠ RIEN N'EST JOURNALISÉ ICI :
// l'appelant sait si un échec se redit à chaque fois ou une seule.
//
// Bruits coupés, rien ne se lit et rien n'échoue : le silence est le réglage
// du joueur.
//
// `held_name`, s'il est donné, reçoit le nom sous lequel le moteur tient le son
// joué : de quoi l'oublier quand le réglage en désigne un autre.
[[nodiscard]] ro::core::Result<void> PlayInterfaceSound(ro::audio::AudioEngine& audio,
                                                        ro::vfs::Vfs& vfs,
                                                        const std::filesystem::path& client_root,
                                                        const InterfaceSoundFile& file,
                                                        std::string* held_name = nullptr);

// JOUE UNE SUITE DE SONS, et dit chaque échec au journal — « son <from> :
// <cause> ». Sans archives, rien ne se joue.
void PlayInterfaceSounds(ro::audio::AudioEngine& audio, ro::vfs::Vfs* vfs,
                         const std::filesystem::path& client_root,
                         std::span<const InterfaceSoundFile> files, std::string_view from);

}  // namespace moonclient
