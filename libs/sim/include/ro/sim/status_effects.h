#pragma once

#include <cstdint>
#include <set>
#include <vector>

namespace ro::sim {

// Un état que le serveur pose sur un acteur et retire plus tard — les EFST de
// `src/map/status.hpp`.
using StatusEffectId = std::uint16_t;

// UN ÉTAT DU JOUEUR, AVEC SA DURÉE.
//
// 🔴 LE MONDE N'EN GARDE LA DURÉE QUE POUR LUI. Les états des autres acteurs
// n'ont besoin que d'une présence — c'est ce qui décide d'une aura ou d'une
// monture. Une barre d'états, elle, montre un compte à rebours, et la durée
// n'arrive que dans les paquets qui la portent.
struct ActiveStatus {
  StatusEffectId effect = 0;

  // ⚠ ZÉRO VEUT DIRE « SANS ÉCHÉANCE », et non « déjà fini ». Un état permanent
  // — une monture, un clan, une altération portée par un bit — n'a pas de fin à
  // annoncer, et le traiter comme échu le ferait disparaître aussitôt posé.
  std::uint32_t expires_ms = 0;

  // De quoi peindre la part écoulée. Zéro quand il n'y a pas d'échéance.
  std::uint32_t total_ms = 0;
};

// 🔴 « 9999 » N'EST PAS UNE DURÉE, C'EST L'AVEU QU'IL N'Y EN A PAS.
// `clif_status_change_sub` l'écrit lui-même : « if (tick <= 0) tick = 9999; this
// is indeed what official servers do ». Le prendre au mot ferait disparaître
// tous les buffs permanents au bout de dix secondes.
inline constexpr std::uint32_t kUnknownDurationMs = 9999;

// FAIRE LE MORT (EFST_TRICKDEAD) : l'acteur se couche comme un mort, et le
// client ne change plus sa pose tant que l'état tient.
inline constexpr StatusEffectId kEfstTrickDead = 29;

// SE TERRER (EFST_SUHIDE) : le Doram s'enfouit, et une compétence qui voudrait
// la posture d'emploi ordinaire ne la lui fait pas prendre.
inline constexpr StatusEffectId kEfstSuHide = 933;

// LE SOMMEIL PROFOND (EFST_DEEP_SLEEP) : endort comme l'état du corps — la
// pose, et les bulles au-dessus de la tête. Voir `NoteSleep`.
inline constexpr StatusEffectId kEfstDeepSleep = 435;

// L'AVEUGLEMENT PROFOND (EFST_HANDICAPSTATE_DEEPBLIND) : il pose lui-même le noir
// du joueur aveuglé, et ce noir survit à la fin de l'aveuglement ordinaire.
inline constexpr StatusEffectId kEfstDeepBlind = 1205;

// ── LES ALTÉRATIONS, QUI N'ARRIVENT PAS COMME LES AUTRES ────────────────────
//
// 🔴 LE SOMMEIL, LE SILENCE ET LE GEL N'ONT PAS D'EFST SUR LE FIL. Vérifié dans
// `db/re/status.yml` : `Sleep` porte un `Opt1: Sleep` et AUCUN `Icon:`,
// `Silence` un `Opt2: Silence` et aucun non plus. Ils n'émettent donc aucun
// paquet d'état — ils arrivent en BITS, dans `ZC_STATE_CHANGE`, et c'est au
// client de les traduire en icônes. Le client natif le fait ; nous n'en avons
// pas sous nous.
//
// ⚠ LA TABLE EST CELLE DU SERVEUR, à la ligne où il la fait lui-même pour son
// propre paquet (`clif.cpp`, `push_once`). Les identifiants, eux, viennent
// d'`efstids.lub` — la table du CLIENT, seule autorité sur les numéros.
inline constexpr StatusEffectId kEfstStoneCurse = 875;
inline constexpr StatusEffectId kEfstFreezing = 876;
inline constexpr StatusEffectId kEfstStun = 877;
inline constexpr StatusEffectId kEfstSleep = 878;
inline constexpr StatusEffectId kEfstStoneCurseInProgress = 880;
inline constexpr StatusEffectId kEfstBurning = 881;
inline constexpr StatusEffectId kEfstImprison = 882;
inline constexpr StatusEffectId kEfstPoison = 883;
inline constexpr StatusEffectId kEfstCurse = 884;
inline constexpr StatusEffectId kEfstSilence = 885;
inline constexpr StatusEffectId kEfstConfusion = 886;
inline constexpr StatusEffectId kEfstHeavyPoison = 890;
inline constexpr StatusEffectId kEfstFear = 891;

// ⚠ CES DEUX-LÀ NE SONT DANS AUCUNE TABLE DU CLIENT : `efstids.lub` ne les
// déclare pas, et `stateiconimginfo.lub` n'a pas d'image pour eux — d'où le
// trou dans la numérotation ci-dessus, 887 et 889 entre des voisins présents.
// L'exécutable n'en nomme pas non plus. La barre d'états n'en invente rien : le
// jour où la table les nomme, leur case paraît d'elle-même.
inline constexpr StatusEffectId kEfstBlind = 887;
inline constexpr StatusEffectId kEfstBleeding = 889;

// L'ÂME QU'UN DORAM TIRE EN FRAPPANT. L'état 982 lève un drapeau de l'acteur,
// et c'est ce seul drapeau qui fait partir `su_soulattack` à chaque coup d'un
// invocateur. Le serveur l'annonce à tous
// (`DisplayPc`) et ne le retire ni à la mort ni au dissipe (`NoRemoveOnDead`,
// `NoClearbuff`, `db/re/status.yml`) — et le client, lui, ne remet jamais le
// drapeau à zéro. La présence de l'état dans `Actor::status_effects` dit donc
// la même chose que lui.
inline constexpr StatusEffectId kEfstSoulAttack = 982;

// LE LIEN D'ÂME d'un Soul Linker, que le serveur pose sur la cible du lien.
inline constexpr StatusEffectId kEfstSoulLink = 149;

// LA NUIT (EFST_SKE). Le serveur la pose sur le joueur quand elle tombe sur une
// carte marquée `nightenabled`, et la retire au jour ou sur une carte qui ne
// l'est pas.
inline constexpr StatusEffectId kEfstNight = 160;

// LES ESPRITS DU SOUL REAPER — fée, faucon, golem, division.
inline constexpr StatusEffectId kEfstSoulFairy = 1056;
inline constexpr StatusEffectId kEfstSoulDivision = 1059;

// SI LA CARTE S'ASSOMBRIT, selon les états du JOUEUR. Ceux d'un autre acteur
// n'y font rien.
//
// ⚠ LA NUIT N'EST PAS SEULE À L'ASSOMBRIR : le lien d'âme et les quatre
// esprits du Soul Reaper lèvent le même drapeau, et donnent la même pénombre.
[[nodiscard]] bool DarkensTheMap(const std::set<StatusEffectId>& player_statuses);

// L'état du corps — `opt1` — et celui de la santé — `opt2` — traduits en EFST.
//
// ⚠ `opt1` EST UNE VALEUR, `opt2` UN MASQUE DE BITS, et le serveur le dit à
// l'endroit où il les lit : les traiter pareil donnerait un sommeil dès qu'un
// poison est actif.
void AilmentStatuses(std::uint16_t body_state, std::uint16_t health_state,
                     std::vector<StatusEffectId>& into);

}  // namespace ro::sim
