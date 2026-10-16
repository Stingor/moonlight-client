#pragma once

// LE BLOC DE FAITS D'UN OBJET, COMPOSÉ DEPUIS L'ITEM_DB DU SERVEUR : type,
// attaque, défense, emplacement, ce que son script dit par motifs exacts
// (élément, soin, guérison, durée, effet, capture), délai, familier, poids,
// niveaux, raffinement, emplacements de carte, puis les prérequis — niveau,
// jobs, classes, sexe.
//
// 🔴 UN SEUL COMPOSANT, PARTOUT OÙ UNE DESCRIPTION S'AFFICHE : il s'ajoute aux
// LIGNES de la description (`DescriptionView`), et la fenêtre, sa comparaison,
// l'aperçu au survol et la fiche de la base de données dessinent ces lignes-là.
//
// Le style est celui des lignes d'`item_info.yml` — libellé en bleu
// `^0000CC`, valeur en noir, trait de vingt-quatre soulignés entre les
// parties —, mais chaque texte passe par les catalogues de langue. Un en-tête
// gris le distingue du texte de la table, affiché au-dessus.
//
// 🔴 LA TABLE NE REDIT PAS CE QUE LE BLOC DIT : `item_info.py --retirer-faits`
// en retire les lignes de faits d'après ce que ce bloc écrit pour chaque objet,
// lu par `roc-cli item db-blocks`. Un libellé renommé ici se renomme aussi dans
// `scripts/yaml_tables/item_info_facts.py`, qui refuse sinon de retirer.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/vfs/item_table.h"
#include "ro/vfs/pet_tables.h"
#include "ro/vfs/server_db.h"

#include "game/item_script_facts.h"

namespace moonclient {

// La couleur des libellés, celle de la table : « Type: », « Weight: »…
inline constexpr std::string_view kItemDbLabelInk = "^0000CC";
// La couleur de l'en-tête « Données du serveur », discrète.
inline constexpr std::string_view kItemDbHeaderInk = "^777777";
// L'encre du texte ordinaire, après un libellé.
inline constexpr std::string_view kItemDbPlainInk = "^000000";

// CE QUI NOMME LES MONSTRES, LES FAMILIERS ET LES OBJETS QU'UN FAIT DÉSIGNE.
// Chaque table peut manquer : un nom inconnu s'écrit par son identifiant.
struct ItemDbNames {
  // La base du serveur : le nom qu'il montre d'un monstre ou d'un objet.
  const ro::vfs::ServerDb* db = nullptr;
  // La pet_db : capture, œuf et nourriture.
  const ro::vfs::PetTables* pets = nullptr;
  // Les noms du client, avant ceux du serveur.
  const ro::vfs::ItemTable* items = nullptr;
};

// 🔴 UN FAIT NUL NE S'ÉCRIT PAS : poids, attaque, défense, portée, niveaux,
// emplacements, soin, durée ou délai qui valent zéro n'apprennent rien, armure
// comprise — une défense absente se lit nulle. Seuls le type, toujours, et
// « Raffinable », qui dit oui ou non d'une pièce, s'écrivent quelle que soit
// leur valeur.

// UNE LIGNE DE FAIT : le libellé en bleu, puis la valeur en noir.
[[nodiscard]] std::string ItemDbLabelled(const char* label, std::string_view value);

// Vrai quand la base ferme l'objet à au moins un job : `Jobs` écrit sans
// `All`, ou `All` avec des exceptions.
[[nodiscard]] bool JobsRestricted(const ro::vfs::ServerItem& item);

// LES JOBS QUI PEUVENT PORTER L'OBJET, en résumé lisible : « Tous les jobs »,
// « Tous les jobs sauf lignée Novice », « Aucun job », ou la liste. Les lignées
// complètes viennent d'abord, puis, après un point-virgule, les jobs seuls :
// « Lignée Archer ; Thief, Rogue ». Une liste plus longue que son complément
// parmi les jobs d'origine se dit par le complément. Traduit.
[[nodiscard]] std::string JobsSummaryOf(const ro::vfs::ServerItem& item);

// LES FAITS QUE DONNENT LE SCRIPT ET LA PET_DB (`item_script_facts.h`) :
// élément d'attaque et d'armure, soin, états guéris, durée, effet des états
// posés — les bonus que leur script dans la status_db de `names.db` donne,
// quand chacun se lit entier —, délai de réutilisation, monstres capturés,
// familier d'un œuf et sa nourriture, familiers qu'un objet nourrit ou qui le
// portent. Traduit.
void AppendItemDbScriptFacts(std::vector<std::string>& lines, const ro::vfs::ServerItem& item,
                             const ItemScriptFacts& facts, const ItemDbNames& names);

// LE BLOC, BALISAGE COMPRIS (`ro/ui/rich_text.h`) : l'en-tête, les faits, puis
// les prérequis après un trait. Traduit.
[[nodiscard]] std::vector<std::string> ItemDbBlockOf(const ro::vfs::ServerItem& item,
                                                     const ItemDbNames& names = {});

// LES LIGNES D'UNE DESCRIPTION, BLOC COMPRIS : celles de la table, un trait,
// puis le bloc de l'objet. Une base qui n'est pas encore lue n'ajoute rien ;
// une base lue qui ignore l'objet le dit d'une ligne grise. `names.db` est
// `db`, quoi qu'il porte.
void AppendItemDbBlock(std::vector<std::string>& lines, const ro::vfs::ServerDb* db,
                       std::uint32_t item_id, const ItemDbNames& names = {});

}  // namespace moonclient
