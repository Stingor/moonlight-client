#include "game/game_pieces.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/formats/image.h"
#include "ro/render/ailment_sprites.h"
#include "ro/render/battlefield_art.h"
#include "ro/render/boss_badge.h"
#include "ro/render/cursor.h"
#include "ro/render/damage_mark.h"
#include "ro/render/emotion.h"
#include "ro/render/floating_number.h"
#include "ro/render/ground_cursor.h"
#include "ro/render/pvp_hud.h"
#include "ro/render/sleep_bubbles.h"
#include "ro/render/sprite_catalog.h"
#include "ro/ui/system_fonts.h"
#include "ro/vfs/achievement_database.h"
#include "ro/vfs/card_illustrations.h"
#include "ro/vfs/card_item_list.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/job_names.h"
#include "ro/vfs/status_table.h"

#include "game/braces_filled.h"
#include "game/skill_tree_load.h"
#include "game/skin_reload.h"
#include "world/overlay_view.h"
#include "world/signboard_style.h"

namespace moonclient {
namespace {

void LoadWardrobe(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto catalog = ro::render::LoadSpriteCatalog(vfs);
  if (!catalog) {
    ro::core::LogWarning("catalogue de sprites : {} — aucun personnage ne se dessinera",
                         ro::core::Describe(catalog.error()));
    return;
  }
  pieces.wardrobe.emplace(vfs, std::move(*catalog));
  // Les icônes d'objets se lisent À LA DEMANDE : l'installation en porte des
  // dizaines de milliers, et un sac plein n'en montre qu'une centaine.
  pieces.item_icons.emplace(vfs, pieces.wardrobe->catalog().items, &pieces.card_illustrations);
  pieces.card_thumbnails.emplace(vfs, &pieces.card_illustrations);
}

// CE QUE REND CHAQUE CONTENEUR. Absente, l'album de cartes se passe de ses
// macarons « où la trouver ».
void LoadPackageItems(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto items = ro::vfs::ReadPackageItems(vfs);
  if (!items) {
    ro::core::LogWarning("tables de tirage : {} — l'album ne dira pas d'où tombe une carte",
                         ro::core::Describe(items.error()));
    return;
  }
  pieces.package_items = std::move(*items);
}

// LES PROPRIÉTÉS D'ITEM DU SERVEUR. Absente ou illisible, la table reste
// inconnue : le staff demande un item sans que rien ne borne sa quantité, et
// les refus de poids et de buying store reviennent du serveur seul.
void LoadItemProperties(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto properties = ro::vfs::ReadItemProperties(vfs);
  if (!properties) {
    ro::core::LogWarning(
        "propriétés d'item : {} — l'@item du staff ne sera pas borné, ni poids ni buying store "
        "jugés avant le serveur",
        ro::core::Describe(properties.error()));
    return;
  }
  pieces.item_properties = std::move(*properties);
}

// LES MONSTRES DU SERVEUR. Absente ou illisible, la table reste inconnue : les
// listes de monstres ne proposent que ceux que le client a vus.
void LoadMonsterList(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto monsters = ro::vfs::ReadMonsterList(vfs);
  if (!monsters) {
    ro::core::LogWarning("liste des monstres : {} — seuls les monstres vus seront proposés",
                         ro::core::Describe(monsters.error()));
    return;
  }
  pieces.monster_list = std::move(*monsters);
}

// LES LIBELLÉS DES OPTIONS ALÉATOIRES. Absents, une option s'écrit par son
// numéro et sa valeur.
void LoadRandomOptionNames(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto names = ro::vfs::ReadRandomOptionNames(vfs);
  if (!names) {
    ro::core::LogWarning("options aléatoires : {} — elles s'écriront par leur numéro",
                         ro::core::Describe(names.error()));
    return;
  }
  pieces.random_option_names = std::move(*names);
}

// LA TABLE DES LIVRES. Absente, aucun item ne propose « Lire ».
void LoadBooks(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto books = ro::vfs::ReadBookList(vfs);
  if (!books) {
    ro::core::LogWarning("livres : {} — aucun ne se lira", ro::core::Describe(books.error()));
    return;
  }
  pieces.books = std::move(*books);
}

void LoadHitPieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  const std::array<ro::render::ActorPieceRequest, 2> request = {
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody,
                                    std::string(ro::render::kDigitsSpritePath)},
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody,
                                    std::string(ro::render::kDamageMarkSpritePath)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  if (!loaded) {
    ro::core::LogWarning("chiffres de dégâts : {} — aucun chiffre ne montera d'un coup",
                         ro::core::Describe(loaded.error()));
    return;
  }
  pieces.hit_sprites = std::move(*loaded);
  // La marque est FACULTATIVE : une installation qui ne l'aurait pas garde ses
  // nombres.
  const bool has_mark = pieces.hit_sprites.size() > 1;
  pieces.numbers.emplace(ro::render::DigitSprite{&pieces.hit_sprites.front().sprite,
                                                 &pieces.hit_sprites.front().animation},
                         has_mark ? &pieces.hit_sprites[1].sprite : nullptr,
                         has_mark ? &pieces.hit_sprites[1].animation : nullptr);
}

void LoadEmotionPieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  const std::array<ro::render::ActorPieceRequest, 1> request = {ro::render::ActorPieceRequest{
      ro::render::ActorPart::kBody, std::string(ro::render::kEmotionSpritePath)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  if (!loaded || loaded->empty()) {
    ro::core::LogWarning("émoticônes : sprite absent, les bulles ne paraîtront pas");
    return;
  }
  pieces.emotion_sprite = std::move(*loaded);
  pieces.emotions.emplace(&pieces.emotion_sprite.front().sprite,
                          &pieces.emotion_sprite.front().animation);
  pieces.emotion_icons.emplace(pieces.emotion_sprite.front().sprite,
                               pieces.emotion_sprite.front().animation);
  pieces.silence_dots.emplace(&pieces.emotion_sprite.front().sprite,
                              &pieces.emotion_sprite.front().animation);
  pieces.quest_markers.emplace(&pieces.emotion_sprite.front().sprite,
                               &pieces.emotion_sprite.front().animation);
}

void LoadSleepBubblePieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  if (pieces.sleep_bubbles.has_value()) {
    return;
  }
  const std::array<ro::render::ActorPieceRequest, 1> request = {ro::render::ActorPieceRequest{
      ro::render::ActorPart::kBody, std::string(ro::render::kSleepBubblesSpritePath)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  if (!loaded || loaded->empty()) {
    ro::core::LogWarning("sommeil : sprite des bulles absent, les dormeurs n'en porteront pas");
    return;
  }
  pieces.sleep_bubbles_sprite = std::move(*loaded);
  pieces.sleep_bubbles.emplace(&pieces.sleep_bubbles_sprite.front().sprite,
                               &pieces.sleep_bubbles_sprite.front().animation);
}

// Le sprite d'un état du corps, ou rien : son absence se dit une fois, et le
// reste du jeu continue sans lui.
std::vector<ro::render::LoadedActorSprite> LoadAilmentSprite(ro::vfs::Vfs& vfs,
                                                             std::string_view path,
                                                             std::string_view absent_note) {
  const std::array<ro::render::ActorPieceRequest, 1> request = {
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody, std::string(path)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  if (!loaded || loaded->empty()) {
    ro::core::LogWarning("{}", absent_note);
    return {};
  }
  return std::move(*loaded);
}

void LoadAilmentPieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  if (!pieces.stun_stars.has_value()) {
    pieces.stun_stars_sprite = LoadAilmentSprite(
        vfs, ro::render::kStunStarsSpritePath,
        "étourdissement : sprite des étoiles absent, les étourdis n'en porteront pas");
    if (!pieces.stun_stars_sprite.empty()) {
      pieces.stun_stars.emplace(&pieces.stun_stars_sprite.front().sprite,
                                &pieces.stun_stars_sprite.front().animation);
    }
  }
  if (!pieces.frozen_ice.has_value()) {
    pieces.frozen_ice_sprite =
        LoadAilmentSprite(vfs, ro::render::kFrozenIceSpritePath,
                          "gel : sprite de la glace absent, les gelés n'en porteront pas");
    if (!pieces.frozen_ice_sprite.empty()) {
      pieces.frozen_ice.emplace(&pieces.frozen_ice_sprite.front().sprite,
                                &pieces.frozen_ice_sprite.front().animation);
    }
  }
  if (!pieces.curse_marks.has_value()) {
    pieces.curse_marks_sprite = LoadAilmentSprite(
        vfs, ro::render::kCurseMarkSpritePath,
        "malédiction : sprite de la marque absent, les maudits n'en porteront pas");
    if (!pieces.curse_marks_sprite.empty()) {
      pieces.curse_marks.emplace(&pieces.curse_marks_sprite.front().sprite,
                                 &pieces.curse_marks_sprite.front().animation);
    }
  }
}

void LoadCursor(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  const std::array<ro::render::ActorPieceRequest, 1> request = {ro::render::ActorPieceRequest{
      ro::render::ActorPart::kBody, std::string(ro::render::kCursorSpritePath)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  auto renderer = ro::render::OverlayRenderer::Create();
  if (!loaded) {
    ro::core::LogWarning("curseur : {} — le pointeur du système restera",
                         ro::core::Describe(loaded.error()));
    return;
  }
  if (!renderer) {
    return;
  }
  pieces.cursor_sprites = std::move(*loaded);
  pieces.cursor =
      CursorSprite{&pieces.cursor_sprites.front().sprite, &pieces.cursor_sprites.front().animation};
  pieces.companion_target_marks.emplace(&pieces.cursor_sprites.front().sprite,
                                        &pieces.cursor_sprites.front().animation);
  pieces.overlay = std::move(*renderer);
  // Cacher celui du système est le COROLLAIRE d'avoir le nôtre, et non une
  // décision à part : les séparer laisserait les deux flèches à l'écran le
  // temps que l'appelant y pense.
  SDL_HideCursor();
}

// UNE FONTE DE CHIFFRES DU HUD : onze images, une par action.
//
// ⚠ ABSENTE N'EST PAS UNE PANNE. Une installation qui ne porte pas ces deux
// fichiers laisse le classement muet, et rien d'autre ne s'en trouve gêné : ce
// n'est ni le curseur, sans lequel on ne vise plus, ni la police, sans laquelle
// rien ne se lit.
void LoadHudDigitFont(ro::vfs::Vfs& vfs, std::string_view path, std::string_view what,
                      std::vector<ro::render::LoadedActorSprite>& into, CursorSprite& font) {
  const std::array<ro::render::ActorPieceRequest, 1> request = {
      ro::render::ActorPieceRequest{ro::render::ActorPart::kBody, std::string(path)}};
  auto loaded = ro::render::LoadActorSprites(vfs, request);
  if (!loaded || loaded->empty()) {
    ro::core::LogWarning("{} : {} — le classement PVP restera muet", what,
                         loaded ? "aucune image" : ro::core::Describe(loaded.error()));
    return;
  }
  into = std::move(*loaded);
  font = CursorSprite{&into.front().sprite, &into.front().animation};
}

void LoadPvpHudFonts(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  LoadHudDigitFont(vfs, ro::render::kPvpRankFontPath, "compteur de rang PVP",
                   pieces.pvp_rank_sprites, pieces.pvp_rank_font);
  LoadHudDigitFont(vfs, ro::render::kPvpStopwatchFontPath, "chronomètre PVP",
                   pieces.pvp_stopwatch_sprites, pieces.pvp_stopwatch_font);
}

void LoadGroundCursor(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto renderer = ro::render::GroundQuadRenderer::Create();
  if (!renderer) {
    ro::core::LogWarning(
        "curseur au sol : pas de renderer ({}) — la case désignée ne se "
        "marquera pas",
        ro::core::Describe(renderer.error()));
    return;
  }
  // ReadRaw : le chemin est celui du fichier lui-même, et non un nom saisi. Le
  // passer à Read le transcoderait une seconde fois — sans dommage ici, où tout
  // est ASCII, mais c'est la règle des textures et il n'y a pas d'exception à
  // ouvrir pour une seule.
  // `Read` : ce chemin est une constante à nous, donc de l'UTF-8. Voir
  // `ro/vfs/encoding.h` — au-dessus de `vfs`, plus rien ne manipule de CP949.
  auto bytes = vfs.Read(std::string(ro::render::kGroundCursorTexturePath));
  if (!bytes) {
    ro::core::LogWarning("curseur au sol : {} absent, la case désignée ne se marquera pas",
                         ro::render::kGroundCursorTexturePath);
    return;
  }
  auto image = ro::formats::DecodeImage(ro::render::kGroundCursorTexturePath, *bytes);
  if (!image) {
    ro::core::LogWarning(
        "curseur au sol : image illisible ({}) — la case désignée ne se "
        "marquera pas",
        ro::core::Describe(image.error()));
    return;
  }
  if (auto used = renderer->UseTexture(*image); !used) {
    ro::core::LogWarning(
        "curseur au sol : texture refusée ({}) — la case désignée ne se "
        "marquera pas",
        ro::core::Describe(used.error()));
    return;
  }
  pieces.ground_cursor = std::move(*renderer);
}

void LoadEffectGround(GamePieces& pieces) {
  // ⚠ AUCUNE IMAGE À POSER ICI, et ce n'est pas un oubli : les marques de sol
  // des effets nomment la leur dans leurs tables, et le renderer la téléverse
  // au premier lot qui la demande. Lui en donner une d'avance obligerait ce
  // fichier à connaître les tables d'effets.
  auto renderer = ro::render::GroundQuadRenderer::Create();
  if (!renderer) {
    ro::core::LogWarning(
        "marques de sol des effets : {} — les flaques et les anneaux au sol ne paraîtront pas",
        ro::core::Describe(renderer.error()));
    return;
  }
  pieces.effect_ground = std::move(*renderer);
}

// LES MOTS DU NOM D'UN OBJET. Trois fichiers de l'installation, dont un en
// base64. Leur absence n'empêche rien : un objet garde son nom de base.
void LoadItemWords(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto words = ro::vfs::ReadItemWords(vfs);
  if (!words) {
    ro::core::LogWarning("mots des noms d'objets : {} — les objets garderont leur nom de base",
                         ro::core::Describe(words.error()));
    return;
  }
  pieces.item_words = std::move(*words);
  // Relus en anglais : la langue devra s'y reposer.
  pieces.item_words_language = static_cast<std::size_t>(-1);
}

// LES TABLES DE CARTES. Une carte ne se reconnaît pas à sa ressource — elles
// la partagent toutes — mais à sa présence dans la table des illustrations, qui
// nomme aussi son dessin, ou dans la liste des cartes. Absente, la première
// laisse aux cartes le dos de carte générique ; la seconde, sa recherche par
// illustration ou par mot.
void LoadCardIllustrations(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  if (auto listed = ro::vfs::ReadCardItemList(vfs); listed) {
    pieces.card_items = std::move(*listed);
  } else {
    ro::core::LogWarning("liste des cartes : {}", ro::core::Describe(listed.error()));
  }
  auto cards = ro::vfs::ReadCardIllustrations(vfs);
  if (!cards) {
    ro::core::LogWarning("illustrations de cartes : {} — les cards garderont le dos générique",
                         ro::core::Describe(cards.error()));
    return;
  }
  pieces.card_illustrations = std::move(*cards);
}

// LES NOMS DE MÉTIER, lus dans `job_names.yml`.
void LoadJobNames(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto names = ro::vfs::ReadJobNames(vfs);
  if (!names) {
    ro::core::LogWarning("noms de métiers : {} — les métiers s'afficheront par leur numéro",
                         ro::core::Describe(names.error()));
    return;
  }
  pieces.job_names = std::move(*names);
}

// CE QUE LE CLIENT SAIT DES STATUTS : leurs noms, leurs icônes, leur rang, lus
// dans `data\yaml_files\status_effects.yml`.
void LoadStatusTable(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto names = ro::vfs::ReadStatusTable(vfs);
  if (!names) {
    ro::core::LogWarning("noms de statuts : {} — les états s'afficheront sans nom",
                         ro::core::Describe(names.error()));
    return;
  }
  pieces.status_table = std::move(*names);
}

// LES IMAGES DE CES STATUTS. ⚠ APRÈS LA TABLE, TOUJOURS : ce cache garde une
// référence dessus, et l'ouvrir avant lui en donnerait une vide — donc aucune
// icône, sans qu'aucun message ne le dise.
void OpenStatusIcons(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  pieces.status_icons.emplace(vfs, pieces.status_table);
}

// LES LIBELLÉS DES TITRES, lus dans `titles.yml`.
void LoadTitles(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto titles = ro::render::TitleTable::Load(vfs);
  if (!titles) {
    ro::core::LogWarning("titres : {} — aucun titre ne paraîtra sous les noms",
                         ro::core::Describe(titles.error()));
    return;
  }
  pieces.titles = std::move(*titles);
}

// LES SUCCÈS : la base donne le titre que chacun récompense, et le catalogue de
// la fenêtre, tiré des succès qui y ont leur fenêtre. ⚠ LEUR ABSENCE NE COUPE
// RIEN D'AUTRE : la fenêtre reste vide et la fiche sans titre, la progression
// se tient quand même.
void LoadAchievements(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto database = ro::vfs::AchievementDatabase::Load(vfs);
  if (!database) {
    ro::core::LogWarning("succès : {} — ni fenêtre des succès ni titre en récompense",
                         ro::core::Describe(database.error()));
    return;
  }
  pieces.achievement_titles = ro::vfs::AchievementTitles::Of(*database);
  ro::core::LogInfo("succès : {} au serveur, {} titre(s) en récompense", database->entries().size(),
                    pieces.achievement_titles->entries().size());
  ro::vfs::AchievementCatalog catalog = ro::vfs::AchievementCatalog::Of(*database);
  ro::core::LogInfo("succès : {} au catalogue, {} sans texte", catalog.entries().size(),
                    catalog.without_texts());
  pieces.achievement_catalog = std::move(catalog);
}

// LE CATALOGUE DES QUÊTES, notre propre table YAML. ⚠ AUCUN REPLI SUR LES
// TABLES DU CLIENT D'ORIGINE : absent, le journal le dit et montre les numéros.
void LoadQuestCatalog(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  const ro::core::Result<std::vector<std::uint8_t>> bytes = vfs.Read(kQuestCatalogPath);
  if (!bytes) {
    pieces.quest_catalog_problem =
        BracesFilled(ro::core::Tr("{} est introuvable."), {kQuestCatalogPath});
    ro::core::LogWarning("quêtes : {} illisible ({}) — le journal montrera des numéros",
                         kQuestCatalogPath, ro::core::Describe(bytes.error()));
    return;
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  auto catalog = QuestCatalog::Parse(text, pieces.quest_language);
  if (!catalog) {
    pieces.quest_catalog_problem =
        BracesFilled(ro::core::Tr("{} ne se lit pas."), {kQuestCatalogPath});
    ro::core::LogWarning("quêtes : {} — {}", kQuestCatalogPath,
                         ro::core::Describe(catalog.error()));
    return;
  }
  ro::core::LogInfo("quêtes : {} au catalogue, {} écartée(s)", catalog->size(), catalog->skipped());
  pieces.quest_catalog = std::move(*catalog);
  pieces.quest_catalog_problem.clear();
}

// LES QUÊTES RECOMMANDÉES, notre propre table YAML, sans repli non plus :
// absente, l'onglet « Recommandées » est vide.
void LoadRecommendedCatalog(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  pieces.recommended_catalog.reset();
  const ro::core::Result<std::vector<std::uint8_t>> bytes = vfs.Read(kRecommendedCatalogPath);
  if (!bytes) {
    ro::core::LogWarning("quêtes : {} illisible ({}) — aucune recommandation",
                         kRecommendedCatalogPath, ro::core::Describe(bytes.error()));
    return;
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  auto catalog = RecommendedCatalog::Parse(text, pieces.quest_language);
  if (!catalog) {
    ro::core::LogWarning("quêtes : {} — {}", kRecommendedCatalogPath,
                         ro::core::Describe(catalog.error()));
    return;
  }
  ro::core::LogInfo("quêtes : {} recommandation(s), {} écartée(s)", catalog->entries().size(),
                    catalog->skipped());
  pieces.recommended_catalog = std::move(*catalog);
}

// LA CARTE DU MONDE, lue dans world_map.yml.
void LoadWorldView(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto tables = ro::vfs::WorldViewTables::Load(vfs);
  if (!tables) {
    ro::core::LogWarning("carte du monde : {} — elle restera fermée",
                         ro::core::Describe(tables.error()));
    return;
  }
  ro::core::LogInfo("carte du monde : {} monde(s)", tables->worlds().size());
  pieces.world_view = std::move(*tables);
  auto agents = ro::vfs::WarpAgents::Load(vfs);
  if (!agents) {
    ro::core::LogWarning("carte du monde : pas de table des Warp Agent — {}",
                         ro::core::Describe(agents.error()));
    return;
  }
  pieces.warp_agents = std::move(*agents);
}

// LE NOM LISIBLE DE CHAQUE CARTE. Absente, une destination s'écrit avec le mot
// d'une destination inconnue — ce que fait le client quand sa table manque.
void LoadMapDisplayNames(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  auto names = ro::vfs::MapDisplayNames::Load(vfs);
  if (!names) {
    ro::core::LogWarning("noms de cartes : {} — les destinations resteront anonymes",
                         ro::core::Describe(names.error()));
    return;
  }
  ro::core::LogInfo("noms de cartes : {} carte(s)", names->size());
  pieces.map_display_names = std::move(*names);
}

void LoadFont(GamePieces& pieces) {
  auto loaded = ro::render::FontAtlas::Load(DefaultFontPath(), kNameFontPixels);
  if (!loaded) {
    ro::core::LogWarning("police : {} — le texte sera dessiné avec la police d'ImGui",
                         ro::core::Describe(loaded.error()));
    return;
  }
  pieces.font = std::move(*loaded);
}

}  // namespace

// Le bandeau d'obtention croise les trois sources : le sac dira ce qui entre,
// la table le nommera, le cache l'illustrera.
//
// ⚠ SANS TABLE D'OBJETS, PAS DE BANDEAU : il n'aurait aucun nom à écrire. Les
// icônes, elles, sont facultatives — le réglage peut les taire de toute façon.
void MakeItemObtainToast(GamePieces& pieces) {
  if (!pieces.wardrobe.has_value()) {
    ro::core::LogWarning(
        "bandeau d'objet obtenu : sans catalogue d'objets, aucun nom à écrire — bandeau tu");
    return;
  }
  pieces.item_toast.emplace(pieces.wardrobe->catalog().items, pieces.item_words,
                            pieces.item_icons.has_value() ? &*pieces.item_icons : nullptr);
}

// CE QUE LE RAMASSAGE DIT DANS LA CONVERSATION.
//
// 🔴 HORS DE LA GARDE DU BANDEAU, et ce n'est pas un rangement : la ligne de
// REFUS ne nomme aucun objet — le paquet d'un refus ne le porte pas —, donc
// elle n'a besoin ni des sprites ni de la table des objets. La faire dépendre
// du catalogue rendrait le ramassage muet sur une installation dont il est
// illisible, c'est-à-dire le défaut même que cette ligne répare.
//
// Seule la ligne de GAIN veut la table, pour nommer ce qui est entré : sans
// elle, elle se tait et le refus continue de parler.
void MakeItemPickupReport(GamePieces& pieces) {
  const ro::vfs::ItemTable* items =
      pieces.wardrobe.has_value() ? &pieces.wardrobe->catalog().items : nullptr;
  pieces.item_report.emplace(items, pieces.item_words);
  // LES LIGNES DE COMBAT, sous la même garde : elles ne veulent que les mots.
  pieces.battle_log.emplace(pieces.item_words);
}

const ro::render::FontAtlas* EnsureNameFont(GamePieces& pieces, float pixels) {
  const float wanted = std::round(
      std::clamp(pixels, ro::render::kSmallestNamePlateFont, ro::render::kLargestNamePlateFont));
  if (!pieces.name_font.has_value() || pieces.name_font_pixels != wanted) {
    auto loaded = ro::render::FontAtlas::Load(DefaultFontPath(), static_cast<int>(wanted));
    if (loaded) {
      // 🔴 OUBLIER L'ANCIENNE TEXTURE AVANT DE DÉTRUIRE L'IMAGE, et c'est ce
      // qui manquait : la couche d'écran reconnaît un atlas par son ADRESSE, et
      // une police regravée dans le même `optional` retombe exactement à la
      // même. Sans cet oubli, elle rendait l'ANCIENNE texture aux nouvelles
      // coordonnées de glyphe — les noms sortaient en charabia.
      if (pieces.name_font.has_value()) {
        pieces.overlay.ForgetAtlas(pieces.name_font->image());
      }
      pieces.name_font = std::move(*loaded);
      pieces.name_font_pixels = wanted;
    } else {
      // ⚠ ON NE RÉESSAIE PAS À CHAQUE IMAGE : la taille est retenue même en
      // échec, sans quoi une police introuvable ferait relire le fichier
      // soixante fois par seconde. Le repli est la police de l'interface.
      pieces.name_font_pixels = wanted;
      ro::core::LogWarning("police des noms : {} — les noms garderont la police de l'interface",
                           ro::core::Describe(loaded.error()));
    }
  }
  if (pieces.name_font.has_value()) {
    return &*pieces.name_font;
  }
  return pieces.font.has_value() ? &*pieces.font : nullptr;
}

namespace {

// LE FICHIER D'UNE FAMILLE NOMMÉE — son gras quand on le veut et qu'elle en a
// un —, ou vide quand la machine ne la porte pas.
[[nodiscard]] std::string SignboardFontFile(const SignboardStyle& style) {
  for (const ro::ui::FontFamilySpec& family : ro::ui::SystemFontFamilies()) {
    if (family.label == style.font_name) {
      return std::string(style.font_bold && !family.bold.empty() ? family.bold : family.regular);
    }
  }
  return {};
}

}  // namespace

const ro::render::FontAtlas* EnsureSignboardFont(GamePieces& pieces, const SignboardStyle& style) {
  const ro::render::FontAtlas* game_font = pieces.font.has_value() ? &*pieces.font : nullptr;
  if (UsesGameFont(style)) {
    return game_font;
  }
  const std::string file = style.font_name.empty() ? ro::core::Utf8FromPath(DefaultFontPath())
                                                   : SignboardFontFile(style);
  if (file.empty()) {
    return game_font;
  }
  const int pixels =
      std::clamp(style.font_pixels, kSmallestSignboardFontPixels, kLargestSignboardFontPixels);
  const std::string key = std::format("{}|{}", file, pixels);
  if (pieces.signboard_font_key != key) {
    // ⚠ LE CHOIX EST RETENU MÊME EN ÉCHEC, pour la raison de `EnsureNameFont` :
    // sans quoi une police illisible se relirait à chaque image.
    pieces.signboard_font_key = key;
    if (pieces.signboard_font.has_value()) {
      pieces.overlay.ForgetAtlas(pieces.signboard_font->image());
      pieces.signboard_font.reset();
    }
    auto loaded = ro::render::FontAtlas::Load(ro::core::PathFromUtf8(file), pixels);
    if (loaded) {
      pieces.signboard_font = std::move(*loaded);
    } else {
      ro::core::LogWarning("police des enseignes : {} — elles garderont la police du jeu",
                           ro::core::Describe(loaded.error()));
    }
  }
  return pieces.signboard_font.has_value() ? &*pieces.signboard_font : game_font;
}

void ForgetShopSignboards(GamePieces& pieces) {
  pieces.overlay.ForgetAtlas(pieces.signboards.vending);
  pieces.overlay.ForgetAtlas(pieces.signboards.buying);
  pieces.overlay.ForgetAtlas(pieces.signboards.spotlit_vending);
  pieces.overlay.ForgetAtlas(pieces.signboards.spotlit_buying);
}

void LoadShellPieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  // L'HABILLAGE D'ABORD : il ne dépend de rien d'autre, et ce qu'il lui manque
  // se dit tout de suite.
  LoadSkinTextures(vfs, pieces);

  // 🔴 AVEC LA COQUILLE ET NON AVEC LA GARDE-ROBE, où elle vivait. Ce n'est
  // qu'un cache paresseux sur le VFS — il ne coûte rien à monter et ne dépend
  // de rien —, mais le placer derrière la garde-robe le rendait absent de
  // l'écran de connexion quand le décor vivant est éteint. Le logo de la porte
  // Discord passe par lui.
  pieces.game_images.emplace(vfs);

  LoadCursor(vfs, pieces);
  LoadPvpHudFonts(vfs, pieces);
  LoadGroundCursor(vfs, pieces);
  LoadEffectGround(pieces);
  LoadFont(pieces);
}

void LoadBackdropPieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  if (pieces.wardrobe.has_value()) {
    return;
  }
  // ⚠ AVANT LA GARDE-ROBE, qui construit les icônes : celles-ci gardent un
  // pointeur sur cette table, et la lire après leur en donnerait une vide.
  LoadCardIllustrations(vfs, pieces);
  LoadWardrobe(vfs, pieces);
  // AVEC LE DÉCOR ET NON AVEC LE MONDE : la ville derrière le formulaire montre
  // son bandeau, qui vient de cette table.
  LoadMapDisplayNames(vfs, pieces);
}

void FollowQuestLanguage(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  std::string language = ro::core::CurrentLanguage();
  if (language == pieces.quest_language) {
    return;
  }
  // ⚠ RETENUE AVANT LA LECTURE, même si elle échoue : une tentative par
  // changement, et non une par image.
  pieces.quest_language = std::move(language);
  // Relit une table, et la rend seulement si elle se lit.
  const auto reread = [&vfs, &pieces](std::string_view path, auto& kept) {
    if (!kept.has_value()) {
      return;
    }
    const ro::core::Result<std::vector<std::uint8_t>> bytes = vfs.Read(path);
    if (!bytes) {
      return;
    }
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    using Catalog = std::remove_cvref_t<decltype(*kept)>;
    if (auto read = Catalog::Parse(text, pieces.quest_language)) {
      *kept = std::move(*read);
    }
  };
  reread(kQuestCatalogPath, pieces.quest_catalog);
  reread(kRecommendedCatalogPath, pieces.recommended_catalog);
  ro::core::LogInfo("quêtes : catalogues relus en « {} »", pieces.quest_language);
}

void LoadWorldPieces(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  // ⚠ LA GARDE-ROBE D'ABORD, parce que trois choses la DÉSIGNENT plus bas — le
  // bandeau d'objet obtenu et les icônes. Elle est souvent déjà là : le décor
  // vivant l'a montée avant que le joueur ait tapé son compte.
  LoadBackdropPieces(vfs, pieces);
  LoadSkillTree(vfs, pieces);
  LoadJobNames(vfs, pieces);
  LoadTitles(vfs, pieces);
  LoadAchievements(vfs, pieces);
  pieces.quest_language = ro::core::CurrentLanguage();
  LoadQuestCatalog(vfs, pieces);
  LoadRecommendedCatalog(vfs, pieces);
  LoadWorldView(vfs, pieces);
  LoadStatusTable(vfs, pieces);
  OpenStatusIcons(vfs, pieces);
  LoadItemWords(vfs, pieces);
  LoadPackageItems(vfs, pieces);
  LoadItemProperties(vfs, pieces);
  LoadMonsterList(vfs, pieces);
  LoadRandomOptionNames(vfs, pieces);
  LoadBooks(vfs, pieces);
  // ⚠ APRÈS LES MOTS ET LA GARDE-ROBE : il les DÉSIGNE tous les deux.
  MakeItemObtainToast(pieces);
  // ⚠ HORS DE LA GARDE DU BANDEAU : le refus parle même sans catalogue.
  MakeItemPickupReport(pieces);
  LoadHitPieces(vfs, pieces);
  LoadEmotionPieces(vfs, pieces);
  LoadSleepBubblePieces(vfs, pieces);
  LoadAilmentPieces(vfs, pieces);
  pieces.boss_badges = ro::render::LoadBossBadges(vfs);
  pieces.battlefield_art = ro::render::LoadBattlefieldArt(vfs);
  pieces.signboards = ro::render::LoadSignboardSurfaces(vfs);
  // Les modèles se chargent à leur première apparition : seul leur renderer
  // naît ici.
  if (!pieces.actor_models.has_value()) {
    pieces.actor_models.emplace(vfs);
  }
  if (!pieces.actor_model_renderer.has_value()) {
    if (auto renderer = ro::render::ActorModelRenderer::Create(); renderer) {
      pieces.actor_model_renderer = std::move(*renderer);
    } else {
      ro::core::LogWarning("acteurs en modèle : {} — ils ne paraîtront pas",
                           ro::core::Describe(renderer.error()));
    }
  }
}

std::vector<std::span<const ro::render::DrawnActor>> DrawnHeldChildSprites(
    GamePieces& pieces, std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  std::vector<std::span<const ro::render::DrawnActor>> lots;
  const auto keep = [&lots](std::span<const ro::render::DrawnActor> held) {
    if (!held.empty()) {
      lots.push_back(held);
    }
  };
  if (pieces.sleep_bubbles.has_value()) {
    keep(pieces.sleep_bubbles->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.silence_dots.has_value()) {
    keep(pieces.silence_dots->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.stun_stars.has_value()) {
    keep(pieces.stun_stars->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.quest_markers.has_value()) {
    keep(pieces.quest_markers->Drawn(now_ms, drawn, ids, world, translucent));
  }
  if (pieces.companion_target_marks.has_value()) {
    keep(pieces.companion_target_marks->Drawn(now_ms, pieces.companion_ai.target_marks(), drawn,
                                              ids, world, translucent));
  }
  return lots;
}

std::span<const ro::render::DrawnActor> DrawnFrozenIce(
    GamePieces& pieces, std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  if (!pieces.frozen_ice.has_value()) {
    return {};
  }
  return pieces.frozen_ice->Drawn(now_ms, drawn, ids, world, translucent);
}

std::span<const ro::render::DrawnActor> DrawnCurseMarks(
    GamePieces& pieces, std::uint32_t now_ms, float camera_yaw_degrees,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, bool translucent) {
  if (!pieces.curse_marks.has_value()) {
    return {};
  }
  return pieces.curse_marks->Drawn(now_ms, camera_yaw_degrees, drawn, ids, world, translucent);
}

}  // namespace moonclient
