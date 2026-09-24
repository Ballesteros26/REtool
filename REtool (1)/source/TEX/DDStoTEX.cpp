#include <stdio.h>
#include <tchar.h>
#include "tex.h"
#include "TEXmisc.h"
#include "..\misc.h"
#include "DDSInfo.h"
#include "..\dir.h"
#include "..\retool.h"
#include "DDSmisc.h"

void DDStoTEX(char *filename)
{
	unsigned char *data = 0, *texData = 0;
	mipHeader_s *mipHeaders = 0;
	bool newMipHeaders = 0;
	unsigned int dataSize;
	if(!ReadFile(filename, &data, &dataSize))
	{
		printf("Failed to open %s for reading.\n", filename);
		goto finish;
	}
	DDS_HEADER *ddsHeader = (DDS_HEADER *)data;
	DDS_HEADER_DXT10 *dx10Header = 0;
	int ddsType;
	if(ddsHeader->ddspf.dwFourCC == 808540228)
	{
		dx10Header = (DDS_HEADER_DXT10 *)&data[sizeof(DDS_HEADER)];
		ddsType = dx10Header->dxgiFormat;
	}
	else
	{
		if(ddsHeader->ddspf.dwFourCC == DXT1)
			ddsType = DXGI_FORMAT_BC1_UNORM;
		else if(ddsHeader->ddspf.dwFourCC == DXT2 || ddsHeader->ddspf.dwFourCC == DXT3)
			ddsType = DXGI_FORMAT_BC2_UNORM;
		else if(ddsHeader->ddspf.dwFourCC == DXT4 || ddsHeader->ddspf.dwFourCC == DXT5)
			ddsType = DXGI_FORMAT_BC3_UNORM;
		else if(ddsHeader->ddspf.dwFourCC == ATI1)
			ddsType = DXGI_FORMAT_BC4_UNORM;
		else if(ddsHeader->ddspf.dwFourCC == ATI2)
			ddsType = DXGI_FORMAT_BC5_UNORM;
		/*
		else if (ddsheader->ddspf.dwRGBBitCount == 24) //RGB 24
			ddsType = DXGI_FORMAT_R8G8B8_TYPELESS;
			*/
		else if(ddsHeader->ddspf.dwRGBBitCount == 32) //RGBA 32
			ddsType = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		else
		{
			printf("Unknown image compression format in %s.\n", filename);
			goto finish;
		}
	}

	/*
	if (dx10Header == 0)
	{
		printf("Missing DX10 header in DDS file %s\n", filename);
		delete[]data;
		return;
	}
	*/

	//Filename without extension
	char filenameWithoutExtension[MAXPATH];
	strcpy(filenameWithoutExtension, filename);
	int firstDot = FirstDot(filenameWithoutExtension);
	if(firstDot != 0)
		filenameWithoutExtension[firstDot] = 0;

	//Path
	char pathOnly[MAXPATH];
	strcpy(pathOnly, filename);
	int lastSlash = LastSlash(filenameWithoutExtension);
	pathOnly[lastSlash] = 0;

	//Filename without extension or path
	char filenameWithoutExtensionAndPath[MAXPATH];
	filenameWithoutExtensionAndPath[0] = 0;
	if((filenameWithoutExtension[lastSlash] == '\\' || filenameWithoutExtension[lastSlash] == '/') && filenameWithoutExtension[lastSlash + 1] != 0)
		memcpy(filenameWithoutExtensionAndPath, &filenameWithoutExtension[lastSlash + 1], strlen(&filenameWithoutExtension[lastSlash + 1]) + 1);
	else
		strcpy(filenameWithoutExtensionAndPath, filenameWithoutExtension);

	//Find TEX file to update
	char searchForFile[MAXPATH];
	searchForFile[0] = 0;
	sprintf(searchForFile, "%s.tex.", filenameWithoutExtensionAndPath);
	if(pathOnly[0] == 0) //If there is no path, then set this to "." so we scan current dir (otherwise we'd be scanning the root)
	{
		pathOnly[0] = '.';
		pathOnly[1] = 0;
	}
	CreateFileQueue(pathOnly, 0, 0, 1, searchForFile);
	if(dir_alphaqueNum != 1)
	{
		if(dir_alphaqueNum == 0)
			printf("Error: Could not find any corresponding TEX file to update.");
		else
			printf("Error: Multiple matching TEX files found. Skipping update.");
		goto finish;
	}

	//We remember the TEX filename as we'll use its name when saving our new TEX
	char texPath[MAXPATH];
	strcpy(texPath, dir_que[dir_alphaque[0]].fileName);

	//Read TEX file
	unsigned int texDataSize;
	if(!ReadFile(dir_que[dir_alphaque[0]].fileName, &texData, &texDataSize))
	{
		printf("Failed to open %s for reading.\n", dir_que[dir_alphaque[0]].fileName);
		goto finish;
	}

	//Check TEX magic
	if((unsigned int &)texData[0] != TEXMAGIC)
	{
		printf("%s is a not valid TEX file.\n", dir_que[dir_alphaque[0]].fileName);
		goto finish;
	}

	//Fill in info from header
	textureInfo_s texInfo = ReadTexHeader(texData);

	//Check if this has more than one image
	if (texInfo.imgCount > 1)
		printf("Warning: This TEX has multiple images. It may not be updated correctly.\n");

	//Read TEX header
	if(texInfo.mipCount && ddsHeader->dwMipMapCount == texInfo.mipCount && dx10Header && dx10Header->dxgiFormat == texInfo.type) //Keep mipheaders from TEX file
	{
		mipHeaders = (mipHeader_s *) &texData[texInfo.headerSize];
	}
	else if(ddsHeader->dwMipMapCount) //Create all-new mipheaders
	{
		mipHeaders = new mipHeader_s[ddsHeader->dwMipMapCount];
		newMipHeaders = 1;
	}

	//Update parts of the header
	int originalMipCount = texInfo.mipCount;
	int originalHeight = texInfo.height;
	int originalWidth = texInfo.width;
	int originalType = texInfo.type;
	if(dx10Header)
	{
		bool sourceBC7 = (dx10Header->dxgiFormat == DXGI_FORMAT_BC7_UNORM || dx10Header->dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB);
		bool destBC7 = (texInfo.type == DXGI_FORMAT_BC7_UNORM || texInfo.type == DXGI_FORMAT_BC7_UNORM_SRGB);
		if(!keepBC7typeDuringDDStoTEX || sourceBC7 != destBC7) //If keepBC7typeDuringDDStoTEX is true, then we'll not change BC7 type if input DDS and output TEX are both BC7
			texInfo.type = dx10Header->dxgiFormat;
	}
	texInfo.mipCount = (unsigned char) ddsHeader->dwMipMapCount;
	if(!keepWidthDuringDDStoTEX) //If keepWidthDuringDDStoTEX is true then we keep original width in TEX without changing it
	{
		if(forcedWidthDuringDDStoTEX > -1) //Use user-defined width
			texInfo.width = (unsigned short) forcedWidthDuringDDStoTEX;
		else //Use width from DDS
			texInfo.width = (unsigned short) ddsHeader->dwWidth;
	}
	texInfo.height = (unsigned short) ddsHeader->dwHeight;

	//Update mip headers
	if(newMipHeaders && mipHeaders)
	{
		int pixelBitSize = BitsPerPixelDX10Format(texInfo.type);
		if(pixelBitSize == -1)
		{
			printf("Error: Could not determine pixel bit depth for %s due to invalid texture type.\n", dir_que[dir_alphaque[0]].fileName);
			goto finish;
		}
		int curWidth = ddsHeader->dwWidth;
		int curHeight = ddsHeader->dwHeight;
		int curOffset = texInfo.headerSize + (sizeof(mipHeader_s) * ddsHeader->dwMipMapCount);
		for(int i = 0; i < texInfo.mipCount; i++)
		{
			mipHeaders[i].imageDataSize = (curWidth * curHeight * pixelBitSize) / 8;
			mipHeaders[i].offsetForImageData = curOffset;
			if(pixelBitSize == 32) //Pixel size * width (we assume this is RGBA)
				mipHeaders[i].pitch = curWidth * 4;
			else if(pixelBitSize == 24) //Pixel size * width (we assume this is RGB)
				mipHeaders[i].pitch = curWidth * 3;
			else //Pixel size + 4x4 grid * width (aka block compression)
				mipHeaders[i].pitch = (4 * curWidth * pixelBitSize) / 8;
			curWidth /= 2;
			curHeight /= 2;
			curOffset += mipHeaders[i].imageDataSize;

			//For block compression, we should behave as if either axis never go below 4 (since the smallest chunk of image data can't be smaller than 4x4)
			if(pixelBitSize != 24 && pixelBitSize != 32)
			{
				if(curWidth < 4)
					curWidth = 4;
				if(curHeight < 4)
					curHeight = 4;
			}
		}
	}

	//Open TEX file for writing
	FILE *file;
	fopen_s(&file, texPath, "wb");
	if(!file)
	{
		printf("Error: Couldn't open %s for writing.\n", texPath);
		goto finish;
	}

	//Write TEX file. Starting with header
	if(TexVersion(texInfo.extension) == TEXVERSION_RE7)
	{
		texHeader_s *texHeader = (texHeader_s *)texData;
		texHeader->width = texInfo.width;
		texHeader->height = texInfo.height;
		texHeader->mipCount = texInfo.mipCount;
		texHeader->flags = texInfo.flags;
		texHeader->type = texInfo.type;
		fwrite(texHeader, sizeof(texHeader_s), 1, file);
	}
	else
	{
		texHeader_v30_s *texHeader = (texHeader_v30_s *)texData;
		texHeader->width = texInfo.width;
		texHeader->height = texInfo.height;
		//TODO: Print warning if mipcount is more than 256? That can't be contained in one byte
		texHeader->mipCount = texInfo.mipCount * 16;
		texHeader->flags = texInfo.flags;
		texHeader->type = texInfo.type;
		fwrite(texHeader, sizeof(texHeader_v30_s), 1, file);
	}
	if(texInfo.mipCount)
		fwrite(mipHeaders, sizeof(mipHeader_s), texInfo.mipCount, file);

	unsigned int totalDDSHeaderSize;
	if(dx10Header)
		totalDDSHeaderSize = sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
	else
		totalDDSHeaderSize = sizeof(DDS_HEADER);

	fwrite(&data[totalDDSHeaderSize], 1, dataSize - totalDDSHeaderSize, file);
	fclose(file);
	printf("Updated %s with DDS image data.\n", texPath);

	/*
	//Various warnings if there is a mismatch in data
	if(texHeader.height != ddsHeader->dwHeight || texHeader.width != ddsHeader->dwWidth)
		printf("Warning: Resolution mismatch (it should be: %ux%u)\n", texHeader.width, texHeader.height);
	if (texHeader.mipCount < ddsHeader->dwMipMapCount)
		printf("Warning: Too many mipmaps (the count should be: %u)\n", texHeader.mipCount);
	if (texHeader.mipCount > ddsHeader->dwMipMapCount)
		printf("Warning: Too few mipmaps (the count should be: %u)\n", texHeader.mipCount);
		*/

	//Messages if we updated size or type
	{
		if(keepWidthDuringDDStoTEX)
		{
			if(ddsHeader->dwWidth == texInfo.width)
				printf("Width of %u kept from original TEX which matches the width of the image data.", texInfo.width);
			else
				printf("Width of %u kept from original TEX which is different than the width of %u used for the image data.", texInfo.width, ddsHeader->dwWidth);
		}
		else if(forcedWidthDuringDDStoTEX != -1)
		{
			if(forcedWidthDuringDDStoTEX == texInfo.width && forcedWidthDuringDDStoTEX == ddsHeader->dwWidth)
				printf("Wrote the user-defined width %i kept to the TEX header which matches the width in the original TEX and image data.", forcedWidthDuringDDStoTEX);
			else if(forcedWidthDuringDDStoTEX == texInfo.width)
				printf("Wrote the user-defined width %i kept to the TEX header which matches the width in the original TEX.", forcedWidthDuringDDStoTEX);
			else if(forcedWidthDuringDDStoTEX == ddsHeader->dwWidth)
				printf("Wrote the user-defined width %i kept to the TEX header which matches the width in the image data.", forcedWidthDuringDDStoTEX);
			else
				printf("Wrote the user-defined width %i kept to the TEX header which is different than both the original TEX header and image data.", forcedWidthDuringDDStoTEX);
		}
		else
		{
			if(originalHeight != texInfo.height || originalWidth != texInfo.width)
				printf("Resolution changed from %ux%u to %ux%u\n", originalWidth, originalHeight, texInfo.width, texInfo.height);
		}
	}
	
	if(originalMipCount != texInfo.mipCount)
		printf("Mip count changed from %u to %u\n", originalMipCount, texInfo.mipCount);
	if(originalType != texInfo.type)
		printf("Type changed from %u to %u\n", originalType, texInfo.type);

	//Free allocated memory
finish:
	if(data)
		delete[]data;
	if(texData)
		delete[]texData;
	if(newMipHeaders && mipHeaders)
		delete[]mipHeaders;
}