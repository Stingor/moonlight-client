#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ÉVENTAIL, et les vingt effets qui l'emploient par le
// sous-type 147 — les dix compteurs de rotation, trois effets de prêtre, et les
// sept lunes de `hanmoon`.
//
// Reproduit DEUX bâtisseurs du client : le sprite dimensionné et le nœud
// d'icône de compétence.
//
// ⚠ LE NUMÉRO DE STYLE N'APPARTIENT PAS AU SEMIS, il appartient à son site
// d'appel. Le style zéro vaut la marque cinq chez l'un et la marque un chez
// l'autre : c'est l'ADR 0019 appliquée à un semis au lieu d'une ressource.
// Chaque nœud porte donc le RANG de son semis dans la table, et non le numéro
// que son bâtisseur a reçu. Garder le style comme clef ferait qu'un bâtisseur
// écraserait l'autre, et l'ordre des tables déciderait de qui gagne.
//
// ⚠ LE SECOND N'EST PAS UN BÂTISSEUR MAIS UNE MISE À JOUR. Le répartiteur le
// rejoue à chaque tic, et il ne bâtit que lorsque le compteur de l'effet atteint
// la PHASE qu'on lui passe. Les sept lunes passent zéro et bâtissent donc au
// premier tic, ce qui revient à semer.
//
// ⚠ ET DEUX IDENTIFIANTS L'APPELLENT EN BOUCLE, avec une phase qui avance de
// trente : dix fois pour EF_RECOGNIZED, cinq pour son frère. Ce ne sont pas des
// patrons à part mais le MÊME, semé dix fois à trente tics d'écart — et c'est le
// compteur NÉGATIF d'une piste qui les échelonne chez nous, comme pour les six
// boules d'EF_STASIS. Les poser tous au même tic montrerait d'un coup ce que le
// client égrène sur neuf secondes.
//
// ⚠ ET LEUR IMAGE N'EST ÉCRITE NULLE PART EN DUR. Le bâtisseur demande au
// Lua le nom d'une compétence TIRÉE AU SORT, le met dans un format coréen et
// charge le fichier ainsi composé. Ce sont les DONNÉES du client qui le
// disent — voir `effect_skill_icon.h`.
//
// ⚠ SON NOM DIT « SPRITE » ET IL NE SÈME AUCUN SPRITE. Il pose une piste de
// quads, et c'est la figure de l'éventail qui la lit. Un nom ne décide de
// rien, le comportement si — ADR 0017.
//
// ⚠ UNE SEULE DES QUATRE PISTES EST VIVE, et les trois autres sont éteintes
// pour de bon : aucun mouvement ne les rallume. Les semer vives ferait sortir
// quatre éventails superposés.
//
// ⚠ ET L'ALPHA NAÎT NUL, comme les dix cases de lame. L'éventail ne se voit pas
// au tic où il est semé : il s'ouvre lame par lame, et c'est le mouvement qui
// l'allume. Le semer brillant lui retirerait son ouverture.

// CE QU'UN SEMIS POSE dans la première piste.
struct EffectSpiralFanStyleSeed {
  // ⚠ LA MARQUE EST CE QUE LA FIGURE LIT, et elle n'est pas le style : les
  // bâtisseurs traduisent. La confondre avec le style donnerait à l'éventail le
  // mauvais écartement, la mauvaise élévation et pas d'aura.
  std::uint8_t draw_mark = 0;
  float radius_at_anchor = 0.0F;
  // De combien l'éventail entier se décale de côté. Nul pour tous les semis
  // connus à ce jour ; le champ existe parce que la figure le lit.
  float sideways = 0.0F;
};

// Tous les semis distincts, dans l'ordre où la table les range.
[[nodiscard]] std::span<const EffectSpiralFanStyleSeed> EffectSpiralFanStyleSeedTable();

// Le semis d'un RANG, ou `nullptr` quand la table n'en a pas tant.
[[nodiscard]] const EffectSpiralFanStyleSeed* EffectSpiralFanStyleSeedAt(std::size_t seed);

// UN ÉVENTAIL QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectSpiralFanNodePattern {
  std::uint32_t effect_id = 0;
  // LE NOM DU BÂTISSEUR qui pose cet éventail chez le client.
  std::string_view client_function;
  std::string_view texture;
  // LE RANG du semis dans la table, et non le numéro de style du client.
  std::uint8_t seed = 0;
  // COMBIEN DE FOIS ce patron se sème, et tous les combien de tics.
  //
  // ⚠ LE SECOND BÂTISSEUR EST UNE MISE À JOUR, pas un semis : le répartiteur le
  // rejoue à chaque tic, et son cas l'appelle en BOUCLE avec une phase qui
  // avance de trente. Dix appels pour EF_RECOGNIZED, cinq pour son frère : ce
  // sont dix et cinq nœuds, nés l'un après l'autre. Les poser tous au même tic
  // montrerait d'un coup ce que le client égrène sur neuf secondes.
  std::uint16_t sow_count = 1;
  std::int32_t sow_period_ticks = 0;
  // ⚠ L'IMAGE SE TIRE AU SORT quand le patron n'en nomme aucune. Le bâtisseur
  // choisit alors une compétence dans deux plages écrites en dur, et compose le
  // chemin de son icône — voir `effect_skill_icon.h`. Un patron sans texture
  // n'est donc pas un patron incomplet.
  bool draws_its_icon = false;
};

// Tous les éventails connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectSpiralFanNodePattern> EffectSpiralFanNodePatternTable();

// TOUS les éventails d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectSpiralFanNodePattern> EffectSpiralFanNodePatternsFor(
    std::uint32_t effect_id);

// LES DEUX PLAGES DE COMPÉTENCES QUE LE TIRAGE COUVRE.
//
// ⚠ ELLES SONT ÉCRITES EN DUR DANS LE BÂTISSEUR : douze à partir de la neuvième
// d'un côté, treize à partir de la quatre-vingtième de l'autre, un pile ou face
// décidant de laquelle. Ce sont les sorts du mage et ceux du sorcier — l'effet
// montre l'icône d'un sort que le personnage pourrait apprendre, et rien d'autre.
inline constexpr std::uint32_t kSpiralFanFirstMageSkill = 9;
inline constexpr std::uint32_t kSpiralFanMageSkillCount = 12;
inline constexpr std::uint32_t kSpiralFanFirstWizardSkill = 80;
inline constexpr std::uint32_t kSpiralFanWizardSkillCount = 13;

// L'ICÔNE QU'UN TIRAGE DÉSIGNE, dans l'une ou l'autre plage.
//
// Rend une vue VIDE quand la table des icônes ne porte pas la compétence tirée —
// ce que la table interdit, puisque chaque chemin est confronté à l'archive, et
// que cette garde dit quand même.
[[nodiscard]] std::string_view EffectSpiralFanDrawnIcon(std::uint32_t& generator);

// LES QUATRE PISTES D'UN ÉVENTAIL : une vive et nue, trois éteintes.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralFanTracks(
    const EffectSpiralFanStyleSeed& seed);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
//
// `delay_ticks` est le RETARD du nœud, NÉGATIF : le mouvement de l'éventail ne
// touche pas une piste tant que son compteur n'est pas remonté à un, et c'est
// ainsi que les dix nœuds d'un même patron s'égrènent. `texture` remplace celle
// du patron quand il n'en nomme aucune — l'icône que le tirage a désignée.
[[nodiscard]] PooledEffectNode SowEffectSpiralFanNode(const EffectSpiralFanNodePattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::int32_t delay_ticks = 0,
                                                      std::string_view texture = {});

}  // namespace ro::render
