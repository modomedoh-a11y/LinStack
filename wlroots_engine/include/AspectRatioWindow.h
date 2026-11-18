#ifndef ASPECT_RATIO_WINDOW_H
#define ASPECT_RATIO_WINDOW_H

#include "WaylandSurfaceRenderer.h"

class AspectRatioWindow {
public:
    AspectRatioWindow(int targetWidth = 1920, int targetHeight = 1080);
    ~AspectRatioWindow();

    void setTargetResolution(int width, int height);
    int getTargetWidth() const { return m_targetWidth; }
    int getTargetHeight() const { return m_targetHeight; }
    float getAspectRatio() const { return m_aspectRatio; }

    /**
     * @brief Computes the centered viewport rectangle inside the given window size
     * preserving the strict aspect ratio (handling letterbox/pillarbox).
     */
    ViewportRect computeViewport(int windowWidth, int windowHeight) const;

    /**
     * @brief Adjusts a requested resize dimension to strictly preserve the aspect ratio.
     * @param inOutWidth Input/output width in pixels.
     * @param inOutHeight Input/output height in pixels.
     * @param anchorWidth If true, width is kept and height is snapped; else height is kept.
     */
    void snapSizeToAspectRatio(int &inOutWidth, int &inOutHeight, bool anchorWidth = true) const;

private:
    int m_targetWidth = 1920;
    int m_targetHeight = 1080;
    float m_aspectRatio = 1.777778f;
};

#endif // ASPECT_RATIO_WINDOW_H
