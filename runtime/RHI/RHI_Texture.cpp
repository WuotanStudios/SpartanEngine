/*
Copyright(c) 2016-2025 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ================================
#include "pch.h"
#include "RHI_Texture.h"
#include "ThreadPool.h"
#include "RHI_CommandList.h"
#include "../IO/FileStream.h"
#include "../Resource/Import/ImageImporter.h"
#include "../Core/ProgressTracker.h"
SP_WARNINGS_OFF
#include "compressonator.h"
SP_WARNINGS_ON
//===========================================

//= NAMESPACES =====
using namespace std;
//==================

namespace Spartan
{
    namespace compressonator
    {
        RHI_Format destination_format = RHI_Format::BC3_Unorm;
        atomic<bool> registered = false;

        CMP_FORMAT to_cmp_format(const RHI_Format format)
        {
            // input
            if (format == RHI_Format::R8G8B8A8_Unorm)
                return CMP_FORMAT::CMP_FORMAT_RGBA_8888;

            // output
            if (format == RHI_Format::ASTC)
                return CMP_FORMAT::CMP_FORMAT_ASTC; // that's a build option in the compressonator

            if (format == RHI_Format::BC3_Unorm)
                return CMP_FORMAT::CMP_FORMAT_BC3;

            if (format == RHI_Format::BC7_Unorm)
                return CMP_FORMAT::CMP_FORMAT_BC7;

            SP_ASSERT_MSG(false, "No equivalent format");
            return CMP_FORMAT::CMP_FORMAT_Unknown;
        }

        void compress(RHI_Texture* texture, const uint32_t mip_index, const RHI_Format destination_format)
        {
            // source texture
            CMP_Texture source_texture = {};
            source_texture.format      = to_cmp_format(texture->GetFormat());
            source_texture.dwSize      = sizeof(CMP_Texture);
            source_texture.dwWidth     = texture->GetWidth() >> mip_index;
            source_texture.dwHeight    = texture->GetHeight() >> mip_index;
            source_texture.dwPitch     = source_texture.dwWidth * texture->GetBytesPerPixel();
            source_texture.dwDataSize  = static_cast<uint32_t>(texture->GetMip(0, mip_index).bytes.size());
            source_texture.pData       = reinterpret_cast<uint8_t*>(texture->GetMip(0, mip_index).bytes.data());

            // destination texture
            CMP_Texture destination_texture = {};
            destination_texture.format      = to_cmp_format(destination_format);
            destination_texture.dwSize      = sizeof(CMP_Texture);
            destination_texture.dwWidth     = source_texture.dwWidth;
            destination_texture.dwHeight    = source_texture.dwHeight;
            destination_texture.dwDataSize  = CMP_CalculateBufferSize(&destination_texture);
            vector<byte> destination_data(destination_texture.dwDataSize);
            destination_texture.pData       = reinterpret_cast<uint8_t*>(destination_data.data());

            // compress texture
            {
                CMP_CompressOptions options = {};
                options.dwSize              = sizeof(CMP_CompressOptions);
                options.fquality            = 0.05f;                            // set for lower quality, faster compression
                options.dwnumThreads        = ThreadPool::GetIdleThreadCount(); // use all free threads
                options.nEncodeWith         = CMP_HPC;                          // set encoder

                SP_ASSERT(CMP_ConvertTexture(&source_texture, &destination_texture, &options, nullptr) == CMP_OK);
            }

            // update texture with compressed data
            texture->GetMip(0, mip_index).bytes = destination_data;
        }

        void compress(RHI_Texture* texture)
        {
            SP_ASSERT(texture != nullptr);

            for (uint32_t mip_index = 0; mip_index < texture->GetMipCount(); mip_index++)
            {
                compress(texture, mip_index, destination_format);
            }

            texture->SetFormat(destination_format);
        }
    }

    namespace mips
    {
        void downsample_bilinear(const vector<byte>& input, vector<byte>& output, uint32_t width, uint32_t height)
        {
            constexpr uint32_t channels = 4; // RGBA32 - engine standard
  
            // calculate new dimensions (halving both width and height)
            uint32_t new_width  = width  >> 1;
            uint32_t new_height = height >> 1;
            
            // ensure minimum size
            if (new_width < 1) new_width = 1;
            if (new_height < 1) new_height = 1;
            
             // perform bilinear downsampling
            for (uint32_t y = 0; y < new_height; y++)
            {
                for (uint32_t x = 0; x < new_width; x++)
                {
                    // calculate base indices for this 2x2 block
                    uint32_t src_idx              = (y * 2 * width + x * 2) * channels;
                    uint32_t src_idx_right        = src_idx + channels;                      // right pixel
                    uint32_t src_idx_bottom       = src_idx + (width * channels);            // bottom pixel
                    uint32_t src_idx_bottom_right = src_idx + (width * channels) + channels; // bottom-right pixel
                    uint32_t dst_idx              = (y * new_width + x) * channels;

                    // process all 4 channels (RGBA)
                    for (uint32_t c = 0; c < channels; c++)
                    {
                        uint32_t sum = to_integer<uint32_t>(input[src_idx + c]);
                        uint32_t count = 1;
            
                        // right pixel
                        if (x * 2 + 1 < width)
                        {
                            sum += to_integer<uint32_t>(input[src_idx_right + c]);
                            count++;
                        }
            
                        // bottom pixel
                        if (y * 2 + 1 < height)
                        {
                            sum += to_integer<uint32_t>(input[src_idx_bottom + c]);
                            count++;
                        }
            
                        // bottom-right pixel
                        if ((x * 2 + 1 < width) && (y * 2 + 1 < height))
                        {
                            sum += to_integer<uint32_t>(input[src_idx_bottom_right + c]);
                            count++;
                        }
            
                        // assign the averaged result to the output
                        output[dst_idx + c] = byte(sum / count);
                    }
                }
            }
        }

        uint32_t compute_count(uint32_t width, uint32_t height)
        {
            uint32_t mip_count = 0;
            while (width > 1 || height > 1)
            {
                width  >>= 1;
                height >>= 1;
                if (width > 0 && height > 0)
                {
                    mip_count++;
                }
            }
            return mip_count;
        }
    }

    RHI_Texture::RHI_Texture() : IResource(ResourceType::Texture)
    {

    }

    RHI_Texture::RHI_Texture(
        const RHI_Texture_Type type,
        const uint32_t width,
        const uint32_t height,
        const uint32_t depth,
        const uint32_t mip_count,
        const RHI_Format format,
        const uint32_t flags,
        const char* name,
        vector<RHI_Texture_Slice> data
    ) : IResource(ResourceType::Texture)
    {
        m_type             = type;
        m_width            = width;
        m_height           = height;
        m_depth            = depth;
        m_mip_count        = mip_count;
        m_format           = format;
        m_flags            = flags;
        m_object_name      = name;
        m_slices           = data;
        m_viewport         = RHI_Viewport(0, 0, static_cast<float>(width), static_cast<float>(height));
        m_channel_count    = rhi_to_format_channel_count(format);
        m_bits_per_channel = rhi_format_to_bits_per_channel(m_format);

        PrepareForGpu();

        if (!compressonator::registered)
        {
            string version = to_string(AMD_COMPRESS_VERSION_MAJOR) + "." + to_string(AMD_COMPRESS_VERSION_MINOR);
            Settings::RegisterThirdPartyLib("AMD Compressonator", version, "https://github.com/GPUOpen-Tools/compressonator");
            compressonator::registered = true;
        }
    }

    RHI_Texture::RHI_Texture(const string& file_path) : IResource(ResourceType::Texture)
    {
        LoadFromFile(file_path);
    }

    RHI_Texture::~RHI_Texture()
    {
        RHI_DestroyResource();
    }

    void RHI_Texture::SaveToFile(const string& file_path)
    {
        // if a file already exists, get the byte count
        m_object_size = 0;
        {
            if (FileSystem::Exists(file_path))
            {
                auto file = make_unique<FileStream>(file_path, FileStream_Read);
                if (file->IsOpen())
                {
                    file->Read(&m_object_size);
                }
            }
        }

        bool append = true;
        auto file = make_unique<FileStream>(file_path, FileStream_Write | FileStream_Append);
        if (!file->IsOpen())
            return;

        // if the existing file has texture data but we don't, don't overwrite them
        bool dont_overwrite_data = m_object_size != 0 && !HasData();
        if (dont_overwrite_data)
        {
            file->Skip
            (
                sizeof(m_object_size) + // byte count
                sizeof(m_depth)       + // array length
                sizeof(m_mip_count)   + // mip count
                m_object_size           // bytes
            );
        }
        else
        {
            ComputeMemoryUsage();

            // write mip info
            file->Write(m_object_size);
            file->Write(m_depth);
            file->Write(m_mip_count);

            // write mip data
            for (RHI_Texture_Slice& slice : m_slices)
            {
                for (RHI_Texture_Mip& mip : slice.mips)
                {
                    file->Write(mip.bytes);
                }
            }

            ClearData();
        }

        // write properties
        file->Write(m_width);
        file->Write(m_height);
        file->Write(m_channel_count);
        file->Write(m_bits_per_channel);
        file->Write(static_cast<uint32_t>(m_type));
        file->Write(static_cast<uint32_t>(m_format));
        file->Write(m_flags);
        file->Write(GetObjectId());
        file->Write(GetResourceFilePath());
    }

    void RHI_Texture::LoadFromFile(const string& file_path)
    {
        if (!FileSystem::IsFile(file_path))
        {
            SP_LOG_ERROR("Invalid file path \"%s\".", file_path.c_str());
            return;
        }

        ProgressTracker::SetGlobalLoadingState(true);

        m_type            = RHI_Texture_Type::Type2D;
        m_depth           = 1;
        m_flags          |= RHI_Texture_Srv;
        m_object_name     = FileSystem::GetFileNameFromFilePath(file_path);
        m_resource_state  = ResourceState::LoadingFromDrive;

        ClearData();

        // load from drive
        if (FileSystem::IsEngineTextureFile(file_path))
        {
            auto file = make_unique<FileStream>(file_path, FileStream_Read);
            if (file->IsOpen())
            {
                // read mip info
                file->Read(&m_object_size);
                file->Read(&m_depth);
                file->Read(&m_mip_count);

                // read mip data
                m_slices.resize(m_depth);
                for (RHI_Texture_Slice& slice : m_slices)
                {
                    slice.mips.resize(m_mip_count);
                    for (RHI_Texture_Mip& mip : slice.mips)
                    {
                        file->Read(&mip.bytes);
                    }
                }

                // read properties
                file->Read(&m_width);
                file->Read(&m_height);
                file->Read(&m_channel_count);
                file->Read(&m_bits_per_channel);
                file->Read(reinterpret_cast<uint32_t*>(&m_type));
                file->Read(reinterpret_cast<uint32_t*>(&m_format));
                file->Read(&m_flags);
                SetObjectId(file->ReadAs<uint64_t>());
                SetResourceFilePath(file->ReadAs<string>());
            }
        }
        else if (FileSystem::IsSupportedImageFile(file_path))
        {
            vector<string> file_paths = { file_path };

            // if this is an array, try to find all the textures
            if (m_type == RHI_Texture_Type::Type2DArray)
            {
                string file_path_extension    = FileSystem::GetExtensionFromFilePath(file_path);
                string file_path_no_extension = FileSystem::GetFilePathWithoutExtension(file_path);
                string file_path_no_digit     = file_path_no_extension.substr(0, file_path_no_extension.size() - 1);

                uint32_t index = 1;
                string file_path_guess = file_path_no_digit + to_string(index) + file_path_extension;
                while (FileSystem::Exists(file_path_guess))
                {
                    file_paths.emplace_back(file_path_guess);
                    file_path_guess = file_path_no_digit + to_string(++index) + file_path_extension;
                }
            }

            // load texture
            for (uint32_t slice_index = 0; slice_index < static_cast<uint32_t>(file_paths.size()); slice_index++)
            {
                ImageImporter::Load(file_paths[slice_index], slice_index, this);
            }

            // set resource file path so it can be used by the resource cache.
            SetResourceFilePath(file_path);
        }

        ComputeMemoryUsage();
        m_resource_state = ResourceState::Max;

        if (!(m_flags & RHI_Texture_DontPrepareForGpu))
        { 
            PrepareForGpu();
        }

        ProgressTracker::SetGlobalLoadingState(false);
    }

    RHI_Texture_Mip& RHI_Texture::GetMip(const uint32_t array_index, const uint32_t mip_index)
    {
        static RHI_Texture_Mip empty;

        if (array_index >= m_slices.size())
            return empty;

        if (mip_index >= m_slices[array_index].mips.size())
            return empty;

        return m_slices[array_index].mips[mip_index];
    }

    RHI_Texture_Slice& RHI_Texture::GetSlice(const uint32_t array_index)
    {
        static RHI_Texture_Slice empty;

        if (array_index >= m_slices.size())
            return empty;

        return m_slices[array_index];
    }

    void RHI_Texture::AllocateMip()
    {
        if (m_slices.empty())
        { 
            m_slices.emplace_back();
        }

        RHI_Texture_Mip& mip = m_slices[0].mips.emplace_back();
        m_depth              = static_cast<uint32_t>(m_slices.size());
        m_mip_count          = static_cast<uint32_t>(m_slices[0].mips.size());
        int32_t mip_index    = static_cast<uint32_t>(m_slices[0].mips.size()) - 1;
        uint32_t width       = max(1u, m_width >> mip_index);
        uint32_t height      = max(1u, m_height >> mip_index);
        uint32_t depth       = (GetType() == RHI_Texture_Type::Type3D) ? (m_depth >> mip_index) : 1;
        size_t size_bytes    = CalculateMipSize(width, height, depth, m_format, m_bits_per_channel, m_channel_count);
        mip.bytes.resize(size_bytes);
    }

    void RHI_Texture::ComputeMemoryUsage()
    {
        m_object_size = 0;

        for (uint32_t array_index = 0; array_index < m_depth; array_index++)
        {
            for (uint32_t mip_index = 0; mip_index < m_mip_count; mip_index++)
            {
                const uint32_t mip_width  = max(1u, m_width >> mip_index);
                const uint32_t mip_height = max(1u, m_height >> mip_index);
                const uint32_t mip_depth  = (GetType() == RHI_Texture_Type::Type3D) ? (m_depth  >> mip_index) : 1;

                m_object_size += CalculateMipSize(mip_width, mip_height, m_depth, m_format, m_bits_per_channel, m_channel_count);
            }
        }
    }

    void RHI_Texture::SetLayout(const RHI_Image_Layout new_layout, RHI_CommandList* cmd_list, uint32_t mip_index /*= all_mips*/, uint32_t mip_range /*= 0*/)
    {
        const bool mip_specified = mip_index != rhi_all_mips;
        const bool ranged        = mip_specified && mip_range != 0;
        mip_index                = mip_specified ? mip_index : 0;
        mip_range                = ranged ? (mip_specified ? m_mip_count - mip_index : m_mip_count) : m_mip_count - mip_index;

        // asserts
        if (mip_specified)
        {
            SP_ASSERT(HasPerMipViews());
            SP_ASSERT(mip_range != 0);
        }

        // check if the layouts are indeed different from the new layout
        // if they are different, then find at which mip the difference starts
        bool transition_required = false;
        for (uint32_t i = mip_index; i < mip_index + mip_range; i++)
        {
            if (m_layout[i] != new_layout)
            {
                mip_index           = i;
                mip_range           = ranged ? (mip_specified ? m_mip_count - mip_index : m_mip_count) : m_mip_count - mip_index;
                transition_required = true;
                break;
            }
        }

        if (!transition_required)
            return;

        // insert memory barrier
        if (cmd_list != nullptr)
        {
            // wait in case this texture loading in another thread
            while (m_resource_state != ResourceState::PreparedForGpu)
            {
                SP_LOG_INFO("Waiting for texture \"%s\" to finish loading...", m_object_name.c_str());
                this_thread::sleep_for(chrono::milliseconds(16));
            }

            // transition
            cmd_list->InsertBarrierTexture(this, mip_index, mip_range, m_depth, m_layout[mip_index], new_layout);
        }

        // update layout
        for (uint32_t i = mip_index; i < mip_index + mip_range; i++)
        {
            m_layout[i] = new_layout;
        }
    }

    void RHI_Texture::ClearData()
    {
         m_slices.clear();
         m_slices.shrink_to_fit();
    }

    void RHI_Texture::PrepareForGpu()
    {
        SP_ASSERT_MSG(m_resource_state == ResourceState::Max, "Only unprepared textures can be prepared");
        m_resource_state = ResourceState::PreparingForGpu;

        bool is_not_compressed   = !IsCompressedFormat();                      // the bistro world loads pre-compressed textures
        bool is_material_texture = IsMaterialTexture();                        // render targets or textures which are written to in compute passes, don't need mip and compression
        bool can_be_prepared     = !(m_flags & RHI_Texture_DontPrepareForGpu); // some textures delay preperation because the material packs their data in a custom way before preparing them

        if (can_be_prepared)
        { 
            if (is_not_compressed && is_material_texture)
            {
                SP_ASSERT(!m_slices.empty());
                SP_ASSERT(!m_slices.front().mips.empty());

                // generate mip chain
                uint32_t mip_count = mips::compute_count(m_width, m_height);
                for (uint32_t mip_index = 1; mip_index < mip_count; mip_index++)
                {
                    AllocateMip();

                    mips::downsample_bilinear(
                        m_slices[0].mips[mip_index - 1].bytes, // larger
                        m_slices[0].mips[mip_index].bytes,     // smaller
                        max(1u, m_width  >> (mip_index - 1)),  // larger width
                        max(1u, m_height >> (mip_index - 1))   // larger height
                    );
                }

                // for thumbnails, find the appropriate mip level close to 128x128 and make it the only mip
                if (m_flags & RHI_Texture_Thumbnail)
                {
                    uint32_t target_mip = 0;
                    for (uint32_t i = 0; i < m_slices[0].mips.size(); i++)
                    {
                        uint32_t mip_width  = max(1u, m_width >> i);
                        uint32_t mip_height = max(1u, m_height >> i);
                        
                        if (mip_width <= 128 && mip_height <= 128)
                        {
                            target_mip = i;
                            break;
                        }
                    }

                    // move the target mip to the top
                    if (target_mip > 0)
                    {
                        m_slices[0].mips[0] = move(m_slices[0].mips[target_mip]);
                        m_width             = max(1u, m_width >> target_mip);
                        m_height            = max(1u, m_height >> target_mip);
                    }
                    
                    // clear all other mips
                    m_slices[0].mips.resize(1);
                    m_mip_count = static_cast<uint32_t>(m_slices[0].mips.size());
                }

                // compress
                bool compress       = m_flags & RHI_Texture_Compress;
                bool not_compressed = !IsCompressedFormat();
                if (compress && not_compressed)
                {
                    compressonator::compress(this);
                }
            }
            
            // upload to gpu
            SP_ASSERT(RHI_CreateResource());
        }

        // clear data
        if (!(m_flags & RHI_Texture_KeepData))
        { 
            ClearData();
        }
        ComputeMemoryUsage();

        if (m_rhi_resource)
        {
            m_resource_state = ResourceState::PreparedForGpu;
        }
        else
        {
            m_resource_state = ResourceState::Max;
        }
    }

    void RHI_Texture::SaveAsImage(const string& file_path)
    {
        SP_ASSERT_MSG(m_mapped_data != nullptr, "The texture needs to be mappable");
        ImageImporter::Save(file_path, m_width, m_height, m_channel_count, m_bits_per_channel, m_mapped_data);
        SP_LOG_INFO("Screenshot has been saved");
    }

    bool RHI_Texture::IsCompressedFormat(const RHI_Format format)
    {
        return
            format == RHI_Format::BC1_Unorm ||
            format == RHI_Format::BC3_Unorm ||
            format == RHI_Format::BC5_Unorm ||
            format == RHI_Format::BC7_Unorm ||
            format == RHI_Format::ASTC;
    }

    size_t RHI_Texture::CalculateMipSize(uint32_t width, uint32_t height, uint32_t depth, RHI_Format format, uint32_t bits_per_channel, uint32_t channel_count)
    {
        SP_ASSERT(width  > 0);
        SP_ASSERT(height > 0);
        SP_ASSERT(depth  > 0);

        if (IsCompressedFormat(format))
        {
            uint32_t block_size;
            uint32_t block_width  = 4; // default block width  for BC formats
            uint32_t block_height = 4; // default block height for BC formats
            switch (format)
            {
            case RHI_Format::BC1_Unorm:
                block_size = 8;
                break;
            case RHI_Format::BC3_Unorm:
            case RHI_Format::BC7_Unorm:
            case RHI_Format::BC5_Unorm:
                block_size = 16;
                break;
            case RHI_Format::ASTC: // VK_FORMAT_ASTC_4x4_UNORM_BLOCK
                block_width  = 4;
                block_height = 4;
                block_size  = 16;
                break;
            default:
                SP_ASSERT(false);
                return 0;
            }
            uint32_t num_blocks_wide = (width + block_width - 1) / block_width;
            uint32_t num_blocks_high = (height + block_height - 1) / block_height;
            return static_cast<size_t>(num_blocks_wide) * static_cast<size_t>(num_blocks_high) * static_cast<size_t>(depth) * static_cast<size_t>(block_size);
        }
        else
        {
            SP_ASSERT(channel_count > 0);
            SP_ASSERT(bits_per_channel > 0);
            return static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * static_cast<size_t>(channel_count) * static_cast<size_t>(bits_per_channel / 8);
        }
    }
}
