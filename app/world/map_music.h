#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

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

  // LE MORCEAU CHOISI PAR LE JOUEUR, au lecteur de musique : il remplace celui
  // de la carte, et le reste d'une carte à l'autre jusqu'à `FollowMapAgain`.
  //
  // Musique coupée, le choix est retenu sans rien lire : il jouera au retour du
  // son, comme l'aurait fait celle de la carte. Un fichier illisible, lui, rend
  // la main à la carte — un choix qui ne joue rien ne se distingue pas d'un
  // silence voulu.
  void Choose(ro::audio::AudioEngine& audio, const std::string& track);

  // Rend la main à la carte : sa musique reprend au prochain `Follow`, sans
  // attendre d'en changer.
  void FollowMapAgain() {
    chosen_.clear();
    followed_.clear();
  }

  // Le morceau choisi, ou vide quand la carte décide.
  [[nodiscard]] const std::string& chosen() const noexcept { return chosen_; }

  [[nodiscard]] const ro::formats::BgmTable& table() const noexcept { return table_; }
  [[nodiscard]] const std::filesystem::path& client_root() const noexcept { return client_root_; }

  [[nodiscard]] std::size_t known_maps() const noexcept { return table_.size(); }

 private:
  // Joue `track` s'il ne joue pas déjà. `why` ne sert qu'au journal : la carte,
  // ou le lecteur. Faux quand le fichier manque ou ne se décode pas.
  bool PlayTrack(ro::audio::AudioEngine& audio, std::string_view track, std::string_view why);

  std::filesystem::path client_root_;
  ro::formats::BgmTable table_;
  // La dernière carte suivie, pour ne pas relire la table à chaque image.
  std::string followed_;
  // Le morceau que le joueur a choisi ; vide quand la carte décide.
  std::string chosen_;
};

}  // namespace moonclient
