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
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace acrion::image
{
    class Vector
    {
    public:
        explicit Vector(const double phi, const double len = 1.0)
            : _v(std::make_pair(
                  std::cos(phi) * len,
                  std::sin(phi) * len))
            , _len(len)
        {
        }
        explicit Vector(const std::pair<double, double>& v)
            : _v(v)
        {
        }
        Vector()          = default;
        virtual ~Vector() = default;

        bool IsValid() const
        {
            return _v.first != INVALID && _v.second != INVALID;
        }

        double Phi() const
        {
            if (_phi == INVALID)
            {
                _phi = std::fmod(std::atan2(Vy(), Vx()) + 2 * M_PI, 2 * M_PI);
            }
            return _phi;
        }
        double Len() const
        {
            if (_len == INVALID)
            {
                _len = std::sqrt(Vx() * Vx() + Vy() * Vy());
            }
            return _len;
        }
        double                    Vx() const { return _v.first; }
        double                    Vy() const { return _v.second; };
        std::pair<double, double> V() { return _v; }
        Vector                    Mix(const std::vector<std::tuple<double, Vector>>& vectors) const
        {
            double sumW  = 0;
            double sumVx = 0;
            double sumVy = 0;

            for (const auto& current : vectors)
            {
                const double  weight = std::get<0>(current);
                const Vector& vector = std::get<1>(current);
                sumW += weight;
                sumVx += weight * vector.Vx();
                sumVy += weight * vector.Vy();
            }

            const double weight = std::max(0.0, std::min(1.0, 1 - sumW));

            return Vector(std::make_pair(
                weight * Vx() + sumVx,
                weight * Vy() + sumVy));
        }

        Vector& operator*=(double rhs) // compound assignment (does not need to be a member,
        {                              // but often is, to modify the private members)
            *this = Vector(std::make_pair(Vx() * rhs, Vy() * rhs));
            return *this; // return the result by reference
        }

        Vector& operator/=(double rhs) // compound assignment (does not need to be a member,
        {                              // but often is, to modify the private members)
            *this = Vector(std::make_pair(Vx() / rhs, Vy() / rhs));
            return *this; // return the result by reference
        }

        Vector& operator+=(double rhs) // compound assignment (does not need to be a member,
        {                              // but often is, to modify the private members)
            *this = Vector(std::fmod(Phi() + rhs + 2 * M_PI, 2 * M_PI), Len());
            return *this; // return the result by reference
        }

        Vector& operator-=(double rhs) // compound assignment (does not need to be a member,
        {                              // but often is, to modify the private members)
            *this = Vector(std::fmod(Phi() - rhs + 2 * M_PI, 2 * M_PI), Len());

            return *this; // return the result by reference
        }

        Vector& operator+=(const Vector& rhs) // compound assignment (does not need to be a member,
        {                                     // but often is, to modify the private members)
            *this = Vector(std::make_pair(Vx() + rhs.Vx(), Vy() + rhs.Vy()));
            return *this; // return the result by reference
        }

        Vector& operator-=(const Vector& rhs) // compound assignment (does not need to be a member,
        {                                     // but often is, to modify the private members)
            *this = Vector(std::make_pair(Vx() - rhs.Vx(), Vy() - rhs.Vy()));
            return *this; // return the result by reference
        }

    private:
        static constexpr double   INVALID{std::numeric_limits<double>::min()};
        std::pair<double, double> _v{std::make_pair(INVALID, INVALID)};
        mutable double            _phi{INVALID};
        mutable double            _len{INVALID};
    };

    inline Vector operator*(Vector lhs, const double& rhs)
    {
        lhs *= rhs;
        return lhs;
    }

    inline Vector operator/(Vector lhs, const double& rhs)
    {
        lhs /= rhs;
        return lhs;
    }

    inline bool operator==(const Vector& lhs, const Vector& rhs)
    {
        return lhs.Vx() == rhs.Vx()
            && lhs.Vy() == rhs.Vy();
    }

    inline bool operator!=(const Vector& lhs, const Vector& rhs)
    {
        return !operator==(lhs, rhs);
    }

    inline bool operator<(const Vector& lhs, const Vector& rhs)
    {
        return lhs.Len() < rhs.Len();
    }

    inline bool operator>(const Vector& lhs, const Vector& rhs)
    {
        return operator<(rhs, lhs);
    }

    inline bool operator<=(const Vector& lhs, const Vector& rhs)
    {
        return !operator<(rhs, lhs);
    }

    inline bool operator>=(const Vector& lhs, const Vector& rhs)
    {
        return !operator<(lhs, rhs);
    }

    inline Vector operator+(Vector lhs, const double& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    inline Vector operator-(Vector lhs, const double& rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    inline Vector operator+(Vector lhs, const Vector& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    inline Vector operator-(Vector lhs, const Vector& rhs)
    {
        lhs -= rhs;
        return lhs;
    }
}
