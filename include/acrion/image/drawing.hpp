/*
Copyright (c) 2026 acrion innovations GmbH
Authors: Stefan Zipproth, s.zipproth@acrion.ch

This file is part of acrion image, see https://github.com/acrion/image

acrion image is offered under a commercial and under the AGPL license.
For commercial licensing, contact us at https://acrion.ch/sales. For AGPL licensing, see below.

AGPL licensing:

acrion image is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

acrion image is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with acrion image. If not, see <https://www.gnu.org/licenses/>.
*/

/// \file
/// \brief Shape drawing on a BitmapData of any pixel type.
///
/// \details BitmapData has had Bresenham line drawing and Plot since the beginning; what was
/// missing is everything a caller needs to draw something that is not a line - and, more
/// importantly, a way to reach any of it from a plugin. These functions are what "acrion
/// image tools" exposes as messages, and what a Lua plugin therefore draws with.
///
/// Three decisions run through all of them:
///
/// - **Every function is stateless.** There is no "current colour", no "current clip
///   rectangle", nothing that a second call inherits from a first. That is not tidiness: an
///   agent may be replicated, each replica runs the script in its own `lua_State`, and state
///   accumulated in one of them is invisible to the others. A stateful `SetClipRect()` would
///   work perfectly until a plugin was replicated and then produce partial results.
/// - **Clipping is a parameter, for the same reason.** Rect{} - the default - means the whole
///   image. A widget passes its own bounds and cannot draw outside them.
/// - **Coordinates are integers and may lie outside the image.** Clipping is done per pixel
///   rather than by rejecting the shape, so a rectangle half off the canvas draws its visible
///   half instead of nothing.
///
/// The colour is a Color<T>, i.e. already in the units of the pixel type. Translating a
/// convenient [0, 1] value into that is the message layer's job, because only there is the
/// displayed brightness range of the image known.

#pragma once

#include "bitmap_data.hpp"
#include "color.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

namespace acrion::image::drawing
{
    /// \brief A rectangle - both the clipping region of a call and the region it dirtied.
    ///
    /// A width or height of zero or less means "empty" everywhere except as a clip, where it
    /// means "the whole image": a caller that does not want to clip should not have to know
    /// the size of the image to say so.
    struct Rect
    {
        int x{0};
        int y{0};
        int w{0};
        int h{0};

        bool IsEmpty() const { return w <= 0 || h <= 0; }

        int Right() const { return x + w - 1; }
        int Bottom() const { return y + h - 1; }

        /// \brief The part of this rectangle that is also in \p other. Empty if they do not
        ///        overlap, which is the answer a caller wants: nothing was touched.
        Rect IntersectedWith(const Rect& other) const
        {
            if (IsEmpty() || other.IsEmpty())
            {
                return {};
            }

            const int left   = std::max(x, other.x);
            const int top    = std::max(y, other.y);
            const int right  = std::min(Right(), other.Right());
            const int bottom = std::min(Bottom(), other.Bottom());

            if (right < left || bottom < top)
            {
                return {};
            }

            return Rect{left, top, right - left + 1, bottom - top + 1};
        }

        /// \brief The smallest rectangle containing both. An empty one contributes nothing,
        ///        so accumulating from Rect{} works without a "first time" special case.
        Rect UnitedWith(const Rect& other) const
        {
            if (IsEmpty())
            {
                return other;
            }

            if (other.IsEmpty())
            {
                return *this;
            }

            const int left   = std::min(x, other.x);
            const int top    = std::min(y, other.y);
            const int right  = std::max(Right(), other.Right());
            const int bottom = std::max(Bottom(), other.Bottom());

            return Rect{left, top, right - left + 1, bottom - top + 1};
        }
    };

    /// \brief A point, in pixels.
    struct Point
    {
        int x{0};
        int y{0};
    };

    namespace detail
    {
        /// \brief Plot one pixel, unless \p clip excludes it. BitmapData::Plot already
        ///        refuses coordinates outside the image, so this only adds the clip test.
        template <typename T>
        void PlotClipped(const BitmapData<T>& data, const int x, const int y, const Color<T>& color, const Rect& clip)
        {
            if (!clip.IsEmpty()
                && (x < clip.x || y < clip.y || x >= clip.x + clip.w || y >= clip.y + clip.h))
            {
                return;
            }

            data.Plot(x, y, color);
        }

        /// \brief A horizontal run of pixels - what every filled shape is made of.
        template <typename T>
        void HorizontalSpan(const BitmapData<T>& data, int xFrom, int xTo, const int y, const Color<T>& color, const Rect& clip)
        {
            if (xFrom > xTo)
            {
                std::swap(xFrom, xTo);
            }

            for (int x = xFrom; x <= xTo; ++x)
            {
                PlotClipped(data, x, y, color, clip);
            }
        }

        /// \brief A filled disc, used as the pen for a line thicker than one pixel.
        ///
        /// A disc rather than a square, so that a polyline has round joins and a diagonal
        /// line is as thick as a horizontal one.
        template <typename T>
        void Disc(const BitmapData<T>& data, const int cx, const int cy, const int radius, const Color<T>& color, const Rect& clip)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                const int dx = (int)std::floor(std::sqrt((double)radius * radius - (double)dy * dy));
                HorizontalSpan(data, cx - dx, cx + dx, cy + dy, color, clip);
            }
        }
    }

    /// \brief A straight line from (\p x0, \p y0) to (\p x1, \p y1), \p width pixels thick.
    ///
    /// \param width 1 draws single pixels through BitmapData's own Bresenham walk; anything
    ///              larger stamps a disc at every point of it.
    template <typename T>
    void Line(const BitmapData<T>& data,
              const int            x0,
              const int            y0,
              const int            x1,
              const int            y1,
              const Color<T>&      color,
              const int            width = 1,
              const Rect&          clip  = {})
    {
        const int radius = (width - 1) / 2;

        // BitmapData::Draw walks the line and calls this back for every point of it, so the
        // stepping is not reimplemented here - only what to leave behind at each point.
        data.Draw(x0, y0, x1, y1, [&](const int x, const int y)
                  {
                      if (radius <= 0)
                      {
                          detail::PlotClipped(data, x, y, color, clip);
                      }
                      else
                      {
                          detail::Disc(data, x, y, radius, color, clip);
                      }
                      return true;
                  });
    }

    /// \brief An axis-aligned rectangle, outlined or filled.
    ///
    /// \param w,h Zero or negative draws nothing. The rectangle covers the pixels from
    ///            (\p x, \p y) to (\p x + \p w - 1, \p y + \p h - 1), so a width of 1 is one
    ///            pixel wide - not zero, and not two.
    template <typename T>
    void Rectangle(const BitmapData<T>& data,
                   const int            x,
                   const int            y,
                   const int            w,
                   const int            h,
                   const Color<T>&      color,
                   const bool           filled     = false,
                   const int            lineWidth  = 1,
                   const Rect&          clip       = {})
    {
        if (w <= 0 || h <= 0)
        {
            return;
        }

        const int right  = x + w - 1;
        const int bottom = y + h - 1;

        if (filled)
        {
            for (int row = y; row <= bottom; ++row)
            {
                detail::HorizontalSpan(data, x, right, row, color, clip);
            }
            return;
        }

        Line(data, x, y, right, y, color, lineWidth, clip);
        Line(data, x, bottom, right, bottom, color, lineWidth, clip);
        Line(data, x, y, x, bottom, color, lineWidth, clip);
        Line(data, right, y, right, bottom, color, lineWidth, clip);
    }

    /// \brief An axis-aligned ellipse around (\p cx, \p cy), outlined or filled.
    ///
    /// \details The midpoint algorithm, which keeps everything in integers and is symmetric
    /// by construction - it computes one quadrant and mirrors it, so an ellipse cannot come
    /// out lopsided the way a naive sampling of the parametric form does.
    template <typename T>
    void Ellipse(const BitmapData<T>& data,
                 const int            cx,
                 const int            cy,
                 const int            rx,
                 const int            ry,
                 const Color<T>&      color,
                 const bool           filled = false,
                 const Rect&          clip   = {})
    {
        if (rx < 0 || ry < 0)
        {
            return;
        }

        if (rx == 0 || ry == 0)
        {
            // Degenerate, but a caller animating a radius down to zero should get the line
            // it asks for rather than nothing.
            Line(data, cx - rx, cy - ry, cx + rx, cy + ry, color, 1, clip);
            return;
        }

        const auto emit = [&](const int x, const int y)
        {
            if (filled)
            {
                detail::HorizontalSpan(data, cx - x, cx + x, cy + y, color, clip);
                detail::HorizontalSpan(data, cx - x, cx + x, cy - y, color, clip);
            }
            else
            {
                detail::PlotClipped(data, cx + x, cy + y, color, clip);
                detail::PlotClipped(data, cx - x, cy + y, color, clip);
                detail::PlotClipped(data, cx + x, cy - y, color, clip);
                detail::PlotClipped(data, cx - x, cy - y, color, clip);
            }
        };

        const long long rx2 = (long long)rx * rx;
        const long long ry2 = (long long)ry * ry;

        long long x = 0;
        long long y = ry;

        // Region 1: the part where the curve is more horizontal than vertical.
        long long sigma = 2 * ry2 + rx2 * (1 - 2 * ry);
        while (ry2 * x <= rx2 * y)
        {
            emit((int)x, (int)y);
            if (sigma >= 0)
            {
                sigma += 4 * rx2 * (1 - y);
                --y;
            }
            sigma += ry2 * (4 * x + 6);
            ++x;
        }

        // Region 2: the remainder, stepped along the other axis.
        x     = rx;
        y     = 0;
        sigma = 2 * rx2 + ry2 * (1 - 2 * rx);
        while (rx2 * y <= ry2 * x)
        {
            emit((int)x, (int)y);
            if (sigma >= 0)
            {
                sigma += 4 * ry2 * (1 - x);
                --x;
            }
            sigma += rx2 * (4 * y + 6);
            ++y;
        }
    }

    /// \brief A sequence of connected line segments; \p closed joins the last point to the first.
    template <typename T>
    void Polyline(const BitmapData<T>&      data,
                  const std::vector<Point>& points,
                  const Color<T>&           color,
                  const bool                closed = false,
                  const int                 width  = 1,
                  const Rect&               clip   = {})
    {
        if (points.size() == 1)
        {
            detail::PlotClipped(data, points[0].x, points[0].y, color, clip);
            return;
        }

        for (size_t i = 0; i + 1 < points.size(); ++i)
        {
            Line(data, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, color, width, clip);
        }

        if (closed && points.size() > 2)
        {
            Line(data, points.back().x, points.back().y, points.front().x, points.front().y, color, width, clip);
        }
    }

    /// \brief A filled polygon, by the even-odd rule.
    ///
    /// \details For every scan line, the x coordinates where an edge crosses it are collected
    /// and sorted, and the spans between the first and second, third and fourth, and so on
    /// are filled. A horizontal edge contributes nothing, which is what stops a flat top or
    /// bottom from filling a whole row of the bounding box.
    ///
    /// The half-open comparison of the two endpoints is what makes a shared vertex count once
    /// rather than twice; getting that wrong is the classic way filled polygons grow a stray
    /// line at the height of every vertex.
    template <typename T>
    void FilledPolygon(const BitmapData<T>&      data,
                       const std::vector<Point>& points,
                       const Color<T>&           color,
                       const Rect&               clip = {})
    {
        if (points.size() < 3)
        {
            return;
        }

        int top    = points[0].y;
        int bottom = points[0].y;
        for (const Point& point : points)
        {
            top    = std::min(top, point.y);
            bottom = std::max(bottom, point.y);
        }

        top    = std::max(top, 0);
        bottom = std::min(bottom, data.Height() - 1);

        std::vector<int> crossings;

        for (int y = top; y <= bottom; ++y)
        {
            crossings.clear();

            for (size_t i = 0; i < points.size(); ++i)
            {
                const Point& a = points[i];
                const Point& b = points[(i + 1) % points.size()];

                if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y))
                {
                    const double t = (double)(y - a.y) / (double)(b.y - a.y);
                    crossings.push_back((int)std::lround(a.x + t * (b.x - a.x)));
                }
            }

            std::sort(crossings.begin(), crossings.end());

            for (size_t i = 0; i + 1 < crossings.size(); i += 2)
            {
                detail::HorizontalSpan(data, crossings[i], crossings[i + 1], y, color, clip);
            }
        }
    }

    /// \brief Fills the area around (\p x, \p y) whose brightness is within \p tolerance of
    ///        the brightness there.
    ///
    /// \details Four-connected and iterative. Recursion is the textbook formulation and is
    /// also how a flood fill takes a process down: on a large uniform image the depth is the
    /// number of pixels, and an astro frame has millions of them.
    ///
    /// \param tolerance in the units of the pixel type, so 0 fills only exactly equal pixels.
    /// \param bounds if given, receives the smallest rectangle containing every pixel that
    ///        was changed - a flood fill is the one shape whose extent cannot be predicted
    ///        from its arguments, so it has to be measured while filling.
    /// \return the number of pixels filled, which lets a caller tell "nothing to do" from
    ///         "filled the whole image" - the two outcomes a wrong tolerance produces.
    template <typename T>
    size_t FloodFill(const BitmapData<T>& data,
                     const int            x,
                     const int            y,
                     const Color<T>&      color,
                     const double         tolerance = 0.0,
                     const Rect&          clip      = {},
                     Rect*                bounds    = nullptr)
    {
        if (x < 0 || y < 0 || x >= data.Width() || y >= data.Height())
        {
            return 0;
        }

        const double  seed      = (double)data.GetGray(x, y);
        const T       newGray   = color.Gray();
        const bool    isNoOp    = std::abs((double)newGray - seed) <= tolerance;
        std::vector<std::pair<int, int>> pending{{x, y}};

        // Without this the fill never terminates: the pixels it writes still match the seed,
        // so they are visited again for ever.
        if (isNoOp)
        {
            return 0;
        }

        std::vector<bool> visited((size_t)data.Width() * data.Height(), false);
        size_t            filled = 0;

        while (!pending.empty())
        {
            const auto [px, py] = pending.back();
            pending.pop_back();

            if (px < 0 || py < 0 || px >= data.Width() || py >= data.Height())
            {
                continue;
            }

            const size_t index = (size_t)py * data.Width() + px;
            if (visited[index])
            {
                continue;
            }
            visited[index] = true;

            if (!clip.IsEmpty()
                && (px < clip.x || py < clip.y || px >= clip.x + clip.w || py >= clip.y + clip.h))
            {
                continue;
            }

            if (std::abs((double)data.GetGray(px, py) - seed) > tolerance)
            {
                continue;
            }

            data.Plot(px, py, color);
            ++filled;

            if (bounds)
            {
                *bounds = bounds->UnitedWith(Rect{px, py, 1, 1});
            }

            pending.push_back({px + 1, py});
            pending.push_back({px - 1, py});
            pending.push_back({px, py + 1});
            pending.push_back({px, py - 1});
        }

        return filled;
    }
}
