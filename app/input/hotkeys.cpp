#include "input/hotkeys.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>

#include "ro/core/build_flavor.h"
#include "ro/core/i18n.h"

namespace moonclient {
namespace {

// LES TOUCHES D'USINE DES TROIS PREMIÈRES BARRES, celles que le joueur a posées
// pour tout le monde : F1 à F12, la rangée des chiffres jusqu'au « = », puis la
// rangée haute d'un clavier AZERTY.
//
// ⚠ LES CINQ AUTRES BARRES N'ONT AUCUN DÉFAUT : il ne reste plus de rangée du
// clavier qui ne serve déjà.
constexpr std::array<std::uint32_t, 12> kFirstBarKeys = {SDLK_F1, SDLK_F2,  SDLK_F3,  SDLK_F4,
                                                         SDLK_F5, SDLK_F6,  SDLK_F7,  SDLK_F8,
                                                         SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12};
constexpr std::array<std::uint32_t, 12> kSecondBarKeys = {
    SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5,          SDLK_6,
    SDLK_7, SDLK_8, SDLK_9, SDLK_0, SDLK_RIGHTPAREN, SDLK_EQUALS};
constexpr std::array<std::uint32_t, 10> kThirdBarKeys = {SDLK_A, SDLK_Z, SDLK_E, SDLK_R, SDLK_T,
                                                         SDLK_Y, SDLK_U, SDLK_I, SDLK_O, SDLK_P};

// LES DIX TOUCHES DES MACROS, tenues avec Alt. ⚠ LE ZÉRO EN DIXIÈME POSITION :
// c'est la rangée du clavier qu'on suit, pas la valeur du chiffre, et c'est
// l'ordre du client.
constexpr std::array<std::uint32_t, kChatMacroSlots> kMacroKeys = {
    SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0};

// Le nom d'une case de raccourci : ses deux nombres, à partir de UN.
//
// ⚠ C'EST LE NUMÉRO QUE LE JOUEUR VOIT, comme dans le fichier des barres. Un
// index à partir de zéro l'obligerait à traduire de tête entre deux écrans qui
// parlent de la même case.
[[nodiscard]] std::string ShortcutSlotName(std::size_t index) {
  std::array<char, 64> text{};
  std::snprintf(text.data(), text.size(), "bar%u_slot%u",
                static_cast<unsigned>(ro::sim::ShortcutBarOfIndex(index) + 1),
                static_cast<unsigned>(ro::sim::ShortcutRankOfIndex(index) + 1));
  return std::string(text.data());
}

[[nodiscard]] bool IsCameraAction(ActionRef action) {
  const std::optional<GameAction> named = action.named();
  return named.has_value() && *named >= GameAction::kCameraReset &&
         *named <= GameAction::kCameraTiltWheel;
}

// LES NOMS GRAVÉS DES ACTIONS, dans l'ordre de `GameAction`.
struct ActionSavedNames {
  GameAction action;
  const char* name;
};

constexpr std::array<ActionSavedNames, kNamedActionCount> kActionSavedNames = {{
    {GameAction::kOpenChat, "talk"},
    {GameAction::kGameMenu, "menu"},
    {GameAction::kOpenCharacterEquipment, "sheet_equipment"},
    {GameAction::kOpenCharacterCostume, "sheet_costume"},
    {GameAction::kOpenCharacterGuild, "sheet_guild"},
    {GameAction::kOpenCharacterGrimoire, "sheet_grimoire"},
    {GameAction::kOpenCharacterHomunculus, "sheet_homunculus"},
    {GameAction::kOpenCharacterMercenary, "sheet_mercenary"},
    {GameAction::kOpenStatus, "status"},
    {GameAction::kOpenPetWindow, "sheet_pet"},
    {GameAction::kOpenInventory, "inventory"},
    {GameAction::kOpenCart, "cart"},
    {GameAction::kOpenParty, "party"},
    {GameAction::kOpenFriends, "friends"},
    {GameAction::kOpenBank, "bank"},
    {GameAction::kOpenCardAlbum, "card_album"},
    {GameAction::kOpenSkillBook, "skills"},
    {GameAction::kOpenChatMacros, "macros"},
    {GameAction::kOpenStyleEditor, "style"},
    {GameAction::kOpenCraftAtlas, "craft_atlas"},
    {GameAction::kOpenNavigation, "navigation"},
    {GameAction::kOpenWorldMap, "world_map"},
    {GameAction::kOpenMvpTracker, "mvp_tracker"},
    {GameAction::kToggleMinimap, "minimap"},
    {GameAction::kOpenCashShop, "cash_shop"},
    {GameAction::kOpenMail, "mail"},
    {GameAction::kOpenAchievements, "achievements"},
    {GameAction::kOpenQuests, "quests"},
    {GameAction::kOpenChatRoom, "chat_room"},
    {GameAction::kOpenMemorialDungeon, "memorial_dungeon"},
    {GameAction::kOpenMarket, "market"},
    {GameAction::kOpenDatabase, "database"},
    {GameAction::kReportBug, "bug_report"},
    {GameAction::kFrameWholeMap, "whole_map"},
    {GameAction::kSitOrStand, "sit"},
    {GameAction::kEffectBench, "effect_bench"},
    {GameAction::kCaptureRecord, "capture_video"},
    {GameAction::kCapturePicture, "capture_picture"},
    {GameAction::kCaptureZone, "capture_zone"},
    {GameAction::kAfkScreen, "afk_screen"},
    {GameAction::kTargetNearest, "target_nearest"},
    {GameAction::kTargetPrevious, "target_previous"},
    {GameAction::kTargetNext, "target_next"},
    {GameAction::kTargetClear, "target_clear"},
    {GameAction::kHomunculusStandby, "homunculus_standby"},
    {GameAction::kMercenaryStandby, "mercenary_standby"},
    {GameAction::kHomunculusOrder, "homunculus_order"},
    {GameAction::kMercenaryOrder, "mercenary_order"},
    {GameAction::kCameraReset, "camera_reset"},
    {GameAction::kCameraResetYaw, "camera_reset_yaw"},
    {GameAction::kCameraResetTilt, "camera_reset_tilt"},
    {GameAction::kCameraResetZoom, "camera_reset_zoom"},
    {GameAction::kCameraResetTiltZoom, "camera_reset_tilt_zoom"},
    {GameAction::kCameraOrbit, "camera_orbit"},
    {GameAction::kCameraTilt, "camera_tilt"},
    {GameAction::kCameraOrbitZoom, "camera_orbit_zoom"},
    {GameAction::kCameraTiltZoom, "camera_tilt_zoom"},
    {GameAction::kCameraZoomWheel, "camera_wheel_zoom"},
    {GameAction::kCameraTiltWheel, "camera_wheel_tilt"},
}};

[[nodiscard]] constexpr bool ActionSavedNamesInOrder() {
  for (std::size_t rank = 0; rank < kActionSavedNames.size(); ++rank) {
    if (static_cast<std::size_t>(kActionSavedNames[rank].action) != rank) {
      return false;
    }
  }
  return true;
}
static_assert(ActionSavedNamesInOrder(), "la table des noms gravés suit l'énumération");

}  // namespace

bool ActionInThisBuild(ActionRef action) {
  if constexpr (ro::core::kDevTools) {
    return true;
  } else {
    const std::optional<GameAction> named = action.named();
    return !named.has_value() ||
           std::ranges::find(kDevelopmentActions, *named) == kDevelopmentActions.end();
  }
}

std::span<const ActionRef> AllBindableActions() {
  // ⚠ ÉCRITE UNE FOIS, DANS L'ORDRE DE LA TABLE. Une seconde liste tenue à la
  // main serait un endroit de plus où oublier une action nouvelle.
  static const std::array<ActionRef, kBindableCount> kAll = [] {
    std::array<ActionRef, kBindableCount> all{};
    for (std::size_t rank = 0; rank < kNamedActionCount; ++rank) {
      all[rank] = static_cast<GameAction>(rank);
    }
    for (std::size_t index = 0; index < ro::sim::kShortcutSlotCount; ++index) {
      all[kNamedActionCount + index] = ActionRef::OfShortcut(index);
    }
    for (std::size_t slot = 0; slot < kChatMacroSlots; ++slot) {
      all[kMacroFirstRank + slot] = ActionRef::OfMacro(slot);
    }
    for (std::size_t slot = 0; slot < kFlagEmoteSlots; ++slot) {
      all[kFlagFirstRank + slot] = ActionRef::OfFlag(slot);
    }
    return all;
  }();
  return kAll;
}

std::string SavedKeyOfAction(ActionRef action) {
  if (action.is_macro()) {
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), "macro_%u",
                  static_cast<unsigned>(action.macro_slot() + 1));
    return std::string(text.data());
  }
  if (action.is_flag()) {
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), "flag_%u",
                  static_cast<unsigned>(action.flag_slot() + 1));
    return std::string(text.data());
  }
  if (action.is_shortcut()) {
    return ShortcutSlotName(action.shortcut_index());
  }
  // ⚠ CES NOMS SONT GRAVÉS : voir l'en-tête. Ils n'ont pas à ressembler au
  // libellé affiché, et ne doivent surtout pas le suivre quand il change.
  return kActionSavedNames[static_cast<std::size_t>(*action.named())].name;
}

std::string GroupOfAction(ActionRef action) {
  if (action.is_macro()) {
    return ro::core::Tr("Macros");
  }
  if (action.is_flag()) {
    return ro::core::Tr("Drapeaux");
  }
  if (IsCameraAction(action)) {
    return ro::core::Tr("Caméra");
  }
  if (!action.is_shortcut()) {
    return {};
  }
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), "%s %u", ro::core::Tr("Barre"),
                static_cast<unsigned>(ro::sim::ShortcutBarOfIndex(action.shortcut_index()) + 1));
  return std::string(text.data());
}

HotkeyTab TabOfAction(ActionRef action) {
  // Les drapeaux sont des émoticônes : ils se rangent avec ce qui parle.
  if (action.is_macro() || action.is_flag()) {
    return HotkeyTab::kMacros;
  }
  if (action.is_shortcut()) {
    return HotkeyTab::kBars;
  }
  // Sans `default` : le compilateur signale une action nouvelle sans onglet.
  switch (*action.named()) {
    case GameAction::kOpenChat:
    case GameAction::kGameMenu:
    case GameAction::kOpenCharacterEquipment:
    case GameAction::kOpenCharacterCostume:
    case GameAction::kOpenCharacterGuild:
    case GameAction::kOpenCharacterGrimoire:
    case GameAction::kOpenCharacterHomunculus:
    case GameAction::kOpenCharacterMercenary:
    case GameAction::kOpenStatus:
    case GameAction::kOpenPetWindow:
    case GameAction::kOpenInventory:
    case GameAction::kOpenCart:
    case GameAction::kOpenParty:
    case GameAction::kOpenFriends:
    case GameAction::kOpenBank:
    case GameAction::kOpenCardAlbum:
    case GameAction::kOpenSkillBook:
    case GameAction::kOpenChatMacros:
    case GameAction::kOpenStyleEditor:
    case GameAction::kOpenCraftAtlas:
    case GameAction::kOpenNavigation:
    case GameAction::kOpenWorldMap:
    case GameAction::kOpenMvpTracker:
    case GameAction::kToggleMinimap:
    case GameAction::kOpenCashShop:
    case GameAction::kOpenMail:
    case GameAction::kOpenAchievements:
    case GameAction::kOpenQuests:
    case GameAction::kOpenChatRoom:
    case GameAction::kOpenMemorialDungeon:
    case GameAction::kOpenMarket:
    case GameAction::kOpenDatabase:
    case GameAction::kReportBug:
    case GameAction::kAfkScreen:
      return HotkeyTab::kInterface;
    case GameAction::kSitOrStand:
    case GameAction::kTargetNearest:
    case GameAction::kTargetPrevious:
    case GameAction::kTargetNext:
    case GameAction::kTargetClear:
    case GameAction::kHomunculusStandby:
    case GameAction::kMercenaryStandby:
    case GameAction::kHomunculusOrder:
    case GameAction::kMercenaryOrder:
      return HotkeyTab::kPlay;
    case GameAction::kFrameWholeMap:
    case GameAction::kCameraReset:
    case GameAction::kCameraResetYaw:
    case GameAction::kCameraResetTilt:
    case GameAction::kCameraResetZoom:
    case GameAction::kCameraResetTiltZoom:
    case GameAction::kCameraOrbit:
    case GameAction::kCameraTilt:
    case GameAction::kCameraOrbitZoom:
    case GameAction::kCameraTiltZoom:
    case GameAction::kCameraZoomWheel:
    case GameAction::kCameraTiltWheel:
      return HotkeyTab::kCamera;
    case GameAction::kEffectBench:
    case GameAction::kCaptureRecord:
    case GameAction::kCapturePicture:
    case GameAction::kCaptureZone:
      return HotkeyTab::kTools;
  }
  return HotkeyTab::kInterface;
}

const char* NameOfTab(HotkeyTab tab) {
  switch (tab) {
    case HotkeyTab::kBars:
      return ro::core::Tr("Barres");
    case HotkeyTab::kInterface:
      return ro::core::Tr("Interface");
    case HotkeyTab::kMacros:
      return ro::core::Tr("Macros");
    case HotkeyTab::kPlay:
      return ro::core::Tr("Jeu");
    case HotkeyTab::kCamera:
      return ro::core::Tr("Caméra");
    case HotkeyTab::kTools:
      return ro::core::Tr("Outils");
  }
  return "";
}

std::string NameOfAction(ActionRef action) {
  if (action.is_macro()) {
    // ⚠ « Macro » Y EST AUSSI, alors que le groupe le porte déjà : c'est ce que
    // la RECHERCHE fouille, et taper « macro » doit rendre les dix lignes.
    std::array<char, 48> text{};
    std::snprintf(text.data(), text.size(), "%s %u", ro::core::Tr("Macro"),
                  static_cast<unsigned>(action.macro_slot() + 1));
    return std::string(text.data());
  }
  if (action.is_flag()) {
    std::array<char, 48> text{};
    std::snprintf(text.data(), text.size(), "%s %u", ro::core::Tr("Drapeau"),
                  static_cast<unsigned>(action.flag_slot() + 1));
    return std::string(text.data());
  }
  if (action.is_shortcut()) {
    // ⚠ LA BARRE Y EST AUSSI, alors que le groupe la porte déjà : c'est ce que
    // la RECHERCHE fouille, et taper « barre 6 » doit rendre ses douze cases.
    std::array<char, 64> text{};
    std::snprintf(text.data(), text.size(), "%s %u, %s %u", ro::core::Tr("Barre"),
                  static_cast<unsigned>(ro::sim::ShortcutBarOfIndex(action.shortcut_index()) + 1),
                  ro::core::Tr("case"),
                  static_cast<unsigned>(ro::sim::ShortcutRankOfIndex(action.shortcut_index()) + 1));
    return std::string(text.data());
  }
  // ⚠ UN SWITCH DE LITTÉRAUX, et non une table indexée : la traduction n'accepte
  // qu'un littéral — une chaîne prise dans un tableau n'entrerait dans aucun
  // catalogue. Même raison que les gravités du journal.
  switch (*action.named()) {
    case GameAction::kOpenChat:
      return ro::core::Tr("Parler");
    case GameAction::kGameMenu:
      return ro::core::Tr("Menu du jeu");
    case GameAction::kOpenCharacterEquipment:
      return ro::core::Tr("Fiche : équipement");
    case GameAction::kOpenCharacterCostume:
      return ro::core::Tr("Fiche : costume");
    case GameAction::kOpenCharacterGuild:
      return ro::core::Tr("Fiche : guilde");
    case GameAction::kOpenCharacterGrimoire:
      return ro::core::Tr("Fiche : skills");
    case GameAction::kOpenCharacterHomunculus:
      return ro::core::Tr("Fiche : homoncule");
    case GameAction::kOpenCharacterMercenary:
      return ro::core::Tr("Fiche : mercenaire");
    case GameAction::kOpenStatus:
      return ro::core::Tr("Statut");
    case GameAction::kOpenPetWindow:
      return ro::core::Tr("Fiche du pet");
    case GameAction::kOpenInventory:
      return ro::core::Tr("Inventory");
    case GameAction::kOpenCart:
      return ro::core::Tr("Cart");
    case GameAction::kOpenParty:
      return ro::core::Tr("Groupe / Amis : groupe");
    case GameAction::kOpenFriends:
      return ro::core::Tr("Groupe / Amis : amis");
    case GameAction::kOpenBank:
      return ro::core::Tr("Banque");
    case GameAction::kOpenCardAlbum:
      return ro::core::Tr("Album de cartes");
    case GameAction::kOpenSkillBook:
      return ro::core::Tr("Lancer un skill");
    case GameAction::kOpenChatMacros:
      return ro::core::Tr("Macros de chat");
    case GameAction::kOpenStyleEditor:
      // ⚠ LE MÊME LIBELLÉ QUE DANS LE MENU DU JEU, et c'est ce qui permet de
      // faire le lien entre les deux : une ligne de raccourci qui nommerait la
      // fenêtre autrement laisserait chercher celle qu'elle ouvre.
      return ro::core::Tr("Couleurs du personnage");
    case GameAction::kOpenCraftAtlas:
      return ro::core::Tr("Atlas des recettes");
    case GameAction::kOpenNavigation:
      return ro::core::Tr("Navigation");
    case GameAction::kOpenWorldMap:
      return ro::core::Tr("Carte du monde");
    case GameAction::kOpenMvpTracker:
      return ro::core::Tr("Carnet de chasse MVP");
    case GameAction::kToggleMinimap:
      return ro::core::Tr("Afficher la minimap");
    case GameAction::kOpenCashShop:
      return ro::core::Tr("Vote Shop");
    case GameAction::kOpenMail:
      return ro::core::Tr("Courrier");
    case GameAction::kOpenAchievements:
      return ro::core::Tr("Succès");
    case GameAction::kOpenQuests:
      return ro::core::Tr("Quêtes");
    case GameAction::kOpenChatRoom:
      return ro::core::Tr("Salon de discussion");
    case GameAction::kOpenMemorialDungeon:
      return ro::core::Tr("Instance");
    case GameAction::kOpenMarket:
      return ro::core::Tr("Marché");
    case GameAction::kOpenDatabase:
      return ro::core::Tr("Base de données");
    case GameAction::kReportBug:
      return ro::core::Tr("Signaler un bug");
    case GameAction::kFrameWholeMap:
      return ro::core::Tr("Voir la carte entière");
    case GameAction::kCameraReset:
      return ro::core::Tr("Remettre la caméra");
    case GameAction::kCameraResetYaw:
      return ro::core::Tr("Remettre la caméra face au nord");
    case GameAction::kCameraResetTilt:
      return ro::core::Tr("Remettre l'inclinaison");
    case GameAction::kCameraResetZoom:
      return ro::core::Tr("Remettre le zoom");
    case GameAction::kCameraResetTiltZoom:
      return ro::core::Tr("Remettre l'inclinaison et le zoom");
    case GameAction::kCameraOrbit:
      return ro::core::Tr("Tourner la caméra");
    case GameAction::kCameraTilt:
      return ro::core::Tr("Incliner la caméra");
    case GameAction::kCameraOrbitZoom:
      return ro::core::Tr("Tourner et zoomer");
    case GameAction::kCameraTiltZoom:
      return ro::core::Tr("Incliner et zoomer");
    case GameAction::kCameraZoomWheel:
      return ro::core::Tr("Zoomer à la molette");
    case GameAction::kCameraTiltWheel:
      return ro::core::Tr("Incliner à la molette");
    case GameAction::kSitOrStand:
      return ro::core::Tr("S'asseoir ou se lever");
    case GameAction::kEffectBench:
      return ro::core::Tr("Banc d'effets");
    case GameAction::kCaptureRecord:
      return ro::core::Tr("Filmer la zone");
    case GameAction::kCapturePicture:
      return ro::core::Tr("Capturer une image de la zone");
    case GameAction::kCaptureZone:
      return ro::core::Tr("Retracer la zone de capture");
    case GameAction::kAfkScreen:
      return ro::core::Tr("Écran de veille");
    case GameAction::kTargetNearest:
      return ro::core::Tr("Cible la plus proche");
    case GameAction::kTargetPrevious:
      return ro::core::Tr("Cible précédente");
    case GameAction::kTargetNext:
      return ro::core::Tr("Cible suivante");
    case GameAction::kTargetClear:
      return ro::core::Tr("Effacer la cible");
    case GameAction::kHomunculusStandby:
      return ro::core::Tr("Homoncule : en attente");
    case GameAction::kMercenaryStandby:
      return ro::core::Tr("Mercenaire : en attente");
    case GameAction::kHomunculusOrder:
      return ro::core::Tr("Homoncule : ordre (aller ici, attaquer)");
    case GameAction::kMercenaryOrder:
      return ro::core::Tr("Mercenaire : ordre (aller ici, attaquer)");
  }
  return ro::core::Tr("Inconnue");
}

Hotkeys::Hotkeys() {
  const auto set = [this](ActionRef action, std::uint32_t key, bool alt = false) {
    bindings_[action.rank()] = Binding{.key = key, .alt = alt};
  };
  set(GameAction::kOpenChat, SDLK_RETURN);
  set(GameAction::kGameMenu, SDLK_ESCAPE);
  // ⚠ ALT+S POUR LE GRIMOIRE, S SEUL POUR LE LANCEMENT : c'est la répartition du
  // jeu, et les deux se ressemblent parce que les deux fenêtres se ressemblent —
  // l'une montre l'arbre, l'autre sert à lancer.
  set(GameAction::kOpenCharacterGrimoire, SDLK_S, /*alt=*/true);
  // ⚠ ALT+Q POUR LE MANNEQUIN : la touche du jeu, celle de sa fenêtre
  // d'équipement.
  set(GameAction::kOpenCharacterEquipment, SDLK_Q, /*alt=*/true);
  // ⚠ ALT+G POUR LA GUILDE : la touche du jeu, celle de sa fenêtre de guilde.
  set(GameAction::kOpenCharacterGuild, SDLK_G, /*alt=*/true);
  // ⚠ ALT+R POUR L'HOMONCULE ET CTRL+R POUR LE MERCENAIRE : les touches du jeu.
  set(GameAction::kOpenCharacterHomunculus, SDLK_R, /*alt=*/true);
  bindings_[ActionRef(GameAction::kOpenCharacterMercenary).rank()] =
      Binding{.key = SDLK_R, .ctrl = true};
  // ⚠ ALT+A POUR LA PETITE FENÊTRE DE STATUT : la touche du jeu.
  set(GameAction::kOpenStatus, SDLK_A, /*alt=*/true);
  // ⚠ ALT+T ET CTRL+T POUR LEUR ATTENTE : les touches du jeu, elles aussi.
  set(GameAction::kHomunculusStandby, SDLK_T, /*alt=*/true);
  bindings_[ActionRef(GameAction::kMercenaryStandby).rank()] = Binding{.key = SDLK_T, .ctrl = true};
  // ⚠ ET LE COSTUME N'A PAS DE TOUCHE D'USINE : le jeu n'a pas cette fenêtre-là,
  // et prendre une place du clavier à tout le monde pour un onglet qu'on ouvre
  // rarement serait un mauvais échange. Il reste réglable.
  // ⚠ ALT+E POUR LE SAC : c'est la touche du jeu, et un joueur la connaît
  // avant de connaître ce client.
  set(GameAction::kOpenInventory, SDLK_E, /*alt=*/true);
  // ⚠ ALT+W POUR LA CHARRETTE : la touche du jeu, là aussi. Elle n'ouvre rien
  // d'utile tant que le personnage n'en pousse pas — la fenêtre le dit alors
  // plutôt que de refuser de s'ouvrir, parce qu'un raccourci muet ne s'explique
  // pas.
  set(GameAction::kOpenCart, SDLK_W, /*alt=*/true);
  // ⚠ ALT+Z POUR LE GROUPE, ALT+H POUR LES AMIS : les touches du jeu.
  set(GameAction::kOpenParty, SDLK_Z, /*alt=*/true);
  set(GameAction::kOpenFriends, SDLK_H, /*alt=*/true);
  // ⚠ CTRL+B POUR LA BANQUE : c'est la touche du jeu, et la SEULE des
  // nôtres qui porte Ctrl — d'où la liaison écrite à la main, le raccourci
  // de pose ne connaissant qu'Alt.
  bindings_[ActionRef(GameAction::kOpenBank).rank()] = Binding{.key = SDLK_B, .ctrl = true};
  if constexpr (ro::core::kDevTools) {
    set(GameAction::kOpenSkillBook, SDLK_S);
  }
  bindings_[ActionRef(GameAction::kToggleMinimap).rank()] = Binding{.key = SDLK_TAB, .ctrl = true};
  // ⚠ LE RECENTRAGE DE LA CAMÉRA N'A PAS DE TOUCHE D'USINE : R est à la troisième
  // barre, et le double-clic droit recentre déjà.
  bindings_[ActionRef(GameAction::kFrameWholeMap).rank()] = Binding{.key = SDLK_R, .shift = true};
  const auto gesture = [this](ActionRef action, MouseGesture made, bool ctrl, bool shift) {
    bindings_[action.rank()] = Binding{.key = KeyOfGesture(made), .ctrl = ctrl, .shift = shift};
  };
  gesture(GameAction::kCameraOrbit, MouseGesture::kRightDrag, false, false);
  gesture(GameAction::kCameraTilt, MouseGesture::kRightDrag, false, true);
  gesture(GameAction::kCameraOrbitZoom, MouseGesture::kRightDrag, true, false);
  gesture(GameAction::kCameraTiltZoom, MouseGesture::kRightDrag, true, true);
  gesture(GameAction::kCameraZoomWheel, MouseGesture::kWheel, false, false);
  gesture(GameAction::kCameraTiltWheel, MouseGesture::kWheel, false, true);
  gesture(GameAction::kCameraResetYaw, MouseGesture::kRightDoubleClick, false, false);
  gesture(GameAction::kCameraResetTilt, MouseGesture::kRightDoubleClick, false, true);
  gesture(GameAction::kCameraResetZoom, MouseGesture::kRightDoubleClick, true, false);
  gesture(GameAction::kCameraResetTiltZoom, MouseGesture::kRightDoubleClick, true, true);
  // ⚠ ALT + CLIC DROIT À L'HOMONCULE, ALT + CLIC GAUCHE AU MERCENAIRE : les
  // gestes du jeu.
  bindings_[ActionRef(GameAction::kHomunculusOrder).rank()] =
      Binding{.key = KeyOfGesture(MouseGesture::kRightClick), .alt = true};
  bindings_[ActionRef(GameAction::kMercenaryOrder).rank()] =
      Binding{.key = KeyOfGesture(MouseGesture::kLeftClick), .alt = true};
  set(GameAction::kSitOrStand, SDLK_INSERT);
  if constexpr (ro::core::kDevTools) {
    set(GameAction::kEffectBench, SDLK_N);
  }
  // ⚠ ALT+M POUR LES MACROS : la touche du jeu, celle qui ouvre sa « Shortcut
  // List ». Un joueur la connaît avant de connaître ce client.
  set(GameAction::kOpenChatMacros, SDLK_M, /*alt=*/true);
  set(GameAction::kOpenChatRoom, SDLK_C, /*alt=*/true);
  set(GameAction::kOpenMemorialDungeon, SDLK_B, /*alt=*/true);
  set(GameAction::kOpenQuests, SDLK_U, /*alt=*/true);
  // ⚠ ALT+P POUR LES COULEURS : les joueurs de ce serveur l'ont déjà dans les
  // doigts. Le jeu n'a pas cette fenêtre-là, donc aucune touche native ne la
  // réclame.
  set(GameAction::kOpenStyleEditor, SDLK_P, /*alt=*/true);
  bindings_[ActionRef(GameAction::kReportBug).rank()] =
      Binding{.key = SDLK_B, .ctrl = true, .alt = true};
  // ⚠ LES TROIS TOUCHES DE LA CAPTURE N'ONT AUCUN DÉFAUT, et c'est voulu : elles
  // ne servent qu'à qui enregistre, et une touche d'usine prise à tout le monde
  // pour un outil qu'on emploie rarement retirerait du clavier trois places que
  // le reste du jeu peut vouloir. C'est aussi ce que fait l'implémentation de
  // référence. Le panneau suffit ; qui filme souvent se les donnera.
  // LES TROIS PREMIÈRES BARRES. Voir `kFirstBarKeys`.
  const auto bar_keys = [&set](std::size_t bar, std::span<const std::uint32_t> keys) {
    for (std::size_t rank = 0; rank < keys.size(); ++rank) {
      set(ActionRef::OfShortcut(ro::sim::ShortcutIndex(bar, rank)), keys[rank]);
    }
  };
  bar_keys(0, kFirstBarKeys);
  bar_keys(1, kSecondBarKeys);
  bar_keys(2, kThirdBarKeys);
  // ALT+1 À ALT+0 SUR LES DIX MACROS, dans cet ordre-là.
  //
  // ⚠ LE ZÉRO EST EN DIXIÈME POSITION, pas en première : c'est la rangée du
  // clavier qu'on suit, pas la valeur du chiffre. La dixième macro est « Alt+0 »
  // dans le client comme sous les doigts.
  for (std::size_t slot = 0; slot < kChatMacroSlots; ++slot) {
    set(ActionRef::OfMacro(slot), kMacroKeys[slot], /*alt=*/true);
  }
  // CTRL+1 À CTRL+9 SUR LES NEUF DRAPEAUX, comme au client du joueur. Les
  // mêmes chiffres que les macros, sans le zéro.
  for (std::size_t slot = 0; slot < kFlagEmoteSlots; ++slot) {
    bindings_[ActionRef::OfFlag(slot).rank()] = Binding{.key = kMacroKeys[slot], .ctrl = true};
  }
}

std::optional<ActionRef> Hotkeys::ActionFor(std::uint32_t key, bool ctrl, bool shift,
                                            bool alt) const {
  if (key == 0) {
    return std::nullopt;
  }
  const Binding wanted{.key = key, .ctrl = ctrl, .shift = shift, .alt = alt};
  for (const ActionRef action : AllBindableActions()) {
    if (bindings_[action.rank()] == wanted && ActionInThisBuild(action)) {
      return action;
    }
  }
  if (key == SDLK_KP_ENTER) {
    return ActionFor(SDLK_RETURN, ctrl, shift, alt);
  }
  return std::nullopt;
}

std::optional<ActionRef> Hotkeys::ActionForGesture(MouseGesture gesture, bool ctrl, bool shift,
                                                   bool alt) const {
  const std::uint32_t key = KeyOfGesture(gesture);
  if (const std::optional<ActionRef> exact = ActionFor(key, ctrl, shift, alt); exact.has_value()) {
    return exact;
  }
  if (!ctrl) {
    return std::nullopt;
  }
  return ActionFor(key, /*ctrl=*/false, shift, alt);
}

void Hotkeys::RestoreDefaults() {
  const std::uint32_t seen = revision_;
  *this = Hotkeys();
  revision_ = seen + 1;
}

std::optional<ActionRef> Hotkeys::Bind(ActionRef action, Binding binding) {
  if (action.rank() >= bindings_.size() || !BindingFits(action, binding)) {
    return std::nullopt;
  }
  ++revision_;
  std::optional<ActionRef> robbed;
  if (binding.bound()) {
    // ⚠ LA TOUCHE SE LIBÈRE D'ABORD. Deux actions sur le même raccourci en
    // laisseraient une morte, et rien à l'écran ne dirait laquelle.
    //
    // ⚠ SE REPOSER SA PROPRE TOUCHE NE DÉPOSSÈDE PERSONNE : sans ce test, la
    // fenêtre annoncerait qu'on vient de se voler à soi-même.
    for (const ActionRef other : AllBindableActions()) {
      if (other != action && bindings_[other.rank()] == binding) {
        bindings_[other.rank()] = Binding{};
        // Une action absente de cette construction perd sa touche sans qu'on
        // l'annonce : le joueur ne la voit nulle part.
        if (ActionInThisBuild(other)) {
          robbed = other;
        }
      }
    }
  }
  bindings_[action.rank()] = binding;
  return robbed;
}

}  // namespace moonclient
