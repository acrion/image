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

#include "color.hpp"
#include "interpolation.hpp"
#include "utility.hpp"
#include "vector.hpp"

#include <cbeam/container/stable_reference_buffer.hpp>
#include <cbeam/logging/log_manager.hpp>

// #include <opencv2/opencv.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace acrion::image
{
    using namespace std::placeholders;

    inline double     _currentGamma = -1;
    inline uint8_t    _gammaTable[65536];
    inline std::mutex _mtx;

    using Drawer  = std::function<bool(const int x, const int y)>;
    using DrawerF = std::function<bool(const double x, const double y)>;

    namespace
    {
        inline double MaxValPlus1(size_t sizeOfType)
        {
            return std::pow(2.0, (double)sizeOfType * 8.0);
        }

        void FillGammaTable(double gamma)
        {
            std::lock_guard<std::mutex> lock(_mtx);

            if (gamma != _currentGamma)
            {
                _currentGamma = gamma;

                double gamma1 = gamma * 2;
                if (gamma1 > 1) gamma1 = 1;

                double       delta  = 9 - gamma * 6;
                const double log2   = log(2.0);
                double       factor = 256.0 / (log(65536.0) / log2 - delta);

                for (int v = 0; v < 65536; ++v)
                {
                    double val0    = v / 256.0;
                    double result  = log((double)v) / log2 - delta; // cppcheck-suppress invalidFunctionArg
                    double val1    = (result <= 0) ? 0 : result * factor;
                    double t       = gamma1 * val1 + (1 - gamma1) * val0;
                    _gammaTable[v] = (unsigned char)t;
                }
            }
        }
    }

    template <typename T>
    class BitmapData
    {
    public:
        BitmapData() = default;
        BitmapData(int width, int height, int channels)
            : _width(width)
            , _height(height)
            , _channels(channels)
            , _buffer(cbeam::container::stable_reference_buffer(Size(), sizeof(uint8_t)))
        {
            Init();
        }

        BitmapData(const cbeam::container::stable_reference_buffer& buffer, int width, int height, int channels)
            : _width(width)
            , _height(height)
            , _channels(channels)
            , _buffer(buffer)
        {
            Init();
        }

        BitmapData(const BitmapData& src)
            : BitmapData(src.Width(), src.Height(), src.Channels())
        {
            src.Copy(*this);
        }

        // BitmapData<T>::operator cv::Mat() const
        // {
        //     return cv::Mat(Height(), Width(), CV_MAKETYPE(sizeof(T), Channels()), Buffer());
        // }

        virtual ~BitmapData() = default;

        // operator cv::Mat() const;

        void Init()
        {
            switch (_channels)
            {
            case 1:
                _redIndex   = 0;
                _greenIndex = 0;
                _blueIndex  = 0;
                _grayIndex  = 0;
                break;
            case 3:
                _redIndex   = 0;
                _greenIndex = 1;
                _blueIndex  = 2;
                break;
            case 4:
                _alphaIndex = 0;
                _redIndex   = 1;
                _greenIndex = 2;
                _blueIndex  = 3;
                break;
            default:
                throw std::runtime_error("BitmapData: Unsupported number of channels: " + std::to_string(_channels));
            }
        }

        bool Empty() const { return Size() == 0 || Buffer() == nullptr; }
        void SetBrightnessRangeForDisplay(const T min, const T max)
        {
            _minDisplayedBrightness = min;
            _maxDisplayedBrightness = max;
        }

        void Copy(BitmapData& destination) const
        {
            if (*this != destination)
            {
                throw std::runtime_error("BitmapData::Copy: destination image has different size");
            }

            std::memcpy(destination.Buffer(), Buffer(), Size());
            destination._grayIndex              = _grayIndex;
            destination._alphaIndex             = _alphaIndex;
            destination._redIndex               = _redIndex;
            destination._greenIndex             = _greenIndex;
            destination._blueIndex              = _blueIndex;
            destination._minDisplayedBrightness = _minDisplayedBrightness;
            destination._maxDisplayedBrightness = _maxDisplayedBrightness;
        }

        BitmapData& operator=(const BitmapData& src)
        {
            if (this == &src)
            {
                return *this;
            }

            if (src != *this) // this image has different size than src image
            {
                _width    = src.Width();
                _height   = src.Height();
                _channels = src.Channels();
                _buffer   = cbeam::container::stable_reference_buffer(Size(), sizeof(uint8_t));
                Init();
            }

            src.Copy(*this);

            return *this;
        }

        BitmapData& operator=(BitmapData&& other) noexcept
        {
            _width                  = other._width;
            _height                 = other._height;
            _channels               = other._channels;
            _buffer                 = other._buffer;
            _grayIndex              = other._grayIndex;
            _alphaIndex             = other._alphaIndex;
            _redIndex               = other._redIndex;
            _greenIndex             = other._greenIndex;
            _blueIndex              = other._blueIndex;
            _minDisplayedBrightness = other._minDisplayedBrightness;
            _maxDisplayedBrightness = other._maxDisplayedBrightness;
            return *this;
        }

        void Set(const Color<T>& color) const
        {
#pragma omp parallel for
            for (int y = 0; y < Height(); ++y)
                for (int x = 0; x < Width(); ++x)
                    Plot(x, y, color);
        }

        T*  Buffer() const { return (T*)_buffer.get(); }
        int Stride() const { return _width * BytesPerPixel(); } // the scan width (in bytes). BitmapData never uses alignment, so it simply equals width * bytesPerPixel
        int BytesPerPixel() const { return _channels * std::abs(Depth()); }
        int Size() const { return Height() * Stride(); }

        int  Width() const { return _width; }
        int  Height() const { return _height; }
        int  Channels() const { return _channels; }
        int  Depth() const { return (int)sizeof(T) * (std::is_floating_point<T>::value ? -1 : 1); }
        T    GetMinDisplayedBrightness() const { return _minDisplayedBrightness; }
        T    GetMaxDisplayedBrightness() const { return _maxDisplayedBrightness; }
        void SetMinDisplayedBrightness(const T val) { _minDisplayedBrightness = val; }
        void SetMaxDisplayedBrightness(const T val) { _maxDisplayedBrightness = val; }

        T GetRed(const int x, const int y) const
        {
            return Buffer()[(y * _width + x) * _channels + _redIndex];
        }

        T GetGreen(const int x, const int y) const
        {
            return Buffer()[(y * _width + x) * _channels + _greenIndex];
        }

        T GetBlue(const int x, const int y) const
        {
            return Buffer()[(y * _width + x) * _channels + _blueIndex];
        }

        T GetAlpha(const int x, const int y) const
        {
            return _alphaIndex != -1 ? Buffer()[(y * _width + x) * _channels + _alphaIndex] : std::numeric_limits<T>().max();
        }

        T GetGray(const int x, const int y) const
        {
            if (_grayIndex != -1)
            {
                return Buffer()[(y * _width + x) * _channels + _grayIndex];
            }
            else
            {
                return Get(x, y).Gray(); // convert to Color, then calculate gray from RGB
            }
        }

        bool IsRed(const int x, const int y) const
        {
            if (_channels == 1)
            {
                return false;
            }
            else
            {
                const T red = GetRed(x, y);
                return red > GetGreen(x, y) && red > GetBlue(x, y);
            }
        }

        bool IsGreen(const int x, const int y) const
        {
            if (_channels == 1)
            {
                return false;
            }
            else
            {
                const T green = GetGreen(x, y);
                return green > GetRed(x, y) && green > GetBlue(x, y);
            }
        }

        bool IsBlue(const int x, const int y) const
        {
            if (_channels == 1)
            {
                return false;
            }
            else
            {
                const T blue = GetBlue(x, y);
                return blue > GetRed(x, y) && blue > GetGreen(x, y);
            }
        }

        bool IsBrighterThanNeighbours(const int i, const int j, const T current) const
        {
            return (i == 0
                    || (current > GetGray(i - 1, j)
                        && (j == 0 || current > GetGray(i - 1, j - 1))
                        && (j == Height() - 1 || current > GetGray(i - 1, j + 1))))
                && (j == 0 || current > GetGray(i, j - 1))
                && (j == Height() - 1 || current > GetGray(i, j + 1))
                && (i == Width() - 1
                    || (current > GetGray(i + 1, j)
                        && (j == 0 || current > GetGray(i + 1, j - 1))
                        && (j == Height() - 1 || current > GetGray(i + 1, j + 1))));
        }

        Color<T> Get(const int x, const int y) const
        {
            if (_channels == 1)
            {
                return Color(GetGray(x, y));
            }
            else
            {
                return Color(GetRed(x, y), GetGreen(x, y), GetBlue(x, y), GetAlpha(x, y));
            }
        }

        Color<T> Get(const double dx, const double dy) const
        {
            interpolation::Getter<Color<T>> getter = [this](const int x, const int y)
            {
                return Get(x, y);
            };

            return interpolation::Do(dx, dy, 0.0, 0.0, Width() - 1.0, Height() - 1.0, getter);
        }

        T GetGray(const double dx, const double dy) const
        {
            interpolation::Getter<MixableScalar<T>> getter = [this](const int x, const int y)
            {
                return MixableScalar<T>(GetGray(x, y));
            };

            return interpolation::Do(dx, dy, 0.0, 0.0, Width() - 1.0, Height() - 1.0, getter);
        }

        Color<T> Max(int x0, int y0, int x1, int y1, int* brightestX = nullptr, int* brightestY = nullptr) const
        {
            std::mutex mtx;

            x0 = std::max(0, x0);
            y0 = std::max(0, y0);
            x1 = std::min(_width - 1, x1);
            y1 = std::min(_height - 1, y1);

            Color<T> max = std::numeric_limits<T>::lowest();

#pragma omp parallel for
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const Color<T>              current = Get(x, y);
                    std::lock_guard<std::mutex> lock(mtx);
                    if (current > max)
                    {
                        max = current;
                        if (brightestX) *brightestX = x;
                        if (brightestY) *brightestY = y;
                    }
                }
            }

            return max;
        }

        Color<T> Min(int x0, int y0, int x1, int y1, int* darkestX = nullptr, int* darkestY = nullptr) const
        {
            std::mutex mtx;

            x0 = std::max(0, x0);
            y0 = std::max(0, y0);
            x1 = std::min(_width - 1, x1);
            y1 = std::min(_height - 1, y1);

            Color<T> min = std::numeric_limits<T>::max();

#pragma omp parallel for
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const Color<T>              current = Get(x, y);
                    std::lock_guard<std::mutex> lock(mtx);
                    if (current < min)
                    {
                        min = current;
                        if (darkestX) *darkestX = x;
                        if (darkestY) *darkestY = y;
                    }
                }
            }

            return min;
        }

        T MaxGray(int x0, int y0, int x1, int y1, int* brightestX = nullptr, int* brightestY = nullptr, T* average = nullptr, int* secondBrightestX = nullptr, int* secondBrightestY = nullptr, T* secondMax = nullptr, int* darkestX = nullptr, int* darkestY = nullptr, T* darkestValue = nullptr, double* stdDeviation = nullptr) const
        {
            std::mutex mtx;

            x0 = std::max(0, x0);
            y0 = std::max(0, y0);
            x1 = std::min(_width - 1, x1);
            y1 = std::min(_height - 1, y1);

            T              max  = 0;
            T              max2 = 0;
            T              min  = std::numeric_limits<T>::max();
            long double    sum  = 0;
            std::vector<T> data;
            if (stdDeviation)
            {
                data.resize((x1 - x0 + 1) * (y1 - y0 + 1));
            }
            int i = 0;

#pragma omp parallel for
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const T current = GetGray(x, y);
                    if (stdDeviation)
                    {
                        data[i++] = (T)current;
                    }
                    sum += current;
                    std::lock_guard<std::mutex> lock(mtx);
                    if (current > max)
                    {
                        if (max > max2)
                        {
                            max2 = max;
                            if (secondBrightestX && brightestX) *secondBrightestX = *brightestX;
                            if (secondBrightestY && brightestY) *secondBrightestY = *brightestY;
                        }

                        max = current;
                        if (brightestX) *brightestX = x;
                        if (brightestY) *brightestY = y;
                    }
                    else if (current > max2)
                    {
                        max2 = current;
                        if (secondBrightestX) *secondBrightestX = x;
                        if (secondBrightestY) *secondBrightestY = y;
                    }
                    if (current < min)
                    {
                        min = current;
                        if (darkestX) *darkestX = x;
                        if (darkestY) *darkestY = y;
                    }
                }
            }

            if (stdDeviation)
            {
                const auto avg = (double)(sum / ((x1 - x0 + 1) * (y1 - y0 + 1)));
                double     s   = 0.0;
                for (T val : data)
                {
                    s += ((double)val - avg) * ((double)val - avg);
                }
                *stdDeviation = std::sqrt(s / data.size());
            }

            if (average)
            {
                *average = (T)((sum + 1) / ((x1 - x0 + 1) * (y1 - y0 + 1)));
            }

            if (secondMax)
            {
                *secondMax = (T)max2;
            }

            if (darkestValue)
            {
                *darkestValue = (T)min;
            }

            return (T)max;
        }

        T MaxGray2(int xLeft, int yTop, int xRight, int yBottom, double* brightestX = nullptr, double* brightestY = nullptr, T* average = nullptr, int* secondBrightestX = nullptr, int* secondBrightestY = nullptr, T* secondMax = nullptr) const
        {
            std::mutex mtx;

            xLeft   = std::max(0, xLeft);
            yTop    = std::max(0, yTop);
            xRight  = std::min(_width - 1, xRight);
            yBottom = std::min(_height - 1, yBottom);

            T           max  = (T)-1;
            T           max2 = (T)-1;
            long double sum  = 0;
            int         x0   = 0;
            int         y0   = 0;
            int         x1   = 0;
            int         y1   = 0;

#pragma omp parallel for
            for (int y = yTop; y <= yBottom; ++y)
            {
                for (int x = xLeft; x <= xRight; ++x)
                {
                    const T current = (T)GetGray(x, y);
                    sum += current;
                    std::lock_guard<std::mutex> lock(mtx);
                    if (current > max)
                    {
                        if (max > max2)
                        {
                            max2 = max;
                            if (secondBrightestX && brightestX) *secondBrightestX = x0;
                            if (secondBrightestY && brightestY) *secondBrightestY = y0;
                        }

                        max = current;
                        x0 = x1 = x;
                        y0 = y1 = y;
                    }
                    else
                    {
                        if (current > max2)
                        {
                            max2 = current;
                            if (secondBrightestX) *secondBrightestX = x;
                            if (secondBrightestY) *secondBrightestY = y;
                        }

                        if (current == max)
                        {
                            if (x < x0) x0 = x;
                            if (y < y0) y0 = y;
                            if (x > x1) x1 = x;
                            if (y > y1) y1 = y;
                        }
                    }
                }
            }

            if (average)
            {
                *average = (T)((sum + 1) / ((xRight - xLeft + 1) * (yBottom - yTop + 1)));
            }

            if (brightestX)
            {
                *brightestX = ((double)x0 + x1) / 2.0;
                *brightestY = ((double)y0 + y1) / 2.0;
            }

            if (secondMax)
            {
                *secondMax = (T)max2;
            }

            return (T)max;
        }

        T MinGray(int x0, int y0, int x1, int y1, int* darkestX = nullptr, int* darkestY = nullptr, T* average = nullptr, int* secondDarkestX = nullptr, int* secondDarkestY = nullptr, T* secondMin = nullptr) const
        {
            std::mutex mtx;

            x0 = std::max(0, x0);
            y0 = std::max(0, y0);
            x1 = std::min(_width - 1, x1);
            y1 = std::min(_height - 1, y1);

            T           min  = std::numeric_limits<T>::max();
            T           min2 = std::numeric_limits<T>::max();
            long double sum  = 0;

#pragma omp parallel for
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const T current = GetGray(x, y);
                    sum += current;
                    std::lock_guard<std::mutex> lock(mtx);
                    if (current < min)
                    {
                        if (min < min2)
                        {
                            min2 = min;
                            if (secondDarkestX && darkestX) *secondDarkestX = *darkestX;
                            if (secondDarkestY && darkestY) *secondDarkestY = *darkestY;
                        }

                        min = current;
                        if (darkestX) *darkestX = x;
                        if (darkestY) *darkestY = y;
                    }
                    else if (current < min2)
                    {
                        min2 = current;
                        if (secondDarkestX) *secondDarkestX = x;
                        if (secondDarkestY) *secondDarkestY = y;
                    }
                }
            }

            if (average)
            {
                *average = (T)((sum + 1) / ((x1 - x0 + 1) * (y1 - y0 + 1)));
            }

            if (secondMin)
            {
                *secondMin = (T)min2;
            }

            return (T)min;
        }

        bool Below(const int centerX, const int centerY, const double r, const std::vector<double>& distribution, const int centerOfDistribution) const
        {
            bool result = true;

            const int x0 = std::max(0, (int)std::lround(centerX - r));
            const int y0 = std::max(0, (int)std::lround(centerY - r));
            const int x1 = std::min(_width - 1, (int)std::lround(centerX + r));
            const int y1 = std::min(_height - 1, (int)std::lround(centerY + r));

            T            center                = Get(centerX, centerY).Gray();
            const double normalizeDistribution = distribution[centerOfDistribution];

#pragma omp parallel for
            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; result && x <= x1; ++x) // we cannot break an omp parallel loop
                {
                    const double currentR = std::sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));

                    if (currentR <= r)
                    {
                        const T      currentGray            = Get(x, y).Gray();
                        const double normalizedDistribution = distribution[(size_t)centerOfDistribution + (size_t)currentR] / normalizeDistribution;
                        const T      maxGray                = (T)std::ceil(center * normalizedDistribution);

                        if (currentGray > maxGray)
                        {
                            result = false;
                        }
                    }
                }
            }

            return result;
        }

        bool Plot(const int x, const int y, const Color<T>& color) const
        {
            if (x < 0 || y < 0 || x >= Width() || y >= Height())
            {
                return true;
            }

            T* ptr = Buffer() + (y * _width + x) * _channels;

            if (_grayIndex != -1)
            {
                ptr[_grayIndex] = color.Gray();
            }
            else
            {
                ptr[_redIndex]   = color.Red();
                ptr[_greenIndex] = color.Green();
                ptr[_blueIndex]  = color.Blue();
            }

            if (_alphaIndex == -1)
            {
                if (color.Alpha() != std::numeric_limits<T>().max())
                {
                    throw std::runtime_error("BitmapData::Set: Cannot set alpha channel to " + std::to_string(color.Alpha()) + " in an image with " + std::to_string(_channels) + " channels.");
                }
            }
            else
            {
                ptr[_alphaIndex] = color.Alpha();
            }

            return true;
        }

        void Draw(const int x0, const int y0, const int x1, const int y1, const Color<T>& color) const
        {
            auto plot = std::bind(&BitmapData<T>::Plot, this, _1, _2, color);
            Draw(x0, y0, x1, y1, plot);
        }

        void Draw(int x0, int y0, const int x1, const int y1, const Drawer& draw) const
        {
            const int dx  = abs(x1 - x0);
            const int sx  = x0 < x1 ? 1 : -1;
            const int dy  = -abs(y1 - y0);
            const int sy  = y0 < y1 ? 1 : -1;
            int       err = dx + dy;

            while (true)
            {
                if (x0 >= 0 && y0 >= 0 && x0 < Width() && y0 < Height())
                {
                    if (!draw(x0, y0))
                    {
                        return;
                    }
                }

                if (x0 == x1 && y0 == y1)
                {
                    break;
                }
                const int e2 = 2 * err;

                if (e2 >= dy) // e_xy+e_x > 0
                {
                    err += dy;
                    x0 += sx;
                }

                if (e2 <= dx) // e_xy+e_y < 0
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        void Draw(const double x0, const double y0, const double x1, const double y1, const DrawerF& draw) const
        {
            const double len_x = x1 - x0;
            const double len_y = y1 - y0;
            const int    len   = static_cast<int>(std::ceil(std::sqrt(len_x * len_x + len_y * len_y)));

            const double dx = len_x / len;
            const double dy = len_y / len;
            double       x  = x0;
            double       y  = y0;

            for (int i = 0; i <= len; ++i)
            {
                if (!draw(x, y))
                {
                    return;
                }
                x += dx;
                y += dy;
            }
        }

        void Draw(const int x0, const int y0, const Vector& v, const Color<T>& color) const
        {
            Draw(x0, y0, std::lround(x0 + v.Vx()), std::lround(y0 + v.Vy()), color);
        }

        BitmapData& operator+=(const BitmapData& rhs)
        {
#pragma omp parallel for
            for (int j = 0; j < _height; ++j)
            {
                for (int i = 0; i < _width; ++i)
                {
                    Plot(i, j, Get(i, j) + rhs.Get(i, j));
                }
            }

            return *this; // return the result by reference
        }

        BitmapData& operator-=(const BitmapData& rhs)
        {
#pragma omp parallel for
            for (int j = 0; j < _height; ++j)
            {
                for (int i = 0; i < _width; ++i)
                {
                    Plot(i, j, Get(i, j) - rhs.Get(i, j));
                }
            }

            return *this; // return the result by reference
        }

        bool ContainsColors() const
        {
            if (_channels == 1)
            {
                return false;
            }

            T* d = Buffer();

            for (int j = 0; j < _height; j++)
            {
                for (int i = 0; i < _width; i++, d += _channels)
                {
                    for (int k = 1; k < _channels; ++k)
                    {
                        if (d[0] != d[k])
                        {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        uint8_t* ConvertToDepth8(double gamma = 0, int x = 0, int y = 0, int w = 0, int h = 0, int scaledWidth = 0, int scaledHeight = 0) const
        {
            if (w <= 0) w = Width() - x;
            if (h <= 0) h = Height() - y;
            if (scaledWidth <= 0) scaledWidth = w;
            if (scaledHeight <= 0) scaledHeight = h;

            FillGammaTable(gamma);

            size_t align, destChannels;
            switch (_channels)
            {
            case 1:
            case 2:
                destChannels = 1;
                align        = 4;
                break;
            case 3:
            case 4:
                destChannels = 4;
                align        = 1;
                break;
            default:
                throw std::runtime_error("acrion::image::BitmapData::ConvertToDepth8: Unsupported number of channels: " + std::to_string(_channels));
            }

            std::stringstream log;
            log << "Converting image to depth 8: " << x << "/" << y << " (scaled from " << w << " x " << h << " to " << scaledWidth << "), destChannels=" << destChannels;
            CBEAM_LOG_DEBUG(log.str());

            size_t         alignedWidth = (scaledWidth + align - 1) / align * align;
            unsigned char* bufferDepth8 = new unsigned char[alignedWidth * scaledHeight * destChannels];

            assert(!(reinterpret_cast<uint64_t>(bufferDepth8) & 0x3)); // make sure address is 32-bit aligned (only for debugger)

            if (scaledWidth == w && scaledHeight == h)
            {
#pragma omp parallel for
                for (int j = 0; j < h; j++)
                {
                    unsigned char* dest = bufferDepth8 + j * alignedWidth * destChannels;
                    T*             src  = Buffer() + (x + (j + y) * Width()) * _channels;

                    for (int i = 0; i < w; i++, src += _channels, dest += destChannels)
                    {
                        if (y + j >= 0 && y + j < Height() && x + i >= 0 && x + i < Width())
                        {
                            if (_channels == 3)
                            {
                                dest[0] = CalculateDisplayValue(src[2]); // B
                                dest[1] = CalculateDisplayValue(src[1]); // G
                                dest[2] = CalculateDisplayValue(src[0]); // R
                                dest[3] = 255;                           // A
                            }
                            else if (_channels == 4)
                            {
                                dest[0] = CalculateDisplayValue(src[3]); // B
                                dest[1] = CalculateDisplayValue(src[2]); // G
                                dest[2] = CalculateDisplayValue(src[1]); // R
                                dest[3] = CalculateDisplayValue(src[0]); // A
                            }
                            else
                            {
                                dest[0] = CalculateDisplayValue(src[0]);
                            }
                        }
                        else
                        {
                            for (size_t k = 0; k < destChannels; ++k)
                            {
                                dest[k] = 55;
                            }
                        }
                    }
                }
            }
            else
            {
                const double aspectRatio        = static_cast<double>(w) / h;
                const bool   destinationIsWider = static_cast<double>(scaledWidth) / scaledHeight > aspectRatio;
                const int    fillWidth          = destinationIsWider ? (int)std::lround(scaledHeight * aspectRatio) : scaledWidth;
                const int    fillHeight         = destinationIsWider ? scaledHeight : (int)std::lround(scaledWidth / aspectRatio);

                std::memset(bufferDepth8 + destChannels * fillHeight * alignedWidth, 55, destChannels * (scaledHeight - fillHeight) * alignedWidth);

#pragma omp parallel for
                for (int j = 0; j < fillHeight; j++)
                {
                    unsigned char* dest = bufferDepth8 + (j * alignedWidth + fillWidth) * destChannels;

                    for (int i = fillWidth; i < scaledWidth; i++, ++dest)
                    {
                        *dest = 55;
                    }
                }

#pragma omp parallel for
                for (int j = 0; j < fillHeight; j++)
                {
                    unsigned char* dest = bufferDepth8 + (j * alignedWidth) * destChannels;

                    for (int i = 0; i < fillWidth; i++, dest += destChannels)
                    {
                        int iSrc = x + (int)std::lround(static_cast<double>(i) * w / fillWidth);
                        int jSrc = y + (int)std::lround(static_cast<double>(j) * h / fillHeight);

                        if (jSrc >= 0 && jSrc < Height() && iSrc >= 0 && iSrc < Width())
                        {
                            const T* src = Buffer() + (jSrc * Width() + iSrc) * _channels;

                            if (_channels == 3)
                            {
                                dest[0] = CalculateDisplayValue(src[2]); // B
                                dest[1] = CalculateDisplayValue(src[1]); // G
                                dest[2] = CalculateDisplayValue(src[0]); // R
                                dest[3] = 255;                           // A
                            }
                            else if (_channels == 4)
                            {
                                dest[0] = CalculateDisplayValue(src[3]); // B
                                dest[1] = CalculateDisplayValue(src[2]); // G
                                dest[2] = CalculateDisplayValue(src[1]); // R
                                dest[3] = CalculateDisplayValue(src[0]); // A
                            }
                            else
                            {
                                dest[0] = CalculateDisplayValue(src[0]);
                            }
                        }
                    }
                }
            }

            return bufferDepth8;
        }

        std::shared_ptr<BitmapData> AbsoluteDiff(const BitmapData& other)
        {
            if (Width() != other.Width() || Height() != other.Height() || Channels() != other.Channels())
            {
                throw std::runtime_error("BitmapData::AbsoluteDiff: Bitmaps have different geometry.");
            }

            std::shared_ptr<BitmapData> result = std::make_shared<BitmapData>(Width(), Height(), Channels());

            T* d = Buffer();
            T* e = other.Buffer();
            T* r = result->Buffer();

            for (int j = 0; j < _height; j++)
            {
                for (int i = 0; i < _width; i++, d += _channels, e += _channels, r += _channels)
                {
                    for (int k = 0; k < _channels; ++k)
                    {
                        int64_t left  = (int64_t)d[k];
                        int64_t right = (int64_t)e[k];
                        int64_t abs   = std::abs(left - right);
                        r[k]          = static_cast<T>(std::min(static_cast<int64_t>(std::numeric_limits<T>::max()), std::max(static_cast<int64_t>(0), abs)));
                    }
                }
            }

            return result;
        }

    private:
        uint8_t CalculateDisplayValue(T val) const
        {
            val = std::min(std::max(val, _minDisplayedBrightness), _maxDisplayedBrightness);

            const T diff = val >= _minDisplayedBrightness ? val - _minDisplayedBrightness : 0;

            const double val0 = 255.0 * diff / (_maxDisplayedBrightness - _minDisplayedBrightness);

            if (_currentGamma == 0)
            {
                return (uint8_t)std::max(0l, std::min(255l, std::lround(val0)));
            }

            double gamma1 = _currentGamma * 2;
            if (gamma1 > 1) gamma1 = 1;

            double       delta  = 9 - _currentGamma * 6;
            const double log2   = log(2.0);
            double       factor = 256.0 / (double)(log1p((long double)_maxDisplayedBrightness - _minDisplayedBrightness) / log2 - delta);

            double result = log((double)val) / log2 - delta; // cppcheck-suppress invalidFunctionArg
            double val1   = (result <= 0) ? 0 : result * factor;
            double t      = gamma1 * val1 + (1 - gamma1) * val0;

            return (uint8_t)std::max(0l, std::min(255l, std::lround(t)));
        }

        int                                       _width{0};
        int                                       _height{0};
        int                                       _channels{0};
        cbeam::container::stable_reference_buffer _buffer; // must be after _width, _height and _channels because buffer size is initialized based on Size()!

        int _grayIndex{-1};
        int _alphaIndex{-1};
        int _redIndex{-1};
        int _greenIndex{-1};
        int _blueIndex{-1};

        T _minDisplayedBrightness{0};
        T _maxDisplayedBrightness{std::numeric_limits<T>::max()};
    };

    template <typename T>
    BitmapData<T> operator+(BitmapData<T>        lhs,
                            const BitmapData<T>& rhs)
    {
        lhs += rhs;
        return lhs;
    }

    template <typename T>
    BitmapData<T> operator-(BitmapData<T>        lhs,
                            const BitmapData<T>& rhs)
    {
        lhs -= rhs;
        return lhs;
    }

    template <typename T, typename U>
    bool operator==(const BitmapData<T>& lhs, const BitmapData<U>& rhs)
    {
        return lhs.Width() == rhs.Width()
            && lhs.Height() == rhs.Height()
            && lhs.Channels() == rhs.Channels()
            && lhs.Depth() == rhs.Depth();
    }
    template <typename T, typename U>
    bool operator!=(const BitmapData<T>& lhs, const BitmapData<U>& rhs) { return !operator==(lhs, rhs); }

    template <typename T, typename U>
    bool operator<(const BitmapData<T>& lhs, const BitmapData<U>& rhs)
    {
        return (double)lhs._width * lhs._height * lhs._channels * std::abs(lhs.Depth())
             < (double)rhs._width * rhs._height * rhs._channels * std::abs(rhs.Depth());
    }
    template <typename T, typename U>
    bool operator>(const BitmapData<T>& lhs, const BitmapData<U>& rhs) { return operator<(rhs, lhs); }
    template <typename T, typename U>
    bool operator<=(const BitmapData<T>& lhs, const BitmapData<U>& rhs) { return !operator>(lhs, rhs); }
    template <typename T, typename U>
    bool operator>=(const BitmapData<T>& lhs, const BitmapData<U>& rhs) { return !operator<(lhs, rhs); }
}
