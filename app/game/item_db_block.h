#pragma once

// LE BLOC DE FAITS D'UN OBJET, COMPOSÉ DEPUIS L'ITEM_DB DU SERVEUR : type,
// attaque, défense, emplacement, poids, niveaux, raffinement, emplacements de
// carte, puis les prérequis — niveau, jobs, classes, sexe.
//
// 🔴 UN SEUL COMPOSANT, PARTOUT OÙ UNE DESCRIPTION S'AFFICHE : il s'ajoute aux
// LIGNES de la description (`DescriptionView`), et la fenêtre, sa comparaison,
// l'aperçu au survol et la fiche de la base de données dessinent ces lignes-là.
//
// Le style est celui des lignes d'`item_info.yml` — libellé en bleu
// `^0000CC`, valeur en noir, trait de vingt-quatre soulignés entre les
// parties —, mais chaque texte passe par les catalogues de langue. Un en-tête
// gris le distingue du texte de la table, qui reste affiché au-dessus tant que
// les deux se comparent.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/vfs/server_db.h"

namespace moonclient {

// La couleur des libellés, celle de la table : « Type: », « Weight: »…
inline constexpr std::string_view kItemDbLabelInk = "^0000CC";
// La couleur de l'en-tête « Données du serveur », discrète.
inline constexpr std::string_view kItemDbHeaderInk = "^777777";
// L'encre du texte ordinaire, après un libellé.
inline constexpr std::string_view kItemDbPlainInk = "^000000";

// Vrai quand la base ferme l'objet à au moins un job : `Jobs` écrit sans
// `All`, ou `All` avec des exceptions.
[[nodiscard]] bool JobsRestricted(const ro::vfs::ServerItem& item);

// LES JOBS QUI PEUVENT PORTER L'OBJET, en résumé lisible : « Tous les jobs »,
// « Tous les jobs sauf Novice, Super Novice », « Aucun job », ou la liste — une lignée
// complète se dit « classes Swordman ». Une liste plus longue que son
// complément parmi les jobs d'origine se dit par le complément. Traduit.
[[nodiscard]] std::string JobsSummaryOf(const ro::vfs::ServerItem& item);

// LE BLOC, BALISAGE COMPRIS (`ro/ui/rich_text.h`) : l'en-tête, les faits, puis
// les prérequis après un trait. Traduit.
[[nodiscard]] std::vector<std::string> ItemDbBlockOf(const ro::vfs::ServerItem& item);

// LES LIGNES D'UNE DESCRIPTION, BLOC COMPRIS : celles de la table, un trait,
// puis le bloc de l'objet. Une base qui n'est pas encore lue n'ajoute rien ;
// une base lue qui ignore l'objet le dit d'une ligne grise.
void AppendItemDbBlock(std::vector<std::string>& lines, const ro::vfs::ServerDb* db,
                       std::uint32_t item_id);

}  // namespace moonclient
