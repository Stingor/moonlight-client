// CE QU'UN RÉGLAGE POUSSE DANS LE MOTEUR, un cas par réglage nommé.
//
// 🔴 À PART DE CE QUE LE JOUEUR CHANGE, ET C'EST LE PLAFOND DE HUIT CENTS LIGNES
// QUI L'A DÉCIDÉ. La couture n'est pas arbitraire pour autant : il y a DEUX
// aiguillages sur la même énumération, et ils ne répondent pas à la même
// question. Celui-ci dit « où cette valeur va-t-elle » — le moteur audio, la
// fenêtre du système, le catalogue de langue ; l'autre dit « que vaut-elle
// maintenant ». Les lire ensemble oblige à traverser deux fois trois cents cas.
//
// ⚠ LES DEUX SE LISENT DE BOUT EN BOUT : un cas qui tombe dans le suivant se
// voit, et c'est la raison pour laquelle ils ne sont pas rangés par sujet.

#include <algorithm>
#include <string>

#include "ro/core/log.h"
#include "ro/sim/world.h"
#include "ro/ui/system_fonts.h"
#include "ro/ui/ui_context.h"

#include "chat/chat_history_file.h"
#include "effects/effect_view.h"
#include "options/display_options.h"
#include "options/game_options.h"

namespace moonclient {
namespace {

// LES RÉGLAGES DE LA MACHINE, ceux que `SettingsFile::MachineScope` déclare :
// un profil ne les change pas, donc ne les repousse pas.
[[nodiscard]] bool OnMachine(GameOption which) {
  switch (which) {
    case GameOption::kMusic:
    case GameOption::kMusicVolume:
    case GameOption::kSound:
    case GameOption::kSoundVolume:
    case GameOption::kSpriteSoundVolume:
    case GameOption::kEffectSoundVolume:
    case GameOption::kAmbienceVolume:
    case GameOption::kSpatialSound:
    case GameOption::kScreen:
    case GameOption::kDisplayMode:
    case GameOption::kResolution:
    case GameOption::kLockWindowSize:
    case GameOption::kVerticalSync:
    case GameOption::kFrameLimit:
    case GameOption::kInterfaceScale:
      return true;
    default:
      return false;
  }
}

}  // namespace

void GameOptions::Apply(GameOption which, const OptionTargets& to) {
  switch (which) {
    case GameOption::kMusic:
      if (to.audio != nullptr) {
        to.audio->SetMusicAudible(sound_options_.music);
      }
      // ⚠ RÉTABLIR LE SON NE SUFFIT PAS : la musique ne se redemande qu'au
      // changement de carte, et sans cet oubli il faudrait franchir une porte
      // pour entendre quelque chose.
      if (sound_options_.music && to.music != nullptr) {
        to.music->Forget();
      }
      break;
    case GameOption::kMusicVolume:
      if (to.audio != nullptr) {
        to.audio->SetMusicVolume(sound_options_.music_volume);
      }
      break;
    case GameOption::kSound:
      if (to.audio != nullptr) {
        to.audio->SetSoundAudible(sound_options_.sound);
      }
      break;
    case GameOption::kSoundVolume:
      if (to.audio != nullptr) {
        to.audio->SetSoundVolume(sound_options_.sound_volume);
      }
      break;
    case GameOption::kSpriteSoundVolume:
      if (to.audio != nullptr) {
        to.audio->SetChannelVolume(ro::audio::SoundChannel::kSprites,
                                   sound_options_.sprite_sound_volume);
      }
      break;
    case GameOption::kEffectSoundVolume:
      // LES SONS DE L'INTERFACE SUIVENT CE CURSEUR : ils n'en ont pas à eux.
      if (to.audio != nullptr) {
        to.audio->SetChannelVolume(ro::audio::SoundChannel::kEffects,
                                   sound_options_.effect_sound_volume);
        to.audio->SetChannelVolume(ro::audio::SoundChannel::kInterface,
                                   sound_options_.effect_sound_volume);
      }
      break;
    case GameOption::kAmbienceVolume:
      if (to.audio != nullptr) {
        to.audio->SetChannelVolume(ro::audio::SoundChannel::kAmbience,
                                   sound_options_.ambience_volume);
      }
      break;
    case GameOption::kSpatialSound:
      if (to.audio != nullptr) {
        to.audio->SetSpatialSound(sound_options_.spatial_sound);
      }
      break;
    case GameOption::kLanguage:
      // Rien à pousser : la langue se pose par `SetLanguage`, qui seule sait
      // dire si le catalogue a pu être lu.
      break;
    case GameOption::kScreen: {
      if (to.display == nullptr) {
        break;
      }
      // ⚠ LE RANG SE RELIT ICI, contre les écrans branchés À CET INSTANT : celui
      // de la liste montrée peut dater d'avant un moniteur débranché.
      const int rank = PickScreen(screen_, ro::render::GlContext::ConnectedScreens());
      if (rank < 0 || rank == to.display->screen_rank()) {
        break;
      }
      // 🔴 LE DÉPLACEMENT QUITTE LE PLEIN ÉCRAN : le mode se repose donc sur le
      // nouvel écran, avec sa résolution, puis la fenêtre — que la taille vient
      // peut-être de rabattre — s'y recentre.
      to.display->MoveToScreen(rank);
      Apply(GameOption::kDisplayMode, to);
      if (DisplayModeOf(display_mode_) == ro::render::DisplayMode::kWindowed) {
        to.display->MoveToScreen(rank);
      }
      // Les tailles offertes sont celles du NOUVEL écran.
      ReadResolutions(to);
      break;
    }
    case GameOption::kDisplayMode:
    case GameOption::kResolution: {
      // 🔴 LES DEUX ENSEMBLE, TOUJOURS. Le mode décide de ce que la résolution
      // veut dire — une taille de fenêtre, un mode vidéo, ou rien du tout en
      // sans bordure —, et les poser l'un après l'autre ferait traverser un état
      // que personne n'a demandé.
      if (to.display == nullptr) {
        break;
      }
      const ro::render::Resolution wanted = ResolutionOf(resolution_);
      if (const ro::core::Result<void> posed =
              to.display->SetDisplayMode(DisplayModeOf(display_mode_), wanted);
          !posed) {
        // ⚠ LE MODE ET LA TAILLE DEMANDÉS, faute de quoi la panne ne se rejoue
        // pas : la ligne ne portait que la description de l'erreur.
        ro::core::LogWarning("affichage : mode {} en {}×{} refusé — {}",
                             static_cast<int>(display_mode_), wanted.width, wanted.height,
                             ro::core::Describe(posed.error()));
      }
      // ⚠ ET LA POIGNÉE SE REPOSE APRÈS, parce que le mode vient de décider s'il
      // y en a une.
      Apply(GameOption::kLockWindowSize, to);
      break;
    }
    case GameOption::kLockWindowSize:
      // ⚠ LA POIGNÉE N'EXISTE QU'EN FENÊTRÉ. Ailleurs il n'y a pas de bord à
      // attraper, et SDL garderait la demande pour le retour au mode fenêtré.
      if (to.display != nullptr) {
        to.display->SetResizable(!lock_window_size_ && DisplayModeOf(display_mode_) ==
                                                           ro::render::DisplayMode::kWindowed);
      }
      break;
    case GameOption::kVerticalSync:
      // ⚠ LE PILOTE A LE DERNIER MOT : certains l'imposent, d'autres
      // l'interdisent. Son refus se dit, plutôt que de laisser croire au
      // réglage.
      if (to.display != nullptr && !to.display->SetVerticalSync(vertical_sync_)) {
        ro::core::LogWarning(
            "affichage : synchronisation verticale refusée par le pilote — le réglage reste sans "
            "effet");
      }
      break;
    case GameOption::kFrameLimit:
      // Rien à pousser : la boucle relit `frame_limit()` à chaque image.
      break;
    case GameOption::kFrameMeter:
    case GameOption::kFrameMeterLocked:
    case GameOption::kFrameMeterGraph:
    case GameOption::kFrameMeterPing:
      // ⚠ LES QUATRE DESCENDENT DANS L'ÉTAT DE LA FENÊTRE, comme les réglages du
      // sac : c'est `Refresh` qui les y pose, et `ui` ne lit aucun fichier.
      break;
    case GameOption::kInterfaceScale:
      if (to.interface != nullptr) {
        to.interface->SetInterfaceScale(interface_scale_);
        // Le moteur borne ce qu'on lui donne : on relit plutôt que de garder
        // une valeur qu'il n'a pas retenue.
        interface_scale_ = to.interface->interface_scale();
      }
      break;
    case GameOption::kNpcMenuSearch:
    case GameOption::kUnlockNotice:
    case GameOption::kSkillFailShown:
      // ⚠ RIEN À POUSSER : chacun est relu là où il sert, à chaque image ou à
      // chaque pompage, comme les réglages du sac.
      break;
    case GameOption::kFixedCamera:
      if (to.fixed_camera != nullptr) {
        *to.fixed_camera = fixed_camera_;
      }
      break;
    case GameOption::kEffectsShown:
    case GameOption::kMinimalEffects:
      // LES DEUX ENSEMBLE, parce qu'elles vont au même endroit — et parce que
      // pousser l'une sans l'autre laisserait la troupe procédurale d'accord
      // avec un seul des deux drapeaux.
      //
      // ⚠ RIEN N'EST TUÉ : les portiers du client relisent son drapeau à chaque
      // image et laissent les instances vivre. Taper `/effect` en plein combat
      // n'éteint rien pour de bon — tout revient si l'on rebascule.
      if (to.effects != nullptr) {
        to.effects->SetEffectSwitches(effects_shown_, minimal_effects_);
      }
      break;
    case GameOption::kSkyEffectsShown:
      // Relu à chaque image par `Game::RaiseMapSky`, qui retire ou relève le ciel.
      break;
    case GameOption::kExpandedZoom:
      // Relu à chaque image par les bornes de la caméra.
      break;
    case GameOption::kNoShift:
    case GameOption::kNoCtrl:
      // ⚠ RIEN À POUSSER : le ciblage les RELIT à chaque clic, par
      // `GameOptions::no_shift` et `no_ctrl`. Un exemplaire recopié ailleurs
      // serait un second état à tenir d'accord avec celui-ci.
      break;
    case GameOption::kSpeakScream:
    case GameOption::kSpeakFrostJoke:
      // Rien à pousser non plus : la boucle les relit avant chaque réplique.
      break;
    // LES QUATRE RÉGLAGES DU BUTIN partent ENSEMBLE, et c'est ce qui les garde
    // cohérents : la forme de l'arc n'a de sens qu'avec l'interrupteur qui la
    // commande, et pousser l'un sans l'autre laisserait un butin qui vole à
    // l'ancienne hauteur jusqu'au geste suivant.
    case GameOption::kItemDropArc:
    case GameOption::kItemDropArcHeight:
    case GameOption::kItemDropArcKick:
    case GameOption::kItemDropArcSourceHeight:
    case GameOption::kItemDropArcDuration:
      if (to.item_drop != nullptr) {
        *to.item_drop = item_drop_;
      }
      break;
    case GameOption::kStatBarsShown:
    case GameOption::kBasicInfoWindowShown:
    case GameOption::kStatBarsLocked:
    case GameOption::kStatBarsSticky:
    case GameOption::kStatBarsCenterMark:
    case GameOption::kStatBarsTuning:
    case GameOption::kStatBarsKeepMaxedExp:
    case GameOption::kStatBarsWarnOverweight:
    case GameOption::kStatBarsZenyReference:
    // ⚠ RIEN À POUSSER : les deux cadres du classement PVP sont relus à chaque
    // image, comme les jauges libres.
    case GameOption::kPvpHudLocked:
    case GameOption::kPvpHudSticky:
    case GameOption::kPvpHudCenterMark:
    case GameOption::kPvpHudBorder:
    // ⚠ RIEN À POUSSER NON PLUS : les barres et les noms des entités, comme la
    // zone des sorts, sont relus à chaque image par la composition d'écran,
    // comme les jauges libres.
    case GameOption::kEntityBarsShown:
    case GameOption::kEntityBarsHideVendors:
    case GameOption::kEntityBarsTuning:
    case GameOption::kEntityNamesShown:
    case GameOption::kEntityNamesFont:
    case GameOption::kEntityNamesEmblem:
    case GameOption::kSkillAreaCastShown:
    case GameOption::kSkillAreaArmedShown:
    case GameOption::kSkillAreaPattern:
    case GameOption::kSkillAreaGap:
    case GameOption::kSkillAreaColor:
    case GameOption::kStatusBarShown:
    case GameOption::kStatusBarLocked:
    case GameOption::kStatusBarBorder:
    case GameOption::kStatusBarBackground:
    case GameOption::kStatusBarPreview:
    case GameOption::kStatusBarSide:
    case GameOption::kStatusBarGap:
    case GameOption::kStatusBarMost:
    case GameOption::kStatusBarRows:
    case GameOption::kStatusBarAnchor:
    case GameOption::kStatusBarSort:
    case GameOption::kStatusBarCountdown:
    case GameOption::kStatusBarCountdownSize:
    case GameOption::kStatusBarCountdownBackground:
    case GameOption::kStatusBarSweep:
    case GameOption::kStatusBarSweepColor:
    case GameOption::kStatusBarOpacity:
    case GameOption::kStatusBarIconBackground:
      // Rien à pousser non plus : la barre d'états relit sa disposition à chaque
      // image, comme les barres de raccourcis. ⚠ SON PROPRE GROUPE, et non la
      // fin de celui d'en dessous : un étiquetage de plus au-dessus, et ce
      // serait l'historique de conversation qu'un curseur d'opacité effacerait.
      break;
    case GameOption::kItemObtainUnlocked:
    case GameOption::kItemObtainCentered:
      // ⚠ CES DEUX-LÀ DÉPLACENT LE BANDEAU SOUS SA POIGNÉE : il faut la
      // replacer, une image, sinon elle reste où elle était. Les autres
      // réglages ne la bougent pas et tombent donc dans le groupe muet.
      if (to.item_obtain_grip != nullptr) {
        to.item_obtain_grip->replace = true;
      }
      break;
    case GameOption::kItemObtainLines:
    case GameOption::kItemObtainNewestOnTop:
    case GameOption::kItemObtainMergeSame:
    case GameOption::kItemObtainDurationMs:
    case GameOption::kItemObtainRowGap:
    case GameOption::kItemObtainPadVertical:
    case GameOption::kItemObtainFontScale:
    case GameOption::kItemObtainIcon:
    case GameOption::kItemObtainFrame:
    case GameOption::kItemObtainBackground:
    case GameOption::kItemObtainBackgroundColor:
    case GameOption::kItemObtainBackgroundAlpha:
    case GameOption::kItemObtainRounding:
    case GameOption::kItemObtainBorder:
    case GameOption::kItemObtainBorderColor:
    case GameOption::kItemObtainBorderAlpha:
    case GameOption::kItemObtainBorderThickness:
    case GameOption::kItemObtainTextColor:
    case GameOption::kItemObtainQuantityColor:
      // Rien à pousser : l'étage d'interface relit le bandeau à chaque image,
      // comme les barres. ⚠ SON PROPRE GROUPE, et non la fin de celui d'en
      // dessous — la garde de ce switch est d'être EXHAUSTIF sans `default`.
      break;
    // ⚠ LES FENÊTRES D'OBJETS RELISENT LEURS CASES À CHAQUE IMAGE : voir
    // `GameSettings::Refresh`, qui les y descend.
    case GameOption::kInventoryFilter:
    case GameOption::kInventoryLockSize:
    case GameOption::kInventoryDropLock:
    case GameOption::kInventorySellLock:
    case GameOption::kInventorySort:
    case GameOption::kInventoryStorageButton:
    case GameOption::kInventoryHoverDescription:
    case GameOption::kCartFilter:
    case GameOption::kCartLockSize:
    case GameOption::kCartStorageButton:
    case GameOption::kCartHoverDescription:
    case GameOption::kStorageFilter:
    case GameOption::kStoragePeerButtons:
    case GameOption::kStorageHoverDescription:
    case GameOption::kCharacterHoverDescription:
    case GameOption::kInventoryTabsVertical:
    case GameOption::kInventoryFreeLayout:
    case GameOption::kInventoryForgetPlaces:
    case GameOption::kCartTabsVertical:
    case GameOption::kStorageTabsVertical:
    case GameOption::kStorageTypeTabs:
    case GameOption::kStorageTabImages:
    case GameOption::kStorageShelfRow:
    case GameOption::kStorageSlotColumn:
    case GameOption::kStorageItemIdColumn:
    case GameOption::kStorageCardSlotsColumn:
    case GameOption::kStorageValueColumn:
    case GameOption::kStorageTotalValue:
    case GameOption::kStorageResetColumns:
    case GameOption::kChatFont:
    case GameOption::kAlignGrid:
    case GameOption::kAlignGridMagnet:
    case GameOption::kAlignGridCell:
    case GameOption::kAlignGridColor:
    case GameOption::kChatPadding:
    case GameOption::kChatLineGap:
    case GameOption::kChatTimestamps:
    case GameOption::kChatGrowInput:
    case GameOption::kChatBattleMode:
    case GameOption::kChatKeepHistory:
    case GameOption::kChatKeptLines:
    case GameOption::kChatKeepTyped:
    // Le journal sur le disque et le diagnostic se repoussent aussi à chaque
    // image : voir `Game::StageChat`.
    case GameOption::kChatKeepLog:
    case GameOption::kChatDiagnostic:
    case GameOption::kChatKeptTyped:
    case GameOption::kChatBody:
    case GameOption::kChatBorder:
    case GameOption::kChatTabs:
    case GameOption::kBubbleShowOwn:
    case GameOption::kBubbleFade:
    case GameOption::kBubbleClientTiming:
    case GameOption::kBubbleBaseMs:
    case GameOption::kBubblePerCharacterMs:
    case GameOption::kBubbleRise:
    case GameOption::kBubbleWidth:
    case GameOption::kBubbleOpacity:
    case GameOption::kBubbleBackground:
    case GameOption::kBubbleCorner:
    case GameOption::kBubblePaddingX:
    case GameOption::kBubblePaddingY:
    case GameOption::kBubbleBackgroundColor:
    case GameOption::kBubbleBorderColor:
    case GameOption::kChatClientColors:
      // Rien à pousser : la chatbox lit son apparence à chaque image, et
      // l'historique se relit au démarrage, s'écrit à la fermeture. Un réglage
      // qu'on leur enverrait serait une seconde copie à tenir à jour.
      break;
    case GameOption::kChatForgetHistory:
      // 🔴 SEUL DANS SON CAS, jamais à la fin d'un groupe de chutes :
      // un étiquetage de plus au-dessus, et c'est une couleur qu'on change qui
      // effacerait l'historique.
      if (to.chat != nullptr) {
        to.chat->ForgetPast();
      }
      break;
    case GameOption::kChatKeptPerKind:
      // Celui-ci, LUI, se pousse : le tampon vit dans le monde, pas dans la
      // fenêtre. C'est la seule pièce de l'apparence qui décrive autre chose
      // que du dessin.
      if (to.world != nullptr) {
        to.world->SetChatLinesKeptPerKind(chat_kept_per_kind_);
        // Le monde borne ce qu'on lui donne : on relit plutôt que de garder
        // une valeur qu'il n'a pas retenue.
        chat_kept_per_kind_ = to.world->chat_lines_kept_per_kind();
      }
      break;
    case GameOption::kFontFamily:
      // ⚠ TOUT DE SUITE, ET SANS REBÂTIR L'ATLAS : les douze familles y sont
      // déjà, bakées à l'ouverture, et la bascule ne coûte qu'un changement de
      // pointeur. C'est pour cela qu'elles sont toutes chargées d'avance — un
      // réglage d'apparence qui demande de redémarrer n'est pas un réglage,
      // c'est une option de lancement.
      if (to.interface != nullptr) {
        to.interface->ChooseFont(font_family_);
      }
      break;
    case GameOption::kCaptureZone:
    case GameOption::kCaptureRecord:
    case GameOption::kCapturePicture:
    case GameOption::kCaptureFramesPerSecond:
    case GameOption::kCaptureSeconds:
    case GameOption::kCaptureWidth:
    case GameOption::kCaptureDelay:
      // Rien à pousser : l'enregistreur relit ses réglages à chaque image, et
      // ses trois gestes passent par la boucle.
      break;
    case GameOption::kDamageShown:
    case GameOption::kDamageOwnDealt:
    case GameOption::kDamageOwnTaken:
    case GameOption::kDamageOthers:
    case GameOption::kDamageMiss:
    case GameOption::kDamageLucky:
    case GameOption::kDamageCriticalStar:
    case GameOption::kDamageHeals:
    case GameOption::kDamageEffectNumbers:
    case GameOption::kDamageSplit:
    case GameOption::kDamageHitInterval:
    case GameOption::kDamageScale:
    case GameOption::kDamageDigitPitch:
    case GameOption::kDamageOpacity:
    case GameOption::kDamageAbbreviate:
    case GameOption::kDamageColorOwnDealt:
    case GameOption::kDamageColorMonster:
    case GameOption::kDamageColorPlayer:
    case GameOption::kDamageColorSelf:
    case GameOption::kDamageColorCritical:
    case GameOption::kDamageColorSp:
    case GameOption::kDamageColorHeal:
    case GameOption::kDamageColorSpRecovery:
    case GameOption::kDamageArrivalX:
    case GameOption::kDamageBendOne:
    case GameOption::kDamageBendTwo:
    case GameOption::kDamageScatter:
    case GameOption::kDamageScatterX:
    case GameOption::kDamageScatterY:
    case GameOption::kDamageColorTotal:
    case GameOption::kDamagePreview:
    case GameOption::kDamageTotalCarries:
    case GameOption::kDamageTotalForget:
    case GameOption::kDamageStartX:
    case GameOption::kDamagePathShown:
    case GameOption::kDamagePathCopy:
    case GameOption::kDamageRehearsalStep:
    case GameOption::kDamagePreviewFloor:
    case GameOption::kDamagePreviewGrid:
    case GameOption::kDamageBirthScale:
    case GameOption::kDamageBirthSettle:
    case GameOption::kDamageFadeStart:
    case GameOption::kDamageLifeSpan:
    case GameOption::kDamageFacingDrift:
    case GameOption::kDamagePathClient:
    case GameOption::kDamageFontText:
    case GameOption::kDamageFontOutline:
    case GameOption::kDamageFontName:
    case GameOption::kDamageFontBold:
    case GameOption::kDamageClientLook:
      // Rien à pousser : les nombres relisent le réglage à chaque image, et une
      // copie qu'on leur enverrait serait une seconde source à tenir à jour.
      break;
    case GameOption::kAfkEnabled:
    case GameOption::kAfkDelay:
    case GameOption::kAfkWakeOn:
    case GameOption::kAfkSpin:
    case GameOption::kAfkTilt:
    case GameOption::kAfkZoom:
    case GameOption::kAfkEase:
    case GameOption::kAfkHideUi:
    case GameOption::kAfkHideCursor:
    case GameOption::kAfkVignette:
    case GameOption::kAfkDesaturate:
    case GameOption::kAfkGrain:
    case GameOption::kAfkShowClock:
    case GameOption::kAfkShowAway:
    case GameOption::kAfkClockAnchor:
    case GameOption::kAfkClockMargin:
    case GameOption::kAfkClockScale:
    case GameOption::kAfkClockColor:
    case GameOption::kAfkClockShadow:
    case GameOption::kAfkTryNow:
    case GameOption::kAfkAnnounce:
    case GameOption::kAfkAnnounceDelay:
    case GameOption::kAfkAnnounceSleepSign:
    case GameOption::kAfkAnnounceTagSign:
      // Rien à pousser non plus, et pour la même raison : la veille relit ses
      // réglages à chaque image. ⚠ Y COMPRIS `kAfkTryNow`, qui n'est pas un
      // réglage : sa demande est armée par `AfkOptions::Change`, et
      // `ApplyAll` — qui parcourt TOUTE l'énumération au démarrage — ne doit
      // surtout pas la rejouer.
      break;
    case GameOption::kCount:
      break;
  }
}
void GameOptions::ApplyAll(const OptionTargets& to, bool machine_too) {
  if (!machine_too) {
    for (std::uint8_t which = 0; which < static_cast<std::uint8_t>(GameOption::kCount); ++which) {
      if (!OnMachine(static_cast<GameOption>(which))) {
        Apply(static_cast<GameOption>(which), to);
      }
    }
    return;
  }
  // ⚠ LA LISTE DE L'ÉCRAN D'ABORD : c'est elle qui donne son sens au rang que
  // les fenêtres montreront, et elle ne coûte qu'une interrogation du système.
  ReadResolutions(to);
  for (std::uint8_t which = 0; which < static_cast<std::uint8_t>(GameOption::kCount); ++which) {
    Apply(static_cast<GameOption>(which), to);
  }
  // 🔴 LA PLACE APRÈS LA TAILLE, jamais avant : `SDL_SetWindowSize` garde le
  // coin haut-gauche et fait grandir la fenêtre vers le bas. La replacer avant
  // de la redimensionner la ferait donc ressortir de l'écran.
  //
  // ⚠ ET ELLE N'EST PAS DANS LA BOUCLE : aucune ligne ne la montre, donc elle
  // n'a pas d'entrée dans l'énumération des réglages.
  PlaceWindow(to);
  // La langue à part : elle n'est pas une valeur qu'on pousse mais un catalogue
  // qu'on lit, et son échec se dit.
  //
  // ⚠ SANS ENREGISTRER. Poser ce qu'on vient de lire n'est pas un changement :
  // le compter comme tel ferait réécrire le fichier à chaque fermeture de
  // partie, même si le joueur n'a touché à rien.
  std::string why;
  if (const std::string wanted = language_; !PoseLanguage(wanted, &why)) {
    ro::core::LogWarning("langue : {} — l'interface reste dans la langue précédente", why);
  }
}

}  // namespace moonclient
