#include "tgadecoder.hpp"

// 9BCD5FD0-93AA-4574-A107-0A7CF1216D0F
const GUID CLSID_TGA_Container = 
{ 0x9BCD5FD0, 0x93AA, 0x4574, { 0xA1, 0x07, 0x0A, 0x7C, 0xF1, 0x21, 0x6D, 0x0F } };

// 369EA481-D011-4D53-9875-7D6A4BD4BD60
const GUID CLSID_TGA_Decoder = 
{ 0x369EA481, 0xD011, 0x4D53, { 0x98, 0x75, 0x7D, 0x6A, 0x4B, 0xD4, 0xBD, 0x60} };

#define WICX_RELEASE(X) if ( NULL != X ) { X->Release(); X = NULL; }

namespace tga
{
	typedef enum
	{
		TGA_NOIMAGE = 0,
		TGA_UNCOMPRESSEDCOLORMAP	= 1,
		TGA_UNCOMPRESSEDRGB			= 2,			//supported
		TGA_UNCOMPRESSEDBW			= 3,
		TGA_RLECOLORMAP				= 9,
		TGA_RLERGB					= 10,
		TGA_COMPRESSEDBW			= 11,
		TGA_COMPRESSEDCOLORMAP		= 32,
		TGA_COMPRESSEDCOLORMAP2		= 33
	};

	#pragma pack( push, 1 )
	struct TGA_HEADER
	{
		char	identificationLength;
		char	colormapType;
		char	imageTypeCode;
		short	colormapOrigin;		//index to first
		short	colormapEntries;
		char	colormapEntrySize;	//bits
		short	lowerLeftX;
		short	lowerLeftY;
		short	width;
		short	height;
		char	pixelSize;			//bits
		char	descriptor;
	};
	#pragma pack( pop )

	struct color
	{
		unsigned char r, g, b, a;
	};
}

namespace tgax
{
	template< typename T >
	static HRESULT ReadFromIStream( IStream *stream, T &val )
	{
		HRESULT result = S_OK;
		if ( NULL == stream ) result = E_INVALIDARG;
		ULONG numRead = 0;
		if ( SUCCEEDED( result )) result = stream->Read( &val, sizeof(T), &numRead );
		if ( SUCCEEDED( result )) result = ( sizeof(T) == numRead ) ? S_OK : E_UNEXPECTED;
		return result;
	}

	static HRESULT ReadBytesFromIStream( IStream *stream, unsigned char *bytes, unsigned count )
	{
		HRESULT result = S_OK;
		if ( NULL == stream ) result = E_INVALIDARG;
		ULONG numRead = 0;
		if ( SUCCEEDED( result )) result = stream->Read( bytes, count, &numRead );
		if ( SUCCEEDED( result )) result = ( count == numRead ) ? S_OK : E_UNEXPECTED;
		return result;
	}

	//----------------------------------------------------------------------------------------
	// TGA_FrameDecode implementation
	//----------------------------------------------------------------------------------------

	TGA_FrameDecode::TGA_FrameDecode( IWICImagingFactory *pIFactory, UINT num )
		: BaseFrameDecode( pIFactory, num )
	{
	}

	HRESULT TGA_FrameDecode::LoadTargaImage( tga::TGA_HEADER &tgaHeader, IStream *pIStream )
	{
		HRESULT result = S_OK;

		// load the identification block
		if( tgaHeader.identificationLength > 0 )
		{
			unsigned char identField[256];
			result = ReadBytesFromIStream( pIStream, identField, tgaHeader.identificationLength );
			if ( FAILED( result ))
				return result;

			// is there anything we want to do with this data?
		}

		// load the palette data
		tga::color *palette = NULL;

		if( tgaHeader.colormapEntries > 0 )
		{
			int bytesPerColour = (tgaHeader.colormapEntries == 15 ? 16 : tgaHeader.colormapEntries) / 8;
			int colourMapBytes = tgaHeader.colormapEntries * bytesPerColour;

			unsigned char *colourMap = new unsigned char[colourMapBytes];
			if( !colourMap )
				result = WINCODEC_ERR_OUTOFMEMORY;

			result = ReadBytesFromIStream( pIStream, colourMap, colourMapBytes );
			if ( SUCCEEDED( result ))
			{
				if( tgaHeader.imageTypeCode == tga::TGA_UNCOMPRESSEDCOLORMAP )
				{
//					if( header.colormapOrigin != 0 )
//						// break!
//					if( header.colormapEntrySize != 24  && header.colormapEntrySize != 32 )
//						// break!

					palette = new tga::color[tgaHeader.colormapEntries];
					if( !palette )
						result = WINCODEC_ERR_OUTOFMEMORY;

					// ignores offsets to first palette entry. What is it anyway?
					for( int i = 0; i < tgaHeader.colormapEntries; i++ )
					{
						palette[i].r = colourMap[2];
						palette[i].g = colourMap[1];
						palette[i].b = colourMap[0];
						colourMap += bytesPerColour;
					}
				}
				else
					result = WINCODEC_ERR_NOTIMPLEMENTED;
			}

			delete[] colourMap;

			if( FAILED( result ) )
				return result;
		}

		// load the image data from the file
		tga::color *pixels = NULL;

		int bytesPerPixel = ( tgaHeader.pixelSize == 15 ? 16 : tgaHeader.pixelSize ) / 8;
		int imageBytes = tgaHeader.width * tgaHeader.height * bytesPerPixel;

		unsigned char *image = new unsigned char[imageBytes];
		if( !image )
			result = WINCODEC_ERR_OUTOFMEMORY;
		else
			result = ReadBytesFromIStream( pIStream, image, imageBytes );

		if ( SUCCEEDED( result ))
		{
			tga::color *pixels = new tga::color[tgaHeader.width * tgaHeader.height];
			tga::color *output = pixels;

			int	w2 = tgaHeader.width;
			if( !(tgaHeader.descriptor & 32) )	// flipped
			{
				output += (tgaHeader.height-1) * tgaHeader.width;
				w2 = -tgaHeader.width;
			}

			switch( tgaHeader.imageTypeCode )
			{
			case tga::TGA_UNCOMPRESSEDRGB:

				if( bytesPerPixel != 3 && bytesPerPixel != 4 )
				{
					result = WINCODEC_ERR_NOTIMPLEMENTED;
					break;
				}

				for( int i = 0; i < tgaHeader.height; i++ )
				{
					for( int j = 0; j < tgaHeader.width; j++ )
					{
						if( bytesPerPixel == 4 )
							output[j].a = image[j*bytesPerPixel + 3];
						output[j].r = image[j*bytesPerPixel + 2];
						output[j].g = image[j*bytesPerPixel + 1];
						output[j].b = image[j*bytesPerPixel + 0];
					}
					output += w2;
					image += bytesPerPixel * tgaHeader.width;
				}
				break;

			case tga::TGA_UNCOMPRESSEDBW:

				if( bytesPerPixel != 1 )
				{
					result = WINCODEC_ERR_NOTIMPLEMENTED;
					break;
				}

				for( int i = 0; i < tgaHeader.height; i++ )
				{
					for( int j = 0; j < tgaHeader.width; j++ )
					{
						output[j].a = 255;
						output[j].r = image[j*bytesPerPixel];
						output[j].g = image[j*bytesPerPixel];
						output[j].b = image[j*bytesPerPixel];
					}
					output += w2;
					image += bytesPerPixel * tgaHeader.width;
				}
				break;

			case tga::TGA_UNCOMPRESSEDCOLORMAP:

				if( bytesPerPixel != 1 )
				{
					result = WINCODEC_ERR_NOTIMPLEMENTED;
					break;
				}

				for( int i = 0; i < tgaHeader.height; i++ )
				{
					for( int j = 0; j < tgaHeader.width; j++ )
					{
						UCHAR cnum = image[j];
						output[j] = palette[cnum];
					}
					output += w2;
					image += tgaHeader.width;
				}
				break;

			case tga::TGA_RLERGB:
				{
					if( bytesPerPixel != 3 && bytesPerPixel != 4 )
					{
						result = WINCODEC_ERR_NOTIMPLEMENTED;
						break;
					}

					unsigned char packetType;
					tga::color tempPixel;
					int iYPos = 0;
					int iXPos = 0;
					while( iYPos < tgaHeader.height )
					{
						packetType = *image++;
						if( packetType < 128 )
						{
							for( int i = 0; i <= packetType; i++ )			 //RAW PACKET
							{
								tempPixel.b = *image++;
								tempPixel.g = *image++;
								tempPixel.r = *image++;
								if( bytesPerPixel == 4 )
									tempPixel.a = *image++;
								output[iXPos] = tempPixel;
								++iXPos;
								if( iXPos == tgaHeader.width )
								{
									iXPos = 0;
									++iYPos;
									output += w2;
								}
							}
						}
						else
						{
							tempPixel.b = *image++;
							tempPixel.g = *image++;
							tempPixel.r = *image++;
							if( bytesPerPixel == 4 )
								tempPixel.a = *image++;
							for( int i = 0; i < packetType-127; i++ )		 //RLE PACKET
							{
								output[iXPos] = tempPixel;
								++iXPos;
								if( iXPos == tgaHeader.width )
								{
									iXPos = 0;
									++iYPos;
									output += w2;
								}
							}
						}
					}
				}
				break;

			case tga::TGA_RLECOLORMAP:
			case tga::TGA_COMPRESSEDBW:
			case tga::TGA_COMPRESSEDCOLORMAP:
			case tga::TGA_COMPRESSEDCOLORMAP2:
			case tga::TGA_NOIMAGE:
			default:
				result = WINCODEC_ERR_NOTIMPLEMENTED;
			break;
			}
		}

		if( palette )
			delete[] palette;
		if( image )
			delete[] image;

		if( FAILED( result ) )
		{
			if( pixels )
				delete[] pixels;
			return result;
		}

		// fill the bitmap with our data
		result = FillBitmapSource( tgaHeader.width, tgaHeader.height, 72, 72, GUID_WICPixelFormat32bpp3ChannelsAlpha, tgaHeader.width*4, tgaHeader.width*4*tgaHeader.height, (BYTE*)pixels );

		delete[] pixels;

		return result;
	}

	HRESULT TGA_FrameDecode::FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY,
		REFWICPixelFormatGUID pixelFormat, UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer )
	{
		UNREFERENCED_PARAMETER( dpiX );
		UNREFERENCED_PARAMETER( dpiY );

		HRESULT result = S_OK;
		IWICImagingFactory *codecFactory = NULL;

		if ( SUCCEEDED( result ))
			result = CoCreateInstance( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
				reinterpret_cast<LPVOID *>( &codecFactory ));

		if ( SUCCEEDED( result ))
			result = codecFactory->CreateBitmapFromMemory ( width, height, pixelFormat, cbStride, cbBufferSize,
				pbBuffer, reinterpret_cast<IWICBitmap **>( &( m_bitmapSource )));

		if ( codecFactory ) codecFactory->Release();

		return result;
	}


	//----------------------------------------------------------------------------------------
	// TGA_Decoder implementation
	//----------------------------------------------------------------------------------------

	TGA_Decoder::TGA_Decoder()
		: BaseDecoder( CLSID_TGA_Decoder, CLSID_TGA_Container )
	{
	}

	TGA_Decoder::~TGA_Decoder()
	{
	}

	// TODO: implement real query capability
	STDMETHODIMP TGA_Decoder::QueryCapability( IStream *pIStream, DWORD *pCapability )
	{
		UNREFERENCED_PARAMETER( pIStream );

		HRESULT result = S_OK;

		*pCapability =
			WICBitmapDecoderCapabilityCanDecodeSomeImages;

		return result;
	}

	STDMETHODIMP TGA_Decoder::Initialize( IStream *pIStream, WICDecodeOptions cacheOptions )
	{
		UNREFERENCED_PARAMETER( cacheOptions );

		HRESULT result = E_INVALIDARG;

		ReleaseMembers( true );

		if ( pIStream )
		{
			tga::TGA_HEADER tgaHeader;
			result = ReadFromIStream( pIStream, tgaHeader ); // read header

			if ( SUCCEEDED( result )) result = VerifyFactory();

			if ( SUCCEEDED( result ))
			{
				TGA_FrameDecode* frame = CreateNewDecoderFrame( m_factory, 0 );

				result = frame->LoadTargaImage( tgaHeader, pIStream );

				if ( SUCCEEDED( result ))
					AddDecoderFrame( frame );
				else
					delete frame;
			}
		}
		else
			result = E_INVALIDARG;

		return result;
	}

	TGA_FrameDecode* TGA_Decoder::CreateNewDecoderFrame( IWICImagingFactory* factory , UINT i )
	{
		return new TGA_FrameDecode( factory, i );
	}

	void TGA_Decoder::Register( RegMan &regMan )
	{
		HMODULE curModule = GetModuleHandle( L"tga-wic-codec.dll" );
		wchar_t tempFileName[MAX_PATH];
		if ( curModule != NULL ) GetModuleFileName( curModule, tempFileName, MAX_PATH );

		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"CLSID", L"{369EA481-D011-4D53-9875-7D6A4BD4BD60}" );
		regMan.SetSZ( L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"FriendlyName", L"TGA Decoder" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"Version", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"Date", _T(__DATE__) );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"SpecVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"ColorManagementVersion", L"1.0.0.0" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"MimeTypes", L"x-image/tga" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"FileExtensions", L".tga" );
		regMan.SetDW( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"SupportsAnimation", 0 );
		regMan.SetDW( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"SupportChromakey", 0 );
		regMan.SetDW( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"SupportLossless", 1 );
		regMan.SetDW( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"SupportMultiframe", 0 );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"ContainerFormat", L"{9BCD5FD0-93AA-4574-A107-0A7CF1216D0F}" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"Author", L"Manu Evans, http://github.com/TurkeyMan" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"Description", L"Targa Format Decoder" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}", L"FriendlyName", L"TGA Decoder" );

		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Formats", L"", L"" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Formats\\{6FDDC324-4E03-4BFE-B185-3D77768DC90F}", L"", L"" );

		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\InprocServer32", L"", tempFileName );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\InprocServer32", L"ThreadingModel", L"Apartment" );
		regMan.SetSZ( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Patterns", L"", L"" );
		regMan.SetDW( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Patterns\\0", L"Position", 0 );
		regMan.SetDW( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Patterns\\0", L"Length", 4 );

		BYTE bytes[8] = { 0 };
		bytes[0] = 0x44; bytes[1] = 0x44; bytes[2] = 0x53; bytes[3] = 0x20;
		regMan.SetBytes( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Patterns\\0", L"Pattern", bytes, 4 );
		bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
		regMan.SetBytes( L"CLSID\\{369EA481-D011-4D53-9875-7D6A4BD4BD60}\\Patterns\\0", L"Mask", bytes, 4 );
	}
}
