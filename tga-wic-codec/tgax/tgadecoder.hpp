#pragma once

#include "../stdafx.hpp"
#include "../wicx/basedecoder.hpp"
#include "../wicx/regman.hpp"

extern const GUID CLSID_TGA_Container;
extern const GUID CLSID_TGA_Decoder;

namespace tga
{
	struct TGA_HEADER;
}

namespace tgax
{
	using namespace wicx;

	class TGA_FrameDecode: public BaseFrameDecode
	{
	public:
		TGA_FrameDecode( IWICImagingFactory *pIFactory, UINT num );

		HRESULT LoadTargaImage( tga::TGA_HEADER &tgaHeader, IStream *pIStream );

	private:
		HRESULT FillBitmapSource( UINT width, UINT height, UINT dpiX, UINT dpiY, REFWICPixelFormatGUID pixelFormat, UINT cbStride, UINT cbBufferSize, BYTE *pbBuffer );
	};

	class TGA_Decoder: public BaseDecoder
	{
	public:
		static void Register( RegMan &regMan );

		TGA_Decoder();
		~TGA_Decoder();

		// IWICBitmapDecoder interface

		STDMETHOD( QueryCapability )( 
			/* [in] */ IStream *pIStream,
			/* [out] */ DWORD *pCapability );

		STDMETHOD( Initialize )( 
			/* [in] */ IStream *pIStream,
			/* [in] */ WICDecodeOptions cacheOptions );

	protected:
		virtual TGA_FrameDecode* CreateNewDecoderFrame( IWICImagingFactory *factory , UINT i );
	};
}
