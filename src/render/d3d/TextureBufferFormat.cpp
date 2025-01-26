#include "stdafx.h"
#include "TextureBuffer.h"

namespace render::d3d
{
    SurfaceInfo calcSurfaceInfo(BufferSize size, DXGI_FORMAT format)
    {
        uint16_t bytesPerPixel = 0;
        switch (format) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
            bytesPerPixel = 4;
            break;
        default:
            return calcSurfaceInfoDxtCompressed(size, format);
        }

        return {
            .bytesPerRow = (uint32_t) bytesPerPixel * size.width,
            .rowCount = size.height
        };
    }

    SurfaceInfo calcSurfaceInfoDxtCompressed(BufferSize size, DXGI_FORMAT format)
    {
        // adapted from https://learn.microsoft.com/en-us/windows/uwp/gaming/complete-code-for-ddstextureloader

        uint16_t numBytesPerBlock = 0;
        switch (format) {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            numBytesPerBlock = 8;
            break;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            numBytesPerBlock = 16;
            break;
        default:
            assert(false, "Formats not supported!");
        }

        uint16_t numBlocksWide = size.width == 0 ? 0 : std::max(1, (size.width + 3) / 4);
        uint16_t numBlocksHigh = size.height == 0 ? 0 : std::max(1, (size.height + 3) / 4);
        return {
            .bytesPerRow = (uint32_t) numBlocksWide * numBytesPerBlock,
            .rowCount = numBlocksHigh,
        };
    }
}