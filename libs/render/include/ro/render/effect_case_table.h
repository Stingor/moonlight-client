#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// LES IDENTIFIANTS D'EFFET QUE LE RÉPARTITEUR PROCÉDURAL TRAITE, et la fonction
// qui traite chacun.
//
// Le répartiteur a trois étages, et mille cent douze identifiants y ont un cas.
// Tous les autres tombent sur son défaut, qui ne dessine rien.
//
// ⚠ ELLE NE DIT PAS TOUT CE QUE LE CLIENT PEUT MONTRER, et la mesure l'a
// tranché : sur les mille cent cinquante-sept identifiants auxquels ses DEUX
// tables de fichiers donnent un `.str`, deux cent vingt-sept seulement ont aussi
// un cas ici. Les deux familles sont donc presque DISJOINTES — un effet en
// fichier n'a le plus souvent aucun cas procédural, et réciproquement. Le
// dénominateur honnête est leur UNION, deux mille quarante-deux identifiants :
// prendre celle-ci seule ferait passer pour introuvables neuf cent trente effets
// que le client montre très bien.
//
// ⚠ MAIS C'EST LA SEULE QUI DISE CE QU'IL Y A À MONTRER de la famille
// procédurale. Nos autres tables disent ce que NOUS savons montrer, et les
// compter ne peut donc jamais rendre un manque : une famille entière oubliée n'y
// laisse aucune trace. Celle-ci décrit le client, pas nous.
struct EffectCaseEntry {
  std::uint16_t effect_id = 0;
  // L'étage du répartiteur qui porte le cas : 1, 2 ou 3. Les trois se suivent —
  // un identifiant que le premier ne connaît pas descend au suivant — et savoir
  // lequel répond dit où relire quand un effet ne montre rien.
  std::uint8_t stage = 0;
  // L'étiquette de la routine qui traite ce cas. C'est ce qui rend un verdict
  // « ne montre rien » actionnable : elle dit quel traitement relire.
  std::string_view client_function;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectCaseEntry> EffectCaseTable();

// Le cas d'un identifiant, ou nul s'il n'en a pas — et il ne se voit alors
// nulle part.
[[nodiscard]] const EffectCaseEntry* EffectCaseFor(std::uint32_t effect_id);

}  // namespace ro::render
