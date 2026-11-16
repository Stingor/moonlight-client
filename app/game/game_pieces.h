#pragma once

// ⚠ CET EN-TÊTE NE TIRE AUCUN ACCESSOIRE : chacun est tenu par `Held`, donc
// seulement déclaré, et une source qui s'en sert inclut son en-tête. C'est ce
// qui borne une reconstruction : toute la partie lit `GamePieces`. Un membre
// neuf d'un type du dépôt se déclare `Held<…>` (voir `game/held.h`).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "game/held.h"

namespace ro::core {
class LoadProgress;
}  // namespace ro::core

namespace ro::render {
class ActorModelRenderer;
class ActorWardrobe;
struct BattlefieldArt;
struct BossBadges;
class CardThumbnails;
struct ClanEmblems;
struct DrawnActor;
class EmotionIcons;
class FontAtlas;
class GameImages;
class GroundQuadRenderer;
class ItemIcons;
struct LoadedActorSprite;
class OverlayRenderer;
struct SignboardSurfaces;
class SkillIcons;
class SkinTextures;
class StatusHeadIcon;
class StatusIcons;
class TitleTable;
}  // namespace ro::render

namespace ro::sim {
class BannedWords;
class ItemNews;
class ProgressReport;
class SkillTree;
class World;
}  // namespace ro::sim

namespace ro::vfs {
class AchievementCatalog;
class AchievementTitles;
struct BookList;
struct CardIllustrations;
struct CardItemList;
class ItemProperties;
struct ItemWords;
struct JobNames;
class MapDisplayNames;
class MonsterList;
struct PackageItems;
struct RandomOptionNames;
class SkillWords;
struct StatusTable;
class Vfs;
class WarpAgents;
class WorldViewTables;
}  // namespace ro::vfs

// LES ACCESSOIRES DU CLIENT : ce qui se charge UNE FOIS, et dont l'absence
// n'empêche pas de jouer.
namespace moonclient {

class AchievementStage;
class ActorCarts;
class ActorFalcons;
class ActorModels;
class AdoptionStage;
class AilmentTints;
class BattleLog;
class BattlegroundStage;
class BroadcastBalloons;
class CartChoiceStage;
class CashShopStage;
class ChatBlockLog;
class ChatMentionWatch;
class ChatRoomStage;
class ChatWebStage;
class CheatGuard;
class CommandsPage;
class CompanionAiStage;
class CompanionTargetMarks;
class CompanionZonesView;
class CraftStage;
class CurseMarks;
struct CursorSprite;
class DamageMeterStage;
class DamagePathSampler;
class DebugConsole;
class DecorVeilView;
class DollEffects;
class DollGifExport;
class EquipWindowCompose;
class EmotionBubbles;
class EntityInspector;
class EntitySampler;
class FrozenIce;
class FrozenPoses;
class GlideWatch;
class GmOutfitAnnouncer;
class GreyWorldGrid;
class GrimoireKeys;
class HitPopups;
class HomunculusStage;
struct ItemBooks;
class ItemEnchantBook;
class ItemObtainToast;
class ItemPickupReport;
class ItemReformBook;
class ItemSheetEditorStage;
class LaphineBook;
class MailStage;
class MapEditSync;
class MapSignStage;
class MapSprites;
class MemorialStage;
class MercenaryStage;
class MinigameStage;
class MusicPlayerStage;
class MvpTrackerStage;
class NightLightView;
class NpcIllustrationStage;
class OverWorldLayer;
class PartyBookingBoard;
class PetStage;
class PlayerReportDesk;
class QuestCatalog;
class QuestMarkerSprites;
class QuestStage;
class RankNotices;
class RankingBoard;
class RecommendedCatalog;
class RepairStage;
class ReputationBoard;
class ReputationBook;
class ShortcutWheel;
class SilenceDots;
class SkillRefusalReport;
class SleepBubbles;
class SpellListStage;
class StaffRoster;
class StunStars;
class SurfaceAnnouncer;
class TalkboxStage;
class TrackerStage;
class TradeStages;
class WhisperStage;

struct SignboardStyle;

// Ils ont tous la même forme — lire des sprites, en faire un objet, et se
// plaindre UNE SEULE FOIS quand le fichier manque. Un client qui refuserait de
// démarrer parce qu'une installation n'a ni police ni curseur serait plus
// fragile que le jeu qu'il remplace ; mieux vaut la flèche de Windows que pas
// de flèche du tout.
//
// ⚠ CHAQUE OBJET POINTE DANS SES SPRITES, et les deux voyagent ENSEMBLE dans
// cette structure : les séparer ferait pendre la vue à la première sortie de
// portée. La DÉPLACER reste sûr — un vecteur déplacé garde son tampon, donc les
// pointeurs continuent de désigner les mêmes images.
struct GamePieces {
  GamePieces();
  ~GamePieces();
  GamePieces(const GamePieces&) = delete;
  GamePieces& operator=(const GamePieces&) = delete;

  // L'HABILLAGE DES FENÊTRES : les bitmaps dont le client compose ses cadres.
  //
  // Ici comme pour le reste, une absence n'empêche rien : sans les huit pièces
  // d'un cadre, l'interface garde son aspect ordinaire. Voir
  // `ro/render/skin_pieces.h`, et `SkinArt` pour ce que l'interface en voit.
  Held<ro::render::SkinTextures> skin;

  // Ce que l'habillage a coûté à lire, en pièces manquantes. À DIRE UNE FOIS :
  // une interface nue s'explique alors, au lieu de passer pour une panne.
  std::size_t skin_missing = 0;

  // La garde-robe : les tables du client, qui disent quels sprites porte
  // chacun d'après ce que le serveur a dit de lui. Sans elles, aucun
  // personnage ne se dessine.
  Held<std::optional<ro::render::ActorWardrobe>> wardrobe;
  // LA TÊTE DU PERSONNAGE SUR LE BOUTON DE STATUT. ⚠ APRÈS LA GARDE-ROBE : elle
  // en compose ses images, et se détruit avant elle.
  Held<std::optional<ro::render::StatusHeadIcon>> status_head;
  // LE GIF DU PANTIN DE LA FICHE : voir `character/doll_gif_export.h`. ⚠ ICI ET
  // NON DANS LA PARTIE, pour survivre à un changement de partie pendant que le
  // dialogue est ouvert. Un pointeur, parce que son fil en garde l'adresse.
  Held<DollGifExport> doll_gif;
  // LA FENÊTRE D'ÉQUIPEMENT DU JEU, à côté de la fiche : ses places, sa
  // silhouette, et « tout retirer ». Bâtie à l'ouverture de la partie, avec les
  // mêmes tables que la fiche — voir `character/equip_window_compose.h`.
  Held<std::optional<EquipWindowCompose>> equip_window;
  // L'ENTITÉ DU CADRE D'ESSAI de la page des barres d'entité.
  Held<EntitySampler> entity_sampler;

  // Ce qu'un coup fait paraître : les chiffres, et le mot qu'il écrit quand il
  // n'entame pas. Sans eux, les coups portent sans rien montrer.
  std::vector<ro::render::LoadedActorSprite> hit_sprites;
  Held<std::optional<HitPopups>> numbers;
  // LE CADRE DES TRAJECTOIRES DE DÉGÂTS, sur les mêmes sprites. ⚠ APRÈS EUX :
  // il en garde les adresses, et ses textures se détruisent avant.
  Held<DamagePathSampler> damage_path_sampler;

  // Les faucons des chasseurs. Leurs sprites se lisent à la PREMIÈRE naissance
  // de chaque oiseau : une installation sans eux laisse les chasseurs sans
  // compagnon, et rien d'autre.
  Held<ActorFalcons> falcons;
  // Les charrettes des marchands, même règle : lues à la première naissance de
  // chaque type.
  Held<ActorCarts> carts;
  // LES SPRITES DE DÉCOR du document d'édition de la carte : voir
  // `world/map_sprites.h`. Composés par la garde-robe, sans elle rien.
  Held<MapSprites> map_sprites;
  // LA LISTE DES CHARRETTES, et les images de ses boutons, lues à la première
  // liste qui les montre.
  Held<CartChoiceStage> cart_choice;
  // LA LISTE DES SORTS D'AUTOSPELL, que le serveur ouvre au lancement.
  Held<SpellListStage> spell_list;
  // LE MODE CLAVIER DU GRIMOIRE : la touche tenue sur une compétence.
  Held<GrimoireKeys> grimoire_keys;
  // LA ROUE D'UTILISATION : une barre en cercle sous le curseur, tant que le
  // bouton est tenu.
  Held<ShortcutWheel> shortcut_wheel;
  // LA LISTE DE REPAIR WEAPON, que le serveur ouvre au lancement.
  Held<RepairStage> repair;
  // LES TROIS COMPAGNONS QUI ONT UNE FICHE : le familier, l'homoncule et le
  // mercenaire.
  Held<PetStage> pet;
  // L'ADOPTION : la demande qu'un couple nous adresse, et les refus du serveur.
  Held<AdoptionStage> adoption;
  // « PROPRIÉTÉS… » DU STAFF : ce que le serveur sait d'une entité désignée.
  Held<EntityInspector> entity_inspector;
  // ET LE JOURNAL DES BLOCAGES DE CHAT D'UN JOUEUR.
  Held<ChatBlockLog> chat_block_log;
  // « JOUEURS EN LIGNE » DU STAFF : la liste que le serveur rend, et sa cadence.
  Held<StaffRoster> staff_roster;
  // « MODIFIER LA FICHE » DU STAFF : la fiche d'un objet éditée, et ses bons.
  Held<ItemSheetEditorStage> item_sheet_editor;
  // « SIGNALER CE JOUEUR » : le joueur visé, l'envoi et l'accusé.
  Held<PlayerReportDesk> player_report;
  Held<HomunculusStage> homunculus;
  Held<MercenaryStage> mercenary;
  // L'IA DE CES DEUX DERNIERS : voir `CompanionAiStage`.
  Held<CompanionAiStage> companion_ai;
  // SES ZONES AU SOL, autour du maître : voir `CompanionZonesView`.
  Held<CompanionZonesView> companion_zones;
  // LA CONSOLE DE MISE AU POINT, servie à chaque image : voir
  // `debug/debug_console.h`.
  Held<DebugConsole> debug_console;
  // LA SAISIE DE TALKIE BOX, et la pose qui attend son texte.
  Held<TalkboxStage> talkbox;
  // LES DEUX ATELIERS, fabrication et refine d'arme : voir `CraftStage`.
  Held<CraftStage> craft;
  // L'illustration que montre un PNJ : voir `NpcIllustrationStage`.
  Held<NpcIllustrationStage> npc_illustration;
  // La réponse qu'attend la barre de progression d'un script : voir
  // `ro::sim::ProgressReport`.
  Held<ro::sim::ProgressReport> progress;
  // Le bandeau du nom de la carte : voir `MapSignStage`.
  Held<MapSignStage> map_sign;

  // Les bulles d'émoticône : un sprite pour toutes, une action par bulle. Il se
  // charge à part de celui des coups — une installation qui n'aurait pas l'un
  // garde l'autre.
  std::vector<ro::render::LoadedActorSprite> emotion_sprite;
  Held<std::optional<EmotionBubbles>> emotions;
  // Les MÊMES sprites, en vignettes pour la grille de la conversation. ⚠ PAS
  // UNE SECONDE LECTURE : la table regarde ceux déjà chargés ci-dessus, et se
  // contente d'en téléverser la première image à la demande.
  Held<std::optional<ro::render::EmotionIcons>> emotion_icons;
  // Les bulles du sommeil, même règle : une installation qui n'aurait pas leur
  // sprite laisse dormir sans bulles.
  std::vector<ro::render::LoadedActorSprite> sleep_bubbles_sprite;
  Held<std::optional<SleepBubbles>> sleep_bubbles;
  // Les points du silence : le sprite des émoticônes, déjà chargé ci-dessus.
  Held<std::optional<SilenceDots>> silence_dots;
  // Les marques de quête des PNJ : le même sprite.
  Held<std::optional<QuestMarkerSprites>> quest_markers;
  // Les étoiles de l'étourdi et la glace du gelé, même règle que les bulles.
  std::vector<ro::render::LoadedActorSprite> stun_stars_sprite;
  Held<std::optional<StunStars>> stun_stars;
  std::vector<ro::render::LoadedActorSprite> frozen_ice_sprite;
  Held<std::optional<FrozenIce>> frozen_ice;
  // La marque du maudit, même règle.
  std::vector<ro::render::LoadedActorSprite> curse_marks_sprite;
  Held<std::optional<CurseMarks>> curse_marks;
  // Ce que les poses figées tiennent d'une image à l'autre : voir `FrozenPoses`.
  Held<FrozenPoses> frozen_poses;
  Held<GlideWatch> glide_watch;
  // La teinte que les altérations ont écrite sur chaque acteur : voir
  // `AilmentTints`.
  Held<AilmentTints> ailment_tints;
  // CE QUI SE PEINT PAR-DESSUS TOUT LE MONDE ET SOUS LES FENÊTRES : voir
  // `OverWorldLayer`.
  Held<OverWorldLayer> over_world;

  // Le curseur du JEU, et de quoi le peindre. Il vient avec son renderer : sans
  // l'un des deux, le curseur du système reste.
  std::vector<ro::render::LoadedActorSprite> cursor_sprites;
  Held<CursorSprite> cursor;
  Held<ro::render::OverlayRenderer> overlay;
  // Le marqueur de cible des compagnons : l'anneau du même fichier.
  Held<std::optional<CompanionTargetMarks>> companion_target_marks;

  // LES DEUX FONTES DE CHIFFRES DU CLASSEMENT PVP : le compteur doré du coin
  // bas-droit, et le chronomètre du premier. ⚠ DEUX FICHIERS ET NON UN — le
  // compteur porte une barre oblique là où le chronomètre porte un deux-points,
  // et leurs chiffres n'ont ni la même taille ni la même couleur.
  //
  // Vides si l'installation ne les porte pas : le classement reste alors muet,
  // et rien d'autre ne s'en trouve gêné.
  std::vector<ro::render::LoadedActorSprite> pvp_rank_sprites;
  std::vector<ro::render::LoadedActorSprite> pvp_stopwatch_sprites;
  Held<CursorSprite> pvp_rank_font;
  Held<CursorSprite> pvp_stopwatch_font;

  // Le curseur AU SOL : le cadre posé sur la case que la souris désigne. Il
  // porte sa propre texture, donc son propre renderer — il est COUCHÉ dans le
  // monde là où tout le reste fait face à la caméra.
  Held<ro::render::GroundQuadRenderer> ground_cursor;

  // LES MARQUES DE SOL DES EFFETS : la grille de visée, et celles qui la
  // suivront. Second renderer, et non le même que le curseur : celui-ci porte
  // une image UNIQUE et fixe, celui-là en retient autant que les tables en
  // nomment.
  //
  // ⚠ IL N'A AUCUNE IMAGE AU DÉMARRAGE, et c'est normal : chaque lot apporte la
  // sienne, prise dans le cache de textures de la troupe. Sans effet de sol en
  // cours, il ne peint rien.
  Held<ro::render::GroundQuadRenderer> effect_ground;
  // LE QUADRILLAGE DE GREYWORLD, peint par le renderer du dessus.
  Held<GreyWorldGrid> grey_world;

  // La police des noms. Sans elle rien ne s'écrit, et le reste tient debout.
  Held<std::optional<ro::render::FontAtlas>> font;

  // LA POLICE DES ÉTIQUETTES D'ENTITÉ, gravée à la taille que le joueur règle.
  //
  // 🔴 UN SECOND ATLAS, ET NON CELUI DU DESSUS : `font` porte aussi les
  // enseignes, le grimoire et le banc d'atelier. Régler « la taille des noms »
  // sur lui les emporterait tous les trois, et le joueur ne verrait pas le
  // rapport.
  //
  // ⚠ REGRAVÉE QUAND LA TAILLE CHANGE, et à ce moment-là seulement : une
  // gravure lit le fichier de police et remplit une texture. Voir
  // `EnsureNameFont`.
  Held<std::optional<ro::render::FontAtlas>> name_font;
  // La taille à laquelle `name_font` est gravée, en pixels ; zéro tant qu'elle
  // ne l'est pas.
  float name_font_pixels = 0.0F;

  // LES DEUX INSIGNES DE BOSS : la tête de mort argent d'un mini-boss, la dorée
  // d'un MVP. ⚠ ELLES VIVENT ICI ET NE SE RECOPIENT PAS — la couche d'écran les
  // reconnaît par leur ADRESSE pour ne les téléverser qu'une fois.
  Held<ro::render::BossBadges> boss_badges;
  // LES EMBLÈMES DE CLAN de l'installation. ⚠ MÊME RÈGLE : reconnus par leur
  // adresse.
  Held<ro::render::ClanEmblems> clan_emblems;
  // LA MARQUE DES ENNEMIS ET LES EMBLÈMES DU SCORE du champ de bataille. ⚠ MÊME
  // RÈGLE : la couche d'écran les reconnaît par leur adresse.
  Held<ro::render::BattlefieldArt> battlefield_art;

  // LES SURFACES DES ENSEIGNES — salons et échoppes —, composées une fois au
  // chargement. ⚠ MÊME RÈGLE : elles ne se recopient pas, la couche d'écran les
  // reconnaît par leur adresse.
  Held<ro::render::SignboardSurfaces> signboards;
  // LA POLICE DU TITRE DES ENSEIGNES, quand le joueur en a choisi une autre que
  // celle du jeu, et le choix d'où elle est gravée — vide tant qu'aucun. Voir
  // `EnsureSignboardFont`.
  Held<std::optional<ro::render::FontAtlas>> signboard_font;
  std::string signboard_font_key;

  // L'ARBRE DES COMPÉTENCES : les cases, les rangs maximaux, les prérequis. Le
  // serveur n'envoie que ce qui est APPRIS ; sans cet arbre le grimoire n'aurait
  // ni grille ni prérequis. Absent, il ne se montre simplement pas.
  Held<std::optional<ro::sim::SkillTree>> skill_tree;

  // Les icônes, lues À LA DEMANDE : mille huit cent quatre-vingt-sept fiches, et
  // une cinquantaine regardées à la fois. Elle garde une référence sur le VFS,
  // qui vit plus longtemps qu'elle.
  Held<std::optional<ro::render::SkillIcons>> skill_icons;

  // CE QUE CHAQUE COMPÉTENCE FAIT, écrit par le jeu. Le fil n'en porte rien : il
  // envoie un numéro et un rang, et le paragraphe qui explique vit dans une table
  // de l'installation. Absente, le grimoire montre des icônes muettes — et rien
  // d'autre ne change.
  //
  // ⚠ ELLE SORT DE LA MÊME CHAÎNE QUE L'ARBRE, et pour cause : sa table s'indexe
  // par `[SKID.X]`, que seul un fichier précédent sait résoudre.
  Held<std::optional<ro::vfs::SkillWords>> skill_words;

  // LES MOTS DONT UN NOM D'OBJET EST FAIT : la multiplicité des cartes, le
  // possessif d'une forge, les mots de chaque carte. Sans eux un objet garde
  // son nom de base, sans ornement — lisible, mais sourd.
  Held<ro::vfs::ItemWords> item_words;
  // Le compte de langue sous lequel ces mots ont été posés. Voir
  // `FollowMessageLanguage` ; une relecture le remet à « jamais ».
  std::size_t item_words_language = static_cast<std::size_t>(-1);

  // LE LIBELLÉ DE CHAQUE TITRE. Le serveur n'envoie qu'un NUMÉRO dans la carte
  // de nom d'un joueur ; le texte est entièrement du côté du client. Absente,
  // une étiquette montre le pseudo sans son titre.
  Held<std::optional<ro::render::TitleTable>> titles;

  // LE COMPTEUR DE DÉGÂTS ENTRE LE MONDE ET SA FENÊTRE.
  Held<DamageMeterStage> damage_meter;

  // LE TITRE QUE RÉCOMPENSE CHAQUE SUCCÈS. Le serveur ne le dit jamais : sans
  // cette table, l'onglet des titres de la fiche reste vide.
  Held<std::optional<ro::vfs::AchievementTitles>> achievement_titles;

  // LE CATALOGUE DES SUCCÈS : nom, objectifs, récompenses, onglet. Le serveur
  // ne pousse que des numéros ; sans lui, la fenêtre des succès reste vide.
  Held<std::optional<ro::vfs::AchievementCatalog>> achievement_catalog;
  // LE CATALOGUE DES QUÊTES, lu dans `data\yaml_files\quests.yml` ; absent,
  // `quest_catalog_problem` dit pourquoi, et le journal le montre.
  Held<std::optional<QuestCatalog>> quest_catalog;
  std::string quest_catalog_problem;
  // LES QUÊTES RECOMMANDÉES, lues dans `recommended_quests.yml` du même
  // dossier. Absentes, l'onglet n'a simplement aucune affiche.
  Held<std::optional<RecommendedCatalog>> recommended_catalog;
  // LA CARTE DU MONDE : ses mondes, leurs cartes et leurs donjons. Absente, la
  // minimap dit que la carte du monde manque.
  Held<std::optional<ro::vfs::WorldViewTables>> world_view;
  // LES CARTES QUI ONT UN WARP AGENT, et sa figure : la carte du monde la
  // montre au survol de la ville. Absentes, elle n'en montre aucun.
  Held<std::optional<ro::vfs::WarpAgents>> warp_agents;
  // La langue dans laquelle les deux catalogues se sont lus. Voir
  // `FollowQuestLanguage`.
  std::string quest_language;

  // LE NOM LISIBLE DE CHAQUE CARTE. Le serveur ne désigne une carte que par son
  // fichier ; sans cette table, les destinations d'un Warp Portal s'écrivent
  // toutes avec le mot d'une destination inconnue, comme chez le client.
  Held<std::optional<ro::vfs::MapDisplayNames>> map_display_names;

  // LE NOM AFFICHABLE DE CHAQUE MÉTIER. Sans lui, la fiche de personnage écrit
  // ses niveaux sans dire de quelle classe ils sont.
  Held<ro::vfs::JobNames> job_names;

  // LE NOM AFFICHABLE DE CHAQUE STATUT. Le serveur n'envoie que des numéros —
  // dans les icônes au-dessus des têtes comme dans les bonus de la fiche.
  Held<ro::vfs::StatusTable> status_table;

  // LES IMAGES DE CES MÊMES STATUTS, lues à la demande. ⚠ APRÈS LA TABLE, qu'elle
  // DÉSIGNE : l'ordre de déclaration décide de l'ordre de destruction, et
  // l'inverse laisserait une référence pendante.
  //
  // ⚠ ET LEUR ABSENCE N'EST PAS UNE PANNE : 67 des 533 images que la table nomme
  // manquent à cette installation, et une case sans image se replie sur son
  // abréviation.
  Held<std::optional<ro::render::StatusIcons>> status_icons;

  // LE NOM DE FICHIER DE L'ILLUSTRATION DE CHAQUE CARTE. ⚠ AVANT LES ICÔNES,
  // qui la DÉSIGNENT : l'ordre de déclaration décide de l'ordre de destruction.
  Held<ro::vfs::CardIllustrations> card_illustrations;
  // LES CARTES QUE NOMME LA LISTE DE L'INSTALLATION, avec ou sans dessin ni mot.
  // La recherche d'échoppes la désigne.
  Held<ro::vfs::CardItemList> card_items;

  // ⚠ APRÈS LA GARDE-ROBE, et ce n'est pas un détail de rangement : ces
  // icônes DÉSIGNENT la table des objets, qui vit dans le catalogue que la
  // garde-robe a absorbé. L'ordre de déclaration décide de l'ordre de
  // destruction, et l'inverse laisserait une référence pendante.
  Held<std::optional<ro::render::ItemIcons>> item_icons;

  // LES VIGNETTES DE L'ALBUM DE CARTES : réduites, bornées, chargées quelques-unes
  // par image. ⚠ APRÈS LES ILLUSTRATIONS, qu'elles DÉSIGNENT.
  Held<std::optional<ro::render::CardThumbnails>> card_thumbnails;
  // CE QUE REND CHAQUE CONTENEUR — boîtes, albums d'objets. L'album de cartes y
  // lit d'où tombe une carte.
  Held<ro::vfs::PackageItems> package_items;
  // CE QUE L'ITEM_DB DU SERVEUR DIT D'UN ITEM : l'empilement, qui borne un
  // « @item » du staff ; le poids et le drapeau de buying store, qui laissent
  // les commerces refuser avant le serveur. Inconnue quand la table manque.
  Held<ro::vfs::ItemProperties> item_properties;
  // CE QUE CHAQUE OBJET DU LAPHINE ACCEPTE. Vide quand la table manque.
  Held<LaphineBook> laphine_book;
  // CE QUE CHAQUE DÉCLENCHEUR DE RÉFORME CHANGE. Vide quand la table manque.
  Held<ItemReformBook> item_reform_book;
  // CE QUE CHAQUE GROUPE D'ENCHANTEMENT POSE. Vide quand la table manque.
  Held<ItemEnchantBook> item_enchant_book;
  // Les trois tables des ateliers, telles que les commerces les lisent.
  [[nodiscard]] ItemBooks item_books() const noexcept;
  // LES FACTIONS DE RÉPUTATION ET LEURS GROUPES. Non lu quand la table manque.
  Held<ReputationBook> reputation_book;
  // LES MONSTRES QUE LE SERVEUR CONNAÎT, que la liste « Monstre de la cible »
  // de l'IA des compagnons propose. Inconnue quand la table manque.
  Held<ro::vfs::MonsterList> monster_list;
  // LES LIBELLÉS DES OPTIONS ALÉATOIRES, que la description écrit sous l'objet.
  Held<ro::vfs::RandomOptionNames> random_option_names;
  // LES ITEMS QUI SONT DES LIVRES, que « Lire » ouvre.
  Held<ro::vfs::BookList> books;

  // LES IMAGES QUE LE SERVEUR NOMME. ⚠ AUCUNE TABLE NE LES DÉCRIT : un script de
  // PNJ désigne un bitmap de l'installation par son chemin, et le client ne peut
  // pas savoir lesquels d'avance. Voir `render::GameImages`, qui dit aussi ce
  // qu'un chemin venu du dehors n'a pas le droit d'atteindre.
  Held<std::optional<ro::render::GameImages>> game_images;

  // LE BANDEAU « objet obtenu ». ⚠ APRÈS LES ICÔNES ET LES MOTS, qu'il
  // DÉSIGNE : l'ordre de déclaration décide de l'ordre de destruction, et
  // l'inverse lui laisserait deux références pendantes.
  Held<std::optional<ItemObtainToast>> item_toast;

  // CE QUE LE RAMASSAGE DIT DANS LA CONVERSATION — le gain en jaune, le
  // refus en rouge. ⚠ APRÈS LES MOTS, qu'il DÉSIGNE, comme son voisin.
  Held<std::optional<ItemPickupReport>> item_report;
  // LES NOUVELLES D'OBJETS que le réseau dépose et que `item_report` écrit : le
  // ramassage d'un membre du groupe, une pièce abîmée.
  Held<ro::sim::ItemNews> item_news;
  // LES BULLES D'ANNONCE du haut de l'écran : ce que le monde a déposé, tenu
  // jusqu'à son extinction.
  Held<BroadcastBalloons> balloons;

  // CE QUE LES COUPS DISENT DANS LA CONVERSATION. ⚠ APRÈS LES MOTS, de même.
  Held<std::optional<BattleLog>> battle_log;

  // CE QU'UN REFUS DE COMPÉTENCE DIT DANS LA CONVERSATION. Il ne désigne
  // rien : il reçoit ses tables à chaque passage.
  Held<SkillRefusalReport> skill_refusals;

  // LA PAGE « COMMANDS SETTINGS » et le miroir des réglages du serveur.
  // ⚠ REMISE À NEUF À CHAQUE PARTIE : un autre personnage n'en partage rien. Son
  // ADRESSE, elle, ne bouge pas — le panneau garde un corps qui la désigne.
  Held<CommandsPage> commands;

  // LES ACTEURS QUI SONT DES MODÈLES — l'Emperium, les gardiens, le drapeau, les
  // coffres — et le renderer qui les peint. Voir `world/actor_models.h`.
  Held<std::optional<ActorModels>> actor_models;
  Held<std::optional<ro::render::ActorModelRenderer>> actor_model_renderer;

  // L'EFFET DE CHAPEAU SUR LE PANTIN DES APERÇUS — celui de la boutique, celui
  // du « ViewID ». Voir `DollEffects`.
  Held<std::optional<DollEffects>> doll_effects;
  // ET CELUI DU PORTRAIT DE STATUT, à lui seul : toujours à l'écran, il ne prend
  // pas le moteur des aperçus, qui ne l'éteignent pas davantage. Né au premier
  // effet à montrer, rendu quand le portrait quitte l'écran.
  Held<std::optional<DollEffects>> portrait_effects;
  // ET CELUI DU PANTIN DE L'ÉDITEUR DE STYLE, qui montre les effets teints de ce
  // qu'on y règle. Né à l'ouverture d'un éditeur dont le joueur affiche un
  // effet, rendu à sa fermeture.
  Held<std::optional<DollEffects>> style_effects;

  // LE DÉCOR QUI SE VOILE DEVANT LE JOUEUR : voir `DecorVeilView`.
  Held<DecorVeilView> decor_veil;

  // LA NUIT DES CARTES : la lumière du sol et du décor. Voir `NightLightView`.
  Held<NightLightView> night_light;

  // LA BOUTIQUE DE VOTE : voir `CashShopStage`. ⚠ EN DERNIER : elle DÉSIGNE
  // les icônes et la garde-robe, et l'ordre de déclaration décide de l'ordre de
  // destruction.
  Held<CashShopStage> cash_shop;

  // LES COMMERCES : boutiques de PNJ, échoppes, troc. Voir `TradeStages`. ⚠ EN
  // DERNIER, pour la même raison : ils désignent les icônes.
  Held<TradeStages> trade;

  // LE COURRIER : voir `MailStage`. ⚠ APRÈS LES COMMERCES, et pour la même
  // raison : il désigne les icônes, les mots, les noms de job et les images de
  // l'installation, que l'ordre de déclaration détruit après lui.
  Held<MailStage> mail;

  // LES SALONS DE DISCUSSION : voir `ChatRoomStage`. Il lit les mots de
  // l'installation, que l'ordre de déclaration détruit après lui.
  Held<ChatRoomStage> chat_rooms;

  // LES CHUCHOTEMENTS ET LES CONVERSATIONS 1:1 : voir `WhisperStage`.
  Held<WhisperStage> whispers;
  // LES MOTS INTERDITS DE L'INSTALLATION, relus à chaque partie : voir
  // `chat/banned_input.h`.
  Held<ro::sim::BannedWords> banned_words;
  // LES ADRESSES ET LES IMAGES DU CHAT, et l'alerte d'une ligne qui nous nomme :
  // voir `ChatWebStage` et `ChatMentionWatch`.
  Held<ChatWebStage> chat_web;
  Held<ChatMentionWatch> mentions;

  // LES SUCCÈS : voir `AchievementStage`. ⚠ APRÈS LE COURRIER, pour la même
  // raison : ils désignent le catalogue, les mots, les icônes et les images.
  Held<AchievementStage> achievements;
  // LES QUÊTES, et le panneau de suivi commun aux succès : voir `QuestStage` et
  // `TrackerStage`. Même raison d'ordre.
  Held<QuestStage> quests;
  Held<TrackerStage> tracker;

  // LE CARNET DE CHASSE MVP : voir `MvpTrackerStage`. ⚠ APRÈS LA GARDE-ROBE ET
  // LES IMAGES, qu'il désigne : l'ordre de déclaration le détruit avant elles.
  Held<MvpTrackerStage> mvp_tracker;
  // LE CHAMP DE BATAILLE : voir `BattlegroundStage`.
  Held<BattlegroundStage> battleground;
  // L'INSTANCE (« Memorial Dungeon ») : voir `MemorialStage`.
  Held<MemorialStage> memorial;
  // LES CLASSEMENTS ET LES AVIS D'ÉTOILE : voir `RankNotices`.
  Held<RankNotices> rank_notices;
  // LE TABLEAU DES CLASSEMENTS : voir `RankingBoard`.
  Held<RankingBoard> ranking_board;
  // LA FENÊTRE DE RÉPUTATION : voir `ReputationBoard`.
  Held<ReputationBoard> reputation_board;
  // LA RECHERCHE DE GROUPE : voir `PartyBookingBoard`.
  Held<PartyBookingBoard> party_booking;
  // LE LECTEUR DE MUSIQUE : voir `MusicPlayerStage`.
  Held<MusicPlayerStage> music_player;
  // LES MINI-JEUX : voir `MinigameStage`.
  Held<MinigameStage> minigames;
  // CE QUE CE CLIENT SAIT MONTRER, annoncé au serveur : voir `SurfaceAnnouncer`.
  Held<SurfaceAnnouncer> surfaces;
  // L'HABIT DU GM DE L'ADMINISTRATEUR, dit au serveur : voir `GmOutfitAnnouncer`.
  Held<GmOutfitAnnouncer> gm_outfit;
  // LES ÉDITIONS DE CARTE QUE LE SERVEUR TIENT : voir `MapEditSync`.
  Held<MapEditSync> map_edit_sync;
  // LE GARDE, qui signale au serveur les outils de triche : voir `CheatGuard`.
  Held<CheatGuard> guard;
};

// Charge les accessoires DANS la structure, plutôt que d'en rendre une.
//
// La garde-robe interdit la copie et ne déclare pas de déplacement — d'autres
// objets pointent dedans —, et la structure hérite de cette contrainte : elle
// se construit là où elle vivra, et ne bouge plus.
//
// TROIS ÉTAPES NOMMÉES, ET NON DES BOOLÉENS.
//
// 🔴 CE QU'ELLES SÉPARENT N'EST PAS « EN JEU OU NON » mais QUI DEMANDE : la
// fenêtre, un décor, une partie. La nuance a un prix à chaque fois. La scène de
// sélection veut la garde-robe SANS aucun monde, et un drapeau `in_game` la lui
// refusait ; le décor vivant la veut AVANT que le joueur ait rien tapé, et
// l'étape de la partie lui coûterait six à dix secondes de démarrage.
//
// L'habillage, le curseur, la police : dès que la fenêtre existe, avant toute
// carte et toute connexion. Rien ici ne dépend d'une partie.
void LoadShellPieces(ro::vfs::Vfs& vfs, GamePieces& pieces);

// LA GARDE-ROBE, ET CE QU'ELLE EXIGE — de quoi habiller quelqu'un, et rien de
// plus.
//
// 🔴 ELLE COÛTE UN TIERS DE SECONDE, ET C'EST MESURÉ : `roc-cli sprite resolve`
// (VFS monté + catalogue) contre `roc-cli map info` (VFS monté seul), trois
// passages chacun, en Release — 563 à 602 ms contre 216 à 265 ms, dont une
// lecture de carte. Le catalogue de sprites est donc AU PLUS trois cent
// cinquante millisecondes, quelque part entre le dixième et le vingtième de
// l'étape du monde. C'est ce qui permet au décor de peupler sa ville sans
// retarder le formulaire.
//
// ⚠ IDEMPOTENTE : la partie la redemande par `LoadWorldPieces`, et un compte
// qui passe par l'écran de connexion l'a déjà.
void LoadBackdropPieces(ro::vfs::Vfs& vfs, GamePieces& pieces);

// Les tables du client, les icônes, les coups et les humeurs — et la garde-robe
// avec, si elle n'est pas déjà là.
//
// ⚠ COÛTEUSE — c'est le poste à six à dix secondes du journal : elle ne se lit
// qu'une fois, voir `ClientShell::EnsureWorldPieces`.
//
// `progress` compte les tables lues, une par une.
void LoadWorldPieces(ro::vfs::Vfs& vfs, GamePieces& pieces,
                     ro::core::LoadProgress* progress = nullptr);

// RELIT LES CATALOGUES DE QUÊTES QUAND LA LANGUE CHANGE, en jeu.
//
// 🔴 APPELÉE À CHAQUE IMAGE, et ne fait rien tant que la langue ne bouge pas.
// ⚠ EN PLACE ET SUR SUCCÈS SEULEMENT : le journal tient l'adresse de chaque
// catalogue, qui ne doit ni changer ni se vider. Un catalogue absent au
// démarrage le reste.
void FollowQuestLanguage(ro::vfs::Vfs& vfs, GamePieces& pieces);

// GRAVE LA POLICE DES NOMS À CETTE TAILLE, si ce n'est pas déjà fait.
//
// 🔴 APPELÉE À CHAQUE IMAGE, ET NE FAIT RIEN LA PLUPART DU TEMPS : c'est ce qui
// garantit qu'un réglage de taille est APPLIQUÉ, et non seulement enregistré.
// Le comparer soi-même chez l'appelant reviendrait à espérer qu'il y pense.
//
// ⚠ LA TAILLE S'ARRONDIT AU PIXEL avant d'être comparée : un curseur rend des
// valeurs continues, et regraver une police à chaque centième de pixel parcouru
// coûterait une lecture de fichier par image.
//
// Rend la police à employer — celle des noms, ou celle de l'interface quand la
// gravure a échoué, ou nul quand aucune des deux n'existe.
[[nodiscard]] const ro::render::FontAtlas* EnsureNameFont(GamePieces& pieces, float pixels);

// LA POLICE DU TITRE DES ENSEIGNES : celle du jeu tant que le joueur n'en a pas
// choisi d'autre, sinon gravée à son choix — et regravée quand il change
// seulement. Une famille introuvable ou illisible retombe sur celle du jeu.
[[nodiscard]] const ro::render::FontAtlas* EnsureSignboardFont(GamePieces& pieces,
                                                               const SignboardStyle& style);

// Fait oublier à la couche d'écran les quatre surfaces d'échoppe, avant qu'on
// les repeigne à la même adresse.
void ForgetShopSignboards(GamePieces& pieces);

// LES SPRITES-ENFANTS QUI TIENNENT au-dessus des acteurs dessinés — bulles du
// sommeil, points du silence, étoiles de l'étourdi, marques de quête, marqueur
// de cible des compagnons —, un lot par sorte, les lots vides écartés. Voir
// `SleepBubbles`, `SilenceDots`, `StunStars`, `QuestMarkerSprites` et
// `CompanionTargetMarks`.
[[nodiscard]] std::vector<std::span<const ro::render::DrawnActor>> DrawnHeldChildSprites(
    GamePieces& pieces, std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

// LA GLACE DES GELÉS DESSINÉS, à peindre DANS la scène : voir `FrozenIce`. Vide
// sans sprite de glace.
[[nodiscard]] std::span<const ro::render::DrawnActor> DrawnFrozenIce(
    GamePieces& pieces, std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent);

// LA MARQUE DES MAUDITS DESSINÉS, à peindre DANS la scène : voir `CurseMarks`.
// Vide sans sprite de marque.
[[nodiscard]] std::span<const ro::render::DrawnActor> DrawnCurseMarks(
    GamePieces& pieces, std::uint32_t now_ms, float camera_yaw_degrees,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, bool translucent);

}  // namespace moonclient
