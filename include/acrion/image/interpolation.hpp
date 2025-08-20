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

#include "mixable_scalar.hpp"

#include <cmath>
#include <functional>

namespace acrion::image::interpolation
{
    template <typename T>
    using Getter = std::function<T(const int x, const int y)>;

    template <typename T>
    T Do(const double dx, const double dy, const double min_x, const double min_y, const double max_x, const double max_y, Getter<T> Get)
    {
        T result(0);

        const int    ix = static_cast<int>(std::floor(std::max(min_x, std::min(max_x, dx))));
        const int    iy = static_cast<int>(std::floor(std::max(min_y, std::min(max_y, dy))));
        const double x  = dx - ix;
        const double y  = dy - iy;

        if (x <= 0.0 || ix + 1 > max_x)
        {
            if (y <= 0.0 || iy + 1 > max_y)
            {
                result = Get(ix, iy);
            }
            else // (x <= 0.0 || ix + 1 > max_x) && y > 0.0 && iy + 1 <= max_y
            {
                const T a = Get(ix, iy);
                const T c = Get(ix, iy + 1);
                result    = a.Mix({{y, c}});
            }
        }
        else if (y <= 0.0 || iy + 1 > max_y) // x > 0.0 && ix + 1 <= max_x
        {
            const T a = Get(ix, iy);
            const T b = Get(ix + 1, iy);
            result    = a.Mix({{x, b}});
        }
        else // x > 0.0 && y > 0.0
        {
            // const double f     = (x - y) / sqrt2;     // distance from (x,y) to diagonal ax+by+c=0 with a=1, b=-1, c=0 (from 0/0 to 1/1)
            // const double e     = (x + y - 1) / sqrt2; // distance from (x,y) to diagonal ax+by+c=0 with a=1, b=1, c=-1 (from 0/1 to 1/0)
            const double x_neg = 1 - x;
            const double y_neg = 1 - y;
            const double da    = std::sqrt(x * x + y * y);                 // distance from    top left
            const double dc    = std::sqrt(x * x + y_neg * y_neg);         // distance from bottom left
            const double db    = std::sqrt(x_neg * x_neg + y * y);         // distance from    top right
            const double dd    = std::sqrt(x_neg * x_neg + y_neg * y_neg); // distance from bottom right
            const double dab   = std::sqrt(y * y);                         // distance from    top middle
            const double dcd   = std::sqrt(y_neg * y_neg);                 // distance from bottom middle
            const double dac   = std::sqrt(x * x);                         // distance from   left middle
            const double dbd   = std::sqrt(x_neg * x_neg);                 // distance from  right middle
            // const double d_orig = std::sqrt((x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5)); // distance from center
            double ci      = 1 - dc; //(f >= 0 ? 0 : -f);
            double bi      = 1 - db; //(f <= 0 ? 0 : f);
            double ai      = 1 - da; //(e >= 0 ? 0 : -e);
            double di      = 1 - dd; //(e <= 0 ? 0 : e);
            double abi     = 1 - dab;
            double cdi     = 1 - dcd;
            double aci     = 1 - dac;
            double bdi     = 1 - dbd;
            ci             = std::max(0.0, ci);
            bi             = std::max(0.0, bi);
            ai             = std::max(0.0, ai);
            di             = std::max(0.0, di);
            abi            = std::max(0.0, abi);
            cdi            = std::max(0.0, cdi);
            aci            = std::max(0.0, aci);
            bdi            = std::max(0.0, bdi);
            const double t = ai + bi + ci + di;
            const double s = abi + cdi + aci + bdi;

            const T a  = Get(ix, iy);
            const T b  = Get(ix + 1, iy);
            const T c  = Get(ix, iy + 1);
            const T d  = Get(ix + 1, iy + 1);
            const T ab = x == 0.0 ? a : a.Mix({{x, b}}); // top
            const T cd = x == 0.0 ? c : c.Mix({{x, d}}); // bottom
            const T ac = y == 0.0 ? a : a.Mix({{y, c}}); // left
            const T bd = y == 0.0 ? b : b.Mix({{y, d}}); // right

            const T col1 = s == 0.0 ? ab.Mix({{0.25, cd},
                                              {0.25, ac},
                                              {0.25, bd}})
                                    : ab.Mix({{cdi / s, cd},
                                              {aci / s, ac},
                                              {bdi / s, bd}});

            const T col2 = t == 0.0 ? a.Mix({{0.25, b},
                                             {0.25, c},
                                             {0.25, d}})
                                    : a.Mix({{bi / t, b},
                                             {ci / t, c},
                                             {di / t, d}});

            const double weight = 2 * std::min(std::min(std::min(dab, dcd), dac), dbd);
            result              = col1.Mix({{weight, col2}});
        }

        return result;
    }
}
