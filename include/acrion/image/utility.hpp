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
    template <typename T>
    T BoundedAdd(const T& a, long double b)
    {
        if (b >= (long double)std::numeric_limits<T>::max() || (long double)a > std::numeric_limits<T>::max() - b)
        {
            return std::numeric_limits<T>::max();
        }

        if (b <= -(long double)std::numeric_limits<T>::max() || (long double)a < std::numeric_limits<T>::lowest() - b)
        {
            return std::numeric_limits<T>::lowest();
        }

        return static_cast<T>(static_cast<int64_t>(a) + b);
    }

    template <typename T>
    T BoundedSub(const T& a, long double b)
    {
        return BoundedAdd(a, -b);
    }

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
}
