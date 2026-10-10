#include "game/ui_stage.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include "ro/core/build_flavor.h"
#include "ro/core/i18n.h"
#include "ro/ui/loading_screen.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/unlock_notice.h"

#include "game/system_fonts.h"

namespace moonclient {
namespace {

// Ce qui distingue, dans le fichier de réglages, l'épingle d'une fenêtre de son
// ouverture. ⚠ COLLÉ À L'IDENTITÉ, jamais au titre : un titre traduit rangerait
// l'épingle sous un nom et la place sous un autre.
constexpr const char* kPinSuffix = ".pinned";
// Le repli des Skills de l'homoncule et du mercenaire, gardé d'une partie à l'autre.
constexpr const char* kHomunculusSkillsKey = "character.homunculus.skills";
constexpr const char* kMercenarySkillsKey = "character.mercenary.skills";

constexpr const char* kPartyDisbandId = "groupe_dissolution";
constexpr const char* kAdoptionId = "adoption_demande";

}  // namespace

ro::core::Result<UiStage> UiStage::Open(SDL_Window* window, Windows& windows, int font_family,
                                        const std::filesystem::path& loose_font_folder,
                                        const std::filesystem::path& settings_file) {
  auto ui = std::make_unique<ro::ui::UiContext>();
  // ⚠ L'ATLAS SE BÂTIT AVANT LE DOS, et avant la première trame : ImGui grave
  // ses textures au fil des trames, et refaire l'atlas en cours de route lui
  // reprendrait ce qu'il vient de poser.
  //
  // ⚠ ET CE N'EST PAS UN LUXE : la police intégrée d'ImGui ne connaît que
  // U+0020..U+00FF. Sans celle-ci, tout ce qui dépasse — points de suspension,
  // tiret cadratin, œ — sort en « ? » à l'écran, y compris dans les textes du
  // jeu, que nous n'écrivons pas.
  std::vector<ro::ui::FontFamilySpec> usable_families;
  ro::ui::FontSetup fonts = ReadableFontSetup(font_family, usable_families);
  // LES POLICES DU JOUEUR, lues une fois ici : en déposer une nouvelle demande
  // de relancer le client, comme toute police de l'atlas.
  const std::vector<LooseFontFile> loose_files = FindLooseFonts(loose_font_folder);
  std::vector<ro::ui::FontFamilySpec> loose_fonts;
  for (const LooseFontFile& file : loose_files) {
    loose_fonts.push_back(ro::ui::FontFamilySpec{.label = file.label, .regular = file.path});
  }
  fonts.loose = loose_fonts;
  ui->UseFonts(fonts);
  ro::core::Result<ro::ui::UiBackendGl> backend = ro::ui::UiBackendGl::Attach(*ui, window);
  if (!backend.has_value()) {
    return std::unexpected(backend.error());
  }

  // OÙ LES RÉGLAGES VIVENT : l'appelant le décide — voir
  // `ClientShell::OpenWindow`.
  if (!settings_file.empty()) {
    ui->UseSettingsFile(settings_file);
  }
  // ⚠ L'IDENTITÉ, PAS LE TITRE. C'est la même clé qu'ImGui emploie pour ranger
  // la position de la fenêtre, et elle ne doit pas dépendre de la langue : un
  // titre traduit donnerait un fichier où la place se souvient sous un nom et
  // l'ouverture sous un autre.
  ui->RememberWindow(ro::ui::kLogWindowId, &windows.log.open);
  ui->RememberWindow(ro::ui::kChatWindowId, &windows.chat.main.open);
  ui->RememberWindow(ro::ui::kPacketWindowId, &windows.packets.open);
  // L'ÉTABLI ROUVRE AU LANCEMENT pour qui s'en sert : le rouvrir chaque jour
  // serait une corvée. ⚠ ET UN `true` HÉRITÉ N'OUVRE RIEN SANS LE DROIT — le
  // dessin le revérifie, ce qui vaut aussi pour l'état venu du fichier.
  ui->RememberWindow(ro::ui::kStaffToolsId, &windows.staff_tools.open);
  ui->RememberWindow(ro::ui::kChatMacroId, &windows.macros.open);
  // L'ATLAS SE RETROUVE OUVERT au lancement s'il l'était : c'est un outil qu'on
  // garde à côté de soi en préparant une fabrication.
  ui->RememberWindow(ro::ui::kCraftAtlasWindowId, &windows.craft_atlas.state.open);
  ui->RememberWindow(ro::ui::kNavigationWindowId, &windows.navigation.state.open);
  // LES ÉPINGLES, à côté des ouvertures et par le même chemin. ⚠ UNE CLÉ
  // DÉRIVÉE DE L'IDENTITÉ, jamais du titre : voir juste au-dessus.
  ui->RememberWindow(std::string(ro::ui::kInventoryWindowId) + kPinSuffix,
                     &windows.inventory.pinned);
  ui->RememberWindow(std::string(ro::ui::kCartWindowId) + kPinSuffix, &windows.cart.pinned);
  ui->RememberWindow(std::string(ro::ui::kCharacterWindowId) + kPinSuffix,
                     &windows.character.pinned);
  ui->RememberWindow(ro::ui::kPartyFriendWindowId, &windows.party.window.open);
  ui->RememberWindow(std::string(ro::ui::kPartyFriendWindowId) + kPinSuffix,
                     &windows.party.window.pinned);
  ui->RememberWindow(std::string(ro::ui::kCompanionAiWindowId) + kPinSuffix,
                     &windows.companion_ai.pinned);
  ui->RememberWindow(std::string(ro::ui::kCompanionTacticWindowId) + kPinSuffix,
                     &windows.companion_tactic.pinned);
  ui->RememberWindow(std::string(ro::ui::kMvpTrackerWindowId) + kPinSuffix,
                     &windows.mvp_tracker.window.pinned);
  ui->RememberWindow(kHomunculusSkillsKey, &windows.character.homunculus.skills_open);
  ui->RememberWindow(kMercenarySkillsKey, &windows.character.mercenary.skills_open);

  return UiStage(std::move(ui), std::move(*backend));
}

UiStage::UiStage(std::unique_ptr<ro::ui::UiContext> ui, ro::ui::UiBackendGl backend)
    : ui_(std::move(ui)), backend_(std::move(backend)) {}

ImFont* UiStage::DamageFont(std::string_view name, bool bold) const {
  // ⚠ UN NOM INCONNU — une police retirée du dossier depuis — RETOMBE SUR CELLE
  // DE L'INTERFACE, comme un nom vide : le réglage garde le sien, et la
  // retrouve si le fichier revient.
  const ro::ui::FontStyle style = bold ? ro::ui::FontStyle::kBold : ro::ui::FontStyle::kRegular;
  ImFont* named = name.empty() ? nullptr : ui_->FontNamed(name, style);
  return named != nullptr ? named : ui_->ChosenFont(style);
}

void UiStage::ChooseFont(int family) {
  // ⚠ PAR LE NOM : le réglage compte dans la table entière, l'atlas dans les
  // familles trouvées, et une famille absente de la machine décalerait tout ce
  // qui la suit d'un cran. Hors de la table — la police intégrée, ou un fichier
  // trafiqué —, c'est la règle du contexte qui tranche.
  const std::span<const ro::ui::FontFamilySpec> families = SystemFontFamilies();
  if (family < 0 || family >= static_cast<int>(families.size())) {
    ui_->ChooseFont(family);
    return;
  }
  ui_->ChooseFontNamed(families[static_cast<std::size_t>(family)].label);
}

void UiStage::TakeEvent(const SDL_Event& event) const {
  // Le verdict de `ProcessEvent` — « ImGui l'a pris » — n'est pas consulté :
  // il vaut vrai pour tout événement de souris ou de clavier, y compris ceux
  // qui ne visent aucune fenêtre. Ce qui tranche est le laissez-passer, qui
  // sait ce qu'il y a SOUS le curseur.
  static_cast<void>(backend_.ProcessEvent(event));
}

void UiStage::UseSkin(const ro::render::SkinTextures& textures) {
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    const ro::render::SkinTextures::Piece& piece =
        textures.Get(static_cast<ro::render::SkinPiece>(rank));
    // ⚠ UNE PIÈCE ABSENTE SE VIDE : l'ancienne texture est morte avec l'ancien skin.
    skin_.Set(static_cast<ro::render::SkinPiece>(rank),
              piece.valid()
                  ? ro::ui::SkinImage{.id = static_cast<ImTextureID>(piece.texture.name()),
                                      .width = static_cast<float>(piece.width),
                                      .height = static_cast<float>(piece.height)}
                  : ro::ui::SkinImage{});
  }
}

UiStage::Answers UiStage::Draw(
    const ro::net::FrameTally& tally, Windows& windows, const NpcTalk& talk,
    const ro::ui::ChatLinePreview& chat_preview, const ChatTabsView& chat_tabs,
    std::span<const ro::ui::ChatInputLink> chat_links,
    std::span<const ro::ui::ChatEmotion> emotions, std::span<const ro::ui::ChatEmojiGroup> emojis,
    std::span<const ro::ui::SkillGroupView> skills, ro::ui::SkillPointTally points,
    const CharacterSheetView& sheet,
    // CE QUE L'ÉDITEUR DE STYLE MONTRE : les pièces du corps porté, leurs
    // pastilles, et le pantin que `app` a composé.
    const ro::ui::PaletteEditorView& style, const ro::ui::StylePreviewView& style_preview,
    std::span<const ro::ui::InventoryCellView> bag, ro::ui::InventoryTally bag_tally,
    std::span<const ro::ui::InventoryCellView> cart, ro::ui::CartTally cart_tally,
    std::span<const ro::ui::InventoryCellView> storage, ro::ui::StorageTally storage_tally,
    const ro::ui::BankWindowView& bank, const ro::ui::HotkeyWindowView& hotkeys,
    std::span<const ro::ui::ShortcutSlotView> shortcuts,
    const ro::ui::ShortcutBarsLayout& shortcut_bars,
    std::span<const ro::ui::StatusCellView> statuses, const ro::ui::StatusBarLayout& status_bar,
    const ro::ui::StatBarValues& stat_bar_values, const ro::ui::StatBarWarnings& stat_bar_warnings,
    ro::ui::StatBarsLook& stat_bars, ro::ui::PvpHudLook& pvp_hud,
    std::span<const ro::ui::OptionSectionView> options, const ro::ui::DescriptionView& described,
    const ro::ui::DescriptionView& previewed, const ro::ui::SkillDescriptionView& skill_described,
    const ro::ui::AlignGrid& grid, std::span<const ro::ui::ChatBubbleView> bubbles,
    const ro::ui::ChatBubbleLook& bubble_look, std::span<const ro::ui::EntityBarView> entity_bars,
    const ro::ui::EntityBarsLook& entity_bar_look, std::span<const ro::ui::CastBarView> cast_bars,
    const ro::ui::CastBarsLook& cast_bar_look, const ro::ui::DamageTexts& damage_texts,
    ro::ui::SessionRights rights, const ro::ui::GameMenuFacts& menu_facts,
    std::span<const ro::ui::ChatMacroCommandGroup> macro_commands,
    std::span<const ro::ui::ItemObtainToastEntry> obtained,
    const ro::ui::ItemObtainToastLook& obtained_look, ro::ui::ItemObtainAnchorGrip& obtained_grip,
    const ro::ui::ZoneCaptureView& capture, std::int32_t round_trip_ms,
    const OverWorldPaint& over_world) {
  Answers answers;
  ro::ui::NpcDialogAnswer& answer = answers.npc;
  backend_.BeginFrame(*ui_);
  // 🔴 L'AIMANT D'ABORD, AVANT QUE QUOI QUE CE SOIT N'AIT LU UNE POSITION.
  // ImGui vient de replacer la fenêtre qu'on traîne d'après la souris ; la
  // corriger ici ne dérive jamais, puisqu'il repart du pointeur à chaque
  // image. La corriger plus tard laisserait une image sur deux montrer la
  // position brute, et la fenêtre trembler le long de la ligne.
  ro::ui::SnapMovingWindow(grid);
  // LES NOMBRES EN POLICE AU FOND DE LA LISTE : c'est là que se peignent ceux
  // du sprite, avec le monde — sous la grille, les jauges et les bulles.
  // Changer d'écriture ne doit rien faire passer devant.
  ro::ui::DrawDamageTexts(damage_texts.texts,
                          DamageFont(damage_texts.font_name, damage_texts.bold));
  // Le quadrillage se peint SOUS les fenêtres : le poser au premier plan
  // quadrillerait ce qu'on cherche à aligner.
  ro::ui::DrawAlignGrid(grid);
  // LES BARRES D'ENTITÉ AVANT LES BULLES, sur la MÊME liste de fond, et l'ordre
  // est tout ce qui les sépare : une phrase doit recouvrir une jauge, jamais
  // l'inverse — on relit une barre à l'image suivante, pas une phrase.
  ro::ui::DrawEntityBars(entity_bars, entity_bar_look);
  // LES INCANTATIONS AVEC ELLES, et sous les bulles pour la même raison.
  ro::ui::DrawCastBars(cast_bars, cast_bar_look);
  // Les bulles juste apres la grille, sur la MEME liste de fond : elles la
  // recouvrent, et c est l ordre qu on veut -- un quadrillage d alignement n a
  // pas a barrer ce que quelqu un vient de dire.
  // ⚠ AVEC CE QUE LE FIL SAIT PEINDRE : une bulle montre la même phrase que lui.
  ro::ui::DrawChatBubbles(bubbles, bubble_look, &windows.chat.rich);
  // CE QUE LA PARTIE PEINT PAR-DESSUS LE MONDE — le voile de ténèbres —, après
  // les barres et les bulles, qui appartiennent au monde, et sous toute
  // fenêtre : la liste de fond passe avant elles.
  PaintOverWorld(over_world);

  // LE JOURNAL EN PREMIER, et ce n'est pas indifférent : ce qu'une autre
  // fenêtre écrirait en se dessinant n'apparaîtrait qu'à l'image suivante si le
  // journal était déjà passé. L'ordre inverse le montre tout de suite.
  //
  // ⚠ LES LIGNES SE RECOPIENT sous verrou, parce que le journal s'écrit aussi
  // depuis le fil du réseau. La recopie est bornée par `kKeptLogLines` et n'est
  // payée que la fenêtre ouverte.
  //
  // ⚠ SOUS LE DROIT DE L'ÉTABLI, comme lui : un `open` retenu d'un compte
  // devenu ordinaire est ignoré, pas effacé — le droit peut revenir.
  if (windows.log.open && rights.staff_tools) {
    const std::vector<ro::core::LoggedLine> lines = ro::core::RecentLogLines();
    static_cast<void>(ro::ui::DrawLogWindow(windows.log, lines));
  }

  // L'ÉTABLI DU STAFF, avec les autres outils : ce qu'il règle agit sur le
  // MONDE, qui est déjà peint quand cette image d'interface commence.
  // Ses boutons « Journal » et « Trames » basculent leurs fenêtres, qui se
  // dessineront à l'image suivante.
  const ro::ui::StaffToolsAction staff_action =
      ro::ui::DrawStaffTools(windows.staff_tools, skin_, rights.staff_tools);
  if (staff_action.toggle_log) {
    windows.log.open = !windows.log.open;
  }
  if (staff_action.toggle_packets) {
    windows.packets.open = !windows.packets.open;
  }
  if (rights.staff_tools) {
    ro::ui::DrawCameraHud(windows.camera_hud, windows.staff_tools.look.camera_hud);
    if (ro::ui::SoundZonesShown(windows.staff_tools)) {
      ro::ui::DrawSoundZones(windows.sound_zones);
    }
    if (windows.staff_tools.look.picking.boxes_shown) {
      ro::ui::DrawPickBoxes(windows.pick_boxes);
    }
  }

  // LES DIX MACROS DE CHAT. ⚠ LA DESTINATION VIENT DE LA MÊME VUE QUE LA BARRE
  // DE SAISIE : la macro part là où partirait ce qu'on taperait, et deux calculs
  // de destination finiraient par se contredire.
  ro::ui::DrawChatMacros(windows.macros, windows.macro_keys, chat_preview.target, macro_commands,
                         emotions, skin_, &answers.macros);

  // L'INSPECTEUR DE TRAMES, SOUS LE DROIT DE L'ÉTABLI comme le journal — ou en
  // Debug, où `/trames` l'ouvre à qui développe. Un `open` retenu d'un compte
  // devenu ordinaire est ignoré, pas effacé.
  if (windows.packets.open && (rights.staff_tools || ro::core::kDevTools)) {
    // LA RECOPIE, et c'est elle qui tient la frontière : `ui` ne connaît pas
    // `net`, donc le relevé traverse en données pures. Elle a un coût, il est
    // borné — le nombre d'opcodes distincts reçus, quelques dizaines — et il
    // n'est payé que la fenêtre ouverte.
    const std::vector<ro::net::FrameTally::Entry> entries = tally.All();
    std::vector<ro::ui::PacketRow> rows;
    rows.reserve(entries.size());
    for (const ro::net::FrameTally::Entry& entry : entries) {
      rows.push_back(ro::ui::PacketRow{.opcode = entry.opcode,
                                       .applied = entry.applied,
                                       .ignored = entry.ignored,
                                       .unhandled = entry.unhandled,
                                       .undecodable = entry.undecodable});
    }
    static_cast<void>(ro::ui::DrawPacketWindow(windows.packets, rows, tally.frames()));
  }

  DrawConversation(windows, chat_preview, chat_tabs, chat_links, emotions, emojis, answers);

  // LA FICHE DE PERSONNAGE : le mannequin, les costumes, les nombres, et le
  // grimoire.
  answers.character = ro::ui::DrawCharacterWindow(
      windows.character, windows.character_view, sheet.header, sheet.slots, sheet.stats,
      sheet.presets, sheet.titles, sheet.guild, skills, points, skin_, sheet.companions);
  // Échap qui a annulé l'attente d'une touche de preset ne ferme pas la fiche.
  if (answers.character.presets.escape_spent) {
    escape_.Suppress();
  }
  if (!sheet.guild_invitation.empty()) {
    bool accepted = false;
    if (ro::ui::DrawGuildInvitation(sheet.guild_invitation, accepted, skin_)) {
      answers.guild_invitation = accepted;
    }
  }
  if (!sheet.guild_alliance.empty()) {
    bool accepted = false;
    if (ro::ui::DrawGuildAllianceOffer(sheet.guild_alliance, accepted, skin_)) {
      answers.guild_alliance = accepted;
    }
  }
  // LA FENÊTRE GROUPE / AMIS, avec la fiche : on y passe de l'une à l'autre.
  if (windows.party.view != nullptr && windows.party.look != nullptr) {
    windows.party.answer = ro::ui::DrawPartyFriendWindow(windows.party.window, *windows.party.view,
                                                         *windows.party.look, skin_);
  }
  if (std::exchange(windows.party.ask_disband, false)) {
    ro::ui::AskModal(kPartyDisbandId);
  }
  if (ro::ui::DrawModalBox(kPartyDisbandId,
                           ro::core::Tr("Aucun membre du groupe ne peut recevoir la direction. "
                                        "Voulez-vous plutôt dissoudre le groupe ?"),
                           ro::core::Tr("Dissoudre"), ro::core::Tr("Annuler"),
                           skin_) == ro::ui::ModalAnswer::kYes) {
    windows.party.disband_confirmed = true;
  }
  // LA DEMANDE D'ADOPTION REÇUE : la boîte s'ouvre à son arrivée, et attend.
  AdoptionBox& adoption = windows.party.adoption;
  if (std::exchange(adoption.ask, false)) {
    ro::ui::AskModal(kAdoptionId);
  }
  if (!adoption.question.empty()) {
    const ro::ui::ModalAnswer adopted =
        ro::ui::DrawModalBox(kAdoptionId, adoption.question.c_str(), ro::core::Tr("Accepter"),
                             ro::core::Tr("Refuser"), skin_);
    if (adopted != ro::ui::ModalAnswer::kWaiting) {
      adoption.answer = adopted == ro::ui::ModalAnswer::kYes;
    }
  }

  // L'ÉDITEUR DE STYLE, juste après la fiche : c'est la même apparence qu'on
  // regarde, par l'autre bout.
  answers.style = ro::ui::DrawPaletteEditor(windows.style, style, skin_);

  // L'APERÇU D'UN STYLE REÇU, juste après l'éditeur : les deux montrent notre
  // corps, et celui qu'on vient d'ouvrir doit passer devant l'autre.
  answers.style_preview = ro::ui::DrawStylePreview(windows.style_preview, style_preview, skin_);

  // LE SAC. Il suit la fiche : les deux se consultent ensemble, et celui qu'on
  // vient d'ouvrir doit passer devant celle qu'on avait laissee ouverte.
  answers.inventory =
      ro::ui::DrawInventoryWindow(windows.inventory, windows.inventory_view, bag, bag_tally, skin_);

  // LE SERTISSAGE APRÈS LE SAC : c'est de lui qu'on l'ouvre, et il passe devant
  // à sa naissance. ⚠ IL NE DÉPEND PAS DU SAC OUVERT : la liste du serveur
  // l'ouvre, et il survit à la fermeture du sac.
  answers.card_insert =
      ro::ui::DrawCardInsertWindow(windows.card_insert, windows.card_insert_view, skin_);

  // LES ATELIERS APRÈS LE SAC : c'est lui qu'on regarde en fabriquant. ⚠ ILS NE
  // DÉPENDENT PAS DU SAC OUVERT : la liste du serveur les ouvre.
  if (windows.make_item_view != nullptr) {
    answers.make_item =
        ro::ui::DrawMakeItemWindow(windows.make_item, *windows.make_item_view, skin_);
  }
  if (windows.weapon_refine_view != nullptr) {
    answers.weapon_refine =
        ro::ui::DrawWeaponRefineWindow(windows.weapon_refine, *windows.weapon_refine_view, skin_);
  }

  // LA CHARRETTE SUIT LE SAC, pour la même raison que le sac suit la fiche :
  // les deux se consultent ensemble, et l'on y déplace des objets de l'une à
  // l'autre.
  answers.cart = ro::ui::DrawCartWindow(windows.cart, windows.cart_view, cart, cart_tally, skin_);

  // LE COFFRE APRÈS LES DEUX AUTRES : c'est lui qu'on vient d'ouvrir quand il
  // est là, et les trois se consultent ensemble.
  answers.storage = ro::ui::DrawStorageWindow(windows.storage, windows.storage_view, storage,
                                              storage_tally, skin_);

  // LA BANQUE APRÈS LE COFFRE : c'est la dernière des fenêtres qu'un serveur
  // ouvre, et elle échange des zeny avec la poche que le sac affiche.
  answers.bank = ro::ui::DrawBankWindow(windows.bank, bank, skin_);

  // LES DESTINATIONS D'UNE COMPÉTENCE DE DÉPLACEMENT : une question du serveur,
  // qui passe au premier plan à son arrivée. ⚠ ELLE PREND ÉCHAP ELLE-MÊME — il
  // vaut « Annuler », qui parle au serveur —, et rien derrière elle ne se ferme
  // alors sur la même touche.
  answers.warp_list = ro::ui::DrawWarpListWindow(windows.warp_list, windows.warp_list_view, skin_);
  if (answers.warp_list.escape_spent) {
    escape_.Suppress();
  }

  // LA LISTE DES CHARRETTES, elle aussi ouverte par une compétence ou par le
  // serveur, et au premier plan à son arrivée.
  answers.cart_choice =
      ro::ui::DrawCartChoiceWindow(windows.cart_choice, windows.cart_choice_view, skin_);
  // LA LISTE DES SORTS D'AUTOSPELL, ouverte par le serveur.
  answers.spell_list =
      ro::ui::DrawSpellListWindow(windows.spell_list, windows.spell_list_view, skin_);
  // LA LISTE DE REPAIR WEAPON, ouverte par le serveur.
  answers.repair = ro::ui::DrawRepairWindow(windows.repair, windows.repair_view, skin_);
  // LA FICHE DU FAMILIER, et la liste d'éclosion que le serveur ouvre.
  answers.pet = ro::ui::DrawPetWindow(windows.pet, windows.pet_view, skin_);
  answers.pet_hatch = ro::ui::DrawPetHatchWindow(windows.pet_hatch, windows.pet_hatch_view, skin_);
  // LA SAISIE DE TALKIE BOX, ouverte par la pose de la compétence.
  answers.talkbox = ro::ui::DrawTalkboxWindow(windows.talkbox, windows.talkbox_view, skin_);
  // LES RÉGLAGES AVANCÉS D'UNE TACTIQUE DE COMPAGNON, ouverts du menu d'entité.
  answers.companion_tactic = ro::ui::DrawCompanionTacticWindow(
      windows.companion_tactic, windows.companion_tactic_view, skin_);
  answers.companion_ai =
      ro::ui::DrawCompanionAiWindow(windows.companion_ai, windows.companion_ai_view, skin_);

  // L'ALBUM DE CARTES APRÈS LES CONTENANTS : c'est le sac qui le remplit, et
  // les deux se consultent ensemble.
  if (windows.card_album_view != nullptr) {
    answers.card_album =
        ro::ui::DrawCardAlbumWindow(windows.card_album, *windows.card_album_view, skin_);
  }

  // LA BARRE AVANT LES FENÊTRES : elle se pose sur le monde, et tout panneau
  // qu'on ouvre doit passer devant elle.
  answers.shortcuts = ro::ui::DrawShortcutBars(shortcut_bars, shortcuts, skin_);
  // LA GRILLE D'ICÔNES DE MENU AVEC ELLE, et pour la même raison : elle reste à
  // l'écran, et ce qu'elle ouvre doit passer devant.
  if (windows.menu_icons.look != nullptr) {
    const ro::ui::MenuIconsAnswer menu =
        ro::ui::DrawMenuIcons(windows.menu_icons.state, windows.menu_icons.icons,
                              *windows.menu_icons.look, grid, hud_grip_, skin_);
    windows.menu_icons.clicked = menu.clicked;
    answers.menu_icons_moved = menu.moved;
  }

  // LA BARRE D'ÉTATS AVEC ELLE, et pour la même raison : elle se pose sur le
  // monde. ⚠ APRÈS, et non avant : une barre de raccourcis qu'on déplace doit
  // pouvoir passer par-dessus celle-ci, qui ne se clique jamais.
  ro::ui::DrawStatusBar(status_bar, statuses);
  // LES SIX JAUGES LIBRES, posées sur le monde comme la barre d'états.
  ro::ui::DrawStatBars(stat_bar_values, stat_bar_warnings, grid, stat_bars, hud_grip_,
                       &answers.stat_bars_moved);
  // LES DEUX POIGNÉES DU CLASSEMENT PVP, avec les jauges et pour la même raison.
  //
  // ⚠ ELLES NE DESSINENT RIEN : le compteur et le chronomètre sont des sprites
  // du GRF, posés par la couche de rendu bien avant cette image d'interface. Ce
  // qui s'ouvre ici est la prise, et elle n'existe que déverrouillée.
  answers.pvp_hud_moved = ro::ui::PvpHudFrames(
      pvp_hud, grid, hud_grip_, {ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y});
  // LA GRILLE DE GROUPE, cadre libre comme les jauges.
  if (windows.party.grid_view != nullptr && windows.party.grid_look != nullptr) {
    windows.party.grid_answer =
        ro::ui::DrawPartyGrid(*windows.party.grid_view, *windows.party.grid_look,
                              windows.party.grid_state, grid, hud_grip_);
  }
  // LE HUD DE CIBLE, cadres libres comme les jauges. Sa vue ne descend que sous
  // le mode Ciblage.
  if (windows.target_hud.view != nullptr && windows.target_hud.look != nullptr) {
    ro::ui::DrawTargetHud(*windows.target_hud.view, grid, *windows.target_hud.look, hud_grip_,
                          windows.target_hud.answer);
  }
  // LE MENU DU CLIC DROIT SUR UNE ENTITÉ, après le HUD dont un cadre l'ouvre.
  // ⚠ À CHAQUE IMAGE, MÊME FERMÉ : sa question se pose une fois le menu
  // refermé, et une boîte qu'on cesse de dessiner disparaît sans réponse.
  windows.entity_menu.answer =
      ro::ui::DrawEntityMenu(windows.entity_menu.state, windows.entity_menu.view, skin_);
  // L'ÉQUIPEMENT D'UN AUTRE JOUEUR, avant l'aperçu qui survole ses pièces et la
  // description qu'on ouvre depuis elles.
  if (windows.view_equip.view != nullptr) {
    windows.view_equip.answer =
        ro::ui::DrawViewEquipWindow(windows.view_equip.state, *windows.view_equip.view, skin_);
  }
  // LA FICHE D'UN MONSTRE, avant l'aperçu qui survole son butin et la
  // description qu'on ouvre depuis lui.
  if (windows.monster_sheet.view != nullptr) {
    windows.monster_sheet.answer = ro::ui::DrawMonsterSheetWindow(
        windows.monster_sheet.state, *windows.monster_sheet.view, skin_);
  }
  // L'ATLAS DES RECETTES, avant l'aperçu qui survole ses lignes et la
  // description qu'on ouvre depuis son menu.
  if (windows.craft_atlas.view != nullptr) {
    windows.craft_atlas.answer =
        ro::ui::DrawCraftAtlasWindow(windows.craft_atlas.state, *windows.craft_atlas.view, skin_);
  }
  // LA MINIMAP, puis la navigation qu'elle ouvre.
  if (windows.minimap.view != nullptr && windows.minimap.settings != nullptr) {
    windows.minimap.answer = ro::ui::DrawMinimapWindow(
        *windows.minimap.settings, windows.minimap.state, *windows.minimap.view, skin_);
  }
  DrawNavigationWindows(windows.navigation);
  DrawMvpTrackerWindows(windows.mvp_tracker);
  DrawBattlegroundWindows(windows.battleground);
  // LA BOUTIQUE DE VOTE, avant la description qu'on ouvre depuis ses cartes.
  if (windows.cash_shop.view != nullptr) {
    windows.cash_shop.answer =
        ro::ui::DrawCashShopWindow(windows.cash_shop.state, *windows.cash_shop.view, skin_);
  }
  DrawTradeWindows(windows.trade, windows.chat.vote_shop);
  DrawMailWindows(windows.mail, windows.chat.vote_shop);
  DrawQuestWindows(windows.quests, windows.tracker);
  DrawAchievementWindows(windows.achievements);

  // L'APERÇU AU SURVOL, APRÈS LES TROIS FENÊTRES D'OBJETS.
  //
  // 🔴 ICI ET NON DANS CHACUNE, et c'est ce qui le rend simple : une infobulle
  // est une FENÊTRE, elle n'a pas à naître dans celle qu'on survole. Posé à
  // l'étage, les trois n'ont qu'à dire ce qu'elles survolent — et il n'y a qu'un
  // seul endroit où le dessiner juste.
  //
  // ⚠ IL RETARDE D'UNE IMAGE, et c'est sans conséquence : l'étage apprend
  // l'objet survolé par la RÉPONSE de l'image précédente, et une infobulle
  // n'apparaît pas avant que la souris se soit arrêtée.
  ro::ui::DrawDescriptionPreview(previewed, skin_);
  // ET CELUI D'UN MONSTRE, au même endroit et pour la même raison.
  if (windows.monster_sheet.preview != nullptr) {
    ro::ui::DrawMonsterSheetPreview(windows.monster_sheet.preview_state,
                                    *windows.monster_sheet.preview, skin_);
  }
  // ET CELUI D'UNE RECETTE, de même.
  if (windows.craft_atlas.preview != nullptr) {
    ro::ui::DrawCraftRecipePreview(*windows.craft_atlas.preview, skin_);
  }

  // LA DESCRIPTION APRÈS LES FENÊTRES D'OBJETS : c'est d'elles qu'on l'ouvre,
  // et elle doit passer devant celle qu'on vient de quitter.
  answers.description = ro::ui::DrawDescriptionWindow(windows.description, described, skin_);
  // ET CELLE D'UNE COMPÉTENCE JUSTE APRÈS : elle s'ouvre depuis le grimoire, qui
  // est un onglet de la fiche — dessinée plus haut, comme les fenêtres d'objets.
  answers.skill_description =
      ro::ui::DrawSkillDescriptionWindow(windows.skill_description, skill_described, skin_);
  if (windows.book_view != nullptr) {
    answers.book = ro::ui::DrawBookWindow(windows.book, *windows.book_view, skin_);
  }
  ro::ui::DrawBookRecital(windows.book_recital);

  answers.hotkeys = ro::ui::DrawHotkeyWindow(windows.hotkeys, hotkeys, skin_);
  // Échap qui a annulé la capture d'une touche ne ferme pas la fenêtre.
  if (std::exchange(windows.hotkeys.escape_spent, false)) {
    escape_.Suppress();
  }
  answers.options = ro::ui::DrawOptionsWindow(windows.options, options, skin_);
  // LE MENU APRÈS SES SOUS-FENÊTRES : il est petit et on l'ouvre pour en sortir,
  // il doit passer devant.
  answers.menu = ro::ui::DrawGameMenu(windows.menu, skin_, menu_facts);
  // LE RAPPORT DE BUG : une boîte modale, qui passe devant tout, et l'accusé.
  answers.bug_report = ro::ui::DrawBugReportWindow(windows.bug_report, skin_);

  // LE DIALOGUE EN DERNIER : c'est la seule fenêtre à laquelle le joueur DOIT
  // répondre, et elle doit donc passer devant celles qui servent à bâtir.
  //
  // ⚠ LA RECOPIE TIENT LA FRONTIÈRE : `ui` ne connaît pas `sim`. Elle ne coûte
  // que des vues — aucune chaîne n'est dupliquée —, et n'a lieu que le dialogue
  // ouvert.
  // L'ILLUSTRATION AVANT LE DIALOGUE : il se pose devant elle quand ils se
  // recouvrent, comme chez le client d'origine. Elle ne dépend pas de lui — le
  // script l'efface quand il veut, et celle qu'il oublie reste.
  if (talk.illustration.has_value()) {
    answers.npc_illustration = ro::ui::DrawNpcIllustration(*talk.illustration, skin_);
  }
  if (talk.view != nullptr) {
    answer = ro::ui::DrawNpcDialogWindow(windows.npc_dialog, *talk.view, skin_, talk.art);
    // ⚠ APRÈS LA FENÊTRE, ET DEHORS : une infobulle vit dans la couche avant
    // d'ImGui, et la poser depuis l'intérieur de la fenêtre la ferait passer
    // sous elle.
    ro::ui::DrawStatusPreview(talk.status_preview, skin_);
    // ⚠ ÉCHAP NE S'INSCRIT QUE LÀ OÙ LA FERMETURE A UN SENS POUR LE SERVEUR :
    // pendant un « suivant », il n'y a rien à fermer — y envoyer une fermeture
    // ferait AVANCER le script —, et la touche doit alors filer au jeu plutôt
    // que d'être avalée sans rien faire.
    if (talk.view->prompt == ro::ui::NpcDialogPrompt::kClose ||
        talk.view->prompt == ro::ui::NpcDialogPrompt::kMenu) {
      escape_.Register(ro::ui::kNpcDialogWindowId, &windows.npc_dialog.open);
    }
  } else {
    // 🔴 LA FENÊTRE FERMÉE, SON ÉTAT REPART À BLANC. Une conversation garde une
    // mise en page en cache, un filtre de menu et ce qu'on tapait : les laisser
    // en place ferait ouvrir la SUIVANTE sur les restes de la précédente — un
    // filtre qui masque des options, une saisie déjà remplie —, et la fenêtre
    // s'ouvrirait là où le joueur avait poussé l'autre.
    windows.npc_dialog = ro::ui::NpcDialogWindowState{};
  }

  // LE COMPTEUR EN DERNIER, par-dessus tout : c'est un instrument, et ce qu'il
  // mesure inclut le dessin de tout ce qui précède.
  //
  // ⚠ APPELÉ MÊME CACHÉ — il tient son historique, qui repartirait vide à
  // chaque réouverture.
  ro::ui::DrawFrameMeter(windows.meter, round_trip_ms);
  // ⚠ APRÈS LES FENÊTRES DE CONVERSATION : lâché sur l'une de leurs bandes, il
  // s'y range, et ce sont les mesures de CETTE image qu'il lit.
  answers.damage_meter =
      ro::ui::DrawDamageMeter(windows.damage_meter, windows.damage_meter_view, &windows.chat.board);

  // 🔴 APRÈS TOUTES LES FENÊTRES, ET C'EST TOUT L'INTÉRÊT : un objet lâché sur
  // le monde est un objet qu'AUCUNE cible n'a pris, et on ne peut le savoir
  // qu'une fois toutes les cibles passées. Interroger plus tôt jetterait au sol
  // ce qu'on rangeait dans une fenêtre qui n'a pas encore tourné.
  answers.dropped_on_nothing = ro::ui::ItemDroppedOnNothing(&answers.dropped_item);

  // ⚠ LES FENÊTRES SE DÉCLARENT ICI, ET NON DANS LEUR DESSIN. Une fenêtre ne
  // connaît pas la pile ; l'étage, lui, sait ce qu'il a ouvert et tient les
  // interrupteurs. C'est la même division que partout : `ui` dessine, `app`
  // route.
  // 🔴 L'ÉTABLI, LE JOURNAL ET LES TRAMES SORTENT DE LA PILE, et c'est voulu
  // deux fois : ce sont des fenêtres d'atelier qu'on garde ouvertes en jouant,
  // donc aucun Échap destiné au menu ne doit les refermer par surprise — et
  // elles n'AVALENT pas la touche non plus, si bien qu'Échap continue d'ouvrir
  // le menu quand l'une d'elles est au premier plan.
  escape_.SkipNext();
  escape_.Register(ro::ui::kPacketWindowId, &windows.packets.open);
  escape_.SkipNext();
  escape_.Register(ro::ui::kLogWindowId, &windows.log.open);
  escape_.SkipNext();
  escape_.Register(ro::ui::kStaffToolsId, &windows.staff_tools.open);
  // Les macros, elles, RENTRENT dans la pile : on les ouvre pour écrire une
  // ligne et on les referme, comme toute fenêtre de réglage.
  escape_.Register(ro::ui::kChatMacroId, &windows.macros.open);
  if (windows.companion_ai.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kCompanionAiWindowId, &windows.companion_ai.open);
  if (windows.companion_tactic.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kCompanionTacticWindowId, &windows.companion_tactic.open);
  // 🔴 UNE FENÊTRE ÉPINGLÉE SORT DE LA PILE, elle n'y entre pas grisée : elle
  // n'avale donc pas non plus la touche, et le jeu reçoit Échap. C'est bien ce
  // que veut un joueur qui a délibérément cloué une fenêtre à l'écran.
  if (windows.character.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kCharacterWindowId, &windows.character.open);
  if (windows.party.window.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kPartyFriendWindowId, &windows.party.window.open);
  if (windows.cart.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kCartWindowId, &windows.cart.open);
  // LE COFFRE AUSSI, comme les ateliers : son interrupteur recopie la session
  // du serveur, et la boucle lit un interrupteur BAISSÉ comme sa croix, qu'elle
  // dit au serveur — voir `Game::ApplyContainerAnswers`. Baisser seulement
  // l'interrupteur laisserait le serveur croire le coffre ouvert.
  escape_.Register(ro::ui::kStorageWindowId, &windows.storage.open);
  // 🔴 LA BANQUE N'EN EST PAS NON PLUS, pour une raison à elle : son
  // interrupteur ne lui appartient pas. Il RECOPIE `world.bank().open()` à
  // chaque image, si bien qu'un Échap qui le baisserait le verrait relevé
  // l'image suivante — une touche muette, que rien n'expliquerait. Elle se
  // ferme par sa croix, qui referme la session dans le monde.
  // ⚠ NI LE SERTISSAGE, pour la même raison : son interrupteur recopie la
  // session du monde, et c'est sa croix ou « Fermer » qui la referme.
  // ⛔ PAS D'ÉPINGLE SUR LA DESCRIPTION, parce que le client ne lui en donne
  // pas : sa barre de titre n'a que sa croix, et le nom d'objet qu'elle porte a
  // besoin de toute la place. Échap la ferme donc toujours — ET D'ABORD : on
  // l'ouvre d'un clic dans l'inventaire, qui garde le focus.
  escape_.RegisterReading(ro::ui::kDescriptionWindowId, &windows.description.open);
  escape_.RegisterReading(ro::ui::kSkillDescriptionWindowId, &windows.skill_description.open);
  // La fiche d'un monstre se lit et se referme, comme une description.
  escape_.RegisterReading(ro::ui::kMonsterSheetWindowId, &windows.monster_sheet.state.open);
  // L'Atlas aussi : on le consulte, puis on le referme.
  escape_.Register(ro::ui::kCraftAtlasWindowId, &windows.craft_atlas.state.open);
  RegisterNavigationEscapes(windows.navigation);
  RegisterMvpTrackerEscapes(windows.mvp_tracker);
  RegisterBattlegroundEscapes(windows.battleground);
  // LES ATELIERS AUSSI : leur interrupteur recopie la session du serveur, mais
  // la boucle lit un interrupteur BAISSÉ comme une fermeture, et répond au
  // serveur — voir `CraftStage::Apply`.
  escape_.Register(ro::ui::kMakeItemWindowId, &windows.make_item.open);
  escape_.Register(ro::ui::kWeaponRefineWindowId, &windows.weapon_refine.open);
  // L'album, lui, EN EST : son interrupteur est au joueur, et la fenêtre rend
  // elle-même le verrou au serveur quand elle se voit fermée.
  escape_.Register(ro::ui::kCardAlbumWindowId, &windows.card_album.open);
  // LA BOUTIQUE DE VOTE AUSSI : son interrupteur recopie la session du serveur,
  // mais la boucle lit un interrupteur BAISSÉ comme une fermeture, et la dit au
  // serveur — voir `CashShopStage::Advance`.
  escape_.Register(ro::ui::kCashShopWindowId, &windows.cash_shop.state.open);
  RegisterTradeEscapes(windows.trade);
  RegisterMailEscapes(windows.mail);
  RegisterChatRoomEscapes(windows.chat_room);
  RegisterWhisperEscapes(windows.whispers);
  RegisterAchievementEscapes(windows.achievements);
  RegisterQuestEscapes(windows.quests);
  if (windows.inventory.pinned) {
    escape_.SkipNext();
  }
  escape_.Register(ro::ui::kInventoryWindowId, &windows.inventory.open);
  escape_.Register(ro::ui::kHotkeyWindowId, &windows.hotkeys.open);
  escape_.Register(ro::ui::kOptionsWindowId, &windows.options.open);
  // La fenêtre d'attente d'une icône se lit et se referme. ⚠ PAS LA GRILLE : elle
  // n'a pas de croix, elle se replie.
  escape_.Register(ro::ui::kMenuPlaceholderId, &windows.menu_icons.state.placeholder_open);
  escape_.Register(ro::ui::kGameMenuId, &windows.menu.open);
  // ⚠ LA CONVERSATION N'EN EST PAS. Elle se ferme dans les options, comme celle
  // du client : Échap doit rendre le clavier au jeu sans faire disparaître ce
  // qu'on vient de lire.
  // ⚠ ET RIEN NE SE FERME DERRIÈRE UN SURGISSANT. Une boîte de confirmation ou
  // une liste déroulante ouverte prend Échap pour elle : la laisser passer
  // fermerait aussi la fenêtre dessous, d'une seule touche.
  // L'ENREGISTREUR DE ZONE, APRÈS TOUT LE RESTE.
  //
  // 🔴 LE TRACÉ RECOUVRE L'ÉCRAN et doit prendre la souris à tout le monde ; le
  // témoin doit rester lisible PAR-DESSUS les fenêtres, puisque ce sont elles
  // qu'on est en train de filmer. Dessinés plus tôt, l'un laisserait cliquer au
  // travers et l'autre passerait dessous.
  // LE BANDEAU D'OBTENTION, SUR LA LISTE DE PREMIER PLAN. Après les fenêtres :
  // une notification ne se range pas derrière celle qu'on vient d'ouvrir.
  //
  // ⚠ ET AVANT L'ENREGISTREUR : son témoin doit rester au-dessus de ce qu'il
  // filme, bandeau compris.
  ro::ui::DrawItemObtainToast(obtained, obtained_look, skin_, obtained_grip, &answers.obtained);
  if (!windows.unlocked_hud.empty()) {
    const ro::ui::UnlockNoticeAnswer notice = ro::ui::DrawUnlockNotice(windows.unlocked_hud);
    answers.lock_hud = notice.lock_all;
    answers.hide_unlock_notice = notice.hide_notice;
  }
  // LE NOM DE LA CARTE, après les fenêtres comme le client, et pour la même
  // raison que l'enregistreur passe après lui.
  if (windows.map_sign.has_value()) {
    ro::ui::DrawMapSign(*windows.map_sign);
  }
  ro::ui::DrawZoneSelection(capture, skin_, zone_trace_, &answers.capture);
  ro::ui::DrawCaptureBadge(capture, skin_);

  if (ro::ui::PopupTakesEscape()) {
    escape_.Suppress();
  }
  escape_.Process();

  ui_->EndFrame();
  backend_.Draw();
  return answers;
}

void UiStage::PaintOverWorld(const OverWorldPaint& over_world) {
  if (over_world.paint == nullptr) {
    return;
  }
  ImDrawList* into = ImGui::GetBackgroundDrawList();
  into->AddCallback(over_world.paint, over_world.painter);
  into->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void UiStage::DrawAfkOnly(const ro::ui::AfkClockView& clock, const OverWorldPaint& over_world) {
  // ⚠ L'IMAGE S'OUVRE ET SE FERME QUAND MÊME : ImGui tient son état d'entrée
  // d'une image à l'autre, et lui en sauter une lui ferait perdre le compte de
  // ce que la souris survole — donc le laisserait croire, au réveil, qu'un
  // bouton est encore enfoncé.
  backend_.BeginFrame(*ui_);
  // LE VOILE RESTE PENDANT LA VEILLE : il appartient au monde, que la veille
  // continue de montrer.
  PaintOverWorld(over_world);
  ro::ui::DrawAfkClock(clock);
  ui_->EndFrame();
  backend_.Draw();
}

void UiStage::DrawLoadingOnly(const char* text) {
  // ⚠ MÊME RAISON QUE LA VEILLE : l'image s'ouvre et se ferme, pour qu'ImGui
  // garde le compte de ce que la souris et le clavier font pendant l'attente.
  backend_.BeginFrame(*ui_);
  ro::ui::DrawLoadingScreen(text);
  ui_->EndFrame();
  backend_.Draw();
}

ro::ui::PatchScreenAnswers UiStage::DrawPatchOnly(const ro::ui::PatchScreenView& view) {
  backend_.BeginFrame(*ui_);
  const ro::ui::PatchScreenAnswers answers = ro::ui::DrawPatchScreen(view);
  ui_->EndFrame();
  backend_.Draw();
  return answers;
}

}  // namespace moonclient
