#include "ro/ui/model_library_index.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

#include "ro/core/ascii.h"

namespace ro::ui {
namespace {

constexpr char kFolderSeparator = '\\';
constexpr char kWordSeparator = ' ';
constexpr char kExtensionMark = '.';

}  // namespace

ModelLibraryTree BuildModelLibraryTree(std::span<const std::string> files) {
  ModelLibraryTree tree;
  tree.folders.emplace_back();
  // Le rang de chaque dossier par son chemin : un dossier se crée une fois.
  std::map<std::string, std::size_t, std::less<>> by_prefix;
  for (const std::string& file : files) {
    std::size_t parent = 0;
    ++tree.folders[parent].model_count;
    std::size_t start = 0;
    for (std::size_t cut = file.find(kFolderSeparator); cut != std::string::npos;
         cut = file.find(kFolderSeparator, start)) {
      const std::string prefix = file.substr(0, cut + 1);
      const std::string_view name = std::string_view(file).substr(start, cut - start);
      start = cut + 1;
      if (name.empty()) {
        continue;
      }
      auto known = by_prefix.find(prefix);
      if (known == by_prefix.end()) {
        const std::size_t made = tree.folders.size();
        tree.folders.push_back(ModelFolder{.name = std::string(name), .prefix = prefix});
        tree.folders[parent].children.push_back(made);
        known = by_prefix.emplace(prefix, made).first;
      }
      parent = known->second;
      ++tree.folders[parent].model_count;
    }
  }
  for (ModelFolder& folder : tree.folders) {
    std::ranges::sort(folder.children, [&tree](std::size_t left, std::size_t right) {
      return core::AsciiLess(tree.folders[left].name, tree.folders[right].name);
    });
  }
  return tree;
}

bool ModelPassesFilter(std::string_view file, std::string_view filter) {
  std::size_t start = 0;
  while (start < filter.size()) {
    std::size_t end = filter.find(kWordSeparator, start);
    if (end == std::string_view::npos) {
      end = filter.size();
    }
    const std::string_view word = filter.substr(start, end - start);
    if (!word.empty() && !core::AsciiContainsFolded(file, word)) {
      return false;
    }
    start = end + 1;
  }
  return true;
}

std::vector<std::size_t> FilterModelFiles(std::span<const std::string> files,
                                          std::string_view folder_prefix, std::string_view filter) {
  std::vector<std::size_t> shown;
  for (std::size_t rank = 0; rank < files.size(); ++rank) {
    if (core::AsciiStartsWithFolded(files[rank], folder_prefix) &&
        ModelPassesFilter(files[rank], filter)) {
      shown.push_back(rank);
    }
  }
  return shown;
}

int GridColumns(float width_px, float cell_px, float gap_px) {
  const float pitch = cell_px + gap_px;
  if (pitch <= 0.0F) {
    return 1;
  }
  // La dernière colonne n'a pas besoin de son écart à droite.
  const auto columns = static_cast<int>(std::floor((width_px + gap_px) / pitch));
  return std::max(columns, 1);
}

GridSpan VisibleGridCells(std::size_t count, int columns, float row_px, float scroll_px,
                          float view_px) {
  if (count == 0 || columns <= 0 || row_px <= 0.0F || view_px <= 0.0F) {
    return {};
  }
  const auto width = static_cast<std::size_t>(columns);
  const std::size_t rows = (count + width - 1) / width;
  const float top = std::max(scroll_px, 0.0F);
  const auto first_row = static_cast<std::size_t>(std::floor(top / row_px));
  const auto end_row = static_cast<std::size_t>(std::ceil((top + view_px) / row_px));
  const std::size_t first = std::min(first_row, rows) * width;
  const std::size_t end = std::min(std::min(end_row, rows) * width, count);
  return GridSpan{.first = std::min(first, end), .end = end};
}

std::vector<std::size_t> VignetteRequestOrder(GridSpan visible, std::size_t count, int columns,
                                              int ahead_rows, int behind_rows, std::size_t most) {
  std::vector<std::size_t> order;
  const auto add = [&](std::size_t cell) {
    if (order.size() < most && cell < count) {
      order.push_back(cell);
    }
  };
  for (std::size_t cell = visible.first; cell < visible.end; ++cell) {
    add(cell);
  }
  const auto width = static_cast<std::size_t>(std::max(columns, 1));
  const std::size_t ahead_end =
      std::min(count, visible.end + width * static_cast<std::size_t>(std::max(ahead_rows, 0)));
  for (std::size_t cell = visible.end; cell < ahead_end; ++cell) {
    add(cell);
  }
  const std::size_t behind = width * static_cast<std::size_t>(std::max(behind_rows, 0));
  const std::size_t behind_first = visible.first > behind ? visible.first - behind : 0;
  // Au-dessus, la rangée la plus proche d'abord.
  for (std::size_t cell = visible.first; cell > behind_first; --cell) {
    add(cell - 1);
  }
  return order;
}

std::string_view ModelStem(std::string_view file) {
  const std::size_t cut = file.find_last_of(kFolderSeparator);
  std::string_view stem = cut == std::string_view::npos ? file : file.substr(cut + 1);
  if (const std::size_t mark = stem.find_last_of(kExtensionMark);
      mark != std::string_view::npos && mark > 0) {
    stem = stem.substr(0, mark);
  }
  return stem;
}

}  // namespace ro::ui
