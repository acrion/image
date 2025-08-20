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

#include <cmath>
#include <functional>

namespace acrion::image
{
    template <typename T>
    class MixableScalar
    {
    public:
        explicit MixableScalar(T val)
            : _val(val) {}
        operator T() const { return _val; } // NOLINT(google-explicit-constructor)

        MixableScalar<T> Mix(const std::vector<std::tuple<double, MixableScalar<T>>>& mixableScalars) const
        {
            double sum  = 0.0;
            double sumW = 0.0;

            for (const auto& current : mixableScalars)
            {
                const double         weight = std::get<0>(current);
                const MixableScalar& v      = std::get<1>(current);
                sumW += weight;
                sum += weight * (double)v;
            }

            const double weight = std::max(0.0, std::min(1.0, 1 - sumW));

            return MixableScalar<T>(static_cast<T>(std::llround((weight * _val + sum))));
        }

    private:
        T _val;
    };
}
