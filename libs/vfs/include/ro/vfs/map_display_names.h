#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE NOM LISIBLE D'UNE CARTE, tiré de son nom de fichier.
//
// Le serveur ne désigne une carte que par son fichier — « prontera.gat ». Le nom
// qu'on montre au joueur vit dans une table Lua de l'installation, indexée par le
// fichier `.rsw` de la carte : `mapTbl["prontera.rsw"].displayName`.
//
// ⚠ LE FICHIER EST CELUI QUE LE CLIENT PATCHÉ LIT, `SystemEN\mapInfo.lub`, et
// non la table coréenne d'origine : c'est la version traduite que ce serveur
// livre, et le client de référence est réglé pour la charger à sa place.
inline constexpr std::string_view kMapInfoPath = "SystemEN\\mapInfo.lub";
inline constexpr std::string_view kMapInfoTableGlobal = "mapTbl";
inline constexpr std::string_view kMapInfoDisplayField = "displayName";

// 🔴 LE NOM D'UNE ENTRÉE SANS `displayName`. C'est la fonction du fichier qui le
// pose avant de déclarer chaque carte, et non le client : une carte déclarée
// sans nom s'appelle ainsi, une carte absente de la table retombe ailleurs.
inline constexpr std::string_view kMapInfoUnnamed = "Unknown Place";

// LE BANDEAU QU'UNE CARTE MONTRE À L'ARRIVÉE : une image de fond, un sous-titre
// et un titre, tirés de la même entrée.
inline constexpr std::string_view kMapInfoNotifyField = "notifyEnter";
inline constexpr std::string_view kMapInfoSignField = "signName";
inline constexpr std::string_view kMapInfoSubtitleField = "subTitle";
inline constexpr std::string_view kMapInfoTitleField = "mainTitle";
inline constexpr std::string_view kMapInfoBackgroundField = "backgroundBmp";
// L'image d'une carte qui n'en nomme aucune.
inline constexpr std::string_view kMapSignNoBackground = "noname";

struct MapSign {
  // Le fond, par son nom de fichier sans dossier ni extension : `village`.
  std::string background{kMapSignNoBackground};
  // ⚠ EN UTF-8, déjà convertis : ils se dessinent tels quels.
  std::string subtitle;
  std::string title;
  // `notifyEnter` : LE JEU NE MONTRE QUE CEUX-LÀ. Ce client peut montrer aussi
  // les autres, pourvu qu'ils aient un titre.
  bool announced = false;

  [[nodiscard]] bool titled() const noexcept { return !subtitle.empty() || !title.empty(); }
};

class MapDisplayNames {
 public:
  // Lit la table de l'installation.
  [[nodiscard]] static core::Result<MapDisplayNames> Load(Vfs& vfs);

  // La MÊME lecture, à partir d'un source en clair — la moitié qui se met sous
  // test, puisqu'aucun asset du jeu n'est versé au dépôt.
  [[nodiscard]] static core::Result<MapDisplayNames> ParseSource(std::string_view source);

  // La même, à partir de globales déjà lues.
  [[nodiscard]] static core::Result<MapDisplayNames> Parse(const formats::LuaTable& globals);

  // Le nom lisible d'une carte désignée par son fichier `.gat`, ou rien si la
  // table ne la connaît pas. ⚠ RIEN N'EST PAS UNE ERREUR : c'est à l'appelant de
  // dire ce que le client écrit alors.
  [[nodiscard]] std::optional<std::string_view> DisplayNameOf(std::string_view gat_name) const;

  [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }

  // LE BANDEAU D'ARRIVÉE D'UNE CARTE, ou nul quand elle n'a de quoi en montrer
  // aucun : ni annonce, ni titre.
  //
  // 🔴 LE JEU NE MONTRE QUE LES ENTRÉES `notifyEnter = true`, et la traduction
  // livrée n'en allume que deux cent dix-neuf, presque toutes récentes — pas
  // une ville. Le titre, lui, est posé sur presque toutes. C'est à l'appelant
  // de choisir entre les deux : voir `MapSign::announced`.
  //
  // ⚠ ANNONCÉE SANS SOUS-TITRE NI TITRE, le bandeau montre son image seule.
  [[nodiscard]] const MapSign* SignOf(std::string_view gat_name) const;

 private:
  // Indexées par le fichier `.rsw`, comme la table du jeu.
  std::map<std::string, std::string, std::less<>> names_;
  std::map<std::string, MapSign, std::less<>> signs_;
};

// LA CLÉ DE LA TABLE POUR UN NOM DE FICHIER DE CARTE, telle que le client la
// calcule.
//
// Deux retouches, dans cet ordre :
// - un nom de cinq caractères ou plus perd ses TROIS derniers, remplacés par
//   « rsw » — « prontera.gat » devient « prontera.rsw » ;
// - s'il porte un « @ » à la position quatre ou au-delà, les trois caractères
//   qui commencent quatre positions avant lui disparaissent : la copie d'une
//   carte d'instance, « 0001@tower.rsw », se range sous « 1@tower.rsw ».
[[nodiscard]] std::string MapInfoKey(std::string_view gat_name);

}  // namespace ro::vfs
