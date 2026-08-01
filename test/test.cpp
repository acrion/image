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

#include "acrion/image/bitmap.hpp"
#include "acrion/image/color.hpp"

#include <string>
#include <vector>

using namespace acrion::image;

namespace
{
    /// Every depth the container supports. A negative depth means floating point; -8 is
    /// what the FITS reader produces, so it is not an exotic corner.
    const std::vector<int> allDepths{1, 2, 4, 8, -8};

    /// Every channel layout the image reader can produce: gray, gray+alpha, RGB, RGBA.
    const std::vector<int> allChannelCounts{1, 2, 3, 4};

    template <typename T>
    void CheckPlotAndGet(const int channels, const T value)
    {
        BitmapData<T> data(4, 3, channels);

        // A freshly constructed bitmap is NOT cleared - the buffer comes straight from
        // malloc, on the assumption that whoever creates it is about to fill it. Set()
        // establishes a known background so "the rest is untouched" means something.
        data.Set(Color<T>(T{}));

        if (channels <= 2)
        {
            data.Plot(2, 1, Color<T>(value));
            EXPECT_EQ(data.GetGray(2, 1), value);
            EXPECT_EQ(data.GetGray(0, 0), T{}); // the rest is untouched
        }
        else
        {
            const Color<T> color(value, T(value / 2), T(value / 4));
            data.Plot(2, 1, color);
            EXPECT_EQ(data.GetRed(2, 1), color.Red());
            EXPECT_EQ(data.GetGreen(2, 1), color.Green());
            EXPECT_EQ(data.GetBlue(2, 1), color.Blue());
            EXPECT_EQ(data.GetRed(0, 0), T{});
        }

        // Out of bounds must be refused rather than corrupt the neighbouring row.
        EXPECT_TRUE(data.Plot(-1, 0, Color<T>(value)));
        EXPECT_TRUE(data.Plot(0, -1, Color<T>(value)));
        EXPECT_TRUE(data.Plot(data.Width(), 0, Color<T>(value)));
        EXPECT_TRUE(data.Plot(0, data.Height(), Color<T>(value)));
    }
}

// Bitmap is the container every plugin manipulates, and until now nothing exercised it -
// the three tests below it cover Color only. The bugs that hurt users in practice
// (BUG-1/2/3/6) all lived in exactly this geometry-and-depth bookkeeping.

TEST(BitmapTest, ReportsTheGeometryAndDepthItWasConstructedWith)
{
    for (const int depth : allDepths)
    {
        for (const int channels : allChannelCounts)
        {
            SCOPED_TRACE("depth=" + std::to_string(depth) + " channels=" + std::to_string(channels));

            Bitmap bitmap(7, 5, channels, depth);

            EXPECT_EQ(bitmap.Width(), 7);
            EXPECT_EQ(bitmap.Height(), 5);
            EXPECT_EQ(bitmap.Channels(), channels);
            EXPECT_EQ(bitmap.Depth(), depth);
            EXPECT_FALSE(bitmap.Empty());
            EXPECT_NE(bitmap.Buffer(), nullptr);
        }
    }
}

TEST(BitmapTest, AnUnsupportedDepthIsRejectedAndNamedInTheMessage)
{
    // 3 bytes per sample is the kind of value a broken reader passes in. The message has
    // to say which depth was refused - it used to report the *uninitialized* depth of the
    // half-constructed bitmap instead of the argument.
    try
    {
        Bitmap bitmap(4, 4, 1, 3);
        FAIL() << "constructing with depth 3 should throw";
    }
    catch (const std::runtime_error& ex)
    {
        EXPECT_NE(std::string(ex.what()).find("3"), std::string::npos) << ex.what();
    }
}

TEST(BitmapTest, PixelsSurviveAWriteAndReadAtEveryDepth)
{
    for (const int channels : allChannelCounts)
    {
        SCOPED_TRACE("channels=" + std::to_string(channels));

        CheckPlotAndGet<uint8_t>(channels, 200);
        CheckPlotAndGet<uint16_t>(channels, 40000);
        CheckPlotAndGet<uint32_t>(channels, 3000000000U);
        CheckPlotAndGet<uint64_t>(channels, 12000000000000000000ULL);
        CheckPlotAndGet<double>(channels, 0.75);
    }
}

TEST(BitmapTest, TheParameterTableRoundTripKeepsTheSameBuffer)
{
    // How every plugin receives an image: the host writes the fields into the message
    // parameters, the plugin rebuilds a Bitmap from them. Plugins that work *in place* -
    // all of acrion imago, and CallInvertImage - depend on the rebuilt Bitmap addressing
    // the very same memory, not a copy of it.
    for (const int depth : allDepths)
    {
        for (const int channels : allChannelCounts)
        {
            SCOPED_TRACE("depth=" + std::to_string(depth) + " channels=" + std::to_string(channels));

            Bitmap original(6, 4, channels, depth);

            // The displayed brightness range is expressed in the pixel type's own units,
            // so it is stored as a uint8_t for an 8-bit image and as a double for FITS.
            // These two values are exact in all five representations; a fractional one
            // would legitimately truncate to 0 for every integer depth.
            original.SetMinDisplayedBrightness(1.0);
            original.SetMaxDisplayedBrightness(200.0);

            const BitmapContainer parameters = static_cast<BitmapContainer>(original);
            const Bitmap          restored(parameters);

            EXPECT_EQ(restored.Width(), original.Width());
            EXPECT_EQ(restored.Height(), original.Height());
            EXPECT_EQ(restored.Channels(), original.Channels());
            EXPECT_EQ(restored.Depth(), original.Depth());
            EXPECT_EQ(restored.Buffer(), original.Buffer());
            EXPECT_DOUBLE_EQ(restored.GetMinDisplayedBrightness(), 1.0);
            EXPECT_DOUBLE_EQ(restored.GetMaxDisplayedBrightness(), 200.0);
        }
    }
}

TEST(BitmapTest, TheParameterTableCarriesEveryKeyAPluginNeeds)
{
    // A missing key surfaces as the documented "Missing parameter value for channels"
    // several messages later, so assert the whole set at the source.
    Bitmap bitmap(3, 2, 3, 1);

    const BitmapContainer parameters = static_cast<BitmapContainer>(bitmap);

    for (const auto key : Bitmap::imageKeys)
    {
        SCOPED_TRACE(std::string(key));
        EXPECT_NE(parameters.data.find(std::string(key)), parameters.data.end());
    }
}

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
