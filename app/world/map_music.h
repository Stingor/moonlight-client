#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "ro/audio/audio_engine.h"
#include "ro/formats/bgm_table.h"
#include "ro/vfs/vfs.h"

// La MUSIQUE des cartes : la table, les fichiers, et le passage de l'une à
// l'autre.
namespace moonclient {

// Ouvre le son, ou rend un mélangeur muet.
//
// Une machine sans carte son ne doit pas empêcher de jouer : l'échec se dit une
// fois, et le mélangeur fermé ne fait rien sans que chaque site d'appel ait à le
// vérifier.
//
// Ce que le mélangeur sait décoder est dit au passage : cette liste dépend de sa
// CONSTRUCTION et non de son interface, et sans le bon décodeur compilé dedans
// un MP3 est refusé comme un fichier abîmé — rien ne dirait alors que c'est la
// construction qui manque.
//
// ⚠ IL NE COUPE RIEN DE LUI-MÊME. Le son partait muet tant que le choix ne se
// retenait pas d'une partie à l'autre : il fallait le couper à chaque
// relance, et le défaut était donc le seul endroit où le dire. Ce sont
// désormais les OPTIONS qui décident, et qui se souviennent — voir
// `GameOptions`.

[[nodiscard]] ro::audio::AudioEngine OpenAudio();

// Joue la musique de la carte où l'on se trouve.
//
// ⚠ Les musiques ne sont PAS dans l'archive : elles vivent en clair dans un
// dossier de l'installation, à côté de l'exécutable. C'est la seule ressource
// du jeu dans ce cas, et c'est pourquoi cette classe connaît la racine du
// client en plus du VFS.
//
// Sans table, sans fichier ou sans carte son, on joue en silence : la musique
// n'est pas une condition pour jouer.
class MapMusic {
 public:
  MapMusic(ro::vfs::Vfs& vfs, std::filesystem::path client_root);

  // Change de morceau si la carte en demande un autre. Sans effet quand la
  // carte partage sa musique avec la précédente — deux villes voisines le font
  // souvent, et redémarrer à chaque porte franchie s'entendrait.
  void Follow(ro::audio::AudioEngine& audio, const std::string& map_name);

  // Oublie la carte suivie, pour que le prochain `Follow` rejoue.
  //
  // ⚠ SANS ÇA, RÉTABLIR LA MUSIQUE NE LA REMET PAS. `Follow` ne fait rien quand
  // la carte n'a pas changé — c'est ce qui évite de redémarrer le morceau à
  // chaque porte franchie —, si bien qu'après une coupure il faudrait changer de
  // carte pour entendre quelque chose.
  void Forget() { followed_.clear(); }

  [[nodiscard]] std::size_t known_maps() const noexcept { return table_.size(); }

 private:
  std::filesystem::path client_root_;
  ro::formats::BgmTable table_;
  // La dernière carte suivie, pour ne pas relire la table à chaque image.
  std::string followed_;
};

}  // namespace moonclient
