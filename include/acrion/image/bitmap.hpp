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

#include "bitmap_data.hpp"

#include <cbeam/container/xpod.hpp>

#include <cbeam/container/nested_map.hpp>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

namespace Magick
{
    // see ImageMagick/MagickCore/magick-type.h
    // MagickCore::Quantum                     == unsigned short in case of MAGICKCORE_QUANTUM_DEPTH==16
    // MagickCore::Quantum                     == unsigned int   in case of MAGICKCORE_QUANTUM_DEPTH==32
    // MagickCore::Quantum == MagickDoubleType == double         in case of MAGICKCORE_QUANTUM_DEPTH==64 (?)
    typedef unsigned int Quantum;
}

namespace acrion::image
{
    using BitmapContainer           = cbeam::container::nested_map<cbeam::container::xpod::type, cbeam::container::xpod::type>;
    using SerializedBitmapContainer = cbeam::serialization::serialized_object;
    using BitmapDataTypes           = std::tuple<BitmapData<uint8_t>, BitmapData<uint16_t>, BitmapData<uint32_t>, BitmapData<uint64_t>, BitmapData<double>>;
    template <std::size_t N>
    using IntBitmapDataType = typename std::tuple_element<N, BitmapDataTypes>::type;

    class Bitmap
    {
    public:
        explicit Bitmap(const BitmapData<uint8_t>& bitmapData)
            : _index{0}
        {
            std::get<0>(_bitmapData) = bitmapData;
        }

        explicit Bitmap(const BitmapData<uint16_t>& bitmapData)
            : _index{1}
        {
            std::get<1>(_bitmapData) = bitmapData;
        }

        explicit Bitmap(const BitmapData<uint32_t>& bitmapData)
            : _index{2}
        {
            std::get<2>(_bitmapData) = bitmapData;
        }

        explicit Bitmap(const BitmapData<uint64_t>& bitmapData)
            : _index{3}
        {
            std::get<3>(_bitmapData) = bitmapData;
        }

        explicit Bitmap(const BitmapData<double>& bitmapData)
            : _index{4}
        {
            std::get<4>(_bitmapData) = bitmapData;
        }

        Bitmap(void* buffer, int width, int height, int channels, int depth)
        {
            switch (depth)
            {
            case 1:
                std::get<0>(_bitmapData) = BitmapData<uint8_t>(cbeam::container::stable_reference_buffer(buffer), width, height, channels);
                _index                   = 0;
                break;
            case 2:
                std::get<1>(_bitmapData) = BitmapData<uint16_t>(cbeam::container::stable_reference_buffer(buffer), width, height, channels);
                _index                   = 1;
                break;
            case 4:
                std::get<2>(_bitmapData) = BitmapData<uint32_t>(cbeam::container::stable_reference_buffer(buffer), width, height, channels);
                _index                   = 2;
                break;
            case 8:
                std::get<3>(_bitmapData) = BitmapData<uint64_t>(cbeam::container::stable_reference_buffer(buffer), width, height, channels);
                _index                   = 3;
                break;
            case -8:
                std::get<4>(_bitmapData) = BitmapData<double>(cbeam::container::stable_reference_buffer(buffer), width, height, channels);
                _index                   = 4;
                break;
            default:
                throw std::runtime_error("acrion::image::Bitmap::Construction from buffer: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        Bitmap(int width, int height, int channels, int depth)
        {
            switch (depth)
            {
            case 1:
                std::get<0>(_bitmapData) = BitmapData<uint8_t>(width, height, channels);
                _index                   = 0;
                break;
            case 2:
                std::get<1>(_bitmapData) = BitmapData<uint16_t>(width, height, channels);
                _index                   = 1;
                break;
            case 4:
                std::get<2>(_bitmapData) = BitmapData<uint32_t>(width, height, channels);
                _index                   = 2;
                break;
            case 8:
                std::get<3>(_bitmapData) = BitmapData<uint64_t>(width, height, channels);
                _index                   = 3;
                break;
            case -8:
                std::get<4>(_bitmapData) = BitmapData<double>(width, height, channels);
                _index                   = 4;
                break;
            default:
                throw std::runtime_error("acrion::image::Bitmap::Construction from buffer: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        Bitmap(const Bitmap& src)
        {
            switch (src.Depth())
            {
            case 1:
                std::get<0>(_bitmapData) = std::get<0>(src._bitmapData);
                _index                   = 0;
                break;
            case 2:
                std::get<1>(_bitmapData) = std::get<1>(src._bitmapData);
                _index                   = 1;
                break;
            case 4:
                std::get<2>(_bitmapData) = std::get<2>(src._bitmapData);
                _index                   = 2;
                break;
            case 8:
                std::get<3>(_bitmapData) = std::get<3>(src._bitmapData);
                _index                   = 3;
                break;
            case -8:
                std::get<4>(_bitmapData) = std::get<4>(src._bitmapData);
                _index                   = 4;
                break;
            default:
                throw std::runtime_error("acrion::image::Bitmap::Copy constructor: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        explicit Bitmap(const BitmapContainer& image)
            : Bitmap(image, (std::string)bufferKey)
        {
        }

        Bitmap(const BitmapContainer& image, const std::string& customBufferKey)
            : Bitmap(image.get_mapped_value_or_throw<cbeam::memory::pointer>(std::string(customBufferKey), "acrion::image::Bitmap constructor"),
                     GetWidth(image),
                     GetHeight(image),
                     GetChannels(image),
                     GetDepth(image))
        {
            SetMinDisplayedBrightness(GetMinBrightness(image));
            SetMaxDisplayedBrightness(GetMaxBrightness(image));
        }

        static constexpr std::string_view bufferKey{"imageBuffer"};
        static constexpr std::string_view widthKey{"width"};
        static constexpr std::string_view heightKey{"height"};
        static constexpr std::string_view channelsKey{"channels"};
        static constexpr std::string_view depthKey{"depth"};
        static constexpr std::string_view minBrightnessKey{"minBrightness"};
        static constexpr std::string_view maxBrightnessKey{"maxBrightness"};

        static constexpr std::array imageKeys{bufferKey, widthKey, heightKey, channelsKey, depthKey};

        explicit operator BitmapContainer() const
        {
            if (Empty())
            {
                throw std::runtime_error("Error converting image to plugin parameters: image is empty");
            }

            BitmapContainer parameters;
            parameters.data[std::string(bufferKey)]        = Buffer();
            parameters.data[std::string(widthKey)]         = Width();
            parameters.data[std::string(heightKey)]        = Height();
            parameters.data[std::string(channelsKey)]      = Channels();
            parameters.data[std::string(depthKey)]         = Depth();
            parameters.data[std::string(minBrightnessKey)] = GetMinDisplayedBrightness();
            parameters.data[std::string(maxBrightnessKey)] = GetMaxDisplayedBrightness();

            return parameters;
        }

        bool Empty() const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).Empty();
            case 1:
                return std::get<1>(_bitmapData).Empty();
            case 2:
                return std::get<2>(_bitmapData).Empty();
            case 3:
                return std::get<3>(_bitmapData).Empty();
            case 4:
                return std::get<4>(_bitmapData).Empty();
            default:
                throw std::runtime_error("acrion::image::Bitmap::Empty: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        int Width() const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).Width();
            case 1:
                return std::get<1>(_bitmapData).Width();
            case 2:
                return std::get<2>(_bitmapData).Width();
            case 3:
                return std::get<3>(_bitmapData).Width();
            case 4:
                return std::get<4>(_bitmapData).Width();
            default:
                throw std::runtime_error("acrion::image::Bitmap::Width: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        int Height() const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).Height();
            case 1:
                return std::get<1>(_bitmapData).Height();
            case 2:
                return std::get<2>(_bitmapData).Height();
            case 3:
                return std::get<3>(_bitmapData).Height();
            case 4:
                return std::get<4>(_bitmapData).Height();
            default:
                throw std::runtime_error("acrion::image::Bitmap::Height: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        int Channels() const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).Channels();
            case 1:
                return std::get<1>(_bitmapData).Channels();
            case 2:
                return std::get<2>(_bitmapData).Channels();
            case 3:
                return std::get<3>(_bitmapData).Channels();
            case 4:
                return std::get<4>(_bitmapData).Channels();
            default:
                throw std::runtime_error("acrion::image::Bitmap::Channels: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        void* Buffer() const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).Buffer();
            case 1:
                return std::get<1>(_bitmapData).Buffer();
            case 2:
                return std::get<2>(_bitmapData).Buffer();
            case 3:
                return std::get<3>(_bitmapData).Buffer();
            case 4:
                return std::get<4>(_bitmapData).Buffer();
            default:
                throw std::runtime_error("acrion::image::Bitmap::Buffer: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        int Depth() const
        {
            return _index < 4 ? (1 << _index) : -(1 << (_index - 1));
        }

        double GetMinDisplayedBrightness() const
        {
            switch (_index)
            {
            case 0:
                return (double)std::get<0>(_bitmapData).GetMinDisplayedBrightness();
            case 1:
                return (double)std::get<1>(_bitmapData).GetMinDisplayedBrightness();
            case 2:
                return (double)std::get<2>(_bitmapData).GetMinDisplayedBrightness();
            case 3:
                return (double)std::get<3>(_bitmapData).GetMinDisplayedBrightness();
            case 4:
                return std::get<4>(_bitmapData).GetMinDisplayedBrightness();
            default:
                throw std::runtime_error("acrion::image::Bitmap::GetMinDisplayedBrightness: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        double GetMaxDisplayedBrightness() const
        {
            switch (_index)
            {
            case 0:
                return (double)std::get<0>(_bitmapData).GetMaxDisplayedBrightness();
            case 1:
                return (double)std::get<1>(_bitmapData).GetMaxDisplayedBrightness();
            case 2:
                return (double)std::get<2>(_bitmapData).GetMaxDisplayedBrightness();
            case 3:
                return (double)std::get<3>(_bitmapData).GetMaxDisplayedBrightness();
            case 4:
                return std::get<4>(_bitmapData).GetMaxDisplayedBrightness();
            default:
                throw std::runtime_error("acrion::image::Bitmap::GetMaxDisplayedBrightness: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        void SetMinDisplayedBrightness(const double val)
        {
            switch (_index)
            {
            case 0:
                std::get<0>(_bitmapData).SetMinDisplayedBrightness((uint8_t)val);
                break;
            case 1:
                std::get<1>(_bitmapData).SetMinDisplayedBrightness((uint16_t)val);
                break;
            case 2:
                std::get<2>(_bitmapData).SetMinDisplayedBrightness((uint32_t)val);
                break;
            case 3:
                std::get<3>(_bitmapData).SetMinDisplayedBrightness((uint64_t)val);
                break;
            case 4:
                std::get<4>(_bitmapData).SetMinDisplayedBrightness(val);
                break;
            default:
                throw std::runtime_error("acrion::image::Bitmap::SetMinDisplayedBrightness: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        void SetMaxDisplayedBrightness(const double val)
        {
            switch (_index)
            {
            case 0:
                std::get<0>(_bitmapData).SetMaxDisplayedBrightness((uint8_t)val);
                break;
            case 1:
                std::get<1>(_bitmapData).SetMaxDisplayedBrightness((uint16_t)val);
                break;
            case 2:
                std::get<2>(_bitmapData).SetMaxDisplayedBrightness((uint32_t)val);
                break;
            case 3:
                std::get<3>(_bitmapData).SetMaxDisplayedBrightness((uint64_t)val);
                break;
            case 4:
                std::get<4>(_bitmapData).SetMaxDisplayedBrightness(val);
                break;
            default:
                throw std::runtime_error("acrion::image::Bitmap::SetMaxDisplayedBrightness: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        uint8_t* ConvertToDepth8(double gamma = 0, int x = 0, int y = 0, int width = 0, int height = 0, int scaledWidth = 0, int scaledHeight = 0) const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).ConvertToDepth8(gamma, x, y, width, height, scaledWidth, scaledHeight);
            case 1:
                return std::get<1>(_bitmapData).ConvertToDepth8(gamma, x, y, width, height, scaledWidth, scaledHeight);
            case 2:
                return std::get<2>(_bitmapData).ConvertToDepth8(gamma, x, y, width, height, scaledWidth, scaledHeight);
            case 3:
                return std::get<3>(_bitmapData).ConvertToDepth8(gamma, x, y, width, height, scaledWidth, scaledHeight);
            case 4:
                return std::get<4>(_bitmapData).ConvertToDepth8(gamma, x, y, width, height, scaledWidth, scaledHeight);
            default:
                throw std::runtime_error("acrion::image::Bitmap::ConvertToDepth8: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        std::shared_ptr<Bitmap> AbsoluteDiff(const Bitmap& other)
        {
            switch (_index)
            {
            case 0:
                return std::make_shared<Bitmap>(*std::get<0>(_bitmapData).AbsoluteDiff(std::get<0>(other._bitmapData)));
            case 1:
                return std::make_shared<Bitmap>(*std::get<1>(_bitmapData).AbsoluteDiff(std::get<1>(other._bitmapData)));
            case 2:
                return std::make_shared<Bitmap>(*std::get<2>(_bitmapData).AbsoluteDiff(std::get<2>(other._bitmapData)));
            case 3:
                return std::make_shared<Bitmap>(*std::get<3>(_bitmapData).AbsoluteDiff(std::get<3>(other._bitmapData)));
            case 4:
                return std::make_shared<Bitmap>(*std::get<4>(_bitmapData).AbsoluteDiff(std::get<4>(other._bitmapData)));
            default:
                throw std::runtime_error("acrion::image::Bitmap::AbsoluteDiff: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        bool ContainsColors() const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData).ContainsColors();
            case 1:
                return std::get<1>(_bitmapData).ContainsColors();
            case 2:
                return std::get<2>(_bitmapData).ContainsColors();
            case 3:
                return std::get<3>(_bitmapData).ContainsColors();
            case 4:
                return std::get<4>(_bitmapData).ContainsColors();
            default:
                throw std::runtime_error("acrion::image::Bitmap::ContainsColors: Unsupported image depth " + std::to_string(Depth()));
            }
        }

        template <typename T>
        explicit operator BitmapData<T>&()
        {
            return std::get<T>(_bitmapData);
        }

        template <size_t N>
        auto& Get() { return std::get<IntBitmapDataType<N>>(_bitmapData); }

        bool operator!=(const Bitmap& b) const
        {
            switch (_index)
            {
            case 0:
                return std::get<0>(_bitmapData) != std::get<0>(b._bitmapData);
            case 1:
                return std::get<1>(_bitmapData) != std::get<1>(b._bitmapData);
            case 2:
                return std::get<2>(_bitmapData) != std::get<2>(b._bitmapData);
            case 3:
                return std::get<3>(_bitmapData) != std::get<3>(b._bitmapData);
            case 4:
                return std::get<4>(_bitmapData) != std::get<4>(b._bitmapData);
            default:
                throw std::runtime_error("acrion::image::Bitmap::operator !=: Unsupported image depth " + std::to_string(Depth()));
            }
        }

    private:
        static void* GetBuffer(const BitmapContainer& image)
        {
            return image.get_mapped_value_or_throw<cbeam::memory::pointer>(std::string(bufferKey), "acrion::image::Bitmap::GetBuffer()");
        }

        static int GetWidth(const BitmapContainer& image)
        {
            return (int)image.get_mapped_value_or_throw<cbeam::container::xpod::type_index::integer>(std::string(widthKey), "acrion::image::Bitmap::GetWidth()");
        }

        static int GetHeight(const BitmapContainer& image)
        {
            return (int)image.get_mapped_value_or_throw<cbeam::container::xpod::type_index::integer>(std::string(heightKey), "acrion::image::Bitmap::GetHeight()");
        }

        static int GetChannels(const BitmapContainer& image)
        {
            return (int)image.get_mapped_value_or_throw<cbeam::container::xpod::type_index::integer>(std::string(channelsKey), "acrion::image::Bitmap::GetChannels()");
        }

        static int GetDepth(const BitmapContainer& image)
        {
            return (int)image.get_mapped_value_or_throw<cbeam::container::xpod::type_index::integer>(std::string(depthKey), "acrion::image::Bitmap::GetDepth()");
        }

        static double GetMinBrightness(const BitmapContainer& image)
        {
            return image.get_mapped_value_or_throw<double>(std::string(minBrightnessKey), "acrion::image::Bitmap::GetMinBrightness()");
        }

        static double GetMaxBrightness(const BitmapContainer& image)
        {
            return image.get_mapped_value_or_throw<double>(std::string(maxBrightnessKey), "acrion::image::Bitmap::GetMaxBrightness()");
        }

        BitmapDataTypes _bitmapData;
        int             _index{-1}; // must only be set by a constructor (although we cannot do it via initializer list)
    };
}
