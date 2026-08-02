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
#include "acrion/image/interpolation.hpp"
#include "acrion/image/mixable_scalar.hpp"

#include <cmath>
#include <string>
#include <tuple>
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

// Colour arithmetic on floating point pixels.
//
// Color<T> is a template over all five pixel types, but two of its members were written as
// if T were always an integer: Gray() and WithBrightness() both ended in std::llround(), and
// WithBrightness() clamped against numeric_limits<T>::max(). For T = double that rounds a
// FITS frame scaled to [0, 1] to two brightness levels, and the clamp is meaningless. The
// gray branch of Gray() returns its value untouched and hid this: it is only reached by a
// *coloured* pixel, and today only the FITS reader produces floating point images, always
// with a single channel.
//
// The cases below assert properties rather than re-deriving the YUV matrix: a colour must
// survive a round trip through its own brightness, and the brightness that was asked for
// must be the brightness that comes back.

TEST(ColorFloatingPointTest, GrayOfAColouredPixelKeepsItsFraction)
{
    const Color<double> colour(0.8, 0.3, 0.3);

    // 0.299 * 0.8 + 0.587 * 0.3 + 0.114 * 0.3, written out rather than recomputed.
    EXPECT_NEAR(colour.Gray(), 0.4495, 1e-9);
}

TEST(ColorFloatingPointTest, GrayOfAGrayPixelIsTheValueItself)
{
    // The branch that always worked, pinned so that a fix to the other one cannot break it.
    EXPECT_EQ(Color<double>(0.8).Gray(), 0.8);
}

namespace
{
    /// How exactly a colour comes back from YUV and returns. The forward and inverse
    /// coefficients are both rounded to five decimals and are not exact inverses of each
    /// other, so the round trip is off by about 1.4e-5 of the value range - measured, not
    /// assumed. For an 8 bit image that is a thousandth of one count, which is why the
    /// integer tests above can afford to compare within 1.
    constexpr double kYuvRoundTrip = 1e-4;
}

TEST(ColorFloatingPointTest, AColourSurvivesARoundTripThroughItsOwnBrightness)
{
    const Color<double> colour(0.8, 0.3, 0.25);
    const Color<double> result = colour.WithBrightness(colour.Gray());

    EXPECT_NEAR(result.Red(), colour.Red(), kYuvRoundTrip);
    EXPECT_NEAR(result.Green(), colour.Green(), kYuvRoundTrip);
    EXPECT_NEAR(result.Blue(), colour.Blue(), kYuvRoundTrip);
}

TEST(ColorFloatingPointTest, WithBrightnessDeliversTheBrightnessItWasAskedFor)
{
    const Color<double> colour(0.8, 0.3, 0.25);
    const Color<double> darker = colour.WithBrightness(0.2);

    EXPECT_NEAR(darker.Gray(), 0.2, kYuvRoundTrip);
    EXPECT_LT(darker.Red(), colour.Red());
    EXPECT_LT(darker.Green(), colour.Green());
    EXPECT_LT(darker.Blue(), colour.Blue());
}

TEST(ColorFloatingPointTest, NegativeComponentsAreNotClampedAway)
{
    // Subtracting a dark frame puts the background of a calibrated frame below zero, and
    // FITS keeps it there. Clamping at zero - right for every integer type, which cannot
    // represent it - would raise that background back to black.
    const Color<double> colour(-0.2, -0.5, -0.4);
    const Color<double> result = colour.WithBrightness(colour.Gray());

    EXPECT_LT(colour.Gray(), 0.0);
    EXPECT_NEAR(result.Red(), colour.Red(), kYuvRoundTrip);
    EXPECT_NEAR(result.Green(), colour.Green(), kYuvRoundTrip);
    EXPECT_NEAR(result.Blue(), colour.Blue(), kYuvRoundTrip);
}

TEST(ColorIntegerTest, GrayStillRoundsToTheNearestWholeNumber)
{
    // The integer behaviour must not move: 0.299 * 192 + 0.587 * 160 + 0.114 * 96 = 162.272.
    EXPECT_EQ(Color<uint8_t>(192, 160, 96).Gray(), 162);
}

TEST(ColorIntegerTest, GrayOfA64BitColourDoesNotWrap)
{
    // The maximum of uint64_t does not fit into an int64_t, and the weighted sum of a bright
    // 64 bit colour exceeds the range of one: std::llround() on it is undefined.
    constexpr uint64_t max  = std::numeric_limits<uint64_t>::max();
    const uint64_t     gray = Color<uint64_t>(max, max / 2, max / 2).Gray();

    EXPECT_GT(gray, max / 2);
    EXPECT_LT(gray, max);
    EXPECT_NEAR((double)gray, 1.1981e19, 1e16); // 0.299 * max + 0.701 * max / 2
}

TEST(ColorIntegerTest, WithBrightnessStillClampsToTheRangeOfTheType)
{
    const Color<uint8_t> saturated(255, 0, 0);

    // Upwards: the red component of the reconstruction overshoots 255 by far.
    const Color<uint8_t> bright = saturated.WithBrightness(255);
    EXPECT_EQ(bright.Red(), 255);

    // Downwards: green and blue come out negative, and must arrive at zero rather than
    // wrapping around into a bright value.
    const Color<uint8_t> dark = saturated.WithBrightness(0);
    EXPECT_EQ(dark.Green(), 0);
    EXPECT_EQ(dark.Blue(), 0);
}

// AbsoluteDiff is the same defect one level up: it computes the difference between two
// images through int64_t. acrionphoto's difference view calls it, and it is instantiated for
// double, so on two FITS frames every fractional difference truncated to zero.

TEST(AbsoluteDiffTest, KeepsFractionalDifferences)
{
    BitmapData<double> left(2, 2, 1);
    BitmapData<double> right(2, 2, 1);
    left.Set(Color<double>(0.8));
    right.Set(Color<double>(0.3));

    const auto difference = left.AbsoluteDiff(right);
    EXPECT_NEAR(difference->GetGray(0, 0), 0.5, 1e-12);

    // ...in both directions: the result is an absolute value.
    const auto reversed = right.AbsoluteDiff(left);
    EXPECT_NEAR(reversed->GetGray(0, 0), 0.5, 1e-12);
}

TEST(AbsoluteDiffTest, IntegerImagesAreUnchanged)
{
    BitmapData<uint16_t> left(2, 2, 1);
    BitmapData<uint16_t> right(2, 2, 1);
    left.Set(Color<uint16_t>(1000));
    right.Set(Color<uint16_t>(300));

    EXPECT_EQ(left.AbsoluteDiff(right)->GetGray(1, 1), 700);
    EXPECT_EQ(right.AbsoluteDiff(left)->GetGray(1, 1), 700);
}

TEST(AbsoluteDiffTest, A64BitDifferenceDoesNotWrap)
{
    constexpr uint64_t max = std::numeric_limits<uint64_t>::max();

    BitmapData<uint64_t> left(2, 2, 1);
    BitmapData<uint64_t> right(2, 2, 1);
    left.Set(Color<uint64_t>(max));
    right.Set(Color<uint64_t>(1));

    EXPECT_EQ(left.AbsoluteDiff(right)->GetGray(0, 0), max - 1);
}

// MaxGray is where acrion imago's star detection gets its threshold: a candidate has to be
// brighter than the average plus the standard deviation of its detection window. Both of
// those come out of the loop below, which is an `#pragma omp parallel for` over the rows -
// with `sum` and the index `i` shared and unsynchronised.

namespace
{
    /// The same image every time: a gradient, so that the standard deviation is not zero and
    /// a lost summand cannot hide behind a uniform field.
    BitmapData<uint16_t> GradientImage(const int width, const int height)
    {
        BitmapData<uint16_t> data(width, height, 1);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                data.Plot(x, y, Color<uint16_t>(static_cast<uint16_t>(1000 + (x % 97) * 137 + (y % 31) * 11)));
            }
        }
        return data;
    }
}

TEST(MaxGrayTest, TheAverageIsTheAverage)
{
    const int            width  = 400;
    const int            height = 400;
    const BitmapData<uint16_t> image = GradientImage(width, height);

    long double expected = 0;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            expected += image.GetGray(x, y);
        }
    }
    expected /= (long double)width * height;

    uint16_t average = 0;
    image.MaxGray(0, 0, width - 1, height - 1, nullptr, nullptr, &average);

    EXPECT_NEAR(average, (double)expected, 1.0);
}

TEST(MaxGrayTest, TheAverageAndDeviationDoNotDependOnThreadScheduling)
{
    // Twenty runs over the same image must give the same two numbers. They did not: `sum`
    // and the index into the sample vector were both written from every thread without
    // synchronisation, so summands were lost and samples overwritten each other.
    const int            width  = 400;
    const int            height = 400;
    const BitmapData<uint16_t> image = GradientImage(width, height);

    uint16_t firstAverage   = 0;
    double   firstDeviation = 0;
    image.MaxGray(0, 0, width - 1, height - 1, nullptr, nullptr, &firstAverage,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &firstDeviation);

    for (int run = 1; run < 20; ++run)
    {
        SCOPED_TRACE("run " + std::to_string(run));

        uint16_t average   = 0;
        double   deviation = 0;
        image.MaxGray(0, 0, width - 1, height - 1, nullptr, nullptr, &average,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &deviation);

        EXPECT_EQ(average, firstAverage);
        EXPECT_DOUBLE_EQ(deviation, firstDeviation);
    }
}

TEST(MaxGrayTest, TheDeviationIsTheDeviationOfTheWholeWindow)
{
    const int            width  = 400;
    const int            height = 400;
    const BitmapData<uint16_t> image = GradientImage(width, height);

    long double sum = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            sum += image.GetGray(x, y);

    const double mean = (double)(sum / ((long double)width * height));

    double variance = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            variance += ((double)image.GetGray(x, y) - mean) * ((double)image.GetGray(x, y) - mean);

    const double expected = std::sqrt(variance / ((double)width * height));

    uint16_t average   = 0;
    double   deviation = 0;
    image.MaxGray(0, 0, width - 1, height - 1, nullptr, nullptr, &average,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &deviation);

    EXPECT_NEAR(deviation, expected, 1.0);
}

TEST(MinGrayTest, TheAverageIsTheAverage)
{
    // MinGray accumulates its sum the same way, and nothing else reads it - so it is only
    // wrong, not visibly wrong.
    const int                  width  = 400;
    const int                  height = 400;
    const BitmapData<uint16_t> image  = GradientImage(width, height);

    long double expected = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            expected += image.GetGray(x, y);
    expected /= (long double)width * height;

    uint16_t average = 0;
    image.MinGray(0, 0, width - 1, height - 1, nullptr, nullptr, &average);

    EXPECT_NEAR(average, (double)expected, 1.0);
}

TEST(MaxGray2Test, TheAverageIsTheAverage)
{
    const int                  width  = 400;
    const int                  height = 400;
    const BitmapData<uint16_t> image  = GradientImage(width, height);

    long double expected = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            expected += image.GetGray(x, y);
    expected /= (long double)width * height;

    uint16_t average = 0;
    image.MaxGray2(0, 0, width - 1, height - 1, nullptr, nullptr, &average);

    EXPECT_NEAR(average, (double)expected, 1.0);
}

// interpolation::Do is what acrion imago samples its corrected images with, so it decides
// the visual quality of the product that is meant to be sold - and it had no tests. The
// scheme is a hand-rolled distance weighting rather than a textbook bilinear one, which is
// exactly the kind of code where a normalisation slip produces output that still looks
// plausible. The cases below therefore assert invariants that must hold whatever the
// weighting does, instead of re-deriving its arithmetic.

namespace
{
    using Scalar = MixableScalar<int>;

    /// A 2x2 neighbourhood with the given corner values, as interpolation::Do expects it.
    interpolation::Getter<Scalar> Corners(const int topLeft, const int topRight, const int bottomLeft, const int bottomRight)
    {
        return [=](const int x, const int y) -> Scalar
        {
            if (x <= 0) return Scalar(y <= 0 ? topLeft : bottomLeft);
            return Scalar(y <= 0 ? topRight : bottomRight);
        };
    }
}

TEST(InterpolationTest, ReturnsTheSampleItselfOnAGridPoint)
{
    // No interpolation is due at an integral coordinate, so any deviation is pure error.
    const auto get = Corners(10, 20, 30, 40);

    EXPECT_EQ((int)interpolation::Do<Scalar>(0.0, 0.0, 0, 0, 1, 1, get), 10);
    EXPECT_EQ((int)interpolation::Do<Scalar>(1.0, 0.0, 0, 0, 1, 1, get), 20);
    EXPECT_EQ((int)interpolation::Do<Scalar>(0.0, 1.0, 0, 0, 1, 1, get), 30);
    EXPECT_EQ((int)interpolation::Do<Scalar>(1.0, 1.0, 0, 0, 1, 1, get), 40);
}

TEST(InterpolationTest, AUniformNeighbourhoodStaysUniform)
{
    // The strongest invariant available for a weighting scheme: if all four samples agree,
    // the result must be that value everywhere, because the weights have to sum to one.
    // A normalisation error shows up here immediately and nowhere else so clearly.
    const auto get = Corners(123, 123, 123, 123);

    for (double dy = 0.0; dy <= 1.0; dy += 0.125)
    {
        for (double dx = 0.0; dx <= 1.0; dx += 0.125)
        {
            EXPECT_EQ((int)interpolation::Do<Scalar>(dx, dy, 0, 0, 1, 1, get), 123)
                << "at " << dx << "," << dy;
        }
    }
}

TEST(InterpolationTest, NeverOvershootsTheSurroundingSamples)
{
    // An interpolated value outside the range of its neighbours is an artefact - in an
    // astrophotograph, a bright rim around every corrected star.
    const auto get = Corners(0, 255, 40, 200);

    for (double dy = 0.0; dy <= 1.0; dy += 0.0625)
    {
        for (double dx = 0.0; dx <= 1.0; dx += 0.0625)
        {
            const int value = interpolation::Do<Scalar>(dx, dy, 0, 0, 1, 1, get);
            EXPECT_GE(value, 0) << "at " << dx << "," << dy;
            EXPECT_LE(value, 255) << "at " << dx << "," << dy;
        }
    }
}

TEST(InterpolationTest, InterpolatesAlongAnEdge)
{
    // With one coordinate integral the problem is one-dimensional, so the answer is not a
    // matter of the weighting scheme: halfway between 0 and 200 is 100.
    const auto get = Corners(0, 200, 0, 200);

    EXPECT_EQ((int)interpolation::Do<Scalar>(0.5, 0.0, 0, 0, 1, 1, get), 100);
    EXPECT_EQ((int)interpolation::Do<Scalar>(0.25, 0.0, 0, 0, 1, 1, get), 50);
    EXPECT_EQ((int)interpolation::Do<Scalar>(0.75, 0.0, 0, 0, 1, 1, get), 150);
}

TEST(InterpolationTest, IsSymmetricAboutTheCentre)
{
    // Two samples opposite each other, sampled at the centre: neither may be preferred.
    // An asymmetry here would shift a corrected image by a fraction of a pixel.
    const auto horizontal = Corners(0, 100, 0, 100);
    const auto vertical   = Corners(0, 0, 100, 100);

    EXPECT_EQ((int)interpolation::Do<Scalar>(0.5, 0.5, 0, 0, 1, 1, horizontal),
              (int)interpolation::Do<Scalar>(0.5, 0.5, 0, 0, 1, 1, vertical));
    EXPECT_EQ((int)interpolation::Do<Scalar>(0.5, 0.5, 0, 0, 1, 1, horizontal), 50);
}

TEST(InterpolationTest, ClampsCoordinatesToTheGivenBounds)
{
    // imago samples along a distortion model, which can point outside the image. Clamping is
    // what keeps that from reading out of bounds.
    const auto get = Corners(10, 20, 30, 40);

    EXPECT_EQ((int)interpolation::Do<Scalar>(-5.0, -5.0, 0, 0, 1, 1, get), 10);
    EXPECT_EQ((int)interpolation::Do<Scalar>(7.0, 9.0, 0, 0, 1, 1, get), 40);
}

// ConvertToDepth8 is the display path: acrionphoto calls it for every image it shows, at
// every zoom level. It had no tests, and BUG-20 - four-channel colours rotated by one - lived
// partly in it. The buffer it returns is a raw new[], so each case deletes it.

namespace
{
    /// \brief A greyscale bitmap whose pixel values are their own x coordinate.
    BitmapData<uint8_t> GrayRamp(const int width, const int height)
    {
        BitmapData<uint8_t> data(width, height, 1);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                data.Plot(x, y, Color<uint8_t>(static_cast<uint8_t>(x)));
            }
        }
        return data;
    }
}

TEST(ConvertToDepth8Test, MapsTheBrightnessRangeOntoTheFullByteRange)
{
    // What the two brightness sliders in acrionphoto do: the displayed range is stretched
    // over 0..255, everything below or above is clamped. Getting this wrong makes every
    // astro image either black or blown out, which is the first thing a user would see.
    BitmapData<uint8_t> data = GrayRamp(8, 1);
    data.SetBrightnessRangeForDisplay(2, 6);

    uint8_t* display = data.ConvertToDepth8();
    ASSERT_NE(display, nullptr);

    EXPECT_EQ(display[0], 0) << "below the range clamps to black";
    EXPECT_EQ(display[2], 0) << "the lower bound is black";
    EXPECT_EQ(display[6], 255) << "the upper bound is white";
    EXPECT_EQ(display[7], 255) << "above the range clamps to white";
    EXPECT_EQ(display[4], 128) << "the middle lands in the middle";

    delete[] display;
}

TEST(ConvertToDepth8Test, ProducesBgraForColourImages)
{
    // The display buffer is BGRA, which is what Qt expects, while the container is RGBA.
    // A swap here shows up as red and blue exchanged in the whole application.
    BitmapData<uint8_t> data(1, 1, 3);
    data.SetBrightnessRangeForDisplay(0, 255);
    data.Plot(0, 0, Color<uint8_t>(10, 20, 30));

    uint8_t* display = data.ConvertToDepth8();
    ASSERT_NE(display, nullptr);

    EXPECT_EQ(display[0], 30) << "blue first";
    EXPECT_EQ(display[1], 20) << "then green";
    EXPECT_EQ(display[2], 10) << "then red";
    EXPECT_EQ(display[3], 255) << "opaque, since the source has no alpha";

    delete[] display;
}

TEST(ConvertToDepth8Test, KeepsAlphaOfFourChannelImages)
{
    // The four-channel counterpart of the case above. Before BUG-20 was fixed this produced
    // alpha where blue belongs and red where alpha belongs.
    BitmapData<uint8_t> data(1, 1, 4);
    data.SetBrightnessRangeForDisplay(0, 255);

    uint8_t* raw = data.Buffer();
    raw[0]       = 10; // R
    raw[1]       = 20; // G
    raw[2]       = 30; // B
    raw[3]       = 40; // A

    uint8_t* display = data.ConvertToDepth8();
    ASSERT_NE(display, nullptr);

    EXPECT_EQ(display[0], 30);
    EXPECT_EQ(display[1], 20);
    EXPECT_EQ(display[2], 10);
    EXPECT_EQ(display[3], 40);

    delete[] display;
}

TEST(ConvertToDepth8Test, PadsGreyscaleRowsToAFourByteBoundary)
{
    // Greyscale display rows are aligned to four bytes, which is what the caller has to
    // assume when it walks the buffer. With a width of 5 that means a stride of 8, and the
    // second row starts there rather than at 5.
    BitmapData<uint8_t> data = GrayRamp(5, 2);
    data.SetBrightnessRangeForDisplay(0, 4);

    uint8_t* display = data.ConvertToDepth8();
    ASSERT_NE(display, nullptr);

    EXPECT_EQ(display[0], 0);
    EXPECT_EQ(display[4], 255);
    EXPECT_EQ(display[8], 0) << "second row starts at the aligned offset, not at 5";
    EXPECT_EQ(display[12], 255);

    delete[] display;
}

TEST(ConvertToDepth8Test, ConvertsOnlyTheRequestedRectangle)
{
    // Panning and zooming ask for a sub rectangle rather than the whole image.
    BitmapData<uint8_t> data = GrayRamp(8, 4);
    data.SetBrightnessRangeForDisplay(0, 7);

    uint8_t* display = data.ConvertToDepth8(0, 4, 1, 4, 2);
    ASSERT_NE(display, nullptr);

    // x = 4..7 of a ramp whose value is its x coordinate, stretched over 0..7 -> 145..255
    EXPECT_EQ(display[0], static_cast<uint8_t>(std::lround(255.0 * 4 / 7)));
    EXPECT_EQ(display[3], 255);

    delete[] display;
}

TEST(ConvertToDepth8Test, ARectangleRendersTheSamePixelsAsTheWholeImageDoes)
{
    // The assumption acrionphoto's partial repaint rests on. When a plugin reports the region
    // it changed, only that rectangle is converted again and painted into the cached pixmap of
    // the whole image. That is only invisible if a rectangle comes out exactly as the
    // corresponding window of the full render - so: no dependence on the pixels around it, no
    // different rounding at an edge, and the right row stride at both sizes.
    //
    // The pattern varies in both directions, because one that varies only in x cannot tell a
    // wrong row from a right one, and the display range is the full range of the type so that
    // the expected value is the stored one.
    constexpr int width = 14, height = 9;

    BitmapData<uint8_t> data(width, height, 1);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            data.Plot(x, y, Color<uint8_t>(static_cast<uint8_t>((x * 7 + y * 31) % 256)));
        }
    }
    data.SetBrightnessRangeForDisplay(0, 255);

    const auto stride = [](const int w)
    { return (w + 3) / 4 * 4; }; // greyscale rows are padded to four bytes

    uint8_t* whole = data.ConvertToDepth8();
    ASSERT_NE(whole, nullptr);

    // Deliberately of a width that is not a multiple of four, and not at the origin: the
    // padding of the patch differs from the padding of the whole image, and an implementation
    // that assumed the two strides were equal would pass at every other size.
    constexpr int rx = 5, ry = 2, rw = 6, rh = 3;

    uint8_t* part = data.ConvertToDepth8(0, rx, ry, rw, rh);
    ASSERT_NE(part, nullptr);

    for (int y = 0; y < rh; ++y)
    {
        for (int x = 0; x < rw; ++x)
        {
            EXPECT_EQ(part[y * stride(rw) + x], whole[(ry + y) * stride(width) + rx + x])
                << "at " << x << "/" << y << " of the rectangle";
        }
    }

    delete[] part;
    delete[] whole;
}

TEST(ConvertToDepth8Test, ARectangleOfAColourImageAlsoMatchesTheWholeRender)
{
    // The same property for the four-byte path, where the stride is not padded and the
    // channels are reordered into BGRA. Colour is where BUG-20 lived, so a partial repaint
    // that got the channel order right only at the origin is a real possibility.
    constexpr int width = 7, height = 5;

    BitmapData<uint8_t> data(width, height, 3);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            data.Plot(x, y, Color<uint8_t>(static_cast<uint8_t>(x * 9),
                                           static_cast<uint8_t>(y * 11),
                                           static_cast<uint8_t>(x * 3 + y * 5)));
        }
    }
    data.SetBrightnessRangeForDisplay(0, 255);

    uint8_t* whole = data.ConvertToDepth8();
    ASSERT_NE(whole, nullptr);

    constexpr int rx = 2, ry = 1, rw = 3, rh = 3;

    uint8_t* part = data.ConvertToDepth8(0, rx, ry, rw, rh);
    ASSERT_NE(part, nullptr);

    for (int y = 0; y < rh; ++y)
    {
        for (int x = 0; x < rw; ++x)
        {
            for (int channel = 0; channel < 4; ++channel)
            {
                EXPECT_EQ(part[(y * rw + x) * 4 + channel],
                          whole[((ry + y) * width + rx + x) * 4 + channel])
                    << "channel " << channel << " at " << x << "/" << y;
            }
        }
    }

    delete[] part;
    delete[] whole;
}

TEST(ConvertToDepth8Test, RefusesAnUnsupportedChannelCount)
{
    // Init() already rejects anything but 1..4, so a bitmap with five channels cannot be
    // constructed - which is the real guarantee, and worth pinning where a reader might
    // otherwise be tempted to pass an arbitrary channel count through.
    EXPECT_THROW(BitmapData<uint8_t>(2, 2, 5), std::runtime_error);
}

// The three places the integer assumption of BUG-29 and BUG-31 still sat, plus the last
// unsynchronised omp loop. None of them had a caller in this workspace, which is the only
// reason they were not defects with a reproduction - they were traps waiting for one.

TEST(BoundedArithmeticTest, AFloatingPointValueKeepsItsFraction)
{
    // utility::BoundedAdd ended in static_cast<T>(static_cast<int64_t>(a) + b), which threw
    // away the fractional part of `a` before adding anything to it.
    Color<double> colour(0.8, 0.3, 0.25);
    colour += 0.1L;

    EXPECT_NEAR(colour.Red(), 0.9, 1e-12);
    EXPECT_NEAR(colour.Green(), 0.4, 1e-12);
    EXPECT_NEAR(colour.Blue(), 0.35, 1e-12);

    colour -= 0.1L;
    EXPECT_NEAR(colour.Red(), 0.8, 1e-12);
}

TEST(BoundedArithmeticTest, SaturationStillHoldsAtBothEnds)
{
    Color<uint8_t> bright(250, 250, 250);
    bright += 20.0L;
    EXPECT_EQ(bright.Red(), 255);

    Color<uint8_t> dark(5, 5, 5);
    dark -= 20.0L;
    EXPECT_EQ(dark.Red(), 0);

    // 64 bit is where the int64_t cast wrapped.
    constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
    Color<uint64_t>    wide(max - 10, max - 10, max - 10);
    wide += 100.0L;
    EXPECT_EQ(wide.Red(), max);
}

TEST(BoundedArithmeticTest, AnIntegerSumIsRoundedRatherThanTruncated)
{
    Color<uint8_t> colour(10, 10, 10);
    colour += 1.6L;
    EXPECT_EQ(colour.Red(), 12); // 11.6 rounds to 12
}

TEST(ColorScalingTest, AnIntegerComponentIsRoundedAndNeverNegative)
{
    Color<uint8_t> colour(100, 50, 20);
    colour *= 3; // 300 saturates, 150 and 60 do not
    EXPECT_EQ(colour.Red(), 255);
    EXPECT_EQ(colour.Green(), 150);
    EXPECT_EQ(colour.Blue(), 60);
}

TEST(ColorScalingTest, AFloatingPointComponentIsNeitherRoundedNorClamped)
{
    Color<double> colour(0.8, 0.3, -0.2);
    colour *= 0.5;

    EXPECT_NEAR(colour.Red(), 0.4, 1e-12);
    EXPECT_NEAR(colour.Green(), 0.15, 1e-12);
    EXPECT_NEAR(colour.Blue(), -0.1, 1e-12); // a calibrated frame has negative pixels
}

TEST(BelowTest, ReportsWhetherTheNeighbourhoodStaysUnderTheProfile)
{
    // BitmapData::Below tests every pixel within a radius against a radial profile scaled by
    // the brightness at the centre. It had a plain `bool` written from every thread of an omp
    // loop - a data race, though a benign-looking one, since the flag only ever goes from
    // true to false. It is std::atomic now.
    //
    // It also had no caller anywhere in the workspace, and therefore no test. Both of those
    // are now untrue.
    const std::vector<double> profile{1.0, 0.5, 0.25, 0.125, 0.0625};

    BitmapData<uint8_t> data(9, 9, 1);
    data.Set(Color<uint8_t>(10));
    data.Plot(4, 4, Color<uint8_t>(100));

    // Everything around it is far below the profile.
    EXPECT_TRUE(data.Below(4, 4, 3.0, profile, 0));

    // One neighbour at distance 1 brighter than ceil(100 * 0.5) is not.
    data.Plot(5, 4, Color<uint8_t>(90));
    EXPECT_FALSE(data.Below(4, 4, 3.0, profile, 0));

    // ...and it is only about the pixels within the radius.
    data.Plot(5, 4, Color<uint8_t>(10));
    data.Plot(8, 4, Color<uint8_t>(90));
    EXPECT_TRUE(data.Below(4, 4, 3.0, profile, 0));
}
