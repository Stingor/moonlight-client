#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/grf.h"
#include "ro/vfs/res_name_table.h"

namespace ro::vfs {

// Une archive montée, avec le chemin d'où elle vient — indispensable pour dire
// LAQUELLE a servi quand deux archives portent le même fichier.
struct MountedArchive {
  std::filesystem::path path;
  formats::GrfArchive archive;
};

struct MountOptions {
  // Dossier d'installation du client.
  std::filesystem::path root;
  // Noms des archives, par priorité DÉCROISSANTE (la première l'emporte).
  std::vector<std::string> archives;
  // Le dossier `data\` du disque passe avant toutes les archives, comme dans le
  // client. C'est ce qui permet de remplacer un fichier sans repacker un GRF.
  bool prefer_data_directory = true;
  // Une archive absente ou illisible est ignorée plutôt que fatale : une
  // installation n'a pas forcément toutes les archives de sa liste.
  bool skip_unreadable_archives = true;
};

// Les chemins du disque où chercher une clé NORMALISÉE, dans l'ordre, SANS
// DOUBLON.
//
// Sous Windows, un nom coréen s'écrit de deux façons sur le disque, selon
// l'outil qui l'a extrait, et les deux s'essaient. Un nom ASCII n'en a qu'une :
// l'interroger deux fois coûterait un second `stat` à chaque lecture, pour un
// fichier que presque toujours une archive sert.
[[nodiscard]] std::vector<std::filesystem::path> DiskSpellings(const std::filesystem::path& root,
                                                               std::string_view normalized_key);

// Système de fichiers virtuel du client : le disque d'abord, puis les archives.
//
// Toute l'API publique parle UTF-8. Le CP949 des noms d'archives ne franchit
// jamais cette frontière — c'est la raison d'être de la couche.
class Vfs {
 public:
  Vfs(const Vfs&) = delete;
  Vfs& operator=(const Vfs&) = delete;
  Vfs(Vfs&&) = default;
  Vfs& operator=(Vfs&&) = default;
  ~Vfs() = default;

  [[nodiscard]] static core::Result<Vfs> Mount(const MountOptions& options);

  // `utf8_path` est un chemin interne, tel qu'il apparaît dans l'archive :
  // « data\sprite\… ». La casse et le sens des séparateurs sont indifférents.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> Read(std::string_view utf8_path);
  [[nodiscard]] bool Contains(std::string_view utf8_path) const;

  // Variante pour un chemin DÉJÀ en octets CP949.
  //
  // Elle existe parce que les formats se citent entre eux : un GND nomme ses
  // textures, un RSW nomme son sol, et ces noms sont des octets CP949 tirés du
  // fichier. Les passer à Read() les ferait transcoder une seconde fois — de
  // l'UTF-8 vers le CP949 alors qu'ils SONT déjà du CP949 — et le fichier
  // deviendrait introuvable.
  //
  // Règle : un chemin venu d'un être humain ou d'une configuration passe par
  // Read ; un chemin venu d'un autre fichier passe par ReadRaw.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> ReadRaw(std::string_view cp949_path);

  // L'existence sans la lecture, pour un chemin déjà en CP949. Un relevé qui
  // veut savoir ce que l'installation POSSÈDE n'a pas à décompresser douze mille
  // bitmaps pour le dire.
  [[nodiscard]] bool ContainsRaw(std::string_view cp949_path) const;

  // Noms UTF-8, triés et dédoublonnés, du disque comme des archives.
  [[nodiscard]] std::vector<std::string> List(std::string_view utf8_prefix,
                                              std::string_view utf8_suffix = {}) const;

  // LE CLONAGE DES CARTES, lu au montage.
  //
  // Il ne s'applique pas dans `Read` : la table se consulte sur le nom NU,
  // avant le préfixe de la famille, et `Read` ne reçoit que des chemins déjà
  // préfixés. C'est donc à qui bâtit un chemin de carte de la traverser.
  [[nodiscard]] const ResNameTable& res_names() const noexcept { return res_names_; }

  // UN DOSSIER DU DISQUE QUI PASSE AVANT TOUT, pour les chemins sous
  // `utf8_prefix` : « data\texture\유저인터페이스\x.bmp » se cherche d'abord
  // dans `folder\x.bmp`, puis, absent, comme tout autre chemin — `data\`, puis
  // les archives. Le dossier ne porte donc que ce qu'il change. C'est la règle
  // des skins du client d'origine.
  //
  // ⚠ UN SEUL DOSSIER PAR PRÉFIXE : le poser de nouveau le remplace, et un
  // dossier vide retire la surcharge. Une lecture faite avant ne se refait pas :
  // c'est à qui a lu de relire, et `overlay_revision` le lui dit.
  void OverlayFolder(std::string_view utf8_prefix, std::filesystem::path folder);
  // Grandit à chaque surcharge posée ou retirée.
  [[nodiscard]] std::uint64_t overlay_revision() const noexcept { return overlay_revision_; }

  [[nodiscard]] const std::vector<MountedArchive>& archives() const noexcept { return archives_; }
  [[nodiscard]] const std::filesystem::path& data_directory() const noexcept {
    return data_directory_;
  }
  [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

 private:
  Vfs() = default;

  // Chemin sur disque correspondant à une clé normalisée, ou vide si absent.
  [[nodiscard]] std::filesystem::path ResolveOnDisk(std::string_view normalized_key) const;

  // Le corps commun aux deux entrées : elles ne diffèrent que par la façon dont
  // la clé est fabriquée. `reported_path` sert au message d'erreur.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> ReadKey(const std::string& key,
                                                                std::string_view reported_path);

  std::filesystem::path root_;
  std::filesystem::path data_directory_;
  std::vector<MountedArchive> archives_;
  ResNameTable res_names_;
  // Les surcharges : un préfixe de clé NORMALISÉE, et son dossier.
  std::vector<std::pair<std::string, std::filesystem::path>> overlays_;
  std::uint64_t overlay_revision_ = 0;
};

// UN FICHIER DU DISQUE sous `root`, nommé comme le jeu le nomme : séparé par
// « \ » ou par « / », et dans une casse quelconque. Vide s'il n'y est pas.
//
// ⚠ C'EST LA SEULE FAÇON D'OUVRIR SUR LE DISQUE UN NOM VENU DES TABLES DU
// JEU — « bgm\131.mp3 » quand l'installation porte `BGM/131.mp3`. Sous
// Linux, l'antislash n'est pas un séparateur, et le système de fichiers
// distingue la casse : le nom tel quel désignerait un fichier absent.
[[nodiscard]] std::filesystem::path FindOnDisk(const std::filesystem::path& root,
                                               std::string_view game_path);

}  // namespace ro::vfs
