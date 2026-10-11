#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/ui/shortcut_bar.h"
#include "ro/ui/stat_bars.h"

namespace moonclient {

// ── CE QU'UNE LIGNE DE RÉGLAGE S'APPELLE ────────────────────────────────────
//
// ⚠ À PART DE LA CLASSE, ET C'EST LE PLAFOND DE HUIT CENTS LIGNES QUI L'A
// DÉCIDÉ. La couture n'est pas arbitraire pour autant : ce fichier ne porte que
// des NOMS — ceux des réglages, ceux des sections, et l'encodage des lignes qui
// n'ont pas de nom à elles. Rien ici ne lit ni n'écrit quoi que ce soit.
//
// ⚠ ET CES NOMBRES NE SONT PERSISTÉS NULLE PART : ils vivent le temps d'un aller
// vers la fenêtre et d'un retour. Ce que le fichier de réglages garde, ce sont
// les chaînes de `game_option_keys.cpp`.

// L'identifiant d'une option. ⚠ IL VOYAGE JUSQU'À LA FENÊTRE, qui le rend tel
// quel dans sa demande : c'est ce qui permet aux sections de bouger sans que
// rien ne se décale.
enum class GameOption : std::uint8_t {
  kMusic = 0,
  kMusicVolume,
  kSound,
  kSoundVolume,
  kSpriteSoundVolume,
  kEffectSoundVolume,
  kAmbienceVolume,
  kSpatialSound,
  kEffectsShown,
  kMinimalEffects,
  kLanguage,
  kInterfaceScale,
  kNpcMenuSearch,
  // L'avis qui paraît tant qu'un élément d'interface est déverrouillé.
  kUnlockNotice,
  // ── L'AFFICHAGE ───────────────────────────────────────────────────────────
  // ⚠ L'ÉCRAN AVANT LE MODE : `ApplyAll` suit cet ordre, et le mode se pose sur
  // l'écran où se tient déjà la fenêtre.
  kScreen,
  kDisplayMode,
  kResolution,
  kLockWindowSize,
  kVerticalSync,
  kFrameLimit,
  kFrameMeter,
  kFrameMeterLocked,
  kFrameMeterGraph,
  kFrameMeterPing,
  // LA GRILLE D'ALIGNEMENT. ⚠ TROIS RÉGLAGES ET NON UN : on aligne une fois
  // à la grille visible, puis on l'éteint en gardant l'aimant — ou l'inverse.
  kAlignGrid,
  kAlignGridMagnet,
  kAlignGridCell,
  kAlignGridColor,
  kFixedCamera,
  // `/zoom`
  kExpandedZoom,
  // LES DEUX MODIFICATEURS DE CIBLAGE, ceux que le jeu appelle /noshift et
  // /noctrl. Ils RENDENT PERMANENT ce que la touche fait, ils ne l'inversent
  // jamais — voir `GameOptions::no_shift`.
  kNoShift,
  kNoCtrl,
  // `/skillfail` : les refus de skill que le jeu tait quand il est éteint.
  kSkillFailShown,
  // LES RÉPLIQUES DU CRI ET DE LA PLAISANTERIE, que le personnage envoie au
  // chat quand la danseuse crie ou que le barde plaisante — voir `ActorTalk`.
  kSpeakScream,
  kSpeakFrostJoke,
  // LE BUTIN QUI TOMBE. Quatre réglages : le jaillissement lui-même, puis
  // sa forme — crête, écart du départ, durée du vol.
  kItemDropArc,
  kItemDropArcHeight,
  kItemDropArcKick,
  kItemDropArcSourceHeight,
  kItemDropArcDuration,
  // LES DÉGÂTS FLOTTANTS. ⚠ DES DIZAINES DE RÉGLAGES NOMMÉS, un par ligne du
  // panneau, et UN SEUL objet : leurs rangées et leur aiguillage vivent dans
  // `game_option_damage.cpp`, et l'aiguillage général les y renvoie en bloc.
  // Voir `HitPopupLook`, dont chaque défaut est le client.
  kDamageShown,
  kDamageOwnDealt,
  kDamageOwnTaken,
  kDamageOthers,
  kDamageMiss,
  kDamageLucky,
  kDamageCriticalStar,
  kDamageHeals,
  kDamageEffectNumbers,
  kDamageSplit,
  kDamageHitInterval,
  kDamageScale,
  kDamageDigitPitch,
  kDamageOpacity,
  kDamageAbbreviate,
  kDamageColorOwnDealt,
  kDamageColorMonster,
  kDamageColorPlayer,
  kDamageColorSelf,
  kDamageColorCritical,
  kDamageColorSp,
  kDamageColorHeal,
  kDamageColorSpRecovery,
  // LES ANCRES DE LA TRAJECTOIRE DU TYPE MONTRÉ — l'arrivée, les poignées,
  // les rayons de la dispersion — et sa case. Le départ est `kDamageStartX`.
  // Voir `damage_path_anchors.h`.
  kDamageArrivalX,
  kDamageBendOne,
  kDamageBendTwo,
  kDamageScatter,
  kDamageScatterX,
  kDamageScatterY,
  // La couleur du cumul d'un coup multiple, et l'aperçu en boucle, qui ne
  // s'enregistre pas.
  kDamageColorTotal,
  kDamagePreview,
  // LE TOTAL QUI SURVIT AUX COUPS, et le délai au bout duquel une cible
  // est oubliée. Ces deux-là s'écartent du client, et c'est le joueur qui
  // les allume.
  kDamageTotalCarries,
  kDamageTotalForget,
  // LE DÉPART ; le type dont l'onglet « Allure » montre la trajectoire et la
  // vie ; l'écriture en police et son cerne.
  kDamageStartX,
  kDamagePathShown,
  // Toute la trajectoire du type montré, copiée vers un autre par le menu du
  // cadre. Pas une rangée.
  kDamagePathCopy,
  // Le temps entre deux coups factices, dans le volet et dans l'aperçu en boucle.
  kDamageRehearsalStep,
  // La place gardée sous les pieds de la cible du volet : de quoi la remonter.
  kDamagePreviewFloor,
  // Le pas de la grille du volet, où les ancres s'aimantent.
  kDamagePreviewGrid,
  // LA VIE DU TYPE MONTRÉ — sa taille de naissance et son retour, le début de
  // son fondu, sa durée —, sa dérive de côté, et le geste qui le rend au
  // client, lui seul.
  kDamageBirthScale,
  kDamageBirthSettle,
  kDamageFadeStart,
  kDamageLifeSpan,
  kDamageFacingDrift,
  kDamagePathClient,
  kDamageFontText,
  kDamageFontOutline,
  // La police des dégâts, par son nom, et son gras.
  kDamageFontName,
  kDamageFontBold,
  // ⚠ UN GESTE, PAS UN RÉGLAGE : rend à toute la section l'affichage du
  // client.
  kDamageClientLook,
  // L'apparence de la conversation. ⚠ ELLE N'EST PAS DANS LA FENÊTRE mais
  // dans les réglages, parce qu'elle survit à la partie : une chatbox qu'on
  // règle à chaque connexion n'est pas réglée.
  kChatFont,
  kChatPadding,
  kChatLineGap,
  kChatTimestamps,
  kChatGrowInput,
  // LE BATTLE MODE : la barre de saisie se replie, et le clavier revient au
  // jeu. Le `/bm` du client d'origine.
  kChatBattleMode,
  kChatKeepHistory,
  kChatKeptLines,
  // CE QU'ON A TAPÉ, retrouvé à la flèche ↑ au démarrage suivant.
  kChatKeepTyped,
  kChatKeptTyped,
  kChatKeptPerKind,
  kChatKeepLog,
  kChatDiagnostic,
  kChatBody,
  kChatBorder,
  kChatTabs,
  // Rend aux trois couleurs celles que le client emploie.
  kChatClientColors,
  // Efface ce qui est gardé d'une partie à l'autre.
  kChatForgetHistory,
  // LES BULLES AU-DESSUS DES TÊTES. Elles ne sont pas la chatbox, et n'en
  // partagent aucun réglage : l'une se lit, l'autre passe.
  kBubbleShowOwn,
  kBubbleFade,
  kBubbleClientTiming,
  kBubbleBaseMs,
  kBubblePerCharacterMs,
  kBubbleRise,
  kBubbleWidth,
  kBubbleOpacity,
  kBubbleBackground,
  kBubbleCorner,
  kBubblePaddingX,
  kBubblePaddingY,
  kBubbleBackgroundColor,
  kBubbleBorderColor,
  // La famille de police de TOUTE l'interface. Elle s'applique TOUT DE SUITE :
  // les familles sont toutes bakées à l'ouverture, et la bascule ne coûte qu'un
  // changement de pointeur.
  kFontFamily,
  // LE SAC.
  kInventoryFilter,
  // ⚠ UN RÉGLAGE PAR FENÊTRE, et non un pour les trois sœurs, et c'est voulu :
  // on cherche un nom dans un coffre de six cents lignes bien plus souvent que
  // dans un sac qu'on connaît par cœur.
  kCartFilter,
  kCartLockSize,
  kStorageFilter,
  // LA DISPOSITION DES ONGLETS, une par fenêtre. ⚠ LE MÊME SENS POUR LES TROIS
  // — « verticaux » —, avec un défaut qui diverge : debout dans le sac et la
  // charrette comme au client, couchés dans le coffre dont les colonnes se
  // disputent déjà la largeur.
  kInventoryTabsVertical,
  // LE PLACEMENT LIBRE, ET LE BOUTON QUI LE REMET À PLAT. ⚠ IL EXIGE LA TAILLE
  // VERROUILLÉE : une case est un index absolu, qu'un changement de largeur
  // ferait glisser.
  kInventoryFreeLayout,
  kInventoryForgetPlaces,
  kCartTabsVertical,
  kStorageTabsVertical,
  // CE QUE LA FENÊTRE DU COFFRE MONTRE AUTOUR DE SA TABLE.
  //
  // ⚠ LES COLONNES SONT DES RÉGLAGES ET NON UN MENU D'EN-TÊTE : ImGui garde le
  // sien dans `imgui.ini`, nous gardons celui-ci dans le fichier du joueur, et
  // deux mémoires pour la même colonne se contrediraient d'un démarrage à
  // l'autre.
  kStorageTypeTabs,
  kStorageTabImages,
  kStorageShelfRow,
  kStorageSlotColumn,
  kStorageItemIdColumn,
  kStorageCardSlotsColumn,
  // ⚠ CES DEUX-LÀ SONT ALLUMÉS D'AVANCE, à rebours des trois colonnes du
  // dessus : la valeur est la seule chose qu'on vienne vraiment demander à un
  // coffre qu'on a oublié.
  kStorageValueColumn,
  kStorageTotalValue,
  // ⚠ UN GESTE, PAS UN RÉGLAGE : l'ordre des colonnes vit dans `imgui.ini`, que
  // la table écrit elle-même. Rien à retenir de ce côté-ci.
  kStorageResetColumns,
  // ⚠ OPT-IN ET PROPRE À CHAQUE FENÊTRE : le raccourci est un AJOUT à la barre
  // de titre du client, pas une pièce de son dessin.
  kInventoryStorageButton,
  kCartStorageButton,
  kInventoryLockSize,
  kInventoryDropLock,
  // ⚠ CES DEUX-LÀ NE VALENT QUE SUR L'ONGLET DES FAVORIS, et ils ne se règlent
  // que depuis le pied de la fenêtre — comme au client. Ils passent quand même
  // par le catalogue des options : c'est lui qui les écrit dans le fichier.
  kInventorySellLock,
  kInventorySort,
  kStoragePeerButtons,
  // L'APERÇU DE DESCRIPTION AU SURVOL, UN PAR FENÊTRE.
  //
  // 🔴 ET NON UN SEUL POUR LES TROIS, comme je l'avais d'abord écrit. Les trois
  // ne se lisent pas de la même façon : on RANGE son sac — on y survole vingt
  // cases par seconde, et un pavé de description à chacune gêne —, alors qu'on
  // CHERCHE dans son coffre, où le texte est justement ce qu'on vient voir.
  // C'est aussi le découpage du client.
  kInventoryHoverDescription,
  kCartHoverDescription,
  kStorageHoverDescription,
  kCharacterHoverDescription,
  // LA BARRE D'ÉTATS : ce client n'en a pas d'autre.
  // ── LES SEPT JAUGES LIBRES ──────────────────────────────────────────────
  // ⚠ CEUX-CI SEULEMENT : l'apparence vit dans chaque jauge (`StatBarField`),
  // parce qu'un réglage commun aux six interdirait la disposition pour laquelle
  // les formes existent : deux orbes muets encadrant des barres qui écrivent
  // leurs valeurs ne se règlent pas avec les mêmes nombres.
  kStatBarsShown,
  kStatBarsLocked,
  kStatBarsSticky,
  kStatBarsCenterMark,
  kStatBarsTuning,
  kStatBarsKeepMaxedExp,
  // ⚠ CES DEUX-LÀ NE CONCERNENT QU'UNE JAUGE CHACUN — le poids, la bourse — et
  // leur rangée vit dans l'onglet de celle-là. Ils restent ici parce qu'ils sont
  // UNIQUES, non parce qu'ils seraient communs.
  kStatBarsWarnOverweight,
  kStatBarsZenyReference,
  // ── LE CLASSEMENT PVP : LA PLACE DE SES DEUX PIÈCES ─────────────────────
  //
  // 🔴 AUCUN RÉGLAGE DE TAILLE ICI, ET C'EST VOULU : la taille EST le cadre. On
  // retaille les deux pièces en tirant leurs bords, comme une jauge libre ; un
  // curseur d'échelle en plus dirait la même chose d'une seconde façon, et les
  // deux se contrediraient dès qu'on emploie l'une puis l'autre.
  kPvpHudLocked,
  kPvpHudSticky,
  kPvpHudCenterMark,
  kPvpHudBorder,
  // ── LES BARRES AU-DESSUS DES ENTITÉS ────────────────────────────────────
  // ⚠ TROIS SEULEMENT ICI : tout le reste se règle PAR ESPÈCE, et vit donc dans
  // `EntityBarField`. Le maître, le mode réglage et le silence des marchands ne
  // peuvent pas différer d'une espèce à l'autre.
  kEntityBarsShown,
  kEntityBarsHideVendors,
  // LE MODE RÉGLAGE. ⚠ IL NE SE DÉCLARE PAS AU FICHIER, comme l'aperçu des
  // états : c'est un outil de réglage, et le retrouver allumé à la session
  // suivante ferait chercher d'où viennent des barres qui ondulent.
  kEntityBarsTuning,
  // ── LES ÉTIQUETTES DES ENTITÉS ──────────────────────────────────────────
  // ⚠ TROIS SEULEMENT ICI : tout le reste se règle PAR ESPÈCE, et vit donc dans
  // `EntityNameField`. La taille de la police est commune parce que l'atlas est
  // GRAVÉ — six tailles voudraient six gravures.
  kEntityNamesShown,
  kEntityNamesFont,
  // L'emblème de guilde à gauche de l'étiquette : un seul interrupteur, comme
  // le « /emblem » du client.
  kEntityNamesEmblem,
  kStatusBarShown,
  kStatusBarLocked,
  kStatusBarBorder,
  kStatusBarBackground,
  // ⚠ CELUI-CI NE SE DÉCLARE PAS AU FICHIER, et c'est délibéré : c'est un outil
  // de réglage, pas un mode de jeu. Le retrouver allumé à la session suivante
  // ferait croire à des buffs qu'on n'a pas — et le premier réflexe serait de
  // chercher d'où ils viennent.
  kStatusBarPreview,
  kStatusBarSide,
  kStatusBarGap,
  kStatusBarMost,
  kStatusBarRows,
  kStatusBarAnchor,
  kStatusBarSort,
  kStatusBarCountdown,
  kStatusBarCountdownSize,
  kStatusBarCountdownBackground,
  kStatusBarSweep,
  kStatusBarSweepColor,
  kStatusBarOpacity,
  kStatusBarIconBackground,
  // L'ENREGISTREUR DE ZONE. ⚠ TROIS BOUTONS ET QUATRE CURSEURS : le tracé et la
  // prise sont des GESTES, pas des états, et ce sont eux qu'on vient chercher
  // dans cette section — les curseurs, on les règle une fois.
  // LA ZONE DES SORTS. Forme PLATE : un seul réglage serait par espèce, et
  // aucun ne l'est — la zone est celle du sort, pas de qui le lance.
  kSkillAreaCastShown,
  kSkillAreaArmedShown,
  kSkillAreaPattern,
  kSkillAreaGap,
  kSkillAreaColor,
  kCaptureZone,
  kCaptureRecord,
  kCapturePicture,
  kCaptureFramesPerSecond,
  kCaptureSeconds,
  kCaptureWidth,
  kCaptureDelay,
  // LE BANDEAU « objet obtenu ». ⚠ PAS DE CASE MAÎTRESSE : le bandeau EST la
  // notification de ramassage du client, et l'éteindre laisserait un ramassage
  // muet. Ce qui se règle, c'est sa forme.
  kItemObtainLines,
  kItemObtainNewestOnTop,
  kItemObtainMergeSame,
  kItemObtainDurationMs,
  kItemObtainUnlocked,
  kItemObtainCentered,
  kItemObtainRowGap,
  kItemObtainPadVertical,
  kItemObtainFontScale,
  kItemObtainIcon,
  kItemObtainFrame,
  kItemObtainBackground,
  kItemObtainBackgroundColor,
  kItemObtainBackgroundAlpha,
  kItemObtainRounding,
  kItemObtainBorder,
  kItemObtainBorderColor,
  kItemObtainBorderAlpha,
  kItemObtainBorderThickness,
  kItemObtainTextColor,
  kItemObtainQuantityColor,
  // ── L'ÉCRAN DE VEILLE ─────────────────────────────────────────────────────
  // Vingt-trois réglages NOMMÉS, un par ligne de la page. ⚠ PAS D'ENCODAGE
  // COMPOSÉ ici : il n'y a qu'un objet à régler, pas huit barres ni six
  // espèces, et une entrée par réglage reste lisible.
  kAfkEnabled,
  kAfkDelay,
  kAfkWakeOn,
  kAfkSpin,
  kAfkTilt,
  kAfkZoom,
  kAfkEase,
  kAfkHideUi,
  kAfkHideCursor,
  kAfkVignette,
  kAfkDesaturate,
  kAfkGrain,
  kAfkShowClock,
  kAfkShowAway,
  kAfkClockAnchor,
  kAfkClockMargin,
  kAfkClockScale,
  kAfkClockColor,
  kAfkClockShadow,
  kAfkTryNow,
  kAfkAnnounce,
  kAfkAnnounceDelay,
  kAfkAnnounceSleepSign,
  kAfkAnnounceTagSign,
  kCount,
};

// LES LIGNES DES BARRES DE RACCOURCIS, IDENTIFIÉES HORS DE `GameOption`.
//
// 🔴 LA FICHE 0038 REFUSE QUARANTE ENTRÉES DANS L'ÉNUMÉRATION, et elle a raison :
// elle en porte une par réglage NOMMÉ, et son aiguillage un `case` par entrée.
// Mais rien n'oblige l'identifiant d'une LIGNE à être un `GameOption` — `ui` le
// promène sans le lire, et c'est écrit dans `OptionRowView`. On encode donc la
// barre et le champ dans un nombre, au-delà de toute valeur de l'énumération, et
// `Change` les décode.
//
// ⚠ CES NOMBRES NE SONT PERSISTÉS NULLE PART : ils vivent le temps d'un aller
// vers la fenêtre et d'un retour. Les clés du fichier, elles, sont les chaînes
// composées de `DeclareShortcutBars`.
inline constexpr int kShortcutStickyRow = 989;
inline constexpr int kShortcutShowKeysRow = 990;
inline constexpr int kShortcutLockedRow = 991;
// LE SORT EN MAIN : ce qu'il montre tant qu'il attend sa cible. Rangé avec les
// barres de raccourcis parce que c'est d'elles qu'on arme, et que c'est là qu'on
// le cherche.
inline constexpr int kArmedLevelShownRow = 992;
inline constexpr int kArmedLevelScaleRow = 993;
inline constexpr int kArmedLevelOffsetRow = 994;
inline constexpr int kArmedBannerShownRow = 995;
inline constexpr int kArmedBannerLevelRow = 996;
inline constexpr int kArmedBannerHeightRow = 997;
inline constexpr int kArmedBannerColorRow = 998;
inline constexpr int kShortcutBarRowBase = 1000;
// Les réglages d'une barre, dans l'ordre où ils s'encodent. ⚠ PAS DANS L'ORDRE
// OÙ ILS S'AFFICHENT : la composition met les nuanciers en dernier, leur
// sélecteur s'ouvrant vers le bas et recouvrant les rangées suivantes.
inline constexpr int kShortcutFieldsPerBar = 21;
enum class ShortcutBarField : int {
  kShown = 0,
  kClickThrough,
  kColumns,
  kShownSlots,
  kSlotSide,
  kGap,
  // ── L'APPARENCE ───────────────────────────────────────────────────────────
  kEmptyColor,
  kSkillColor,
  kItemColor,
  kBorderColor,
  kBorderThickness,
  kKeyColor,
  kKeyScale,
  kCountColor,
  kCountScale,
  kCountdownColor,
  kShadowColor,
  kShadowKind,
  kShadowOffset,
  // ⚠ UN GESTE, ET NON UN RÉGLAGE : il recopie l'apparence de cette barre sur
  // les sept autres. Il est DANS l'énumération parce qu'il lui faut un
  // identifiant de ligne comme les autres, et qu'il appartient à une barre —
  // celle dont on copie.
  kCopyLook,
  // UN GESTE AUSSI : il centre cette barre, et elle seule, sur la largeur de
  // l'écran.
  kCenter,
  kCount,
};
// 🔴 CE QUI REND UN SIXIÈME RÉGLAGE IMPOSSIBLE À OUBLIER. Le pas d'encodage et
// le nombre de champs sont deux nombres qui doivent rester égaux ; s'ils
// divergeaient, les lignes d'une barre déborderaient sur la suivante — et le
// curseur de la barre 4 réglerait la barre 5, sans que rien ne le dise.
static_assert(static_cast<int>(ShortcutBarField::kCount) == kShortcutFieldsPerBar);

// ── LES LIGNES D'UNE JAUGE LIBRE ────────────────────────────────────────────
//
// ⚠ MÊME ENCODAGE QUE LES BARRES DE RACCOURCIS, et pour la même raison : sept
// jauges qui portent les mêmes réglages, écrites à la main, seraient douze
// occasions de se tromper de numéro.
//
// ⚠ ET LA GÉOMÉTRIE N'EN EST PAS. Elle se pose à la SOURIS et se persiste dans
// le fichier sans jamais paraître en ligne de panneau : personne ne tape la
// position d'une jauge.
inline constexpr int kStatBarRowBase = 2000;
inline constexpr int kStatBarFieldsPerBar = 18;
enum class StatBarField : int {
  kShown = 0,
  kShape,
  kReversed,
  kVertical,
  kRounding,
  kRingThickness,
  kFill,
  kBackground,
  kGlossy,
  kBorder,
  kBorderColor,
  kText,
  kLabel,
  kFontPixels,
  kTextColor,
  kTextShiftX,
  kTextShiftY,
  // UN GESTE, PAS UN RÉGLAGE : il donne le style de cette jauge aux cinq
  // autres. Voir `ro::ui::CopyStatBarStyle`, qui dit ce qui part et ce qui reste.
  kCopyLook,
  kCount,
};
// 🔴 LE PAS D'ENCODAGE ET LE NOMBRE DE CHAMPS DOIVENT RESTER ÉGAUX : s'ils
// divergeaient, les lignes d'une jauge déborderaient sur la suivante — et le
// nuancier de la jauge de poids réglerait celle de la bourse, sans un mot.
static_assert(static_cast<int>(StatBarField::kCount) == kStatBarFieldsPerBar);

[[nodiscard]] constexpr int StatBarRow(std::size_t bar, StatBarField field) {
  return kStatBarRowBase + static_cast<int>(bar) * kStatBarFieldsPerBar + static_cast<int>(field);
}

[[nodiscard]] constexpr int ShortcutBarRow(std::size_t bar, ShortcutBarField field) {
  return kShortcutBarRowBase + static_cast<int>(bar) * kShortcutFieldsPerBar +
         static_cast<int>(field);
}

// ── LES BARRES DE VIE ET DE MAGIE DES ENTITÉS ───────────────────────────────
//
// Même encodage que les jauges libres, et pour la même raison : SIX espèces qui
// portent les mêmes vingt-quatre réglages sont cent quarante-quatre entrées
// d'énumération qu'on écrirait à la main, et cent quarante-quatre occasions de
// se tromper de numéro.
//
// ⚠ TROIS MILLE, PARCE QUE LES DEUX AUTRES BASES SONT PRISES. Les gardes des
// trois aiguillages se recouvrent sans se marcher dessus : chacun rejette ce qui
// dépasse son propre nombre d'objets.
inline constexpr int kEntityBarRowBase = 3000;
inline constexpr int kEntityBarFieldsPerKind = 24;
enum class EntityBarField : int {
  kShown = 0,
  kHealth,
  kMagic,
  kWhen,
  kHideDead,
  kUnderFeet,
  kOffset,
  kWidth,
  kHeight,
  kMagicHeight,
  kGap,
  kFill,
  kLowFill,
  kLowShare,
  kGradient,
  kMagicFill,
  kBackground,
  kBorder,
  kBorderColor,
  kRounding,
  kOpacity,
  kText,
  kFontPixels,
  kTextColor,
  kCount,
};
// 🔴 MÊME GARDE QUE POUR LES JAUGES : un pas d'encodage qui diverge du nombre de
// champs fait déborder les lignes d'une espèce sur la suivante, et le nuancier
// des monstres réglerait celui des boss sans un mot.
static_assert(static_cast<int>(EntityBarField::kCount) == kEntityBarFieldsPerKind);

[[nodiscard]] constexpr int EntityBarRow(std::size_t kind, EntityBarField field) {
  return kEntityBarRowBase + static_cast<int>(kind) * kEntityBarFieldsPerKind +
         static_cast<int>(field);
}

// LE CADRE D'ESSAI DE L'ONGLET. ⚠ IL NE CHANGE RIEN, et son identifiant ne sert
// qu'à le distinguer des rangées voisines : sous la base, aucun aiguillage ne le
// reconnaît.
inline constexpr int kEntityBarSampleRow = kEntityBarRowBase - 1;
// Et la note d'un onglet qui n'a pas de nom à lui.
inline constexpr int kEntityNameNoteRow = kEntityBarRowBase - 3;

// ── LES ÉTIQUETTES DES ENTITÉS ──────────────────────────────────────────────
//
// Même encodage, et pour la même raison : six espèces qui portent les mêmes
// onze réglages.
//
// ⚠ QUATRE MILLE, AU-DESSUS DES BARRES. Les bases se suivent sans se
// recouvrir parce que chaque aiguillage rejette ce qui dépasse SON propre
// nombre d'objets — et celui des barres s'arrête à 3000 + 6 × 24, soit 3144.
inline constexpr int kEntityNameRowBase = 4000;
inline constexpr int kEntityNameFieldsPerKind = 12;
enum class EntityNameField : int {
  kShown = 0,
  kWhen,
  kTitle,
  kAside,
  kBanner,
  kBossBadge,
  kColor,
  kLevelGap,
  kShadow,
  kShadowColor,
  kOffset,
  kOpacity,
  kCount,
};
// 🔴 MÊME GARDE QUE PARTOUT AILLEURS : un pas d'encodage qui diverge du nombre
// de champs fait déborder les lignes d'une espèce sur la suivante, et le
// nuancier des monstres réglerait celui des boss sans un mot.
static_assert(static_cast<int>(EntityNameField::kCount) == kEntityNameFieldsPerKind);

// L'OPTION QUE CET IDENTIFIANT NOMME, ou rien du tout.
//
// 🔴 TOUT CE QUI REÇOIT UN IDENTIFIANT DE LA FENÊTRE DOIT PASSER PAR ICI, et
// c'est un bug livré qui l'a montré. `GameOption` tient sur HUIT BITS : un
// `static_cast` d'un identifiant COMPOSÉ — ceux des jauges, des barres, des
// étiquettes, qui se comptent par milliers — ne déborde pas, il TRONQUE, et
// retombe sur une option parfaitement valable.
//
// ⚠ ET LA COLLISION N'EST PAS THÉORIQUE : la ligne « Montrer pour cette espèce »
// du premier onglet des étiquettes porte le numéro 4000, qui tronqué vaut 160 —
// `kCaptureRecord`. Cocher cette case de réglage lancerait l'enregistrement
// d'un film, et la ligne d'à côté, 4001, prendrait une capture d'écran.
//
// ⚠ UN `if` DANS CHAQUE APPELANT NE SUFFIRAIT PAS : c'est celui qu'on oublie
// d'écrire qui coûte.
[[nodiscard]] constexpr std::optional<GameOption> NamedOption(int id) {
  if (id < 0 || id >= static_cast<int>(GameOption::kCount)) {
    return std::nullopt;
  }
  return static_cast<GameOption>(id);
}

[[nodiscard]] constexpr int EntityNameRow(std::size_t kind, EntityNameField field) {
  return kEntityNameRowBase + static_cast<int>(kind) * kEntityNameFieldsPerKind +
         static_cast<int>(field);
}

// 🔴 LES PLAGES NE SE RECOUVRENT PAS, ET C'EST L'UNIQUE CHOSE QUI LE GARANTIT.
// Les aiguillages se suivent en cascade — chacun rejette ce qui dépasse SON
// nombre d'objets — et deux plages qui se chevaucheraient feraient régler le
// nuancier d'une barre par le curseur d'une étiquette. Rien à l'écran ne le
// dirait : le mauvais réglage bougerait, simplement.
//
// ⚠ SIX EST LE NOMBRE D'ESPÈCES des deux familles. Il est écrit ici plutôt
// qu'inclus depuis `ro/ui` et `ro/render` : ce fichier ne connaît aucune des
// deux couches, et la garde mordrait de toute façon dès qu'un champ s'ajoute.
inline constexpr int kEntityKindsPerFamily = 6;
static_assert(kEntityBarRowBase + kEntityKindsPerFamily * kEntityBarFieldsPerKind <
                  kEntityNameRowBase,
              "les lignes des barres d'entité déborderaient sur celles des étiquettes");

}  // namespace moonclient
