#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ANNEAU EN COUCHES, et les seize effets qui l'emploient — les cercles de
// magie qui tournent sous un lanceur.
//
// DEUX bâtisseurs : celui de l'anneau en couches, qui sème quatre pistes, et
// celui des trois anneaux, qui n'en sème que TROIS — il éteint la quatrième
// explicitement.
//
// ⚠ LEURS PISTES N'ONT AUCUNE VALEUR EN COMMUN. Rayons, inclinaisons et clés
// diffèrent toutes, et le second est plus serré : des rayons de 3,9 à 4,3 là où
// le premier va de 4 à 5,5. Une seule table de pistes valait tant qu'un seul
// bâtisseur la remplissait ; chaque patron nomme désormais SA tranche.
//
// ⚠ SES QUATRE PISTES NE SONT PAS QUATRE COPIES. C'est ce qui le sépare du
// bâtisseur du sous-type 30, dont les quatre pistes partagent rayon et
// inclinaison. Ici chacune a SON rayon, SON inclinaison, SON azimut, SA clé et
// SON alpha, tous écrits en dur : quatre couronnes emboîtées et penchées
// différemment, d'où le nom. Les semer identiques donnerait un seul anneau
// épais.
//
// ⚠ ET LE SEMIS NE DÉPEND PAS DU PARAMÈTRE. Les cinq valeurs que les sites
// d'appel emploient donnent toutes les mêmes quatre pistes ; le paramètre ne
// choisit que la TEINTE du ruban. Un style par piste n'aurait donc rien à
// décrire, et la table n'en porte pas.
//
// ⚠ LA VALEUR TRENTE-TROIS FAIT BASCULER LE SOUS-TYPE de 23 à 25, et sa teinte
// tombe à zéro. Les deux sous-types vont au MÊME dessinateur, si bien que la
// figure ne change pas ; seule la teinte le fait.

// CE QU'UNE PISTE PORTE À SA NAISSANCE. Les quatre rangées de la table sont les
// quatre pistes, dans l'ordre.
struct EffectLayeredRingTrackSeed {
  std::int16_t degrees = 0;
  std::int16_t azimuth_degrees = 0;
  std::int16_t tilt_degrees = 0;
  std::int16_t alpha = 0;
  float radius_at_anchor = 0.0F;
  // ⚠ CE N'EST PAS UNE HAUTEUR. La figure la compare à des flottants exacts —
  // voir `effect_track_seed.h`. La lire comme une élévation soulèverait des
  // couronnes que le client laisse à plat.
  float lift_key = 0.0F;
};

// TOUTES les pistes des deux bâtisseurs, à plat. Un patron n'en emploie qu'une
// tranche : voir `EffectLayeredRingTracksFor`.
[[nodiscard]] std::span<const EffectLayeredRingTrackSeed> EffectLayeredRingTrackSeedTable();

// UN ANNEAU QU'UN EFFET SÈME : ce que son site d'appel passe au bâtisseur.
//
// ⚠ UN EFFET PEUT EN SEMER DEUX. L'effet 1709 appelle le bâtisseur deux fois, à
// deux textures ; n'en poser qu'un montrerait la moitié de la figure.
struct EffectLayeredRingNodePattern {
  std::uint32_t effect_id = 0;
  // LE BÂTISSEUR qui pose cet anneau, par son nom.
  std::string_view client_function;
  std::string_view texture;
  // Le numéro de teinte que la primitive de ruban lit — voir
  // `effect_trail_palette.h`. C'est la SEULE chose que le paramètre décide.
  //
  // ⚠ ET LES DEUX BÂTISSEURS NE L'ÉCRIVENT PAS PAREIL. Le premier la pose comme
  // une teinte ; le second pose la teinte du nœud à quatre ou sept selon son
  // style, à part du parcours des pistes. C'est le même nombre au bout.
  std::uint8_t trail_variant = 0;

  // LES PISTES DE CE NŒUD, dans la table à plat des deux bâtisseurs. Voir
  // `EffectLayeredRingTracksFor`.
  std::uint16_t first_track = 0;
  std::uint16_t track_count = 0;

  // COMBIEN DE TICS CE NŒUD VIT.
  //
  // ⚠ ELLE N'EST POSÉE QUE PAR UN DES DEUX BÂTISSEURS. Le premier relève la
  // vie de son instance à soixante-dix ; le second ne pose AUCUNE vie, ni sur le
  // nœud ni sur l'instance, et garde donc celle du constructeur du client. La
  // porter dans le patron dit laquelle sans forcer l'une sur l'autre.
  std::int32_t life_ticks = 0;
  // 🔴 SA DERNIÈRE PISTE ONDULE SANS FIN : le paramètre trente-trois fait semer
  // un autre sous-type, au mouvement presque jumeau — voir
  // `AdvanceParticleConeSpawnTracks`. Les halos vert et violet des éléments 2
  // et 5.
  bool waves_last_track = false;
};

// Tous les anneaux connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectLayeredRingNodePattern> EffectLayeredRingNodePatternTable();

// TOUS les anneaux en couches d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectLayeredRingNodePattern> EffectLayeredRingNodePatternsFor(
    std::uint32_t effect_id);

// COMBIEN DE TICS VIT une instance qui n'appelle que le bâtisseur de l'anneau en
// couches.
//
// Ce bâtisseur-LÀ relève lui-même la vie de son instance à ce plancher, et aucun
// des seize identifiants qu'il sert n'appelle d'autre bâtisseur qui la porterait
// plus haut. Le second bâtisseur de la famille n'en pose aucune : son patron
// porte celle du constructeur.
inline constexpr std::int32_t kEffectLayeredRingLifeTicks = 70;

// LES PISTES D'UN PATRON, sa tranche de la table à plat.
[[nodiscard]] std::span<const EffectLayeredRingTrackSeed> EffectLayeredRingTracksFor(
    const EffectLayeredRingNodePattern& pattern);

// LES PISTES, semées. Les rangs au-delà de la tranche restent ÉTEINTS — c'est
// ainsi que le second bâtisseur n'en montre que trois.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectLayeredRingTracks(
    std::span<const EffectLayeredRingTrackSeed> seeds);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectLayeredRingNode(const EffectLayeredRingNodePattern& pattern,
                                                        glm::vec3 ground_position);

}  // namespace ro::render
