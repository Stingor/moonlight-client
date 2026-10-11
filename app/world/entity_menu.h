#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_tactics.h"
#include "ro/ui/entity_menu.h"

namespace moonclient {

// ── LE MENU DU CLIC DROIT SUR UNE ENTITÉ ────────────────────────────────────
//
// Ce fichier décide SUR QUOI le menu s'ouvre et CE QU'IL PROPOSE ; la partie
// joue les lignes choisies, `ui` les dessine.
//
// 🔴 LE BUT N'EST PAS DE REPRODUIRE LE CLIENT D'ORIGINE, C'EST DE COMBLER SES
// MANQUES. Là où il laissait cliquer une action que le serveur allait refuser —
// inviter dans sa guilde quelqu'un qui en a déjà une, proposer une alliance à un
// joueur sans guilde —, la ligne se grise et dit pourquoi. Là où il ne disait
// pas QUI était visé, le menu l'écrit en tête.
//
// 🔴 ET UNE LIGNE QUE CE CLIENT NE SAIT PAS ENCORE JOUER EST ABSENTE, pas
// grisée. Griser dit « pas pour cette cible » ; promettre un geste qui ne peut
// rien faire est pire que de se taire. Ce qui manque est consigné dans
// `TODO.txt` et dans l'ADR 0061.

// Ce que désigne le clic.
enum class EntityFamily : std::uint8_t {
  kNone,
  kSelf,
  kPlayer,
  kMonster,
  // Un PNJ, portail compris.
  kNpc,
  kSkillUnit,
  kGroundItem,
  // Un familier d'autrui, ou ce qu'on ne sait pas classer. Rien d'autre que son
  // identité : le menu n'en est ouvert qu'au staff.
  kOther,
  // NOS TROIS COMPAGNONS À FICHE : le client d'origine leur ouvre un menu à eux,
  // comme aux joueurs.
  kOwnPet,
  kOwnHomunculus,
  kOwnMercenary,
};

// LES ACTEURS DE NOS COMPAGNONS, tels que le serveur les a désignés. Zéro pour
// aucun.
struct OwnCompanionIds {
  std::uint32_t pet_id = 0;
  std::uint32_t homunculus_id = 0;
  std::uint32_t mercenary_id = 0;
};

// Le nom de la famille, en tête du menu et dans ce qu'on copie. DÉJÀ TRADUIT.
[[nodiscard]] const char* EntityFamilyLabel(EntityFamily family);

// ── LES PNJ À IDENTIFIANT FIXE ──────────────────────────────────────────────
//
// Le serveur épingle ses PNJ de service (Kafra, maître des métiers, styliste,
// passeurs…) dans une plage que son allocateur ne distribue jamais : un GID
// d'ici ne peut désigner ni un monstre, ni un familier, ni un PNJ dynamique, et
// il reste le même d'un démarrage à l'autre. C'est ce qui permet au client de
// les reconnaître tout seul, sans rien demander.
//
// ⚠ LA PLAGE EST EN DUR DES DEUX CÔTÉS : l'élargir demande de recompiler le
// serveur.
inline constexpr std::uint32_t kFixedNpcFirstId = 3000000;
inline constexpr std::uint32_t kFixedNpcLastId = 3000099;
[[nodiscard]] constexpr bool IsFixedIdNpc(std::uint32_t id) noexcept {
  return id >= kFixedNpcFirstId && id <= kFixedNpcLastId;
}

// LE NIVEAU DE GROUPE DE L'ADMINISTRATEUR, au-dessus de celui du staff : ce qui
// MODIFIE le monde pour tous les joueurs connectés — décharger un PNJ, recharger
// son script — ne s'ouvre qu'à lui. Le serveur refait le même test.
inline constexpr std::uint8_t kAdminMinGroupLevel = 99;

// La famille d'un acteur, pour le joueur `own_id`.
//
// 🔴 LE GENRE QUE LE SERVEUR ANNONCE TRANCHE, pas la classe affichée : un PNJ
// porte souvent l'apparence d'un joueur ou d'un monstre, et le prendre pour
// l'un d'eux lui ferait proposer « Attaquer ».
//
// 🔴 UN COMPAGNON N'EST LE NÔTRE QUE PAR SON IDENTIFIANT ET SON GENRE ENSEMBLE :
// l'identifiant retenu peut survivre à l'acteur, et le familier d'autrui porte
// le même genre que le nôtre.
[[nodiscard]] EntityFamily ClassifyEntity(const ro::sim::Actor& actor, std::uint32_t own_id,
                                          const OwnCompanionIds& companions = {});

// CE QUE LE JOUEUR A RÉGLÉ, et ses défauts.
struct EntityMenuSettings {
  // Ouvrir aussi sur les monstres, les PNJ, les objets au sol — et sur soi.
  // Éteint d'usine : le client d'origine ne l'a jamais fait, c'est au joueur
  // de le demander.
  bool all_entities = false;
  // Sous-réglage du précédent : le menu sur soi ne porte presque rien, et on se
  // clique dessus sans le vouloir.
  bool self_menu = true;
  // La section du staff, et l'ouverture sur TOUTES les entités sans attendre
  // le réglage du dessus : c'est sur les cibles inattendues qu'elle sert.
  bool staff_extras = true;
  // Le blocage du clic gauche sur les PNJ de service.
  bool npc_block = true;
};

// Le geste du monde : ce qui était tenu quand le bouton droit s'est relâché.
struct WorldMenuGesture {
  bool left_held = false;
  // Maj force l'attaque : pas de menu.
  bool shift_held = false;
  // Alt + clic droit sur un monstre est un ordre au familier : pas de menu.
  bool alt_held = false;
  // Une fenêtre était sous le curseur.
  bool over_interface = false;
};

// Le relâchement du bouton droit sur l'acteur `id`, relevé par le routage des
// événements et joué par la partie dans la même image.
struct EntityMenuAsk {
  std::uint32_t id = 0;
  WorldMenuGesture gesture;
};

class BlockedNpcs;

// CE QUE LES GESTES DU MONDE ÉCHANGENT AVEC LE MENU, passé d'un bloc au routage
// des événements.
struct EntityMenuGestures {
  // Posé au relâchement du bouton droit, consommé par la partie.
  std::optional<EntityMenuAsk> ask;
  // Lus par le clic gauche : un PNJ bloqué ne répond plus. Nul hors partie.
  BlockedNpcs* blocked = nullptr;
  bool npc_block = true;
};

// Le clic droit du MONDE ouvre-t-il le menu sur cette entité ? `staff` : le
// compte est du staff ET veut la section. `storage_open` : le coffre est ouvert,
// et le client d'origine n'ouvre alors aucun menu.
[[nodiscard]] bool WorldClickOpensMenu(EntityFamily family, std::uint32_t id,
                                       const WorldMenuGesture& gesture, bool storage_open,
                                       const EntityMenuSettings& settings, bool staff);

// Le clic droit d'un cadre du HUD de cible ouvre-t-il le menu ?
//
// 🔴 SANS LES GARDES DU MONDE, ni « toutes les entités » : elles servent à
// décider si un clic droit du monde nous appartient, et celui-ci nous
// appartient par construction — on vise un cadre qui n'a pas d'autre usage.
[[nodiscard]] bool FrameClickOpensMenu(EntityFamily family, const EntityMenuSettings& settings);

// ── LES LIGNES ──────────────────────────────────────────────────────────────

// Ce qu'une ligne fait, tel que `ui` le rend. ⚠ ZÉRO N'EST RIEN.
enum class EntityVerb : int {
  kNone = 0,
  kTarget,
  kViewEquip,
  kGuildInvite,
  kGuildAlliance,
  kGuildOpposition,
  kWhisper,
  kCopyName,
  kAttack,
  kTalkToNpc,
  kNpcClickBlock,
  kPickup,
  kItemDescription,
  kItemLink,
  kItemInfo,
  kWhoDrops,
  kHideTargetFrame,
  kCopyId,
  kCopyPickIdentity,
  kComeHere,
  kSitStand,
  kEventPoints,
  kMute,
  kUnmute,
  kNuke,
  kKick,
  kJail,
  kBlockAccount,
  kNpcReloadFile,
  kNpcMoveHere,
  kNpcUnload,
  kMonsterSheet,
  kMonsterLink,
  kPartyInvite,
  kPartyLeader,
  kPartyExpel,
  kFriendAdd,
  kFriendRemove,
  kDealPropose,
  // La conversation 1:1 à part, ouverte sur ce joueur.
  kWhisperWindow,
  // NOTRE PET : sa fiche, et les quatre ordres de son menu.
  kPetStatus,
  kPetFeed,
  kPetPerform,
  kPetUnequip,
  kPetToEgg,
  // NOTRE HOMONCULE : son onglet de la fiche, et le repas.
  kHomunculusStatus,
  kHomunculusFeed,
  // NOTRE MERCENAIRE : son onglet de la fiche.
  kMercenaryStatus,
  // « EN ATTENTE », une case sur nos deux compagnons : l'ordre à leur IA.
  kHomunculusStandby,
  kMercenaryStandby,
  // « AMI DU COMPAGNON », une case sur un joueur : son nom entre dans la liste
  // d'amis du profil de ce compagnon, ou en sort.
  kHomunculusFriend,
  kMercenaryFriend,
  // « INVITER DANS MON CARNET MVP », sur un autre joueur.
  kMvpInvite,
  // « LINKER CE NPC » : sa recherche `<NAVS>`, bornée à notre carte, posée
  // dans la saisie.
  kNpcLink,
  // « BLOQUER » OU « AUTORISER LES CHUCHOTEMENTS » de ce joueur : une seule des
  // deux, selon la liste que le serveur tient.
  kWhisperBlock,
  kWhisperAllow,
  // « ENVOYER UN COURRIER » : l'écriture, adressée à ce joueur.
  kMailSend,
  // « ADOPTER » ce joueur, avec son conjoint.
  kAdopt,
  // L'OBJET AU SOL entre dans la liste de ramassage automatique, ou en sort :
  // une seule des deux, selon la liste que le serveur tient.
  kAutolootAdd,
  kAutolootRemove,
  // « SUIVRE » ce joueur, ou « NE PLUS SUIVRE » celui qu'on suit : une seule
  // des deux. Voir `world/follow_rules.h`.
  kFollow,
  kUnfollow,
  // « PROPRIÉTÉS… », au staff : ce que le serveur sait de l'entité, dans
  // l'inspecteur. Voir `world/entity_inspector.h`.
  kInspect,
  // « JOURNAL DES BLOCAGES DE CHAT », au staff, sur un joueur : les @mute et
  // @unmute qui l'ont visé. Voir `world/chat_block_log.h`.
  kChatBlockLog,
  // « SIGNALER CE JOUEUR » : la fenêtre de signalement, sur un autre joueur.
  // Voir `world/player_report.h`.
  kReportPlayer,
};

// ── LE SOUS-MENU « TACTIQUE DU COMPAGNON » ──────────────────────────────────
//
// Ses lignes ne sont pas dans `EntityVerb` : un verbe par compagnon et par
// choix, au-delà de l'énumération, que `TacticVerbOf` relit.
enum class TacticMenuChoice : std::uint8_t {
  // Les sept rangs, dans l'ordre de `ro::sim::TacticRank`.
  kRankFirst = 0,
  // Les trois déclencheurs, dans l'ordre de `ro::sim::TacticTrigger`.
  kTriggerFirst = 7,
  kRemove = 10,
  kAdvanced = 11,
};

// Les verbes du sous-menu commencent ici, bien au-delà de `EntityVerb`, et
// chaque compagnon en a un bloc.
inline constexpr int kTacticVerbBase = 1000;
inline constexpr int kTacticVerbsPerCompanion = 16;
// 🔴 `EntityVerb` grandit à chaque geste ajouté : s'il atteignait la base, un
// verbe du menu se lirait comme une tactique. Son DERNIER verbe est ici.
static_assert(static_cast<int>(EntityVerb::kReportPlayer) < kTacticVerbBase);
static_assert(static_cast<int>(TacticMenuChoice::kTriggerFirst) ==
              static_cast<int>(ro::sim::kTacticRankCount));
static_assert(static_cast<int>(TacticMenuChoice::kRemove) ==
              static_cast<int>(TacticMenuChoice::kTriggerFirst) +
                  static_cast<int>(ro::sim::kTacticTriggerCount));
static_assert(static_cast<int>(TacticMenuChoice::kAdvanced) < kTacticVerbsPerCompanion);

struct TacticVerbParts {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  // Le rang ou le déclencheur se lisent par décalage depuis leur premier.
  int choice = 0;
};

[[nodiscard]] int TacticVerb(ro::sim::AiCompanion companion, int choice) noexcept;
[[nodiscard]] std::optional<TacticVerbParts> TacticVerbOf(int verb) noexcept;

// Les libellés d'un rang et d'un déclencheur, déjà traduits.
[[nodiscard]] const char* TacticRankLabel(ro::sim::TacticRank rank);
[[nodiscard]] const char* TacticTriggerLabel(ro::sim::TacticTrigger trigger);

// ── LE SOUS-MENU « CONDUITE DU COMPAGNON » ──────────────────────────────────
//
// Sur un joueur, les mêmes choix que la tactique d'un monstre (`TacticMenuChoice`),
// dans un bloc de verbes qui suit ceux des tactiques.
inline constexpr int kTacticVerbCompanions = 2;
inline constexpr int kConductVerbBase =
    kTacticVerbBase + (kTacticVerbCompanions * kTacticVerbsPerCompanion);

[[nodiscard]] int ConductVerb(ro::sim::AiCompanion companion, int choice) noexcept;
[[nodiscard]] std::optional<TacticVerbParts> ConductVerbOf(int verb) noexcept;

// CE QU'UN COMPAGNON PRÉSENT FAIT DE CE JOUEUR : la conduite à son nom, si elle
// existe.
struct CompanionConductFacts {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  std::optional<ro::sim::CompanionPlayerConduct> line;
};

// LES CASES DU SOUS-MENU « CONDUITE » d'un compagnon, recochées d'après sa
// ligne : l'état vrai est celui du profil, pas celui de la case cliquée.
void RefreshConductChecks(ro::ui::EntityMenuView& view, ro::sim::AiCompanion companion,
                          const std::optional<ro::sim::CompanionPlayerConduct>& line);

// CE QU'UN COMPAGNON PRÉSENT FAIT DE CE MONSTRE.
struct CompanionTacticFacts {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  // La ligne de sa classe, si elle existe : la case du rang et celle du
  // déclencheur se cochent d'après elle.
  std::optional<ro::sim::CompanionTactic> line;
};

// CE QU'UN COMPAGNON PRÉSENT PENSE DE CE JOUEUR : un ami de son profil ou non,
// et sa liste pleine.
struct CompanionFriendFacts {
  ro::sim::AiCompanion companion = ro::sim::AiCompanion::kHomunculus;
  bool befriended = false;
  bool full = false;
};

// Ce qu'on sait de la cible et de soi, relevé UNE FOIS à l'ouverture.
struct EntityMenuFacts {
  EntityFamily family = EntityFamily::kNone;
  std::uint32_t id = 0;
  // La classe affichée : un portail n'a pas de dialogue.
  std::uint16_t job = 0;
  // Vide quand on ne le connaît pas.
  std::string name;
  // Le mode Ciblage est allumé : « Cibler » a un sens.
  bool targeting = false;
  // La fenêtre de cible montre CETTE entité.
  bool target_frame_shown = false;
  std::uint32_t own_guild_id = 0;
  bool guild_master = false;
  // Les droits du rang que le joueur occupe sont connus ET lui refusent
  // d'inviter. Faux tant qu'ils sont inconnus : on ne grise pas sur un doute.
  bool guild_invite_forbidden = false;
  std::uint32_t target_guild_id = 0;
  // LE GROUPE ET LES AMIS, vus du joueur. `target_other_party` vient de la carte
  // de nom de la cible : un groupe qui n'est pas le mien.
  bool in_party = false;
  bool party_leader = false;
  bool target_in_party = false;
  bool target_other_party = false;
  bool target_is_friend = false;
  bool friends_full = false;
  // Ses chuchotements sont bloqués, nommément : la ligne propose alors de les
  // autoriser.
  bool whisper_blocked = false;
  // Un échange est ouvert, ou une proposition attend : on n'en propose pas un
  // second.
  bool dealing = false;
  // Le personnage suit déjà ce joueur.
  bool followed = false;
  // Ce qui grise « Envoyer un courrier », DÉJÀ TRADUIT ; vide quand il est
  // offert.
  std::string mail_refusal;
  // Ce que le client sait de l'adoption la permet : sans quoi, pas d'entrée.
  bool adoption_possible = false;
  // Un objet au sol : son identifiant, et s'il est identifié.
  std::uint32_t item_id = 0;
  bool item_identified = false;
  // Sa place dans la liste de ramassage automatique ; sans valeur tant que le
  // serveur n'a pas dit la liste, et l'entrée est alors absente.
  std::optional<bool> item_autolooted;
  bool npc_blocked = false;
  // Notre pet porte un accessoire : sans lui, « Retirer l'accessoire » se grise.
  bool pet_has_accessory = false;
  // Ce qui refuse de nourrir notre pet avant toute question — un courrier en
  // rédaction, un échange en cours —, DÉJÀ TRADUIT ; vide quand rien ne s'y oppose.
  std::string pet_feed_refusal;
  // Ce qui grise « Inviter dans mon carnet MVP », DÉJÀ TRADUIT ; vide quand
  // l'entrée est offerte.
  std::string mvp_invite_refusal;
  // Le carnet de chasse MVP est éteint : l'entrée n'est pas même montrée.
  bool mvp_invite_withdrawn = false;
  // Notre homoncule est au repos : le serveur refuse de le nourrir.
  bool homunculus_resting = false;
  // Le compagnon visé — notre homoncule ou notre mercenaire — est en attente.
  bool companion_standby = false;
  // SUR UN MONSTRE : la tactique de chacun de nos compagnons présents envers
  // sa classe, un sous-menu par compagnon.
  std::vector<CompanionTacticFacts> companion_tactics;
  // SUR UN JOUEUR : ce que chacun de nos compagnons présents pense de lui, et
  // sa conduite envers lui.
  std::vector<CompanionFriendFacts> companion_friends;
  std::vector<CompanionConductFacts> companion_conducts;
  // Niveau de groupe 80 et plus, réglage « outils du staff » allumé.
  bool staff = false;
  // Niveau de groupe 99 et plus.
  bool admin = false;
};

// Compose le menu. Vide quand rien ne se propose.
[[nodiscard]] ro::ui::EntityMenuView BuildEntityMenu(const EntityMenuFacts& facts,
                                                     const EntityMenuSettings& settings);

}  // namespace moonclient
