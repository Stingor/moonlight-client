#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA TABLE DES OBJETS DU CLIENT : tout ce que le serveur ne dit pas.
//
// Le fil ne porte qu'un identifiant. Le nom, l'apparence, la description, le
// nombre d'emplacements de carte : rien de tout cela ne vient du serveur, et
// c'est cette table de l'installation qui les donne. Sans elle, un sac n'affiche
// que des numéros.
//
// ⚠ ELLE VIT DANS `vfs` ET NON DANS `render`. Le catalogue de sprites n'en
// lisait que deux nombres, ce qui suffisait tant qu'on ne dessinait que des
// armes ; l'interface en veut le texte. Une table de treize méga-octets ne se
// lit pas deux fois, et « catalogue de sprites » aurait fini par désigner un
// endroit où l'on range les noms d'objets.

// Un objet, tel que la table le décrit.
struct ItemEntry {
  // Le nom affiché, en UTF-8.
  //
  // ⚠ TRANSCODÉ EN TOLÉRANT, et c'est mesuré : sur 27 208 noms, dix-neuf
  // portent des octets hauts, et ces dix-neuf-là mêlent deux encodages — des
  // résidus de fusion de sources. Les refuser strictement ferait perdre la table
  // entière, et un point d'interrogation au milieu de dix-neuf lignes est le
  // bon prix. ⚠ LA RECHERCHE D'ÉCHOPPES COMPARE PAR CE NOM : un de ces
  // dix-neuf ne se retrouve qu'en tapant ce point d'interrogation.
  std::string name;

  // Le nom de RESSOURCE : celui du fichier, dans l'archive.
  //
  // ⚠ IL RESTE EN CP949, tel quel. C'est un chemin d'archive, et les archives
  // sont indexées dans cet encodage : le transcoder ferait chercher un fichier
  // qui n'existe nulle part.
  std::string resource;

  // La description, une entrée par ligne, en UTF-8.
  //
  // ⚠ LE BALISAGE DU CLIENT EST CONSERVÉ : les jetons de couleur `^RRGGBB`, les
  // liens `<URL>` et `<NAVI>`, et le `#` de fin de ligne qui vaut trait de
  // séparation. Les retirer ici priverait de couleur qui saura les rendre ; ce
  // n'est pas à la lecture d'en décider.
  std::vector<std::string> description;

  // L'APPARENCE que l'objet donne quand on le porte — la classe d'une arme, la
  // forme d'un bouclier. La table d'origine la nomme `ClassNum`.
  std::uint16_t view_id = 0;

  // Combien de cartes l'objet peut recevoir. C'est ce nombre qui compose le
  // suffixe « [N] » du nom affiché.
  std::uint8_t card_slots = 0;

  // Une pièce d'apparat : elle se voit et ne protège pas.
  bool costume = false;
};

class ItemTable {
 public:
  void Put(std::uint32_t item_id, ItemEntry entry);

  [[nodiscard]] const ItemEntry* Find(std::uint32_t item_id) const;

  // L'apparence d'un objet, ou zéro s'il n'en donne pas — et zéro aussi pour un
  // objet que la table ignore. Les deux se dessinent pareil : les mains vides.
  [[nodiscard]] std::uint16_t ViewOf(std::uint32_t item_id) const;

  // Son nom de ressource, EN CP949, ou vide.
  [[nodiscard]] std::string_view ResourceOf(std::uint32_t item_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_id_.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id_.empty(); }

  // Tout ce que la table porte, pour qui la RELÈVE — un outil hors jeu, un
  // compte. ⚠ SANS ORDRE : deux parcours ne rendent pas la même suite, et rien
  // de ce qui s'affiche ne doit en dépendre.
  [[nodiscard]] const std::unordered_map<std::uint32_t, ItemEntry>& entries() const noexcept {
    return by_id_;
  }

 private:
  std::unordered_map<std::uint32_t, ItemEntry> by_id_;
};

// Où le client de ce serveur range sa table, et le nom de la globale qu'elle
// pose. ⚠ HORS DES ARCHIVES : c'est un fichier du dossier de jeu, que le client
// évalue par un script à lui.
inline constexpr std::string_view kItemTablePath = "SystemEN\\itemInfoMerged.lua";
inline constexpr std::string_view kItemTableGlobal = "tbl";

// ── CE QUE LE SCRIPT DE CHARGEMENT AJOUTE, ET QUE NOUS DEVONS AJOUTER AUSSI ──
//
// 🔴 LA TABLE N'EST PAS LA DESCRIPTION. Le client ne l'affiche pas telle quelle :
// il l'évalue par un SCRIPT — `SystemEN\itemInfo.lua` —, et ce script ajoute
// trois lignes AVANT le texte de l'objet :
//
//     AddItemIdentifiedDesc(id, URL_PREFIX .. id .. URL_MID .. id .. URL_SUFFIX)
//     if DESC.ViewID > 0 then AddItemIdentifiedDesc(id, "ViewID : " .. ViewID) end
//     AddItemIdentifiedDesc(id, " ")
//
// ⚠ NOUS NE LISONS PAS CE SCRIPT — seulement la table de données qu'il requiert.
// Les règles qu'il porte se recopient donc ICI, où elles sont vérifiables, et
// c'est la seule raison pour laquelle le lien de base de données et le numéro
// d'apparence apparaissent dans une description : ils ne sont dans AUCUNE donnée.
//
// ⚠ L'ADRESSE EST CELLE DE CE SERVEUR-LÀ. Elle vit dans son dossier de jeu, et
// c'est lui qui la choisit ; la recopier ici est un emprunt assumé, pas une
// dépendance que le client irait chercher ailleurs.
inline constexpr std::string_view kDatabaseLinkPrefix = "<URL>ItemID : ";
// Le balisage qui entoure l'adresse dans une ligne de description.
inline constexpr std::string_view kDatabaseLinkInfo = "<INFO>";
inline constexpr std::string_view kDatabasePageUrl =
    "https://moonlight-destiny.fr/index.php?page=itemdb&itemid=";
inline constexpr std::string_view kDatabaseLinkSuffix = "</INFO></URL>";
// ⚠ UNE ESPACE, PAS UNE LIGNE VIDE : c'est ce que le script écrit, et une ligne
// vide ne se replierait pas de la même façon.
inline constexpr std::string_view kDescriptionGap = " ";

// L'ADRESSE DE LA FICHE D'UN OBJET, sans le balisage qui l'entoure dans une
// description. ⚠ COMPOSÉE ICI ET NULLE PART AILLEURS : c'est la même adresse que
// la première ligne d'une description, et deux endroits qui l'assemblent
// finiraient par ne plus pointer au même endroit.
[[nodiscard]] std::string ItemDatabaseUrl(std::uint32_t item_id);

// Lit la table. Une table ABSENTE rend une table VIDE, pas une erreur : le jeu
// se joue sans, en montrant des numéros là où il montrerait des noms. Une table
// ILLISIBLE, elle, est une erreur — mieux vaut le savoir que jouer aveugle.
[[nodiscard]] core::Result<ItemTable> ReadItemTable(Vfs& vfs);

}  // namespace ro::vfs
