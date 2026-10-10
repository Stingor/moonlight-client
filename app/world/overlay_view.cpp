#include "world/overlay_view.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/signboard.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/world.h"

#include "game/system_fonts.h"
#include "world/entity_bar_view.h"
#include "world/item_dropper.h"
#include "world/name_plate_view.h"

namespace moonclient {
namespace {

void Append(std::vector<ro::render::ScreenQuad>& into,
            const std::vector<ro::render::ScreenQuad>& from) {
  into.insert(into.end(), from.begin(), from.end());
}

// Une couleur rendue plus transparente. ⚠ LA PART SE MULTIPLIE À L'ALPHA QU'ELLE
// PORTE DÉJÀ : une ombre à deux cents réglée à moitié en garde cent.
[[nodiscard]] std::array<std::uint8_t, 4> Faded(std::array<std::uint8_t, 4> color, float share) {
  const float kept = std::clamp(share, 0.0F, 1.0F) * static_cast<float>(color[3]);
  color[3] = static_cast<std::uint8_t>(kept + 0.5F);
  return color;
}

// Le caractère où s'arrête le titre d'une enseigne.
constexpr char kCarriageReturn = '\r';

// L'absence d'ombre, en une couleur : un alpha nul, que la composition
// reconnaît et qui ne coûte alors aucun quad.
constexpr std::array<std::uint8_t, 4> kNoShadowColor = {0, 0, 0, 0};

// La teinte d'un quad qui ne veut pas être teinté. ⚠ ELLE PORTE QUAND MÊME
// L'OPACITÉ : c'est le seul canal par lequel un quad d'image s'estompe.
constexpr std::array<std::uint8_t, 4> kOpaqueWhite = {255, 255, 255, 255};

// La largeur de la plus large des lignes, en pixels.
//
// ⚠ C'EST LA PLUS LARGE, ET NON LA PREMIÈRE : le client mesure toutes ses
// lignes pour dimensionner le widget, et c'est cette largeur-là qui décide où
// tombe son bord gauche — donc où se pose l'insigne.
[[nodiscard]] float WidestLine(const ro::render::FontAtlas& font,
                               const std::vector<std::string>& lines) {
  float widest = 0.0F;
  for (const std::string& line : lines) {
    widest = std::max(widest, ro::render::MeasureText(font, line));
  }
  return widest;
}

}  // namespace

GroundHover GroundHoverOn(const MapStage& stage, const std::optional<ro::sim::CellPosition>& cell) {
  if (!stage.gat.has_value()) {
    return GroundHover::kUnknown;
  }
  if (!cell.has_value()) {
    return GroundHover::kRefused;
  }
  const auto cell_x = static_cast<std::int32_t>(cell->x);
  const auto cell_y = static_cast<std::int32_t>(cell->y);
  if (!stage.gat->InBounds(cell_x, cell_y)) {
    return GroundHover::kRefused;
  }
  return ro::render::CellRefusesWalk(stage.gat->at(cell_x, cell_y).type) ? GroundHover::kRefused
                                                                         : GroundHover::kOpen;
}

ro::render::CursorShape CursorShapeOver(const ro::sim::Actor* hovered, GroundHover ground,
                                        VersusGate versus) {
  if (hovered == nullptr) {
    return ground == GroundHover::kRefused ? ro::render::CursorShape::kForbidden
                                           : ro::render::CursorShape::kArrow;
  }
  switch (hovered->kind) {
    case ro::sim::ActorKind::kItem:
      return ro::render::CursorShape::kPickUp;
    case ro::sim::ActorKind::kMonster:
      // Un mort ne se frappe plus : la flèche dit qu'il n'y a rien à en faire.
      return hovered->posture == ro::sim::Posture::kDead ? ro::render::CursorShape::kArrow
                                                         : ro::render::CursorShape::kAttack;
    case ro::sim::ActorKind::kNonPlayer:
      // Un compagnon — pet, homoncule, mercenaire, élémentaire — ne se parle
      // pas : la flèche reste, et le clic descend au sol.
      if (hovered->familiar != ro::sim::Familiar::kNone) {
        break;
      }
      return hovered->look.job == ro::sim::kPortalJob ? ro::render::CursorShape::kWarp
                                                      : ro::render::CursorShape::kTalk;
    case ro::sim::ActorKind::kPlayer:
      // ⚠ LA MÊME RÈGLE QUE POUR UN MONSTRE SUR LE CADAVRE, et elle vient du
      // même endroit : le client d'origine rend « rien à faire » dès que l'état
      // de l'acteur vaut trois, qui est sa valeur de mort. Un corps à terre
      // laisse donc le clic descendre au sol, et l'on marche dessus.
      if (hovered->posture == ro::sim::Posture::kDead) {
        break;
      }
      // ⚠ PORTE FERMÉE, LE JOUEUR N'EST PAS UN OBSTACLE : la flèche reste, et
      // le clic descend au sol. C'est ce qui permet de marcher à travers un
      // attroupement en ville, et c'est aussi ce que fait le client d'origine.
      // Sur un champ de bataille, la porte regarde le camp : voir `opens_on`.
      return versus.opens_on(hovered->id) ? ro::render::CursorShape::kAttack
                                          : ro::render::CursorShape::kArrow;
    case ro::sim::ActorKind::kSkillUnit:
    case ro::sim::ActorKind::kUnknown:
      break;
  }
  return ro::render::CursorShape::kArrow;
}

namespace {

// LA SURFACE D'UNE ENSEIGNE, d'après ce que le monde en dit.
//
// C'est ICI que se fait la traduction, et nulle part ailleurs : la couche de
// rendu ne connaît pas les échoppes, elle ne connaît que des surfaces.
//
// ⚠ UN SALON PLEIN NE CHANGE PAS D'ASPECT : seule la clef d'un salon privé
// ferme sa porte. Le compte « 20/20 » suffit à dire qu'il est plein.
//
// `spotlit` : l'échoppe est celle du dernier résultat cliqué dans la recherche
// d'échoppes, et son enseigne se peint en jaune pâle.
const ro::formats::Image& SurfaceOf(const ro::sim::Signboard& board,
                                    const ro::render::SignboardSurfaces& surfaces, bool spotlit) {
  switch (board.kind) {
    case ro::sim::SignboardKind::kVendingShop:
      return spotlit ? surfaces.spotlit_vending : surfaces.vending;
    case ro::sim::SignboardKind::kBuyingStore:
      return spotlit ? surfaces.spotlit_buying : surfaces.buying;
    case ro::sim::SignboardKind::kChatRoom:
      break;
  }
  return board.status == ro::sim::kRoomPrivate ? surfaces.locked_room : surfaces.open_room;
}

// De combien l'enseigne monte au-dessus de la tête.
int LiftOf(const ro::sim::Signboard& board) {
  return board.kind == ro::sim::SignboardKind::kChatRoom ? ro::render::kChatRoomSignLiftPixels
                                                         : ro::render::kShopSignLiftPixels;
}

// Ce qu'une enseigne ÉCRIT, avant toute coupe à sa largeur. Un salon annonce
// combien il tient de monde — sauf celui d'une zone PK, qui n'écrit que son
// titre —, une échoppe n'annonce que son titre.
//
// ⚠ UN TITRE S'ARRÊTE AU PREMIER RETOUR CHARIOT : ce qui suit ne s'écrit pas.
std::string SignboardText(const ro::sim::Signboard& board) {
  const std::string_view title =
      std::string_view(board.title).substr(0, board.title.find(kCarriageReturn));
  if (board.kind != ro::sim::SignboardKind::kChatRoom || board.status == ro::sim::kRoomPkZone) {
    return std::string(title);
  }
  return std::format("{} ({}/{})", title, board.users, board.limit);
}

}  // namespace

std::filesystem::path DefaultFontPath() {
  // Le dossier des polices de Windows est à sa place standard : le lire dans
  // l'environnement ne couvrirait qu'une installation déplacée, et une police
  // introuvable ne coûte que l'absence de texte.
  //
  // ⚠ SOUS LINUX, AUCUN CHEMIN N'EST STANDARD : chaque distribution range ses
  // polices ailleurs. C'est la police latine de `BackslashFontSource`, cherchée
  // par son nom dans tous les dossiers de polices.
#ifdef _WIN32
  return std::filesystem::path("C:\\Windows\\Fonts\\arial.ttf");
#else
  return std::filesystem::path(BackslashFontSource());
#endif
}

const ro::formats::Image* BossBadgeOf(ro::sim::BossRank rank,
                                      const ro::render::BossBadges& badges) {
  const ro::formats::Image* chosen = nullptr;
  switch (rank) {
    case ro::sim::BossRank::kMiniBoss:
      chosen = &badges.mini_boss;
      break;
    case ro::sim::BossRank::kMvp:
      chosen = &badges.mvp;
      break;
    case ro::sim::BossRank::kNone:
      break;
  }
  // ⚠ UNE IMAGE VIDE N'EST PAS UN INSIGNE : l'installation peut ne pas avoir le
  // fichier, et un quad sans pixels laisserait un trou de vingt-quatre pixels
  // devant le nom.
  if (chosen == nullptr || chosen->empty()) {
    return nullptr;
  }
  return chosen;
}

std::string GroundItemName(const ro::sim::Actor& actor, const ro::vfs::ItemTable* items,
                           const ro::vfs::ItemWords& words) {
  if (actor.kind != ro::sim::ActorKind::kItem || !actor.ground_item.has_value() ||
      items == nullptr) {
    return {};
  }
  ro::vfs::ItemNameFacts facts;
  facts.item_id = actor.ground_item->item_id;
  facts.type = actor.ground_item->type;
  facts.identified = actor.ground_item->identified;
  return ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items, words));
}

std::vector<ro::render::ScreenQuad> DrawnName(
    const ro::render::FontAtlas& font, const ro::render::Camera& camera, float aspect_ratio,
    const ro::render::DrawnActor& drawn, const ro::sim::Actor& actor,
    const ro::render::TitleTable* titles, const ro::sim::World& world,
    const std::optional<ro::ui::ScreenBand>& bars_under_feet,
    const ro::render::EntityNamesLook& names, const ro::render::BossBadges& badges,
    const ro::formats::Image* emblem, float viewport_width, float viewport_height,
    std::string_view name) {
  const ro::render::EntityNameLook& how =
      names.kinds[static_cast<std::size_t>(NamePlateKindOf(actor, world))];

  ro::render::NamePlateText what;
  what.name = name.empty() ? std::string_view(actor.name) : name;
  // ⚠ LA QUANTITÉ PREND LA PLACE DE L'« ASIDE », celle qui s'écrit entre
  // parenthèses à la suite du nom. Un objet au sol n'a pas de carte de nom : la
  // place est libre, et elle donne « Red Potion (5) » sans rien inventer.
  const std::string amount = GroundItemAmount(actor);
  what.aside = amount;
  what.cut_at_hash = NameCutAtHash(actor);
  std::string_view from_table;
  if (actor.card.has_value()) {
    what.aside = actor.card->aside;
    what.banner = actor.card->banner;
    what.badge = actor.card->badge;
    // 🔴 DEUX SOURCES POUR UN SEUL TITRE, ET ELLES NE SE MÉLANGENT PAS : un PNJ
    // reçoit le sien EN TOUTES LETTRES, un joueur un NUMÉRO que la table de
    // l'installation traduit. Le second ne se lit que si le premier manque.
    what.title = actor.card->title;
    if (what.title.empty() && titles != nullptr) {
      from_table = titles->Find(actor.card->title_id);
      what.title = from_table;
    }
  }
  // 🔴 CE QU'ON NE VEUT PAS VOIR SE VIDE AVANT LA COMPOSITION, et non après :
  // la composition décide du nombre de LIGNES — une bannière tue fait tomber la
  // seconde ligne entière, insigne compris. La retirer après coup laisserait une
  // ligne vide qui pousse le reste vers le bas.
  if (!how.title) {
    what.title = {};
  }
  if (!how.aside) {
    what.aside = {};
  }
  if (!how.banner) {
    what.banner = {};
    what.badge = {};
  }
  // LA COMPOSITION CORÉENNE, parce que c'est le service que l'installation
  // déclare : les deux lignes prennent la parenthèse et une espace finale. Voir
  // `NamePlateLook`.
  constexpr ro::render::NamePlateLook kComposition = {.korean_service = true};
  const std::vector<std::string> lines = ro::render::ComposeNamePlateLines(what, kComposition);
  if (lines.empty()) {
    return {};
  }
  // Aux PIEDS, et non au-dessus de la tête : c'est là que le client pose ses
  // noms, et cela ne dépend donc pas de la taille du sprite.
  const std::optional<glm::vec2> feet = ro::render::ProjectToScreen(
      camera, aspect_ratio, drawn.ground_position, viewport_width, viewport_height);
  if (!feet.has_value()) {
    return {};
  }
  // 🔴 L'OPACITÉ S'APPLIQUE À TOUT — texte, ombre et insigne —, et elle se
  // MULTIPLIE à ce que la couleur porte déjà : une ombre à deux cents d'alpha
  // réglée à moitié en garde cent, et non cent vingt-sept.
  const std::array<std::uint8_t, 4> color = Faded(NameColorOf(actor, how, world), how.opacity);
  const std::array<std::uint8_t, 4> shadow =
      how.shadow ? Faded(ro::render::UnpackedColor(how.shadow_color), how.opacity) : kNoShadowColor;

  std::vector<ro::render::ScreenQuad> quads;
  // 🔴 SA PLACE À LUI, mesurée depuis les pieds comme chez le client — la jauge
  // à `y + échelle(12)`, le nom à `y + échelle(20)`. Il ne s'en écarte QUE s'il
  // chevauche une barre réellement peinte sur cet acteur : voir
  // `NameTopClearOfBars`.
  // ⚠ L'ÉCART SUIT LA TAILLE APPARENTE DU SPRITE — fenêtre, recul et corps : ce
  // qui s'ancre sur un acteur doit rester collé à lui à tous les zooms. Voir
  // `ScreenAnchorScale`.
  const float anchor = ro::render::ScreenAnchorScale(
      viewport_height, ro::render::ApparentScale(camera), drawn.scale);
  const float top = ro::ui::NameTopClearOfBars(
      feet->y + how.offset_pixels * anchor, static_cast<float>(lines.size()) * font.line_height(),
      bars_under_feet);

  // L'IMAGE DE TÊTE PREND SA PLACE À GAUCHE : l'emblème de guilde, sinon
  // l'insigne de boss — le client ne dessine l'insigne que sans emblème. Voir
  // `PlaceNamePlate` : le widget s'élargit pour elle, et toutes les lignes
  // partent du même bord.
  const ro::formats::Image* badge = how.boss_badge ? BossBadgeOf(actor.boss, badges) : nullptr;
  const ro::formats::Image* lead = emblem != nullptr ? emblem : badge;
  ro::render::NamePlateLead lead_kind = ro::render::NamePlateLead::kNone;
  if (emblem != nullptr) {
    lead_kind = ro::render::NamePlateLead::kEmblem;
  } else if (badge != nullptr) {
    lead_kind = ro::render::NamePlateLead::kBossBadge;
  }
  const ro::render::NamePlateLayout placed = ro::render::PlaceNamePlate(
      feet->x, top, WidestLine(font, lines), lines.size(), font.line_height(), lead_kind);
  if (lead != nullptr) {
    ro::render::ScreenQuad quad;
    quad.min = placed.lead_min;
    // ⚠ À SA TAILLE PROPRE, jamais étirée : un emblème de fichier peut ne pas
    // faire vingt-quatre pixels, et le client le blitte tel quel.
    //
    // ⚠ ET SUR LA GRILLE DES PIXELS, que `PlaceNamePlate` tient : chaque pixel
    // tombe au centre d'un texel, et le filtrage n'en mélange aucun. C'est ce
    // qui permet à la couche d'écran de la téléverser en atlas, SANS remplir ses
    // bords transparents : l'étirer ramènerait le magenta de la clé.
    quad.max =
        quad.min + glm::vec2{static_cast<float>(lead->width), static_cast<float>(lead->height)};
    quad.atlas = lead;
    // ⚠ LA TEINTE PORTE L'OPACITÉ, et rien d'autre : l'image garde ses
    // couleurs, elle ne prend pas celle du nom.
    quad.tint = Faded(kOpaqueWhite, how.opacity);
    quads.push_back(quad);
  }

  // LA LIGNE DE BASE SE TIRE DU HAUT QUE LA DISPOSITION A RAMENÉ AU PIXEL : le
  // texte et l'image de tête partagent ainsi la même grille.
  float baseline = placed.text_top_y + font.ascent();
  for (const std::string& line : lines) {
    Append(quads, ro::render::ComposeLabel(font, line, glm::vec2{placed.text_left_x, baseline},
                                           color, shadow));
    baseline += font.line_height();
  }
  return quads;
}

std::vector<ro::render::ScreenQuad> DrawnSignboards(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::sim::PartyRoster& party,
    const ro::render::FontAtlas& font, const ro::render::SignboardSurfaces& surfaces,
    const ro::ui::EntityBarsLook& bars, bool shops_shown, std::uint32_t spotlit_owner_id,
    float viewport_width, float viewport_height, std::vector<SignboardHit>* hits) {
  std::vector<ro::render::ScreenQuad> quads;
  if (hits != nullptr) {
    hits->clear();
  }
  // ⚠ LE RECUL SE PREND UNE FOIS, la taille du CORPS par acteur : l'enseigne
  // garde sa taille, mais l'écart des jauges qu'elle enjambe suit cette
  // échelle. Voir `ScreenAnchorScale`.
  const float apparent = ro::render::ApparentScale(camera);
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr || !actor->signboard.has_value() ||
        (!shops_shown && SignboardOpensStore(actor->signboard->kind))) {
      continue;
    }
    const std::optional<glm::vec2> feet = ro::render::ProjectToScreen(
        camera, aspect_ratio, drawn[index].ground_position, viewport_width, viewport_height);
    const std::optional<ro::render::ScreenRect> bounds = ro::render::ActorScreenBounds(
        camera, aspect_ratio, drawn[index], viewport_width, viewport_height);
    if (!feet.has_value() || !bounds.has_value()) {
      continue;
    }
    // LA TÊTE QUE LA BARRE D'INCANTATION VISE AUSSI : l'abscisse des pieds, la
    // hauteur du haut du sprite. Le client ancre les deux au même point.
    //
    // ⚠ SAUF QUAND LE RÉGLAGE POSE LES JAUGES au-dessus de la tête, ce que le
    // client ne fait jamais : l'enseigne monte alors de ce qu'elles réservent,
    // et quitte l'ancrage de la barre d'incantation plutôt que de recouvrir la
    // vie d'un vendeur. Voir `EntityBarsAbove`.
    const float head_y =
        bounds->min.y - EntityBarsAbove(*actor, world, party, bars,
                                        ro::render::ScreenAnchorScale(viewport_height, apparent,
                                                                      drawn[index].scale));
    const glm::vec2 top_left =
        ro::render::SignboardTopLeft(glm::vec2{feet->x, head_y}, LiftOf(*actor->signboard));
    const std::string text = SignboardText(*actor->signboard);
    const std::string shown = ro::render::FitSignboardTitle(font, text);
    const bool spotlit = spotlit_owner_id != 0 && actor->id == spotlit_owner_id;
    Append(quads, ro::render::ComposeSignboard(
                      font, SurfaceOf(*actor->signboard, surfaces, spotlit), shown, top_left));
    if (hits != nullptr) {
      hits->push_back(SignboardHit{
          .owner_id = actor->id,
          .kind = actor->signboard->kind,
          .rect = ro::render::SignboardRect(top_left),
          .title_rect = ro::render::SignboardTitleRect(top_left),
          .cut_title = shown == text ? std::string() : text,
      });
    }
  }
  return quads;
}

std::vector<ro::render::ScreenQuad> DrawnCursor(const CursorSprite& cursor,
                                                ro::render::CursorShape shape, glm::vec2 mouse_px,
                                                float elapsed_ms, bool pressed) {
  if (cursor.sprite == nullptr || cursor.animation == nullptr) {
    return {};
  }
  const ro::render::CursorMotion motion = ro::render::CursorMotionOf(shape);
  if (motion.action >= cursor.animation->actions().size()) {
    return {};
  }
  const std::size_t images = cursor.animation->actions()[motion.action].frames.size();
  if (images == 0) {
    return {};
  }
  // La cadence du CLIENT, pas celle du fichier : voir CursorMotion.
  const float ticks = std::max(0.0F, elapsed_ms) / ro::render::kCursorTickMs;
  std::size_t frame = 0;
  if (!motion.while_pressed || pressed) {
    frame = static_cast<std::size_t>(ticks / motion.ticks_per_frame);
    frame = motion.plays_once ? std::min(frame, images - 1) : frame % images;
  }
  return ro::render::ComposeScreenSprite(*cursor.sprite, *cursor.animation, motion.action, frame,
                                         mouse_px);
}

std::vector<ro::render::ScreenQuad> DrawnArmedLevel(const ro::render::DigitSprite& digits,
                                                    const ro::render::ArmedSkillLook& look,
                                                    std::uint16_t level, glm::vec2 mouse_px) {
  if (!look.level_shown || level == 0 || digits.sprite == nullptr || digits.animation == nullptr) {
    return {};
  }
  // ⚠ SANS ABRÉGER : le client borne le sien à quatre chiffres et n'y met ni K
  // ni M, et un rang de compétence n'approche jamais le millier.
  const std::vector<std::uint8_t> glyphs = ro::render::NumberGlyphs(level, false);
  const float scale = std::clamp(look.level_scale, ro::render::kSmallestArmedLevelScale,
                                 ro::render::kLargestArmedLevelScale);
  std::vector<ro::render::ScreenQuad> quads;
  for (std::size_t rank = 0; rank < glyphs.size(); ++rank) {
    Append(quads, ro::render::ComposeScreenSprite(
                      *digits.sprite, *digits.animation, 0, glyphs[rank],
                      ro::render::ArmedLevelDigitAt(look, mouse_px, glyphs.size(), rank), scale));
  }
  return quads;
}

}  // namespace moonclient
