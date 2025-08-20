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

#include "utility.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

namespace acrion::image
{
    template <typename T>
    class Color
    {
    public:
        Color(const T red, const T green, const T blue, const T alpha = std::numeric_limits<T>::max())
            : _red(red)
            , _green(green)
            , _blue(blue)
            , _alpha(alpha)
        {
        }
        Color(const T gray, const T alpha = std::numeric_limits<T>::max()) // cppcheck-suppress noExplicitConstructor NOLINT(google-explicit-constructor)
            : _red(gray)
            , _green(gray)
            , _blue(gray)
            , _alpha(alpha)
        {
        }
        virtual ~Color() = default;

        T Red() const
        {
            return _red;
        }

        T Green() const
        {
            return _green;
        }

        T Blue() const
        {
            return _blue;
        }

        T Alpha() const
        {
            return _alpha;
        }

        T Gray() const
        {
            return _red == _green && _red == _blue ? _red
                                                   : static_cast<T>(std::llround(0.299 * _red + 0.587 * _green + 0.114 * _blue));
        }

        bool IsColored() const
        {
            return _red != _green || _red != _blue;
        }

        Color<T> WithBrightness(const T Y) const
        {
            if (IsColored())
            {
                const double U = -0.14713 * _red - 0.28886 * _green + 0.436 * _blue;
                const double V = 0.615 * _red - 0.51498 * _green - 0.10001 * _blue;

                const double red   = Y + 1.13983 * V;
                const double green = Y - 0.39465 * U - 0.58060 * V;
                const double blue  = Y + 2.03211 * U;

                return Color<T>(static_cast<T>(std::llround(std::min(std::max(red, 0.0), (double)std::numeric_limits<T>::max()))),
                                static_cast<T>(std::llround(std::min(std::max(green, 0.0), (double)std::numeric_limits<T>::max()))),
                                static_cast<T>(std::llround(std::min(std::max(blue, 0.0), (double)std::numeric_limits<T>::max()))),
                                _alpha);
            }
            else
            {
                return Color<T>(Y, _alpha);
            }
        }

        Color<T> Mix(const std::vector<std::tuple<double, Color<T>>>& colors) const
        {
            long double sumW = 0;
            long double sumR = 0;
            long double sumG = 0;
            long double sumB = 0;
            long double sumA = 0;

            for (const auto& current : colors)
            {
                const long double weight = std::get<0>(current);
                const Color&      color  = std::get<1>(current);
                sumW += weight;
                sumR += weight * color.Red();
                sumG += weight * color.Green();
                sumB += weight * color.Blue();
                sumA += weight * color.Alpha();
            }

            const long double weight = std::max(0.0l, std::min(1.0l, 1 - sumW));

            return Color<T>(utility::Convert<T>(weight * Red() + sumR),
                            utility::Convert<T>(weight * Green() + sumG),
                            utility::Convert<T>(weight * Blue() + sumB),
                            utility::Convert<T>(weight * Alpha() + sumA));
        }

        Color& operator+=(long double rhs) // compound assignment (does not need to be a member,
        {                                  // but often is, to modify the private members)
            _red   = utility::BoundedAdd(_red, rhs);
            _green = utility::BoundedAdd(_green, rhs);
            _blue  = utility::BoundedAdd(_blue, rhs);

            return *this; // return the result by reference
        }

        Color& operator-=(long double rhs) // compound assignment (does not need to be a member,
        {                                  // but often is, to modify the private members)
            _red   = utility::BoundedSub(_red, rhs);
            _green = utility::BoundedSub(_green, rhs);
            _blue  = utility::BoundedSub(_blue, rhs);

            return *this; // return the result by reference
        }

        Color& operator+=(Color rhs) // compound assignment (does not need to be a member,
        {                            // but often is, to modify the private members)
            _red += rhs._red;
            _green += rhs._green;
            _blue += rhs._blue;

            return *this; // return the result by reference
        }

        Color& operator-=(Color rhs) // compound assignment (does not need to be a member,
        {                            // but often is, to modify the private members)
            _red -= rhs._red;
            _green -= rhs._green;
            _blue -= rhs._blue;

            return *this; // return the result by reference
        }

        Color& operator*=(T rhs) // compound assignment (does not need to be a member,
        {                        // but often is, to modify the private members)
            const long double red   = static_cast<long double>(_red) * rhs;
            const long double green = static_cast<long double>(_green) * rhs;
            const long double blue  = static_cast<long double>(_blue) * rhs;
            const long double max   = (long double)std::numeric_limits<T>::max();

            _red   = static_cast<T>(std::min(max, red));
            _green = static_cast<T>(std::min(max, green));
            _blue  = static_cast<T>(std::min(max, blue));

            return *this; // return the result by reference
        }

        Color& operator/=(T rhs)
        {
            _red /= rhs;
            _green /= rhs;
            _blue /= rhs;

            return *this; // return the result by reference
        }

        // Color& operator*=(double rhs)
        // {
        //     const uint64_t red   = std::llround(_red * rhs);
        //     const uint64_t green = std::llround(_green * rhs);
        //     const uint64_t blue  = std::llround(_blue * rhs);
        //     const uint64_t max   = std::numeric_limits<T>::max();

        //     _red   = static_cast<T>(std::min(max, red));
        //     _green = static_cast<T>(std::min(max, green));
        //     _blue  = static_cast<T>(std::min(max, blue));

        //     return *this; // return the result by reference
        // }

        // Color& operator/=(double rhs)
        // {
        //     _red   = std::llround(_red / rhs);
        //     _green = std::llround(_green / rhs);
        //     _blue  = std::llround(_blue / rhs);

        //     return *this; // return the result by reference
        // }

    private:
        T _red{std::numeric_limits<T>::min()};
        T _green{std::numeric_limits<T>::min()};
        T _blue{std::numeric_limits<T>::min()};
        T _alpha{std::numeric_limits<T>::min()};
    };

    template <typename T, typename U>
    bool operator==(const Color<T>& lhs, const Color<U>& rhs)
    {
        return lhs.Red() == rhs.Red()
            && lhs.Green() == rhs.Green()
            && lhs.Blue() == rhs.Blue()
            && lhs.Alpha() == rhs.Alpha();
    }
    template <typename T, typename U>
    bool operator!=(const Color<T>& lhs, const Color<U>& rhs) { return !operator==(lhs, rhs); }

    template <typename T, typename U>
    bool operator<(const Color<T>& lhs, const Color<U>& rhs)
    {
        return lhs.Gray() < rhs.Gray();
    }

    template <typename T, typename U>
    bool operator>(const Color<T>& lhs, const Color<U>& rhs) { return operator<(rhs, lhs); }

    template <typename T, typename U>
    bool operator<=(const Color<T>& lhs, const Color<U>& rhs) { return !operator<(rhs, lhs); }

    template <typename T, typename U>
    bool operator>=(const Color<T>& lhs, const Color<U>& rhs) { return !operator<(lhs, rhs); }

    template <typename T>
    Color<T> operator+(Color<T>       lhs,
                       const int64_t& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    template <typename T>
    Color<T> operator-(Color<T>       lhs,
                       const int64_t& rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    template <typename T>
    Color<T> operator+(Color<T>        lhs,
                       const Color<T>& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    template <typename T>
    Color<T> operator-(Color<T>        lhs,
                       const Color<T>& rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    template <typename T>
    Color<T> operator*(Color<T> lhs,
                       const T& rhs)
    {
        lhs *= rhs;
        return lhs;
    }

    template <typename T>
    Color<T> operator/(Color<T> lhs,
                       const T& rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    // template <typename T>
    // Color<T> operator*(Color<T>      lhs,
    //                    const double& rhs)
    // {
    //     lhs *= rhs;
    //     return lhs;
    // }

    // template <typename T>
    // Color<T> operator/(Color<T>      lhs,
    //                    const double& rhs)
    // {
    //     lhs /= rhs;
    //     return lhs;
    // }
}
