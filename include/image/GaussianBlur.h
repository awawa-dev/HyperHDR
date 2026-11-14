// NOUVEAU FICHIER : include/image/GaussianBlur.h
// Flou gaussien séparable haute performance pour HyperHDR
// Par 2 passes 1D (horizontal puis vertical) => O(n * kSize) au lieu de O(n * kSize²)

#pragma once

#include <image/Image.h>
#include <image/ColorRgb.h>
#include <cmath>
#include <vector>
#include <algorithm>

namespace GaussianBlur
{
    // Construit un noyau gaussien 1D normalisé de rayon `radius`
    inline std::vector<float> buildKernel(int radius)
    {
        if (radius <= 0) return {};
        const int size = 2 * radius + 1;
        std::vector<float> kernel(size);
        const float sigma = radius / 2.5f;
        float sum = 0.0f;
        for (int i = -radius; i <= radius; ++i)
        {
            float val = std::exp(-(static_cast<float>(i * i)) / (2.0f * sigma * sigma));
            kernel[i + radius] = val;
            sum += val;
        }
        for (auto& v : kernel) v /= sum;
        return kernel;
    }

    // Applique un flou gaussien séparable sur l'image (in-place)
    // radius = 0 : désactivé / aucune modification
    // radius = 1 : léger (kernel 3x3)
    // radius = 5 : fort (kernel 11x11)
    inline void apply(Image<ColorRgb>& image, int radius)
    {
        if (radius <= 0) return;

        const int width  = static_cast<int>(image.width());
        const int height = static_cast<int>(image.height());

        if (width < 2 || height < 2) return;

        const auto kernel = buildKernel(radius);

        // Buffer temporaire pour stocker le résultat de la passe horizontale
        Image<ColorRgb> tmp(width, height);

        // --- Passe horizontale ---
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float r = 0.0f, g = 0.0f, b = 0.0f;
                for (int k = -radius; k <= radius; ++k)
                {
                    const int sx = std::clamp(x + k, 0, width - 1);
                    const ColorRgb& px = image(sx, y);
                    const float w = kernel[k + radius];
                    r += px.red   * w;
                    g += px.green * w;
                    b += px.blue  * w;
                }
                tmp(x, y) = ColorRgb{
                    static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f))
                };
            }
        }

        // --- Passe verticale (sur tmp -> image) ---
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float r = 0.0f, g = 0.0f, b = 0.0f;
                for (int k = -radius; k <= radius; ++k)
                {
                    const int sy = std::clamp(y + k, 0, height - 1);
                    const ColorRgb& px = tmp(x, sy);
                    const float w = kernel[k + radius];
                    r += px.red   * w;
                    g += px.green * w;
                    b += px.blue  * w;
                }
                image(x, y) = ColorRgb{
                    static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f)),
                    static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f))
                };
            }
        }
    }

} // namespace GaussianBlur
