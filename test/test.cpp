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

#include <gtest/gtest.h>

#include "acrion/image/color.hpp"

using namespace acrion::image;

TEST(ImageFrameworkTest, ColorWrap)
{
    Color<uint64_t> a(3, 5, 7);
    a -= Color<uint64_t>(5, 3, 2);
    Color<uint64_t> result(-2, 2, 5);
    EXPECT_EQ(a, result);
}

TEST(ImageFrameworkTest, WithBrightnessStaySame)
{
    const Color<uint8_t> col1(192, 160, 96);
    const auto           brightnessCol1 = col1.Gray();
    const auto           col1b          = col1.WithBrightness(brightnessCol1);

    EXPECT_NEAR(col1b.Red(), col1.Red(), 1);
    EXPECT_NEAR(col1b.Green(), col1.Green(), 1);
    EXPECT_NEAR(col1b.Blue(), col1.Blue(), 1);
}

TEST(ImageFrameworkTest, WithBrightnessPlausible)
{
    const Color<uint8_t> col1(192, 160, 96);
    const auto           brightnessCol1 = col1.Gray();
    const auto           col1b          = col1.WithBrightness(brightnessCol1 - 10);

    EXPECT_LT(col1b.Red(), col1.Red());
    EXPECT_LT(col1b.Green(), col1.Green());
    EXPECT_LT(col1b.Blue(), col1.Blue());
}
