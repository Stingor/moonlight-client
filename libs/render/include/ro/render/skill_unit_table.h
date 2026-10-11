#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/render/skill_named_table.h"

namespace ro::render {

// Le dossier où le client range ses modèles. Les chemins de la table lui sont
// relatifs.
inline constexpr std::string_view kModelFolder = "data\\model\\";

// Une entrée de la table des unités au sol.
struct SkillUnitEntry {
  std::uint16_t unit_id = 0;
  // Le chemin du modèle, relatif à kModelFolder, extension comprise : le client
  // en emploie deux versions, .rsm et .rsm2.
  std::string_view model;
};

// La table entière, triée par identifiant d'unité.
[[nodiscard]] std::span<const SkillUnitEntry> SkillUnitTable();

// Le modèle d'une unité au sol, chemin complet, ou une chaîne VIDE pour une
// unité que rien ne dessine.
//
// Une unité posée au sol n'est ni un acteur ni un effet : c'est un MODÈLE 3D,
// du même genre que l'Emperium. Les unités qui n'y figurent pas — un mur de
// feu, un sanctuaire — se voient par leur effet, ou pas du tout.
[[nodiscard]] std::string SkillUnitModelPath(std::uint32_t unit_id);

// Une entrée de la table des EFFETS d'unité.
struct SkillUnitEffectEntry {
  std::uint16_t unit_id = 0;
  std::uint16_t effect_id = 0;
};

// La table entière, triée par identifiant d'unité.
[[nodiscard]] std::span<const SkillUnitEffectEntry> SkillUnitEffectTable();

// L'effet qu'une unité fait naître EN PLUS de son modèle, ou ZÉRO.
//
// ⚠ La clé est l'identifiant d'UNITÉ, pas celui de la compétence : le client
// range cet effet avec le modèle de l'unité, dans le même répartiteur. Chercher
// par compétence ne le trouve pas — c'est ainsi que Pneuma restait sans image
// alors que son fichier est dans l'archive.
//
// Un modèle et un effet ne s'excluent pas : un piège a les deux, le modèle
// qu'on voit posé et la bouffée qui l'accompagne.
[[nodiscard]] std::uint16_t SkillUnitEffect(std::uint32_t unit_id);

// Comment le client ENTRETIENT l'effet d'une unité posée.
enum class SkillUnitVisualShape : std::uint8_t {
  // Créé une fois, gardé tant que l'unité vit : chaque tic de l'unité repousse
  // la fin de l'instance, et rien n'est à relancer.
  kPermanent,
  // Relancé quand `durée − retrait` tics ont passé. Un effet ne boucle pas ; ce
  // qui doit durer est REJOUÉ, jamais prolongé.
  kRepeated,
};

// Une entrée de la table des visuels ENTRETENUS.
struct SkillUnitVisualEntry {
  std::uint16_t unit_id = 0;
  std::uint16_t effect_id = 0;
  SkillUnitVisualShape shape = SkillUnitVisualShape::kPermanent;
  // Le RECOUVREMENT, en tics de seize millisecondes : de combien le client
  // avance la relance sur la fin de l'effet pour qu'aucune image ne montre le
  // sol nu. Zéro pour une forme permanente, qui ne se relance pas.
  std::uint8_t withhold_ticks = 0;
};

// La table entière, triée par identifiant d'unité.
[[nodiscard]] std::span<const SkillUnitVisualEntry> SkillUnitVisualTable();

// Ce qu'une unité montre TANT QU'ELLE VIT, ou NUL pour une unité dont le client
// n'entretient rien.
//
// ⚠ CE N'EST PAS L'EFFET DE SA POSE. Le client sépare les deux moments, et dans
// deux fonctions différentes : l'une joue ce qui se voit à l'apparition — voir
// SkillUnitEffect —, l'autre, à chaque tic, entretient ce qui se voit ensuite. Une unité peut
// n'avoir que l'un, que l'autre, ou les deux.
//
// La plupart de ces effets sont PROCÉDURAUX : ils n'ont pas de fichier, et ne
// se peignent que si un patron de nœud les décrit.
[[nodiscard]] const SkillUnitVisualEntry* SkillUnitVisual(std::uint32_t unit_id);

// Une entrée de la table des visuels qui dépendent du NIVEAU.
struct SkillUnitLevelVisualEntry {
  std::uint16_t unit_id = 0;
  std::uint8_t skill_level = 0;
  std::uint16_t effect_id = 0;
};

// La table entière, triée par unité puis par niveau.
[[nodiscard]] std::span<const SkillUnitLevelVisualEntry> SkillUnitLevelVisualTable();

// L'effet entretenu d'une unité dont le visuel dépend du NIVEAU de la
// compétence qui l'a posée, ou ZÉRO.
//
// Le client ne le range pas dans le cas de son tic : c'est l'APPARITION qui
// choisit l'identifiant selon le niveau et le pose dans un champ de l'acteur,
// que le tic consomme ensuite une fois. Le chercher dans `SkillUnitVisual` ne
// donne donc rien, et l'unité paraît n'avoir aucun visuel.
//
// Il se peint comme un effet PERMANENT : semé une fois, et reposé seulement
// quand tout ce qu'il avait semé est mort de sa propre vie.
[[nodiscard]] std::uint16_t SkillUnitLevelVisual(std::uint32_t unit_id, std::uint8_t skill_level);

// ⚠ UN ÉCART VOULU AU CLIENT D'ORIGINE, à la demande du joueur : LE VISUEL QUI
// DURE AUTANT QUE SON UNITÉ.
//
// Le client d'origine sème le champ de Gravitation une fois ; sa troupe
// s'éteint au bout de sa propre vie, quatre secondes, alors que la zone tient
// de cinq à neuf secondes selon le niveau. Ce client-ci le relance avant qu'il
// ne s'efface, tant que l'unité tient sa case, et l'unité levée par le serveur
// l'emporte : le champ se voit exactement le temps que la zone agit.
inline constexpr std::uint16_t kGravitationFieldUnit = 184;

// Vrai quand le visuel entretenu de cette unité se relance tant qu'elle vit.
[[nodiscard]] constexpr bool SkillUnitVisualLastsWithUnit(std::uint32_t unit_id) noexcept {
  return unit_id == kGravitationFieldUnit;
}

// 🔴 UN VISUEL PERMANENT QU'UNE FABRIQUE DE SPRITE BÂTIT NE SE RESÈME JAMAIS.
//
// Le client sème un visuel permanent une fois et prolonge son instance à chaque
// tic de l'unité. La plupart se maintiennent par leur mise à jour, et ce client
// imite la prolongation en resemant ce qui est mort. Mais la fabrique de sprite
// ne crée le sien qu'à la naissance, et il joue son action une fois : prolongée,
// l'instance ne le refait pas. Le resemer ferait rejouer le tatami de
// Tatamigaeshi une seconde fois, à mi-vie de l'unité.
[[nodiscard]] bool SkillUnitVisualSownOnce(const SkillUnitVisualEntry& visual);

// L'ATTENTE ENTRE DEUX SEMIS d'un tel visuel, en millisecondes : la troupe
// suivante naît quand la première des précédentes commence à s'effacer, et les
// deux se croisent le temps de la sortie — aucune image ne montre le sol nu.
// Un patron sans tic de sortie compte jusqu'à la fin de sa vie. Zéro pour un
// effet qui ne sème aucun nœud.
[[nodiscard]] std::uint32_t SustainedUnitVisualPeriodMs(std::uint32_t effect_id);

// L'ATTENTE ENTRE DEUX SEMIS du visuel entretenu d'une unité, en millisecondes :
// celle du client — `durée − retrait` de l'effet, voir `SkillUnitRespawnMs` —,
// ou celle d'un visuel qui dure autant que son unité. Zéro : pas de relance.
[[nodiscard]] std::uint32_t SkillUnitVisualPeriodMs(const SkillUnitVisualEntry& visual);

// QUAND une fabrique d'unité joue ses noms. Son objet d'action a deux méthodes,
// et elles ne disent pas la même chose : la première à la POSE, la seconde à la
// LEVÉE. N'en lire qu'une laisse l'unité s'installer sans jamais se retirer.
enum class SkillUnitNamedMoment : std::uint8_t {
  kOnPlace,
  kOnVanish,
};

// Une entrée de la table des effets NOMMÉS d'une unité.
//
// Le VERBE vient du vocabulaire commun aux effets nommés : une fabrique d'unité
// et une action de compétence font les mêmes gestes, et les nommer deux fois
// ferait croire à deux mécanismes.
struct SkillUnitNamedEffectEntry {
  std::uint16_t unit_id = 0;
  // Le nom, tel que la fabrique l'écrit. Il ne désigne aucun fichier par
  // lui-même : c'est une table de DONNÉES de l'installation qui le résout —
  // voir ro::formats::NamedEffectTable.
  std::string_view name;
  SkillUnitNamedMoment moment = SkillUnitNamedMoment::kOnPlace;
  NamedEffectVerb verb = NamedEffectVerb::kSow;
  // UN EFFET PAR IDENTIFIANT que la fabrique pose sur l'unité au même instant,
  // et non un nom — `name` est alors vide. Zéro pour une entrée nommée. C'est
  // toujours une CLASSE du gestionnaire d'effets, attachée à l'unité.
  std::uint16_t effect_id = 0;
  // Vrai quand la fabrique lui passe le NIVEAU de la compétence en paramètre.
  bool level_as_param = false;
  // Vrai quand il naît à la place de celui qui a POSÉ l'unité, et non sur elle :
  // le shuriken d'un piège part de son poseur.
  bool born_at_creator = false;
};

// La table entière, triée par identifiant d'unité.
[[nodiscard]] std::span<const SkillUnitNamedEffectEntry> SkillUnitNamedEffectTable();

// Les effets qu'une fabrique d'unité désigne par un NOM, ou une plage VIDE.
//
// ⚠ CES UNITÉS NE PASSENT NI PAR L'APPARITION NI PAR L'ENTRETIEN. Leur cas
// tombe sur le défaut de la table de sauts, qui fabrique un objet d'action et le
// lance ; c'est cet objet qui sème, et il nomme ce qu'il sème. Les chercher dans
// SkillUnitEffect ou SkillUnitVisual ne rend rien, et l'unité paraît n'avoir
// aucune image — c'est exactement ce qu'elle a, tant que le nom n'est pas résolu.
//
// ⚠ TOUS CEUX D'UN MÊME MOMENT SE SÈMENT EN MÊME TEMPS. Ce qui les ORDONNE est
// dans la donnée : le délai de départ de chaque fiche et son drapeau d'infini.
// Un `_loop` part quand son `_start` a fini parce que sa fiche porte le délai,
// pas parce que le code l'enchaîne.
[[nodiscard]] std::span<const SkillUnitNamedEffectEntry> SkillUnitNamedEffects(
    std::uint32_t unit_id);

// Ceux d'un moment donné, dans l'ordre où la fabrique les joue.
[[nodiscard]] std::span<const SkillUnitNamedEffectEntry> SkillUnitNamedEffects(
    std::uint32_t unit_id, SkillUnitNamedMoment moment);

}  // namespace ro::render
