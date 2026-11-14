// Le cache de mise en page du fil : ce qu'il rend, et ce qu'il refuse de rendre.
//
// 🔴 UNE HAUTEUR PÉRIMÉE NE SE VOIT PAS COMME UNE ERREUR : le fil se décale, une
// ligne en recouvre une autre ou laisse un trou, et rien ne le signale. Chaque
// essai compare donc le cache au moteur lui-même, `ChatLogHeight`, qui remet
// tout en page.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_log.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kWide = 600.0F;
constexpr float kNarrow = 150.0F;
const ro::ui::ChatLogStyle kStyle{.font_size = 14.0F, .line_gap = 2.0F, .timestamps = true};

// Un fil dont les lignes ont des longueurs différentes, donc des hauteurs
// différentes sur une largeur étroite.
struct Thread {
  std::vector<std::string> texts;
  std::vector<ro::ui::ChatLineView> lines;

  void Add(std::string text, std::uint64_t id) {
    texts.push_back(std::move(text));
    lines.push_back(ro::ui::ChatLineView{.id = id});
  }
  // Les vues regardent les textes : elles se reposent après chaque ajout, un
  // vecteur qui grandit déplaçant ce qu'il porte.
  [[nodiscard]] std::vector<ro::ui::ChatLineView>& Views() {
    for (std::size_t rank = 0; rank < lines.size(); ++rank) {
      lines[rank].text = texts[rank];
    }
    return lines;
  }
};

[[nodiscard]] Thread SomeThread(int count) {
  Thread thread;
  for (int rank = 1; rank <= count; ++rank) {
    std::string text = "Ligne " + std::to_string(rank);
    for (int word = 0; word < rank % 7; ++word) {
      text += " avec quelques mots de plus";
    }
    thread.Add(text, static_cast<std::uint64_t>(rank));
  }
  return thread;
}

}  // namespace

TEST_CASE("Fil : le cache rend la hauteur du moteur, et ne remesure que le neuf", "[ui][chatlog]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  Thread thread = SomeThread(40);
  ro::ui::ChatLogLayout layout;

  const float first = layout.Update(thread.Views(), kStyle, kNarrow);
  CHECK(first == ro::ui::ChatLogHeight(thread.Views(), kStyle, kNarrow));
  CHECK(layout.measured() == 40);

  // La même image, recommencée : rien n'est remesuré, rien ne change.
  CHECK(layout.Update(thread.Views(), kStyle, kNarrow) == first);
  CHECK(layout.measured() == 0);

  // Une ligne arrive : elle seule se mesure.
  thread.Add("Une ligne neuve, assez longue pour se replier sur la largeur étroite.", 41);
  const float grown = layout.Update(thread.Views(), kStyle, kNarrow);
  CHECK(layout.measured() == 1);
  CHECK(grown == ro::ui::ChatLogHeight(thread.Views(), kStyle, kNarrow));
  CHECK(grown > first);
  ui.EndFrame();
}

TEST_CASE("Fil : une largeur ou une taille changée remet tout en page", "[ui][chatlog]") {
  // 🔴 LA HAUTEUR D'UNE LIGNE DÉPEND DE LA LARGEUR : sur la large, elle tient sur
  // une rangée, sur l'étroite, sur plusieurs. Garder l'ancienne ferait chevaucher
  // les lignes dès qu'on redimensionne la fenêtre.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  Thread thread = SomeThread(40);
  ro::ui::ChatLogLayout layout;
  const float narrow = layout.Update(thread.Views(), kStyle, kNarrow);

  const float wide = layout.Update(thread.Views(), kStyle, kWide);
  CHECK(layout.measured() == 40);
  CHECK(wide == ro::ui::ChatLogHeight(thread.Views(), kStyle, kWide));
  CHECK(wide < narrow);

  ro::ui::ChatLogStyle larger = kStyle;
  larger.font_size = 20.0F;
  const float bigger = layout.Update(thread.Views(), larger, kWide);
  CHECK(layout.measured() == 40);
  CHECK(bigger == ro::ui::ChatLogHeight(thread.Views(), larger, kWide));
  ui.EndFrame();
}

TEST_CASE("Fil : une identité qui ne croît pas se remesure", "[ui][chatlog]") {
  // ⚠ LE CACHE CHERCHE EN AVANÇANT, et une identité qui ne croît pas désignerait
  // pour lui la ligne qu'il a vue sous ce numéro — ici une ligne courte, alors
  // que celle qui le reprend est longue.
  constexpr std::string_view kLong =
      "Une ligne bien plus longue que la courte, qui se replie sur plusieurs rangées.";
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);

  SECTION("reprise deux fois de suite") {
    ro::ui::ChatLogLayout layout;
    Thread before;
    before.Add("Courte.", 3);
    static_cast<void>(layout.Update(before.Views(), kStyle, kNarrow));

    Thread after;
    after.Add("Courte.", 3);
    after.Add(std::string(kLong), 3);
    const float height = layout.Update(after.Views(), kStyle, kNarrow);
    CHECK(layout.measured() == 1);
    CHECK(height == ro::ui::ChatLogHeight(after.Views(), kStyle, kNarrow));
  }
  SECTION("en recul") {
    ro::ui::ChatLogLayout layout;
    Thread before;
    before.Add("Courte.", 3);
    static_cast<void>(layout.Update(before.Views(), kStyle, kNarrow));

    Thread after;
    after.Add("Une première ligne.", 5);
    after.Add(std::string(kLong), 3);
    const float height = layout.Update(after.Views(), kStyle, kNarrow);
    CHECK(layout.measured() == 2);
    CHECK(height == ro::ui::ChatLogHeight(after.Views(), kStyle, kNarrow));
  }
  ui.EndFrame();
}

TEST_CASE("Fil : une ligne sans identité se remet en page à chaque image", "[ui][chatlog]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::ChatLogLayout layout;
  Thread thread = SomeThread(5);
  thread.Add("Le repère entre deux parties.", 0);

  static_cast<void>(layout.Update(thread.Views(), kStyle, kNarrow));
  static_cast<void>(layout.Update(thread.Views(), kStyle, kNarrow));
  CHECK(layout.measured() == 1);
  ui.EndFrame();
}

namespace {

constexpr float kPicturePixels = 48.0F;
constexpr std::string_view kRichText = "**gras** puis une image et encore des mots";
constexpr std::uint32_t kBoldLength = 4;
constexpr ImTextureID kIconTexture = 0xC0DE;
// Où l'icône et la vignette s'insèrent : après « puis », après « image ».
constexpr std::uint32_t kIconAt = 14;
constexpr std::uint32_t kPictureAt = 25;
constexpr std::uint32_t kIconItem = 501;
constexpr int kManyIconLines = 2000;

}  // namespace

TEST_CASE("Fil : le cache rend la hauteur du moteur, styles et images compris", "[ui][chatlog]") {
  // 🔴 LA MESURE ET LE DESSIN PASSENT PAR LA MÊME POLICE : un gras simulé qui
  // élargirait le dessin sans élargir la mesure ferait mentir le cache.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::array<ro::ui::ChatStyleRun, 1> bold{
      ro::ui::ChatStyleRun{.offset = 0, .length = kBoldLength, .style = ro::ui::kChatBold}};
  std::array<ro::ui::ChatInlineImage, 2> images{
      ro::ui::ChatInlineImage{.offset = kIconAt,
                              .kind = ro::ui::ChatInlineKind::kItemIcon,
                              .item_id = kIconItem,
                              .texture = kIconTexture},
      ro::ui::ChatInlineImage{.offset = kPictureAt,
                              .kind = ro::ui::ChatInlineKind::kPicture,
                              .state = ro::ui::ChatPictureState::kPending}};
  ro::ui::ChatRichArt rich{.picture_pixels = kPicturePixels};
  ro::ui::ChatLogStyle style = kStyle;
  style.rich = &rich;
  Thread thread = SomeThread(10);
  thread.Add(std::string(kRichText), 11);
  std::vector<ro::ui::ChatLineView>& views = thread.Views();
  views.back().styles = bold;
  views.back().images = images;

  ro::ui::ChatLogLayout layout;
  const float height = layout.Update(views, style, kNarrow);
  CHECK(layout.measured() == 11);
  CHECK(height == ro::ui::ChatLogHeight(views, style, kNarrow));
  CHECK(height > ro::ui::ChatLogHeight(views, kStyle, kNarrow));

  // RENDU DEPUIS LE CACHE : rien de remesuré, la même hauteur.
  CHECK(layout.Update(views, style, kNarrow) == height);
  CHECK(layout.measured() == 0);

  // Les vignettes éteintes : la clé change, tout se remesure.
  rich.picture_pixels = 0.0F;
  const float without = layout.Update(views, style, kNarrow);
  CHECK(layout.measured() == 11);
  CHECK(without == ro::ui::ChatLogHeight(views, style, kNarrow));
  CHECK(without < height);
  ui.EndFrame();
}

TEST_CASE("Fil : une ligne à vignette suit l'état de son image, et lui seul", "[ui][chatlog]") {
  // ⚠ L'IDENTITÉ NE CHANGE PAS QUAND L'IMAGE ARRIVE OU ÉCHOUE : sans la copie
  // de ses images, la ligne garderait la place du carré d'attente.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  std::array<ro::ui::ChatInlineImage, 1> images{
      ro::ui::ChatInlineImage{.offset = 0,
                              .kind = ro::ui::ChatInlineKind::kPicture,
                              .state = ro::ui::ChatPictureState::kPending}};
  const ro::ui::ChatRichArt rich{.picture_pixels = kPicturePixels};
  ro::ui::ChatLogStyle style = kStyle;
  style.rich = &rich;
  Thread thread = SomeThread(3);
  thread.Add("vignette", 4);
  std::vector<ro::ui::ChatLineView>& views = thread.Views();
  views.back().images = images;

  ro::ui::ChatLogLayout layout;
  const float waiting = layout.Update(views, style, kWide);
  static_cast<void>(layout.Update(views, style, kWide));
  CHECK(layout.measured() == 0);

  // Prête, et deux fois plus large que haute : elle se remesure seule.
  constexpr float kWideRatio = 2.0F;
  images[0].state = ro::ui::ChatPictureState::kReady;
  images[0].texture = kIconTexture;
  images[0].width_px = kPicturePixels * kWideRatio;
  images[0].height_px = kPicturePixels;
  const float ready = layout.Update(views, style, kWide);
  CHECK(layout.measured() == 1);
  CHECK(ready == ro::ui::ChatLogHeight(views, style, kWide));

  images[0].state = ro::ui::ChatPictureState::kFailed;
  const float failed = layout.Update(views, style, kWide);
  CHECK(layout.measured() == 1);
  CHECK(failed < waiting);
  CHECK(failed == ro::ui::ChatLogHeight(views, style, kWide));
  ui.EndFrame();
}

TEST_CASE("Fil : deux mille lignes à icône ne se remesurent pas", "[ui][chatlog][mesure]") {
  // 🔴 LE COÛT PAR IMAGE : une ligne à image qui se remesurerait d'office ferait
  // payer chaque image pour tout un fil de liens d'objets.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::array<ro::ui::ChatInlineImage, 1> icon{
      ro::ui::ChatInlineImage{.offset = 0,
                              .kind = ro::ui::ChatInlineKind::kItemIcon,
                              .item_id = kIconItem,
                              .texture = kIconTexture}};
  std::array<ro::ui::ChatInlineImage, 1> picture{
      ro::ui::ChatInlineImage{.offset = 0,
                              .kind = ro::ui::ChatInlineKind::kPicture,
                              .state = ro::ui::ChatPictureState::kPending}};
  const ro::ui::ChatRichArt rich{.picture_pixels = kPicturePixels};
  ro::ui::ChatLogStyle style = kStyle;
  style.rich = &rich;
  Thread thread = SomeThread(kManyIconLines);
  std::vector<ro::ui::ChatLineView>& views = thread.Views();
  for (ro::ui::ChatLineView& view : views) {
    view.images = icon;
  }
  views.back().images = picture;

  ro::ui::ChatLogLayout layout;
  static_cast<void>(layout.Update(views, style, kNarrow));
  CHECK(layout.measured() == static_cast<std::size_t>(kManyIconLines));
  static_cast<void>(layout.Update(views, style, kNarrow));
  CHECK(layout.measured() == 0);

  picture[0].state = ro::ui::ChatPictureState::kReady;
  picture[0].texture = kIconTexture;
  static_cast<void>(layout.Update(views, style, kNarrow));
  CHECK(layout.measured() == 1);
  ui.EndFrame();
}
