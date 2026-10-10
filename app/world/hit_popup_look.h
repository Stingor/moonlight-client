#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "ro/render/floating_number.h"
#include "ro/render/number_look.h"

namespace moonclient {

// CE QUE LE JOUEUR RÈGLE DES DÉGÂTS FLOTTANTS : ce qui se montre, comment un
// coup multiple s'écrit, l'allure de chaque famille de nombres, d'où ils
// partent et où ils arrivent, et leurs couleurs.
//
// 🔴 CHAQUE DÉFAUT EST LE CLIENT. La structure vide reproduit le jeu à
// l'identique, et c'est le fichier de réglages qui lit ces initialisateurs
// comme défauts — voir `settings_file.h`. Il n'y a donc pas de « retour au
// client » à écrire ailleurs : c'est `HitPopupLook{}`.
//
// ⚠ DES DONNÉES PURES, sans monde ni sprite : `HitPopups` les relit à chaque
// image, et les règles qui les appliquent vivent dans `hit_popup_rules.h`, où
// elles s'essaient sans rien monter.

// COMMENT UN COUP MULTIPLE ÉCRIT SES NOMBRES.
//
// ⚠ L'ORDRE EST CELUI DU FICHIER DE RÉGLAGES : c'est un nombre qui s'y écrit,
// et une entrée intercalée changerait le choix de qui l'a déjà fait. Le client
// est donc le DERNIER, parce qu'il a été ajouté en dernier.
enum class HitSplit : int {
  // Coup par coup, chacun portant la somme des coups déjà portés — un tiers,
  // deux tiers, le tout.
  kCumulative = 0,
  // Coup par coup, chacun ne portant que sa part.
  kEachPart,
  // Le total, une fois, à l'instant du premier coup.
  kTotalOnce,
  // Chaque part, ET à côté la somme des coups déjà portés : c'est le client.
  // Il annonce chaque coup avec sa part et son cumul ; sur un monstre, la part
  // jaillit et le cumul MONTE en jaune. Sur un joueur, il ne montre que les
  // parts.
  kPartsAndTotal,
  kCount,
};

// LES TYPES DE DÉGÂTS, QUI ONT CHACUN LEUR VIE ET LEUR TRAJECTOIRE : six
// familles de nombres, et les mots « Miss » et « LUCKY!! ». Même règle d'ordre :
// les clés du fichier de réglages portent le nom du type, mais le cadre d'essai
// le choisit par son rang.
enum class DamagePath : int {
  kOnMonster = 0,
  kSkillOnMonster,
  kOnPlayer,
  kCritical,
  // Le total, à côté des parts d'un coup multiple.
  kTotal,
  // Le soin, et ce qui en prend l'allure : la magie rendue, les nombres des
  // effets.
  kHeal,
  kWords,
  kCount,
};

inline constexpr std::size_t kDamagePathCount = static_cast<std::size_t>(DamagePath::kCount);

[[nodiscard]] constexpr DamagePath DamagePathOf(int rank) noexcept {
  return rank > 0 && rank < static_cast<int>(DamagePath::kCount) ? static_cast<DamagePath>(rank)
                                                                 : DamagePath::kOnMonster;
}

// L'ALLURE DU CLIENT POUR CE TYPE : ce qui lui donne la taille où il s'arrête
// et le point où il naît. ⚠ PLUS UN CHOIX : sa vie et son chemin sont des
// réglages, dont l'allure ne donne que les défauts. Les mots ont la leur, qui
// ne passe pas par là.
[[nodiscard]] constexpr ro::render::NumberStyle StyleOfPath(DamagePath path) noexcept {
  switch (path) {
    case DamagePath::kCritical:
      return ro::render::NumberStyle::kCriticalBurst;
    case DamagePath::kTotal:
    case DamagePath::kWords:
      return ro::render::NumberStyle::kRising;
    case DamagePath::kHeal:
      return ro::render::NumberStyle::kHealing;
    case DamagePath::kOnMonster:
    case DamagePath::kSkillOnMonster:
    case DamagePath::kOnPlayer:
    case DamagePath::kCount:
      break;
  }
  return ro::render::NumberStyle::kBursting;
}

// LA TRAJECTOIRE D'UN TYPE DE DÉGÂT : d'où il part, où il arrive, la courbe
// entre les deux, et la dispersion autour de l'arrivée. En pixels de sprite,
// APRÈS l'échelle ; x vers la droite, y vers le haut.
//
// 🔴 C'EST TOUT SON MOUVEMENT : le départ s'ajoute au point où le client fait
// naître le nombre, et rien d'autre ne le déplace — sauf la dérive de côté,
// quand elle est allumée. Le défaut est le chemin du client : voir
// `ClientPathOf`.
struct NumberPath {
  float start_x_pixels = 0.0F;
  float lift_pixels = 0.0F;
  // Où le nombre finit, par rapport à son point de départ.
  float arrival_x_pixels = 0.0F;
  float arrival_y_pixels = 0.0F;
  // Les deux poignées de la courbe, écartées du tiers puis des deux tiers du
  // chemin droit : voir `ro::render::NumberLook::bend_one_x_pixels`.
  float bend_one_x_pixels = 0.0F;
  float bend_one_y_pixels = 0.0F;
  float bend_two_x_pixels = 0.0F;
  float bend_two_y_pixels = 0.0F;
  // LA DISPERSION tire, pour chaque nombre, un écart au hasard dans ces deux
  // rayons autour du point d'arrivée : deux coups rapprochés ne finissent plus
  // au même pixel, et chacun décrit son propre arc. L'écart est tiré UNE fois,
  // à la naissance — voir `ScatterOf`.
  bool scatter = false;
  float scatter_x_pixels = 12.0F;
  float scatter_y_pixels = 6.0F;
  // LA DÉRIVE DE CÔTÉ DU CLIENT : un déplacement DANS LE MONDE, en diagonale
  // choisie d'après l'orientation de la cible — voir `BurstDriftUnitsAt`. Le
  // client la donne à ses cloches ; elle suit la vie du nombre.
  bool facing_drift = false;
};

// LE CHEMIN QUE LE CLIENT DONNE À CE TYPE : sa montée, ou sa cloche et sa
// dérive de côté. C'est le défaut de chaque trajectoire.
[[nodiscard]] inline NumberPath ClientPathOf(DamagePath path) {
  NumberPath client;
  if (path == DamagePath::kWords) {
    return client;
  }
  const ro::render::NumberStyle style = StyleOfPath(path);
  const ro::render::NumberRoute route = ro::render::ClientRouteOf(style);
  client.arrival_y_pixels = route.arrival_y_pixels;
  client.bend_one_y_pixels = route.bend_y_pixels;
  client.bend_two_y_pixels = route.bend_y_pixels;
  client.facing_drift = ro::render::IsBurst(style);
  return client;
}

[[nodiscard]] inline std::array<NumberPath, kDamagePathCount> ClientPaths() {
  std::array<NumberPath, kDamagePathCount> paths{};
  for (std::size_t rank = 0; rank < kDamagePathCount; ++rank) {
    paths[rank] = ClientPathOf(static_cast<DamagePath>(rank));
  }
  return paths;
}

// LA VIE QUE LE CLIENT DONNE À CHAQUE TYPE. ⚠ CELLE DES MOTS N'EST PAS LUE : ils
// gardent celle du client, qui tient à leur animation.
[[nodiscard]] inline std::array<ro::render::NumberLife, kDamagePathCount> ClientLives() {
  std::array<ro::render::NumberLife, kDamagePathCount> lives{};
  for (std::size_t rank = 0; rank < kDamagePathCount; ++rank) {
    lives[rank] = ro::render::ClientLifeOf(StyleOfPath(static_cast<DamagePath>(rank)));
  }
  return lives;
}

[[nodiscard]] constexpr HitSplit HitSplitOf(int rank) noexcept {
  return rank > 0 && rank < static_cast<int>(HitSplit::kCount) ? static_cast<HitSplit>(rank)
                                                               : HitSplit::kCumulative;
}

// Une couleur en ABGR, transparence comprise — la forme du fichier de réglages
// et celle des nuanciers de l'interface, dont `IM_COL32` range ses octets ainsi.
[[nodiscard]] constexpr std::uint32_t PackAbgr(std::uint8_t red, std::uint8_t green,
                                               std::uint8_t blue,
                                               std::uint8_t alpha = 255) noexcept {
  return (static_cast<std::uint32_t>(alpha) << 24U) | (static_cast<std::uint32_t>(blue) << 16U) |
         (static_cast<std::uint32_t>(green) << 8U) | static_cast<std::uint32_t>(red);
}

// LES COULEURS DU CLIENT, telles que le message 88 les porte : blanc pour un
// monstre ou un PNJ frappé, rouge pour un joueur touché, jaune pour un coup
// critique et pour le cumul d'un coup multiple, et le bleu des points de magie
// quand ce sont eux qu'on entame. Le vert du soin et le bleu de la magie
// rendue viennent de `floating_number.h`, et sont repris tels quels.
inline constexpr std::uint32_t kClientNumberWhite = PackAbgr(255, 255, 255);
inline constexpr std::uint32_t kClientNumberRed = PackAbgr(255, 0, 0);
inline constexpr std::uint32_t kClientNumberYellow = PackAbgr(255, 255, 0);
inline constexpr std::uint32_t kClientNumberSpBlue = PackAbgr(30, 144, 255);
inline constexpr std::uint32_t kClientHealGreen =
    PackAbgr(ro::render::kHealNumberColor[0], ro::render::kHealNumberColor[1],
             ro::render::kHealNumberColor[2]);
inline constexpr std::uint32_t kClientSpRecoveryBlue =
    PackAbgr(ro::render::kSpRecoveryNumberColor[0], ro::render::kSpRecoveryNumberColor[1],
             ro::render::kSpRecoveryNumberColor[2]);

struct HitPopupLook {
  // ── CE QUI SE MONTRE ──────────────────────────────────────────────────────
  //
  // ⚠ TROIS INTERRUPTEURS POUR LES COUPS, selon QUI FRAPPE QUI, et non un seul :
  // en groupe, ce sont les nombres des autres qu'on veut taire pour lire les
  // siens ; en duel, ce sont les siens qu'on connaît et ceux qu'on reçoit qui
  // comptent.
  bool shown = true;
  bool own_dealt = true;
  bool own_taken = true;
  bool others = true;
  bool miss = true;
  bool lucky = true;
  bool critical_star = true;
  // Le soin et la magie rendue par une compétence, et les nombres que dix
  // effets font paraître — la vie et la magie rendues au fil du temps, et les
  // huit chiffres colorés des scripts du serveur.
  bool heals = true;
  bool effect_numbers = true;

  // L'APERÇU EN BOUCLE : des coups factices joués sur le personnage, une
  // famille après l'autre, pour régler la section sans chercher un monstre.
  // ⚠ PAS DANS LE FICHIER DE RÉGLAGES, et éteint dès que le panneau se ferme :
  // un joueur ne doit jamais retrouver son personnage battu par personne.
  bool preview = false;
  // LE TYPE DONT LE VOLET DES DÉGÂTS MONTRE ET RÈGLE LA TRAJECTOIRE. ⚠ PAS
  // DANS LE FICHIER DE RÉGLAGES non plus : c'est la page qu'on regarde.
  int path_shown = static_cast<int>(DamagePath::kOnMonster);
  // LE TEMPS ENTRE DEUX COUPS FACTICES, dans le volet comme dans l'aperçu en
  // boucle, en millisecondes. Plus court, on voit plus vite l'effet d'un
  // réglage ; les nombres se chevauchent d'autant.
  float rehearsal_interval_ms = 800.0F;
  // LA PLACE GARDÉE SOUS LES PIEDS DE LA CIBLE DU VOLET, en pixels d'écran :
  // de quoi la remonter quand une trajectoire descend sous le sol du cadre.
  float preview_floor_pixels = 40.0F;
  // LE PAS DE LA GRILLE DU VOLET, en pixels de sprite : les ancres s'y
  // aimantent. Zéro : ni grille ni aimant.
  float preview_grid_pixels = 10.0F;

  // ── LES COUPS MULTIPLES ───────────────────────────────────────────────────
  int split = static_cast<int>(HitSplit::kPartsAndTotal);
  float hit_interval_ms = ro::render::kMultiHitIntervalMs;
  // LE TOTAL QUI SURVIT D'UN COUP À L'AUTRE. Le client repart de zéro à chaque
  // annonce : son second nombre ne réunit que les frappes d'un même coup, et
  // vaut donc la part quand il n'y en a qu'une. Allumé, ce nombre-là continue
  // d'additionner ce que la MÊME cible encaisse tant que les coups
  // s'enchaînent, et oublie tout après `total_forget_ms` sans rien recevoir.
  // ⚠ IL S'ÉCARTE DU CLIENT, et c'est voulu : un sort de zone ne dit
  // autrement jamais ce qu'il a fait en tout.
  bool total_carries = false;
  float total_forget_ms = 2000.0F;

  // ── CE QUI VAUT POUR TOUS LES NOMBRES ─────────────────────────────────────
  //
  // L'échelle, l'écart entre chiffres, l'opacité, l'abréviation. ⚠ SON CHEMIN
  // ET SA VIE NE SONT PAS LUS : chaque nombre prend ceux de son type, dans
  // `paths` et `lives`. Voir `OnPath`.
  ro::render::NumberLook motion;

  // ── LA VIE ET LA TRAJECTOIRE DE CHAQUE TYPE ───────────────────────────────
  //
  // Au rang de `DamagePath`. ⚠ LE CLIENT CHOISIT SON ALLURE SUR LE MESSAGE DU
  // COUP, PAS SUR LA CIBLE : un coup ordinaire comme une compétence jaillissent
  // sur un monstre et sur un joueur, un critique jaillit un peu plus gros, et
  // c'est le CUMUL d'un coup multiple qui monte. Les défauts de chaque type le
  // reproduisent — voir `StyleOfPath`. Les coups de pied du Taekwon, que le
  // client fait monter, prennent ceux du TOTAL : voir `IsRisingSkill`.
  std::array<NumberPath, kDamagePathCount> paths = ClientPaths();
  std::array<ro::render::NumberLife, kDamagePathCount> lives = ClientLives();

  // ── L'ÉCRITURE ────────────────────────────────────────────────────────────
  //
  // Écrire les nombres avec une police plutôt qu'avec le sprite des chiffres.
  // Même pose, même taille, même couleur : seul le dessin change. Les MOTS —
  // « Miss », la bulle « LUCKY!! » — et les étoiles gardent leur image : ce ne
  // sont pas des chiffres.
  bool font_text = false;
  // LA POLICE, PAR SON NOM : une famille de l'interface, ou une police déposée
  // dans le dossier `fonts\` du jeu. Vide : celle que l'interface emploie. ⚠ PAR
  // LE NOM ET NON PAR UN RANG : le dossier change d'une partie à l'autre, et un
  // rang désignerait alors une autre police sans rien dire.
  std::string font_name;
  // Le gras, pour une famille qui en livre un.
  bool font_bold = true;
  // Le cerne de ce texte, en pixels de sprite AVANT l'échelle : il grossit avec
  // le nombre, comme le liseré dessiné dans les chiffres du client. Zéro : pas
  // de cerne.
  float font_outline_pixels = 1.0F;

  // ── LES COULEURS, transparence comprise ───────────────────────────────────
  //
  // ⚠ LA CIBLE DÉCIDE, PUIS QUI FRAPPE : sur moi, sur un autre joueur, puis mes
  // coups sur un monstre, puis ceux des autres. « Mes coups » ne vaut donc que
  // sur les monstres — sur un joueur, c'est la couleur des joueurs qui prime,
  // comme au client. Le critique l'emporte sur tout cela, et la magie sur le
  // critique : c'est l'ordre du client, et deux nuanciers égaux le reproduisent.
  // Le cumul d'un coup multiple a la sienne, jaune comme le critique.
  //
  // 🔴 LE MOT « MISS » PREND CELLE DE SON PORTEUR, qui est l'ATTAQUANT et non
  // la cible : les trois mêmes nuanciers servent, lus à l'autre bout du coup —
  // voir `MissColor`. Rouge quand je rate, blanc quand un monstre me rate.
  std::uint32_t color_own_dealt = kClientNumberWhite;
  std::uint32_t color_on_monster = kClientNumberWhite;
  std::uint32_t color_on_player = kClientNumberRed;
  std::uint32_t color_on_self = kClientNumberRed;
  std::uint32_t color_critical = kClientNumberYellow;
  std::uint32_t color_total = kClientNumberYellow;
  std::uint32_t color_sp = kClientNumberSpBlue;
  std::uint32_t color_heal = kClientHealGreen;
  std::uint32_t color_sp_recovery = kClientSpRecoveryBlue;
};

// Les bornes de l'intervalle entre deux coups d'un coup multiple, en
// millisecondes. Le client en met deux cents.
inline constexpr float kShortestHitIntervalMs = 40.0F;
inline constexpr float kLongestHitIntervalMs = 600.0F;

// Les bornes de l'oubli du total continu, en millisecondes. Trop court, un sort
// de zone recommence entre deux battements de ses unités ; trop long, deux
// combats séparés se confondent.
inline constexpr float kShortestTotalForgetMs = 200.0F;
inline constexpr float kLongestTotalForgetMs = 10000.0F;

// Les bornes du temps entre deux coups factices, en millisecondes.
inline constexpr float kShortestRehearsalIntervalMs = 100.0F;
inline constexpr float kLongestRehearsalIntervalMs = 2000.0F;

// La plus grande place gardée sous la cible du volet, en pixels d'écran.
inline constexpr float kLargestPreviewFloorPixels = 480.0F;

// Le plus grand pas de la grille du volet, en pixels de sprite.
inline constexpr float kLargestPreviewGridPixels = 50.0F;

// Les bornes d'un rayon de dispersion, en pixels de sprite. ⚠ LE PLUS PETIT
// N'EST PAS NUL : son carré, dans le volet, se poserait sur la croix de
// l'arrivée, et l'on ne pourrait plus le saisir.
inline constexpr float kSmallestScatterPixels = 4.0F;
inline constexpr float kLargestScatterPixels = 200.0F;

// Le cerne le plus épais d'un nombre écrit en police, en pixels de sprite.
inline constexpr float kLargestFontOutlinePixels = 3.0F;

}  // namespace moonclient
