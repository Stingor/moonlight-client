#include "ro/render/inset_paint.h"

#include <glad/glad.h>

namespace ro::render {

void BeginInsetPaint(const InsetRect& view, const InsetRect& clip) {
  glViewport(view.left, view.bottom, view.width, view.height);
  glEnable(GL_SCISSOR_TEST);
  glScissor(clip.left, clip.bottom, clip.width, clip.height);
  // ⚠ L'ÉCRITURE DE PROFONDEUR D'ABORD : un renderer passé avant l'a peut-être
  // coupée, et l'effacement la respecte.
  glDepthMask(GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void EndInsetPaint() {
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_DEPTH_TEST);
}

}  // namespace ro::render
