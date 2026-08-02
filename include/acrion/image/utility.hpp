/*
Copyright (c) 2025 acrion innovations GmbH
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

#pragma once

#define _USE_MATH_DEFINES
#ifdef _WIN32
    #include <math.h> // required for C++ to provide math constants, contradicting both C++ standard and https://docs.microsoft.com/en-us/cpp/c-runtime-library/math-constants?view=msvc-160
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>

namespace acrion::image
{
    template <typename T>
    class BitmapData;
}

namespace acrion::image::utility
{
    inline int msb(const uint8_t value)
    {
        return (int)(std::ceil(std::log2(value)));
    }

    inline uint8_t* CastToSmallerType(const uint16_t* val)
    {
        return (uint8_t*)val;
    }

    inline uint16_t* CastToSmallerType(const uint32_t* val)
    {
        return (uint16_t*)val;
    }

    inline uint32_t* CastToSmallerType(const uint64_t* val)
    {
        return (uint32_t*)val;
    }

    template <typename T>
    T Convert(const long double num);

    template <>
    inline double Convert(const long double num)
    {
        double result = (double)num;

        if (std::isinf(result) && !std::isinf(num))
        {
            result = num >= 0 ? std::numeric_limits<double>::max() : std::numeric_limits<double>::lowest();
        }

        return result;
    }

    template <>
    inline uint64_t Convert(const long double num)
    {
        return (uint64_t)(num + (long double)0.5);
    }

    template <typename T>
    T Convert(const long double num)
    {
        static_assert(std::numeric_limits<T>::is_integer);
        return (T)std::llround(num);
    }

    /// \brief Turns a computed value back into a pixel of type \p T.
    ///
    /// Integer pixels are clamped into the range of their type and rounded to the nearest
    /// whole number; floating point ones are neither, and that difference is the whole point
    /// of this function:
    ///
    /// - a floating point image has no type range to clamp against, and rounding one to
    ///   whole numbers leaves a FITS frame scaled to [0, 1] with two brightness levels;
    /// - its pixels may legitimately be negative, because subtracting a dark frame puts the
    ///   background below zero - clamping that at zero would raise it back to black.
    ///
    /// The comparison is made in `long double`, whose 64 bit mantissa holds the maximum of
    /// `uint64_t` exactly. `int64_t` does not: written that way the clamp for a 64 bit image
    /// wraps to -1, and for a floating point one it is undefined outright. Both of those
    /// were real defects (imago BUG-29), which is why this lives in one place now.
    template <typename T>
    T ToPixel(const long double value)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return Convert<T>(value);
        }
        else
        {
            if (value <= 0)
            {
                return T{0};
            }

            if (value >= (long double)std::numeric_limits<T>::max())
            {
                return std::numeric_limits<T>::max();
            }

            return Convert<T>(value);
        }
    }

    /// \brief \p a + \p b, saturating at the limits of \p T.
    ///
    /// \details The sum is formed in `long double`, whose 64 bit mantissa holds the maximum of
    /// `uint64_t` exactly, and converted back through Convert - which rounds an integer pixel
    /// and leaves a floating point one alone.
    ///
    /// It used to end in `static_cast<T>(static_cast<int64_t>(a) + b)`. That truncated the
    /// fractional part of a floating point pixel before adding anything to it, so
    /// `Color<double>(0.8, 0.3, 0.3) += 0.1` gave 0.1 rather than 0.9, and for `uint64_t` the
    /// cast wrapped. The same integer assumption as BUG-29 and BUG-31; unreachable here only
    /// because nothing calls Color's scalar operators today.
    template <typename T>
    T BoundedAdd(const T& a, long double b)
    {
        const long double sum = (long double)a + b;

        if (sum >= (long double)std::numeric_limits<T>::max())
        {
            return std::numeric_limits<T>::max();
        }

        if (sum <= (long double)std::numeric_limits<T>::lowest())
        {
            return std::numeric_limits<T>::lowest();
        }

        return Convert<T>(sum);
    }

    template <typename T>
    T BoundedSub(const T& a, long double b)
    {
        return BoundedAdd(a, -b);
    }
}
