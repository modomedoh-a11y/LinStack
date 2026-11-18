#include "AspectRatioWindow.h"
#include <algorithm>
#include <cmath>

AspectRatioWindow::AspectRatioWindow(int targetWidth, int targetHeight) {
    setTargetResolution(targetWidth, targetHeight);
}

AspectRatioWindow::~AspectRatioWindow() = default;

void AspectRatioWindow::setTargetResolution(int width, int height) {
    m_targetWidth = std::max(1, width);
    m_targetHeight = std::max(1, height);
    m_aspectRatio = static_cast<float>(m_targetWidth) / static_cast<float>(m_targetHeight);
}

ViewportRect AspectRatioWindow::computeViewport(int windowWidth, int windowHeight) const {
    if (windowWidth <= 0 || windowHeight <= 0) {
        return ViewportRect{0, 0, m_targetWidth, m_targetHeight};
    }

    int vpW = 0;
    int vpH = 0;

    // Compare aspect ratio of window against target aspect ratio
    if (windowWidth * m_targetHeight > windowHeight * m_targetWidth) {
        // Window is wider than target ratio -> Pillarbox (vertical bars on sides)
        vpH = windowHeight;
        vpW = static_cast<int>(std::round(vpH * (static_cast<float>(m_targetWidth) / m_targetHeight)));
    } else {
        // Window is taller than target ratio -> Letterbox (horizontal bars top & bottom)
        vpW = windowWidth;
        vpH = static_cast<int>(std::round(vpW * (static_cast<float>(m_targetHeight) / m_targetWidth)));
    }

    int vpX = (windowWidth - vpW) / 2;
    int vpY = (windowHeight - vpH) / 2;

    return ViewportRect{vpX, vpY, vpW, vpH};
}

void AspectRatioWindow::snapSizeToAspectRatio(int &inOutWidth, int &inOutHeight, bool anchorWidth) const {
    if (anchorWidth) {
        inOutHeight = static_cast<int>(std::round(inOutWidth / m_aspectRatio));
    } else {
        inOutWidth = static_cast<int>(std::round(inOutHeight * m_aspectRatio));
    }
}
