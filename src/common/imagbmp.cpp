/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/imagbmp.cpp
// Purpose:     wxImage BMP,ICO and CUR handlers
// Author:      Robert Roebling, Chris Elliott
// Copyright:   (c) Robert Roebling, Chris Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"


#if wxUSE_IMAGE

#include "wx/imagbmp.h"

#ifndef WX_PRECOMP
    #ifdef __WXMSW__
        #include "wx/msw/wrapwin.h"
    #endif
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/bitmap.h"
    #include "wx/palette.h"
    #include "wx/intl.h"
    #include "wx/math.h"
#endif

#include "wx/filefn.h"
#include "wx/wfstream.h"
#include "wx/quantize.h"
#include "wx/scopedarray.h"
#include "wx/anidecod.h"
#include "wx/private/icondir.h"

// For memcpy
#include <string.h>

#include <memory>

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

#if wxUSE_ICO_CUR

static bool CanReadICOOrCUR(wxInputStream *stream, wxUint16 resourceType);

#endif // wxUSE_ICO_CUR

//-----------------------------------------------------------------------------
// wxBMPHandler
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxBMPHandler,wxImageHandler);

#if wxUSE_STREAMS

#ifndef BI_RGB
    #define BI_RGB       0
#endif

#ifndef BI_RLE8
#define BI_RLE8      1
#endif

#ifndef BI_RLE4
#define BI_RLE4      2
#endif

#ifndef BI_BITFIELDS
#define BI_BITFIELDS 3
#endif

#define poffset (line * width * 3 + column * 3)

bool wxBMPHandler::SaveFile(wxImage *image,
                            wxOutputStream& stream,
                            bool verbose)
{
    return SaveDib(image, stream, verbose, true/*IsBmp*/, false/*IsMask*/);
}

bool wxBMPHandler::SaveDib(wxImage *image,
                           wxOutputStream& stream,
                           bool verbose,
                           bool IsBmp,
                           bool IsMask)

{
    wxCHECK_MSG( image, false, wxT("invalid pointer in wxBMPHandler::SaveFile") );

    if ( !image->IsOk() )
    {
        if ( verbose )
        {
            wxLogError(_("BMP: Couldn't save invalid image."));
        }
        return false;
    }

    // For icons, save alpha channel if available.
    const bool saveAlpha = !IsBmp && image->HasAlpha();

    // get the format of the BMP file to save,
    // else (and always if alpha channel is present) use 24bpp
    unsigned format = wxBMP_24BPP;
    if ( image->HasOption(wxIMAGE_OPTION_BMP_FORMAT) && !saveAlpha )
        format = image->GetOptionInt(wxIMAGE_OPTION_BMP_FORMAT);

    wxUint16 bpp;     // # of bits per pixel
    int palette_size; // # of color map entries, ie. 2^bpp colors

    // set the bpp and appropriate palette_size, and do additional checks
    if ( (format == wxBMP_1BPP) || (format == wxBMP_1BPP_BW) )
    {
        bpp = 1;
        palette_size = 2;
    }
    else if ( format == wxBMP_4BPP )
    {
        bpp = 4;
        palette_size = 16;
    }
    else if ( (format == wxBMP_8BPP) || (format == wxBMP_8BPP_GREY) ||
              (format == wxBMP_8BPP_RED) || (format == wxBMP_8BPP_PALETTE) )
    {
        // need to set a wxPalette to use this, HOW TO CHECK IF VALID, SIZE?
        if (format == wxBMP_8BPP_PALETTE
#if wxUSE_PALETTE
                && !image->HasPalette()
#endif // wxUSE_PALETTE
            )
        {
            if ( verbose )
            {
                wxLogError(_("BMP: wxImage doesn't have own wxPalette."));
            }
            return false;
        }
        bpp = 8;
        palette_size = 256;
    }
    else  // you get 24bpp or 32bpp with alpha
    {
        format = wxBMP_24BPP;
        bpp = saveAlpha ? 32 : 24;
        palette_size = 0;
    }

    unsigned width = image->GetWidth();
    unsigned row_padding = (4 - ((width * bpp + 7) / 8) % 4) % 4; // # bytes to pad to dword
    unsigned row_width = (width * bpp + 7) / 8 + row_padding; // # of bytes per row

    struct
    {
        // BitmapHeader:
        wxUint16  magic;          // format magic, always 'BM'
        wxUint32  filesize;       // total file size, inc. headers
        wxUint32  reserved;       // for future use
        wxUint32  data_offset;    // image data offset in the file

        // BitmapInfoHeader:
        wxUint32  bih_size;       // 2nd part's size
        wxUint32  width, height;  // bitmap's dimensions
        wxUint16  planes;         // num of planes
        wxUint16  bpp;            // bits per pixel
        wxUint32  compression;    // compression method
        wxUint32  size_of_bmp;    // size of the bitmap
        wxUint32  h_res, v_res;   // image resolution in pixels-per-meter
        wxUint32  num_clrs;       // number of colors used
        wxUint32  num_signif_clrs;// number of significant colors
    } hdr;

    wxUint32 hdr_size = 14/*BitmapHeader*/ + 40/*BitmapInfoHeader*/;

    hdr.magic = wxUINT16_SWAP_ON_BE(0x4D42/*'BM'*/);
    hdr.filesize = wxUINT32_SWAP_ON_BE( hdr_size + palette_size*4 +
                                        row_width * image->GetHeight() );
    hdr.reserved = 0;
    hdr.data_offset = wxUINT32_SWAP_ON_BE(hdr_size + palette_size*4);

    hdr.bih_size = wxUINT32_SWAP_ON_BE(hdr_size - 14);
    hdr.width = wxUINT32_SWAP_ON_BE(image->GetWidth());
    if ( IsBmp )
    {
        hdr.height = wxUINT32_SWAP_ON_BE(image->GetHeight());
    }
    else
    {
        hdr.height = wxUINT32_SWAP_ON_BE(2 * image->GetHeight());
    }
    hdr.planes = wxUINT16_SWAP_ON_BE(1); // always 1 plane
    hdr.bpp = wxUINT16_SWAP_ON_BE(bpp);
    hdr.compression = 0; // RGB uncompressed
    hdr.size_of_bmp = wxUINT32_SWAP_ON_BE(row_width * image->GetHeight());

    // get the resolution from the image options  or fall back to 72dpi standard
    // for the BMP format if not specified
    int hres, vres;
    switch ( GetResolutionFromOptions(*image, &hres, &vres) )
    {
        default:
            wxFAIL_MSG( wxT("unexpected image resolution units") );
            wxFALLTHROUGH;

        case wxIMAGE_RESOLUTION_NONE:
            hres =
            vres = 72;
            wxFALLTHROUGH;// fall through to convert it to correct units

        case wxIMAGE_RESOLUTION_INCHES:
            // convert resolution in inches to resolution in centimeters
            hres = (int)(10*mm2inches*hres);
            vres = (int)(10*mm2inches*vres);
            wxFALLTHROUGH;// fall through to convert it to resolution in meters

        case wxIMAGE_RESOLUTION_CM:
            // convert resolution in centimeters to resolution in meters
            hres *= 100;
            vres *= 100;
            break;
    }

    hdr.h_res = wxUINT32_SWAP_ON_BE(hres);
    hdr.v_res = wxUINT32_SWAP_ON_BE(vres);
    hdr.num_clrs = wxUINT32_SWAP_ON_BE(palette_size); // # colors in colormap
    hdr.num_signif_clrs = 0;     // all colors are significant

    if ( IsBmp )
    {
        if (// VS: looks ugly but compilers tend to do ugly things with structs,
            //     like aligning hdr.filesize's ofset to dword :(
            // VZ: we should add padding then...
            !stream.WriteAll(&hdr.magic, 2) ||
            !stream.WriteAll(&hdr.filesize, 4) ||
            !stream.WriteAll(&hdr.reserved, 4) ||
            !stream.WriteAll(&hdr.data_offset, 4)
           )
        {
            if (verbose)
            {
                wxLogError(_("BMP: Couldn't write the file (Bitmap) header."));
                }
            return false;
        }
    }
    if ( !IsMask )
    {
        if (
            !stream.WriteAll(&hdr.bih_size, 4) ||
            !stream.WriteAll(&hdr.width, 4) ||
            !stream.WriteAll(&hdr.height, 4) ||
            !stream.WriteAll(&hdr.planes, 2) ||
            !stream.WriteAll(&hdr.bpp, 2) ||
            !stream.WriteAll(&hdr.compression, 4) ||
            !stream.WriteAll(&hdr.size_of_bmp, 4) ||
            !stream.WriteAll(&hdr.h_res, 4) ||
            !stream.WriteAll(&hdr.v_res, 4) ||
            !stream.WriteAll(&hdr.num_clrs, 4) ||
            !stream.WriteAll(&hdr.num_signif_clrs, 4)
           )
        {
            if (verbose)
            {
                wxLogError(_("BMP: Couldn't write the file (BitmapInfo) header."));
            }
            return false;
        }
    }

#if wxUSE_PALETTE
    std::unique_ptr<wxPalette> palette; // entries for quantized images
#endif // wxUSE_PALETTE
    wxScopedArray<wxUint8> rgbquad; // for the RGBQUAD bytes for the colormap
    std::unique_ptr<wxImage> q_image;   // destination for quantized image

    // if <24bpp use quantization to reduce colors for *some* of the formats
    if ( (format == wxBMP_1BPP) || (format == wxBMP_4BPP) ||
         (format == wxBMP_8BPP) || (format == wxBMP_8BPP_PALETTE) )
    {
        // make a new palette and quantize the image
        if (format != wxBMP_8BPP_PALETTE)
        {
            q_image.reset(new wxImage());

            // I get a delete error using Quantize when desired colors > 236
            int quantize = ((palette_size > 236) ? 236 : palette_size);
            // fill the destination too, it gives much nicer 4bpp images
#if wxUSE_PALETTE
            wxPalette* paletteTmp;
            wxQuantize::Quantize( *image, *q_image, &paletteTmp, quantize, nullptr,
                                  wxQUANTIZE_FILL_DESTINATION_IMAGE );
            palette.reset(paletteTmp);
#else // !wxUSE_PALETTE
            wxQuantize::Quantize( *image, *q_image, nullptr, quantize, 0,
                                  wxQUANTIZE_FILL_DESTINATION_IMAGE );
#endif // wxUSE_PALETTE/!wxUSE_PALETTE
        }
        else
        {
#if wxUSE_PALETTE
            palette.reset(new wxPalette(image->GetPalette()));
#endif // wxUSE_PALETTE
        }

        int i;
        unsigned char r, g, b;
        wxScopedArray<wxUint8> rgbquadTmp(palette_size*4);
        rgbquad.swap(rgbquadTmp);

        for (i = 0; i < palette_size; i++)
        {
#if wxUSE_PALETTE
            if ( !palette->GetRGB(i, &r, &g, &b) )
#endif // wxUSE_PALETTE
                r = g = b = 0;

            rgbquad[i*4] = b;
            rgbquad[i*4+1] = g;
            rgbquad[i*4+2] = r;
            rgbquad[i*4+3] = 0;
        }
    }
    // make a 256 entry greyscale colormap or 2 entry black & white
    else if ( (format == wxBMP_8BPP_GREY) || (format == wxBMP_8BPP_RED) ||
              (format == wxBMP_1BPP_BW) )
    {
        wxScopedArray<wxUint8> rgbquadTmp(palette_size*4);
        rgbquad.swap(rgbquadTmp);

        for ( int i = 0; i < palette_size; i++ )
        {
            // if 1BPP_BW then the value should be either 0 or 255
            wxUint8 c = (wxUint8)((i > 0) && (format == wxBMP_1BPP_BW) ? 255 : i);

            rgbquad[i*4] =
            rgbquad[i*4+1] =
            rgbquad[i*4+2] = c;
            rgbquad[i*4+3] = 0;
        }
    }

    // if the colormap was made, then it needs to be written
    if (rgbquad)
    {
        if ( !IsMask )
        {
            if ( !stream.WriteAll(rgbquad.get(), palette_size*4) )
            {
                if (verbose)
                {
                    wxLogError(_("BMP: Couldn't write RGB color map."));
                }
                return false;
            }
        }
    }

    // pointer to the image data, use quantized if available
    const unsigned char* const data = q_image && q_image->IsOk()
                                        ? q_image->GetData()
                                        : image->GetData();
    const unsigned char* const alpha = saveAlpha ? image->GetAlpha() : nullptr;

    wxScopedArray<wxUint8> buffer(row_width);
    memset(buffer.get(), 0, row_width);
    int y; unsigned x;
    long int pixel;
    const int dstPixLen = saveAlpha ? 4 : 3;

    for (y = image->GetHeight() -1; y >= 0; y--)
    {
        if ( format == wxBMP_24BPP ) // 3 bytes per pixel red,green,blue
        {
            for ( x = 0; x < width; x++ )
            {
                pixel = 3*(y*width + x);

                buffer[dstPixLen*x    ] = data[pixel+2];
                buffer[dstPixLen*x + 1] = data[pixel+1];
                buffer[dstPixLen*x + 2] = data[pixel];
                if ( saveAlpha )
                    buffer[dstPixLen*x + 3] = alpha[y*width + x];
            }
        }
        else if ((format == wxBMP_8BPP) ||       // 1 byte per pixel in color
                 (format == wxBMP_8BPP_PALETTE))
        {
            for (x = 0; x < width; x++)
            {
                pixel = 3*(y*width + x);
#if wxUSE_PALETTE
                buffer[x] = (wxUint8)palette->GetPixel( data[pixel],
                                                        data[pixel+1],
                                                        data[pixel+2] );
#else
                // FIXME: what should this be? use some std palette maybe?
                buffer[x] = 0;
#endif // wxUSE_PALETTE
            }
        }
        else if ( format == wxBMP_8BPP_GREY ) // 1 byte per pix, rgb ave to grey
        {
            for (x = 0; x < width; x++)
            {
                pixel = 3*(y*width + x);
                buffer[x] = (wxUint8)(.299*data[pixel] +
                                      .587*data[pixel+1] +
                                      .114*data[pixel+2]);
            }
        }
        else if ( format == wxBMP_8BPP_RED ) // 1 byte per pixel, red as greys
        {
            for (x = 0; x < width; x++)
            {
                buffer[x] = (wxUint8)data[3*(y*width + x)];
            }
        }
        else if ( format == wxBMP_4BPP ) // 4 bpp in color
        {
            for (x = 0; x < width; x+=2)
            {
                pixel = 3*(y*width + x);

                // fill buffer, ignore if > width
#if wxUSE_PALETTE
                buffer[x/2] = (wxUint8)(
                    ((wxUint8)palette->GetPixel(data[pixel],
                                                data[pixel+1],
                                                data[pixel+2]) << 4) |
                    (((x+1) >= width)
                     ? 0
                     : ((wxUint8)palette->GetPixel(data[pixel+3],
                                                   data[pixel+4],
                                                   data[pixel+5]) ))    );
#else
                // FIXME: what should this be? use some std palette maybe?
                buffer[x/2] = 0;
#endif // wxUSE_PALETTE
            }
        }
        else if ( format == wxBMP_1BPP ) // 1 bpp in "color"
        {
            for (x = 0; x < width; x+=8)
            {
                pixel = 3*(y*width + x);

#if wxUSE_PALETTE
                buffer[x/8] = (wxUint8)(
                                           ((wxUint8)palette->GetPixel(data[pixel], data[pixel+1], data[pixel+2]) << 7) |
                    (((x+1) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+3], data[pixel+4], data[pixel+5]) << 6)) |
                    (((x+2) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+6], data[pixel+7], data[pixel+8]) << 5)) |
                    (((x+3) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+9], data[pixel+10], data[pixel+11]) << 4)) |
                    (((x+4) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+12], data[pixel+13], data[pixel+14]) << 3)) |
                    (((x+5) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+15], data[pixel+16], data[pixel+17]) << 2)) |
                    (((x+6) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+18], data[pixel+19], data[pixel+20]) << 1)) |
                    (((x+7) >= width) ? 0 : ((wxUint8)palette->GetPixel(data[pixel+21], data[pixel+22], data[pixel+23])     ))    );
#else
                // FIXME: what should this be? use some std palette maybe?
                buffer[x/8] = 0;
#endif // wxUSE_PALETTE
            }
        }
        else if ( format == wxBMP_1BPP_BW ) // 1 bpp B&W colormap from red color ONLY
        {
            for (x = 0; x < width; x+=8)
            {
                pixel = 3*(y*width + x);

                buffer[x/8] = (wxUint8)(
                                          (((wxUint8)(data[pixel]   /128.)) << 7) |
                   (((x+1) >= width) ? 0 : (((wxUint8)(data[pixel+3] /128.)) << 6)) |
                   (((x+2) >= width) ? 0 : (((wxUint8)(data[pixel+6] /128.)) << 5)) |
                   (((x+3) >= width) ? 0 : (((wxUint8)(data[pixel+9] /128.)) << 4)) |
                   (((x+4) >= width) ? 0 : (((wxUint8)(data[pixel+12]/128.)) << 3)) |
                   (((x+5) >= width) ? 0 : (((wxUint8)(data[pixel+15]/128.)) << 2)) |
                   (((x+6) >= width) ? 0 : (((wxUint8)(data[pixel+18]/128.)) << 1)) |
                   (((x+7) >= width) ? 0 : (((wxUint8)(data[pixel+21]/128.))     ))    );
            }
        }

        if ( !stream.WriteAll(buffer.get(), row_width) )
        {
            if (verbose)
            {
                wxLogError(_("BMP: Couldn't write data."));
            }
            return false;
        }
    }

    return true;
}


namespace
{

struct BMPPalette
{
    unsigned char r, g, b;
};

struct BMPDesc
{
    int width, height, bpp, ncolors;

    int comp; // BI_RGB, BI_RLE4, BI_RLE8 or BI_BITFIELDS only supported

    wxScopedArray<BMPPalette> paletteData;

    int rmask, gmask, bmask;
    int amask = 0;
};

// This seems to be the method Windows uses for up-scaling color components.
// It works well with 4 bits or more, not so well with less. But using it
// allows tests to compare against native behavior under Windows.
inline wxUint8 UpscaleTo8Bits(wxUint8 x, unsigned nbits)
{
    if (nbits < 8)
    {
        x <<= (8 - nbits);
        x |= x >> nbits;
    }
    return x;
}

// Read the data in BMP format into the given image.
//
// The stream must be positioned at the start of the bitmap data
// (i.e., after any palette data)
bool LoadBMPData(wxImage * image, const BMPDesc& desc,
                 wxInputStream& stream, bool verbose, bool isBmp)
{
    const int width = desc.width;
    int height = desc.height;

    const int bpp = desc.bpp;
    const int ncolors = desc.ncolors;

    unsigned rshift = 0, gshift = 0, bshift = 0;
    unsigned rbits = 0, gbits = 0, bbits = 0;

    BMPPalette cmapMono[2];
    BMPPalette* cmap = nullptr;

    bool isUpsideDown = true;

    if (height < 0)
    {
        isUpsideDown = false;
        height = -height;
    }

    if (!image->Create(width, height, false /* clear */))
    {
        if ( verbose )
        {
            wxLogError( _("BMP: Couldn't allocate memory.") );
        }
        return false;
    }

    unsigned char* ptr = image->GetData();
    unsigned char* alpha = nullptr;

    // Reading the palette, if it exists:
    if ( bpp < 16 && ncolors != 0 )
    {
        // Use the data from the bitmap header if we have it.
        cmap = desc.paletteData.get();
        if ( !cmap )
        {
            // Otherwise allocate it here to use when reading ICO file mask: in
            // this case we have just 2 colours, black and white.
            cmap = cmapMono;

            cmap[0].r = cmap[0].g = cmap[0].b = 0;
            cmap[1].r = cmap[1].g = cmap[1].b = 255;
        }

#if wxUSE_PALETTE
        wxScopedArray<unsigned char>
             r(ncolors),
             g(ncolors),
             b(ncolors);

        for (int j = 0; j < ncolors; j++)
        {
            r[j] = cmap[j].r;
            g[j] = cmap[j].g;
            b[j] = cmap[j].b;
        }
        // Set the palette for the wxImage
        image->SetPalette(wxPalette(ncolors, r.get(), g.get(), b.get()));
#endif // wxUSE_PALETTE
    }
    else if ( bpp == 16 || bpp == 32 )
    {
        wxUint32 rmask, gmask, bmask;
        wxUint32 amask = 0;

        if ( desc.comp == BI_BITFIELDS )
        {
            rmask = desc.rmask;
            gmask = desc.gmask;
            bmask = desc.bmask;

            // Windows ignores alpha unless the format is 8-bit ARGB
            if ( rmask == 0x00FF0000 &&
                 gmask == 0x0000FF00 &&
                 bmask == 0x000000FF )
            {
                amask = desc.amask;
            }
        }
        else if ( bpp == 16 )
        {
            rmask = 0x7C00;
            gmask = 0x03E0;
            bmask = 0x001F;
        }
        else // bpp == 32
        {
            rmask = 0x00FF0000;
            gmask = 0x0000FF00;
            bmask = 0x000000FF;
            if (!isBmp)
                amask = 0xFF000000;
        }

        if (amask == 0xFF000000)
        {
            image->SetAlpha();
            alpha = image->GetAlpha();
            if (!alpha)
            {
                if (verbose)
                {
                    wxLogError(_("BMP: Couldn't allocate memory."));
                }
                return false;
            }
        }

        // Determine shift counts and move masks to low byte,
        // discarding lowest bits of any mask with more than 8 bits
        for (; rmask && ((rmask & 1) == 0 || rmask > 0xff); rmask >>= 1)
            rshift++;
        for (; gmask && ((gmask & 1) == 0 || gmask > 0xff); gmask >>= 1)
            gshift++;
        for (; bmask && ((bmask & 1) == 0 || bmask > 0xff); bmask >>= 1)
            bshift++;
        // Count mask bits
        for (; rmask; rmask >>= 1)
            rbits++;
        for (; gmask; gmask >>= 1)
            gbits++;
        for (; bmask; bmask >>= 1)
            bbits++;
    }

    // RLE-compressed bitmaps do not necessarily specify every pixel explicitly,
    // as the delta escape sequence allows offsetting the current pixel position.
    // They therefore have an implicit background, which is either:
    //  1. The colour of the first entry in the colour table
    //     (as done by LoadImage() with LR_CREATEDIBSECTION)
    //  2. Black (as done by functions like LoadBitmap() and CreateDIBitmap())
    // wxWidgets has historically implemented (1). See #23638
    if ( desc.comp == BI_RLE4 || desc.comp == BI_RLE8 )
    {
        unsigned char* pPix = ptr;
        while ( pPix < ptr + width * height * 3 )
        {
            *pPix++ = cmap[0].r;
            *pPix++ = cmap[0].g;
            *pPix++ = cmap[0].b;
        }
    }

    int linesize = ((width * bpp + 31) / 32) * 4;

    // flag used to detect fully transparent alpha channels, as
    // the alpha will be discarded in that case
    bool hasNonTransparentAlpha = false;

    for ( int row = 0; row < height; row++ )
    {
        wxUint8 aByte;
        int line = isUpsideDown ? height - 1 - row : row;

        int linepos = 0;
        for ( int column = 0; column < width ; )
        {
            if ( bpp < 16 )
            {
                linepos++;
                aByte = stream.GetC();
                if ( !stream.IsOk() )
                    return false;

                if ( bpp == 1 )
                {
                    for (int bit = 0; bit < 8 && column < width; bit++)
                    {
                        int index = ((aByte & (0x80 >> bit)) ? 1 : 0);
                        ptr[poffset] = cmap[index].r;
                        ptr[poffset + 1] = cmap[index].g;
                        ptr[poffset + 2] = cmap[index].b;
                        column++;
                    }
                }
                else if ( bpp == 4 )
                {
                    if ( desc.comp == BI_RLE4 )
                    {
                        wxUint8 first;
                        first = aByte;
                        aByte = stream.GetC();
                        if ( !stream.IsOk() )
                            return false;

                        if ( first == 0 )
                        {
                            // This is an escape sequence with special meaning.
                            if ( aByte == 0 )
                            {
                                // end of scanline marker
                                // This is ignored if the end-of-line was
                                // implicitly assumed when column==width,
                                // in which case column is now 0.
                                if (column != 0)
                                    column = width;
                            }
                            else if ( aByte == 1 )
                            {
                                // end of RLE data marker, stop decoding
                                column = width;
                                row = height;
                            }
                            else if ( aByte == 2 )
                            {
                                // delta marker, move in image

                                // process column offset
                                aByte = stream.GetC();
                                if ( !stream.IsOk() )
                                    return false;
                                column += aByte;

                                // process row offset
                                aByte = stream.GetC();
                                if ( !stream.IsOk() )
                                    return false;
                                row += aByte;
                                if (row >= height)
                                    return false;
                                line = isUpsideDown ? height - 1 - row : row;
                            }
                            else
                            {
                                // absolute mode (pixels not runs)
                                int absolute = aByte;
                                wxUint8 nibble[2] ;
                                int readBytes = 0 ;
                                for (int k = 0; k < absolute; k++)
                                {
                                    if ( !(k % 2 ) )
                                    {
                                        ++readBytes ;
                                        aByte = stream.GetC();
                                        if ( !stream.IsOk() )
                                            return false;
                                        nibble[0] = (wxUint8)( (aByte & 0xF0) >> 4 ) ;
                                        nibble[1] = (wxUint8)( aByte & 0x0F ) ;
                                    }
                                    ptr[poffset    ] = cmap[nibble[k%2]].r;
                                    ptr[poffset + 1] = cmap[nibble[k%2]].g;
                                    ptr[poffset + 2] = cmap[nibble[k%2]].b;
                                    column++;
                                }
                                if ( readBytes & 0x01 )
                                {
                                    aByte = stream.GetC();
                                    if ( !stream.IsOk() )
                                        return false;
                                }
                            }
                        }
                        else
                        {
                            wxUint8 nibble[2] ;
                            nibble[0] = (wxUint8)( (aByte & 0xF0) >> 4 ) ;
                            nibble[1] = (wxUint8)( aByte & 0x0F ) ;

                            for ( int l = 0; l < first && column < width; l++ )
                            {
                                ptr[poffset    ] = cmap[nibble[l%2]].r;
                                ptr[poffset + 1] = cmap[nibble[l%2]].g;
                                ptr[poffset + 2] = cmap[nibble[l%2]].b;
                                column++;
                            }
                        }
                    }
                    else
                    {
                        for (int nibble = 0; nibble < 2 && column < width; nibble++)
                        {
                            int index = ((aByte & (0xF0 >> (nibble * 4))) >> (!nibble * 4));
                            if ( index >= 16 )
                                index = 15;
                            ptr[poffset] = cmap[index].r;
                            ptr[poffset + 1] = cmap[index].g;
                            ptr[poffset + 2] = cmap[index].b;
                            column++;
                        }
                    }
                }
                else if ( bpp == 8 )
                {
                    if ( desc.comp == BI_RLE8 )
                    {
                        unsigned char first;
                        first = aByte;
                        aByte = stream.GetC();
                        if ( !stream.IsOk() )
                            return false;

                        if ( first == 0 )
                        {
                            if ( aByte == 0 )
                            {
                                // end of scanline marker
                                // This is ignored if the end-of-line was
                                // implicitly assumed when column==width,
                                // in which case column is now 0.
                                if (column != 0)
                                    column = width;
                            }
                            else if ( aByte == 1 )
                            {
                                // end of RLE data marker, stop decoding
                                column = width;
                                row = height;
                            }
                            else if ( aByte == 2 )
                            {
                                // delta marker, move in image

                                // process column offset
                                aByte = stream.GetC();
                                if ( !stream.IsOk() )
                                    return false;
                                column += aByte;

                                // process row offset
                                aByte = stream.GetC();
                                if ( !stream.IsOk() )
                                    return false;
                                row += aByte;
                                if (row >= height)
                                    return false;
                                line = isUpsideDown ? height - 1 - row : row;
                            }
                            else
                            {
                                // absolute mode (pixels not runs)
                                int absolute = aByte;
                                for (int k = 0; k < absolute; k++)
                                {
                                    aByte = stream.GetC();
                                    if ( !stream.IsOk() )
                                        return false;
                                    ptr[poffset    ] = cmap[aByte].r;
                                    ptr[poffset + 1] = cmap[aByte].g;
                                    ptr[poffset + 2] = cmap[aByte].b;
                                    column++;
                                }
                                if ( absolute & 0x01 )
                                {
                                    aByte = stream.GetC();
                                    if ( !stream.IsOk() )
                                        return false;
                                }
                            }
                        }
                        else
                        {
                            // encoded mode (repeat aByte first times)
                            for ( int l = 0; l < first && column < width; l++ )
                            {
                                ptr[poffset    ] = cmap[aByte].r;
                                ptr[poffset + 1] = cmap[aByte].g;
                                ptr[poffset + 2] = cmap[aByte].b;
                                column++;
                            }
                        }
                    }
                    else
                    {
                        ptr[poffset    ] = cmap[aByte].r;
                        ptr[poffset + 1] = cmap[aByte].g;
                        ptr[poffset + 2] = cmap[aByte].b;
                        column++;
                        // linepos += size;    seems to be wrong, RR
                    }
                }
            }
            else if ( bpp == 24 )
            {
                wxUint8 bbuf[4];
                if ( !stream.ReadAll(bbuf, 3) )
                    return false;
                linepos += 3;
                ptr[poffset    ] = bbuf[2];
                ptr[poffset + 1] = bbuf[1];
                ptr[poffset + 2] = bbuf[0];
                column++;
            }
            else if ( bpp == 16 )
            {
                wxUint16 aWord;
                if ( !stream.ReadAll(&aWord, 2) )
                    return false;
                wxUINT16_SWAP_ON_BE_IN_PLACE(aWord);
                linepos += 2;

                ptr[poffset    ] = UpscaleTo8Bits(aWord >> rshift, rbits);
                ptr[poffset + 1] = UpscaleTo8Bits(aWord >> gshift, gbits);
                ptr[poffset + 2] = UpscaleTo8Bits(aWord >> bshift, bbits);
                column++;
            }
            else
            {
                wxUint32 aDword;
                if ( !stream.ReadAll(&aDword, 4) )
                    return false;

                wxUINT32_SWAP_ON_BE_IN_PLACE(aDword);
                linepos += 4;
                ptr[poffset    ] = UpscaleTo8Bits(aDword >> rshift, rbits);
                ptr[poffset + 1] = UpscaleTo8Bits(aDword >> gshift, gbits);
                ptr[poffset + 2] = UpscaleTo8Bits(aDword >> bshift, bbits);
                if ( alpha )
                {
                    wxUint8 temp = aDword >> 24;
                    alpha[line * width + column] = temp;

                    if (temp != wxALPHA_TRANSPARENT)
                        hasNonTransparentAlpha = true;
                }
                column++;
            }
        }
        while ( (linepos < linesize) && (desc.comp != BI_RLE8) && (desc.comp != BI_RLE4) )
        {
            ++linepos;
            if ( !stream.ReadAll(&aByte, 1) )
                break;
        }
    }

    image->SetMask(false);

    if (alpha && !hasNonTransparentAlpha)
    {
        // discard alpha if it is all zeros
        image->ClearAlpha();
    }

    const wxStreamError err = stream.GetLastError();
    return err == wxSTREAM_NO_ERROR || err == wxSTREAM_EOF;
}

} // anonymous namespace

bool wxBMPHandler::LoadDib(wxImage *image, wxInputStream& stream,
                           bool verbose, bool IsBmp)
{
    wxUint16        aWord;
    wxInt32         dbuf[4];

    // offset to bitmap data
    wxFileOffset offset;
    // DIB header size (used to distinguish different versions of DIB header)
    wxInt32 hdrSize;
    if ( IsBmp )
    {
        // read the header off the .BMP format file
        if ( !stream.ReadAll(&aWord, 2) ||
             !stream.ReadAll(dbuf, 16) )
            return false;

        #if 0 // unused
            wxInt32 size = wxINT32_SWAP_ON_BE(dbuf[0]);
        #endif
        offset = wxINT32_SWAP_ON_BE(dbuf[2]);
        hdrSize = wxINT32_SWAP_ON_BE(dbuf[3]);
    }
    else
    {
        if ( !stream.ReadAll(dbuf, 4) )
            return false;

        offset = 0; // not used in loading ICO/CUR DIBs
        hdrSize = wxINT32_SWAP_ON_BE(dbuf[0]);
    }

    // Bitmap files come in old v1 format using BITMAPCOREHEADER or a newer
    // format (typically BITMAPV5HEADER, but we don't
    // really support any features specific to later formats such as gamma
    // correction or ICC profiles, so it doesn't matter much to us).
    const bool usesV1 = hdrSize == 12;

    BMPDesc desc;
    if ( usesV1 )
    {
        wxInt16 buf[2];
        if ( !stream.ReadAll(buf, sizeof(buf)) )
            return false;

        desc.width = wxINT16_SWAP_ON_BE((short)buf[0]);
        desc.height = wxINT16_SWAP_ON_BE((short)buf[1]);
    }
    else // We have at least BITMAPINFOHEADER
    {
        if ( !stream.ReadAll(dbuf, 4 * 2) )
            return false;

        desc.width = wxINT32_SWAP_ON_BE((int)dbuf[0]);
        desc.height = wxINT32_SWAP_ON_BE((int)dbuf[1]);
    }
    if ( !IsBmp) desc.height /= 2; // for icons divide by 2

    if ( desc.width > 32767 )
    {
        if (verbose)
        {
            wxLogError( _("DIB Header: Image width > 32767 pixels for file.") );
        }
        return false;
    }
    if ( desc.height > 32767 )
    {
        if (verbose)
        {
            wxLogError( _("DIB Header: Image height > 32767 pixels for file.") );
        }
        return false;
    }
    if (desc.width <= 0 || desc.height == 0)
        return false;

    if ( !stream.ReadAll(&aWord, 2) )
        return false;

    /*
            TODO
            int planes = (int)wxUINT16_SWAP_ON_BE( aWord );
        */
    if ( !stream.ReadAll(&aWord, 2) )
        return false;

    desc.bpp = wxUINT16_SWAP_ON_BE((int)aWord);
    switch ( desc.bpp )
    {
        case 1:
        case 4:
        case 8:
        case 16:
        case 24:
        case 32:
            // OK
            break;

        default:
            if (verbose)
            {
                wxLogError( _("DIB Header: Unknown bitdepth in file.") );
            }
            return false;
    }

    class Resolution
    {
    public:
        Resolution()
        {
            m_valid = false;

            // Still initialize them as some compilers are smart enough to
            // give "use of possibly uninitialized variable" for them (but not
            // smart enough to see that this is not really the case).
            m_x =
            m_y = 0;
        }

        void Init(int x, int y)
        {
            m_x = x;
            m_y = y;
            m_valid = true;
        }

        bool IsValid() const { return m_valid; }

        int GetX() const { return m_x; }
        int GetY() const { return m_y; }

    private:
        int m_x, m_y;
        bool m_valid;
    } res;

    int hdrBytesRead = 0;
    if ( usesV1 )
    {
        // The only possible format is BI_RGB and colours count is not used.
        desc.comp = BI_RGB;
        desc.ncolors = 0;
    }
    else // We have at least BITMAPINFOHEADER
    {
        if ( !stream.ReadAll(dbuf, 4 * 4) )
            return false;

        // Sanity check: encoding must be consistent with the depth.
        bool mismatch = false;

        desc.comp = wxINT32_SWAP_ON_BE((int)dbuf[0]);
        switch ( desc.comp )
        {
            case BI_RGB:
                break;

            case BI_RLE4:
                if ( desc.bpp != 4 )
                    mismatch = true;
                break;

            case BI_RLE8:
                if ( desc.bpp != 8 )
                    mismatch = true;
                break;

            case BI_BITFIELDS:
                if ( desc.bpp != 16 && desc.bpp != 32 )
                    mismatch = true;
                break;

            default:
                if (verbose)
                {
                    wxLogError( _("DIB Header: Unknown encoding in file.") );
                }
                return false;
        }

        if ( mismatch )
        {
            if (verbose)
            {
                wxLogError( _("DIB Header: Encoding doesn't match bitdepth.") );
            }
            return false;
        }

        res.Init(dbuf[2]/100, dbuf[3]/100);

        if ( !stream.ReadAll(dbuf, 4 * 2) )
            return false;

        desc.ncolors = wxINT32_SWAP_ON_BE( (int)dbuf[0] );
        if ( desc.ncolors < 0 || 256 < desc.ncolors )
        {
            if ( verbose )
            {
                wxLogError(_("BMP Header: Invalid number of colors (%d)."),
                           desc.ncolors);
            }
            return false;
        }

        // We've read all BITMAPINFOHEADER data so far, but we may have to
        // forward the stream to after the actual bitmap header as there could
        // be more BITMAPV4HEADER or BITMAPV5HEADER fields following.
        //
        // Note: hardcode its size as struct BITMAPINFOHEADER is not defined on
        // non-MSW platforms.
        hdrBytesRead = 40 /* sizeof(BITMAPINFOHEADER) */;

        if ( desc.comp == BI_BITFIELDS )
        {
            // Read the mask values from the header.
            if ( !stream.ReadAll(dbuf, hdrSize >= 56 ? 4 * 4 : 4 * 3) )
                return false;

            hdrBytesRead += 4 * 3;

            desc.rmask = wxINT32_SWAP_ON_BE(dbuf[0]);
            desc.gmask = wxINT32_SWAP_ON_BE(dbuf[1]);
            desc.bmask = wxINT32_SWAP_ON_BE(dbuf[2]);

            if (hdrSize >= 56)
            {
                hdrBytesRead += 4;
                desc.amask = wxINT32_SWAP_ON_BE(dbuf[3]);
            }
        }

        // Now that we've read everything we needed from the header, advance
        // past it.
        if ( hdrSize > hdrBytesRead )
        {
            if ( stream.SeekI(hdrSize - hdrBytesRead, wxFromCurrent) == wxInvalidOffset )
                return false;
        }
    }

    // We must have read the header entirely by now and we also read the 14
    // bytes preceding it: "BM" signature and 3 other DWORDs.
    // And possibly component masks.
    wxFileOffset bytesRead = 14 + wxMax(hdrSize, hdrBytesRead);

    // We must have a palette for 1bpp, 4bpp and 8bpp bitmaps.
    if (desc.ncolors == 0 && desc.bpp < 16)
        desc.ncolors = 1 << desc.bpp;

    // Now read the palette data, which follows the header, if we have it.
    if ( desc.ncolors )
    {
        const int paletteEntrySize = usesV1 ? 3 : 4;

        const int paletteSize = paletteEntrySize*desc.ncolors;

        wxScopedArray<unsigned char> paletteData(paletteSize);
        unsigned char* data = paletteData.get();
        if ( !stream.ReadAll(data, paletteSize) )
            return false;

        bytesRead += paletteSize;

        // Copy it into the format the existing code uses: this could probably
        // be optimized to avoid copying, but palette size is small enough for
        // an extra copy not to really matter.
        desc.paletteData.reset(new BMPPalette[paletteSize]);
        for ( int n = 0; n < desc.ncolors; ++n, data += paletteEntrySize )
        {
            BMPPalette& entry = desc.paletteData[n];

            // This is not a typo: entries are actually in BGR order.
            entry.b = data[0];
            entry.g = data[1];
            entry.r = data[2];
        }
    }

    // We may have a gap between the palette and start of the pixel data, skip
    // it if necessary.
    if ( bytesRead < offset )
    {
        if ( stream.SeekI(offset - bytesRead, wxFromCurrent) == wxInvalidOffset )
            return false;
    }

    //read DIB; this is the BMP image or the XOR part of an icon image
    if ( !LoadBMPData(image, desc, stream, verbose, IsBmp) )
    {
        if (verbose)
        {
            wxLogError( _("Error in reading image DIB.") );
        }
        return false;
    }

    if ( !IsBmp )
    {
        //read Icon mask which is monochrome
        BMPDesc descMask;
        descMask.width = desc.width;
        descMask.height = desc.height;
        descMask.bpp = 1;
        descMask.ncolors = 2;
        descMask.comp = BI_RGB;

        //there is no palette, so we will create one
        wxImage mask;
        if ( !LoadBMPData(&mask, descMask, stream, verbose, IsBmp) )
        {
            if (verbose)
            {
                wxLogError( _("ICO: Error in reading mask DIB.") );
            }
            return false;
        }
        image->SetMaskFromImage(mask, 255, 255, 255);

    }

    // the resolution in the bitmap header is in meters, convert to centimeters
    if ( res.IsValid() )
    {
        image->SetOption(wxIMAGE_OPTION_RESOLUTIONUNIT, wxIMAGE_RESOLUTION_CM);
        image->SetOption(wxIMAGE_OPTION_RESOLUTIONX, res.GetX());
        image->SetOption(wxIMAGE_OPTION_RESOLUTIONY, res.GetY());
    }

    return true;
}

bool wxBMPHandler::LoadFile(wxImage *image, wxInputStream& stream,
                            bool verbose, int WXUNUSED(index))
{
    // Read a single DIB fom the file:
    return LoadDib(image, stream, verbose, true/*isBmp*/);
}

bool wxBMPHandler::DoCanRead(wxInputStream& stream)
{
    unsigned char hdr[2];

    if ( !stream.ReadAll(hdr, WXSIZEOF(hdr)) )     // it's ok to modify the stream position here
        return false;

    // do we have the BMP file signature?
    return hdr[0] == 'B' && hdr[1] == 'M';
}

#endif // wxUSE_STREAMS


#if wxUSE_ICO_CUR
//-----------------------------------------------------------------------------
// wxICOHandler
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxICOHandler, wxBMPHandler);

#if wxUSE_STREAMS

bool wxICOHandler::SaveFile(wxImage *image,
                            wxOutputStream& stream,
                            bool verbose)

{
    // sanity check; icon must be no larger than 256x256
    if ( image->GetHeight () > 256 )
    {
        if ( verbose )
        {
            wxLogError(_("ICO: Image too tall for an icon."));
        }
        return false;
    }
    if ( image->GetWidth () > 256 )
    {
        if ( verbose )
        {
            wxLogError(_("ICO: Image too wide for an icon."));
        }
        return false;
    }

    const int images = 1; // only generate one image

    // VS: This is a hack of sort - since ICO and CUR files are almost
    //     identical, we have all the meat in wxICOHandler and check for
    //     the actual (handler) type when the code has to distinguish between
    //     the two formats
    int type = (this->GetType() == wxBITMAP_TYPE_CUR) ? 2 : 1;

    // write a header, (ICONDIR)
    // Calculate the header size
    wxUint32 offset = 3 * sizeof(wxUint16);

    ICONDIR IconDir;
    IconDir.idReserved = 0;
    IconDir.idType = wxUINT16_SWAP_ON_BE((wxUint16)type);
    IconDir.idCount = wxUINT16_SWAP_ON_BE((wxUint16)images);
    if ( !stream.WriteAll(&IconDir.idReserved, sizeof(IconDir.idReserved)) ||
         !stream.WriteAll(&IconDir.idType, sizeof(IconDir.idType)) ||
         !stream.WriteAll(&IconDir.idCount, sizeof(IconDir.idCount)) )
    {
        if ( verbose )
        {
            wxLogError(_("ICO: Error writing the image file!"));
        }
        return false;
    }

    // for each iamage write a description ICONDIRENTRY:
    ICONDIRENTRY icondirentry;
    for (int img = 0; img < images; img++)
    {
        wxImage mask;

        if ( image->HasMask() )
        {
            // make another image with black/white:
            mask = image->ConvertToMono (image->GetMaskRed(), image->GetMaskGreen(), image->GetMaskBlue() );

            // now we need to change the masked regions to black:
            unsigned char r = image->GetMaskRed();
            unsigned char g = image->GetMaskGreen();
            unsigned char b = image->GetMaskBlue();
            if ( (r != 0) || (g != 0) || (b != 0) )
            {
                // Go round and apply black to the masked bits:
                int i, j;
                for (i = 0; i < mask.GetWidth(); i++)
                {
                    for (j = 0; j < mask.GetHeight(); j++)
                    {
                        if ((r == mask.GetRed(i, j)) &&
                            (g == mask.GetGreen(i, j))&&
                            (b == mask.GetBlue(i, j)) )
                                image->SetRGB(i, j, 0, 0, 0 );
                    }
                }
            }
        }
        else
        {
            // just make a black mask all over:
            mask = image->Copy();
            int i, j;
            for (i = 0; i < mask.GetWidth(); i++)
                for (j = 0; j < mask.GetHeight(); j++)
                    mask.SetRGB(i, j, 0, 0, 0 );
        }
        // Set the formats for image and mask

        // The format depends on the number of the colours used, so count them,
        // but stop at 257 because we have to use 24 bpp anyhow if we have that
        // many of them.
        const int colours = image->CountColours(257);
        int bppFormat;
        int bpp;
        if ( image->HasAlpha() )
        {
            // Icons with alpha channel are always stored in ARGB format.
            bppFormat = wxBMP_24BPP;
            bpp = 32;
        }
        else if ( colours > 256 )
        {
            bppFormat = wxBMP_24BPP;
            bpp = 24;
        }
        else if ( colours > 16 )
        {
            bppFormat = wxBMP_8BPP;
            bpp = 8;
        }
        else if ( colours > 2 )
        {
            bppFormat = wxBMP_4BPP;
            bpp = 4;
        }
        else
        {
            bppFormat = wxBMP_1BPP;
            bpp = 1;
        }
        image->SetOption(wxIMAGE_OPTION_BMP_FORMAT, bppFormat);

        // monochrome bitmap:
        mask.SetOption(wxIMAGE_OPTION_BMP_FORMAT, wxBMP_1BPP_BW);
        bool IsBmp = false;
        bool IsMask = false;

        //calculate size and offset of image and mask
        wxCountingOutputStream cStream;
        bool bResult;
#if wxUSE_LIBPNG
        // Typically, icons larger then 128x128 are saved as PNG images.
        bool saveAsPNG = false;
        if ( image->GetHeight() > 128 || image->GetWidth() > 128 )
        {
            wxPNGHandler handlerPNG;
            bResult = handlerPNG.SaveFile(image, cStream, verbose);
            if ( !bResult )
            {
                if ( verbose )
                {
                    wxLogError(_("ICO: Error writing the image file!"));
                }
                return false;
            }

            saveAsPNG = true;
        }
        if ( !saveAsPNG )
#endif // wxUSE_LIBPNG
        {
            bResult = SaveDib(image, cStream, verbose, IsBmp, IsMask);
            if ( !bResult )
            {
                if ( verbose )
                {
                    wxLogError(_("ICO: Error writing the image file!"));
                }
                return false;
            }
            IsMask = true;

            bResult = SaveDib(&mask, cStream, verbose, IsBmp, IsMask);
            if ( !bResult )
            {
                if ( verbose )
                {
                    wxLogError(_("ICO: Error writing the image file!"));
                }
                return false;
            }
        }
        wxUint32 Size = cStream.GetSize();

        // wxCountingOutputStream::IsOk() always returns true for now and this
        // "if" provokes VC++ warnings in optimized build
#if 0
        if ( !cStream.IsOk() )
        {
            if ( verbose )
            {
                wxLogError(_("ICO: Error writing the image file!"));
            }
            return false;
        }
#endif // 0

        offset = offset + sizeof(ICONDIRENTRY);

        // Notice that the casts work correctly for width/height of 256 as it's
        // represented by 0 in ICO file format -- and larger values are not
        // allowed at all.
        icondirentry.bWidth = (wxUint8)image->GetWidth();
        icondirentry.bHeight = (wxUint8)image->GetHeight();
        icondirentry.bColorCount = 0;
        icondirentry.bReserved = 0;
        icondirentry.wPlanes = wxUINT16_SWAP_ON_BE(1);
        icondirentry.wBitCount = wxUINT16_SWAP_ON_BE(bpp);
        if ( type == 2 /*CUR*/)
        {
            int hx = image->HasOption(wxIMAGE_OPTION_CUR_HOTSPOT_X) ?
                         image->GetOptionInt(wxIMAGE_OPTION_CUR_HOTSPOT_X) :
                         image->GetWidth() / 2;
            int hy = image->HasOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y) ?
                         image->GetOptionInt(wxIMAGE_OPTION_CUR_HOTSPOT_Y) :
                         image->GetHeight() / 2;

            // actually write the values of the hot spot here:
            icondirentry.wPlanes = wxUINT16_SWAP_ON_BE((wxUint16)hx);
            icondirentry.wBitCount = wxUINT16_SWAP_ON_BE((wxUint16)hy);
        }
        icondirentry.dwBytesInRes = wxUINT32_SWAP_ON_BE(Size);
        icondirentry.dwImageOffset = wxUINT32_SWAP_ON_BE(offset);

        // increase size to allow for the data written:
        offset += Size;

        // write to stream:
        if ( !stream.WriteAll(&icondirentry.bWidth, sizeof(icondirentry.bWidth)) ||
             !stream.WriteAll(&icondirentry.bHeight, sizeof(icondirentry.bHeight)) ||
             !stream.WriteAll(&icondirentry.bColorCount, sizeof(icondirentry.bColorCount)) ||
             !stream.WriteAll(&icondirentry.bReserved, sizeof(icondirentry.bReserved)) ||
             !stream.WriteAll(&icondirentry.wPlanes, sizeof(icondirentry.wPlanes)) ||
             !stream.WriteAll(&icondirentry.wBitCount, sizeof(icondirentry.wBitCount)) ||
             !stream.WriteAll(&icondirentry.dwBytesInRes, sizeof(icondirentry.dwBytesInRes)) ||
             !stream.WriteAll(&icondirentry.dwImageOffset, sizeof(icondirentry.dwImageOffset)) )
        {
            if ( verbose )
            {
                wxLogError(_("ICO: Error writing the image file!"));
            }
            return false;
        }

        // actually save it:
#if wxUSE_LIBPNG
        if ( saveAsPNG )
        {
            wxPNGHandler handlerPNG;
            bResult = handlerPNG.SaveFile(image, stream, verbose);
            if ( !bResult )
            {
                if ( verbose )
                {
                    wxLogError(_("ICO: Error writing the image file!"));
                }
                return false;
            }
        }
        else
#endif // wxUSE_LIBPNG
        {
            IsMask = false;
            bResult = SaveDib(image, stream, verbose, IsBmp, IsMask);
            if ( !bResult )
            {
                if ( verbose )
                {
                    wxLogError(_("ICO: Error writing the image file!"));
                }
                return false;
            }
            IsMask = true;

            bResult = SaveDib(&mask, stream, verbose, IsBmp, IsMask);
            if ( !bResult )
            {
                if ( verbose )
                {
                    wxLogError(_("ICO: Error writing the image file!"));
                }
                return false;
            }
        }

    } // end of for loop

    return true;
}

bool wxICOHandler::LoadFile(wxImage *image, wxInputStream& stream,
                            bool verbose, int index)
{
    if ( stream.IsSeekable() && stream.SeekI(0) == wxInvalidOffset )
    {
        return false;
    }

    return DoLoadFile(image, stream, verbose, index);
}

bool wxICOHandler::DoLoadFile(wxImage *image, wxInputStream& stream,
                            bool verbose, int index)
{
    bool bResult wxDUMMY_INITIALIZE(false);

    ICONDIR IconDir;

    if ( !stream.ReadAll(&IconDir, sizeof(IconDir)) )
        return false;

    wxUint16 nIcons = wxUINT16_SWAP_ON_BE(IconDir.idCount);

    // nType is 1 for Icons, 2 for Cursors:
    wxUint16 nType = wxUINT16_SWAP_ON_BE(IconDir.idType);

    // loop round the icons and choose the best one:
    wxScopedArray<ICONDIRENTRY> pIconDirEntry(nIcons);
    ICONDIRENTRY *pCurrentEntry = pIconDirEntry.get();
    int wMax = 0;
    int colmax = 0;
    int iSel = wxNOT_FOUND;

    // remember how many bytes we read from the stream:
    wxFileOffset alreadySeeked = sizeof(IconDir);

    for (unsigned int i = 0; i < nIcons; i++ )
    {
        if ( !stream.ReadAll(pCurrentEntry, sizeof(ICONDIRENTRY)) )
            return false;

        alreadySeeked += stream.LastRead();

        // ICO file format uses only a single byte for width and if it is 0, it
        // means that the width is actually 256 pixels.
        const wxUint16
            widthReal = pCurrentEntry->bWidth ? pCurrentEntry->bWidth : 256;

        // bHeight and bColorCount are wxUint8
        if ( widthReal >= wMax )
        {
            // see if we have more colors, ==0 indicates > 8bpp:
            if ( pCurrentEntry->bColorCount == 0 )
                pCurrentEntry->bColorCount = 255;
            if ( pCurrentEntry->bColorCount >= colmax )
            {
                iSel = i;
                wMax = widthReal;
                colmax = pCurrentEntry->bColorCount;
            }
        }

        pCurrentEntry++;
    }

    if ( index != -1 )
    {
        // VS: Note that we *have* to run the loop above even if index != -1, because
        //     it reads ICONDIRENTRies.
        iSel = index;
    }

    if ( iSel == wxNOT_FOUND || iSel < 0 || iSel >= nIcons )
    {
        wxLogError(_("ICO: Invalid icon index."));
        bResult = false;
    }
    else
    {
        // seek to selected icon:
        pCurrentEntry = pIconDirEntry.get() + iSel;

        // NOTE: seeking a positive amount in wxFromCurrent mode allows us to
        //       load even non-seekable streams (see wxInputStream::SeekI docs)!
        wxFileOffset offset = wxUINT32_SWAP_ON_BE(pCurrentEntry->dwImageOffset) - alreadySeeked;
        if (offset != 0 && stream.SeekI(offset, wxFromCurrent) == wxInvalidOffset)
            return false;

#if wxUSE_LIBPNG
        // We can't fall back to loading an icon in the usual BMP format after
        // trying to load it as PNG if we have an unseekable stream, so to
        // avoid breaking the existing code which does successfully load icons
        // from such streams, we only try to load them as PNGs if we can unwind
        // back later.
        //
        // Ideal would be to modify LoadDib() to accept the first 8 bytes not
        // coming from the stream but from the signature buffer below, as then
        // we'd be able to load PNG icons from any kind of streams.
        bool isPNG;
        if ( stream.IsSeekable() )
        {
            // Check for the PNG signature first to avoid wasting time on
            // trying to load typical ICO files which are not PNGs at all.
            static const unsigned char signaturePNG[] =
            {
                0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
            };
            static const int signatureLen = WXSIZEOF(signaturePNG);

            unsigned char signature[signatureLen];
            if ( !stream.ReadAll(signature, signatureLen) )
                return false;

            isPNG = memcmp(signature, signaturePNG, signatureLen) == 0;

            // Rewind to the beginning of the image in any case.
            if ( stream.SeekI(-signatureLen, wxFromCurrent) == wxInvalidOffset )
                return false;
        }
        else // Not seekable stream
        {
            isPNG = false;
        }

        if ( isPNG )
        {
            wxPNGHandler handlerPNG;
            bResult = handlerPNG.LoadFile(image, stream, verbose);
        }
        else
#endif // wxUSE_LIBPNG
        {
            bResult = LoadDib(image, stream, verbose, false /* not BMP */);
        }
        bool bIsCursorType = (this->GetType() == wxBITMAP_TYPE_CUR) || (this->GetType() == wxBITMAP_TYPE_ANI);
        if ( bResult && bIsCursorType && nType == 2 )
        {
            // it is a cursor, so let's set the hotspot:
            image->SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, wxUINT16_SWAP_ON_BE(pCurrentEntry->wPlanes));
            image->SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, wxUINT16_SWAP_ON_BE(pCurrentEntry->wBitCount));
        }
    }

    return bResult;
}

int wxICOHandler::DoGetImageCount(wxInputStream& stream)
{
    // It's ok to modify the stream position in this function.

    if ( stream.IsSeekable() && stream.SeekI(0) == wxInvalidOffset )
    {
        return 0;
    }

    ICONDIR IconDir;

    if ( !stream.ReadAll(&IconDir, sizeof(IconDir)) )
        return 0;

    return (int)wxUINT16_SWAP_ON_BE(IconDir.idCount);
}

bool wxICOHandler::DoCanRead(wxInputStream& stream)
{
    return CanReadICOOrCUR(&stream, 1 /*for identifying an icon*/);

}

#endif // wxUSE_STREAMS


//-----------------------------------------------------------------------------
// wxCURHandler
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxCURHandler, wxICOHandler);

#if wxUSE_STREAMS

bool wxCURHandler::DoCanRead(wxInputStream& stream)
{
    return CanReadICOOrCUR(&stream, 2 /*for identifying a cursor*/);
}

#endif // wxUSE_STREAMS

//-----------------------------------------------------------------------------
// wxANIHandler
//-----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxANIHandler, wxCURHandler);

#if wxUSE_STREAMS

bool wxANIHandler::LoadFile(wxImage *image, wxInputStream& stream,
                            bool WXUNUSED(verbose), int index)
{
    wxANIDecoder decoder;
    if (!decoder.Load(stream))
        return false;

    return decoder.ConvertToImage(index != -1 ? (size_t)index : 0, image);
}

bool wxANIHandler::DoCanRead(wxInputStream& stream)
{
    wxANIDecoder decod;
    return decod.CanRead(stream);
             // it's ok to modify the stream position here
}

int wxANIHandler::DoGetImageCount(wxInputStream& stream)
{
    wxANIDecoder decoder;
    if (!decoder.Load(stream))  // it's ok to modify the stream position here
        return wxNOT_FOUND;

    return decoder.GetFrameCount();
}

static bool CanReadICOOrCUR(wxInputStream *stream, wxUint16 resourceType)
{
    // It's ok to modify the stream position in this function.

    if ( stream->IsSeekable() && stream->SeekI(0) == wxInvalidOffset )
    {
        return false;
    }

    ICONDIR iconDir;
    if ( !stream->ReadAll(&iconDir, sizeof(iconDir)) )
    {
        return false;
    }

    return !iconDir.idReserved // reserved, must be 0
        && wxUINT16_SWAP_ON_BE(iconDir.idType) == resourceType // either 1 or 2
        && iconDir.idCount; // must contain at least one image
}

#endif // wxUSE_STREAMS

#endif // wxUSE_ICO_CUR

#endif // wxUSE_IMAGE
