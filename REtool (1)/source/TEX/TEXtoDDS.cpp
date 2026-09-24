#include <stdio.h>
#include <tchar.h>
#include "tex.h"
#include "..\misc.h"
#include "..\retool.h"
#include "DDSInfo.h"
#include "DDSHeaderDefaults.h"
#include "TEXmisc.h"
#include "DDSmisc.h"

void TEXtoDDS(char *filename)
{
	//Filename without path
	char *filename_short;
	{
		int lastSlash = -1;
		int pos = 0;
		while(filename[pos] != 0)
		{
			if(filename[pos] == '\\' || filename[pos] == '/')
				lastSlash = pos;
			pos++;
		}

		if(lastSlash > 0)
		{
			if(filename[lastSlash + 1] != 0)
				filename_short = &filename[lastSlash + 1];
			else
				filename_short = filename;
		}
		else
			filename_short = filename;
	}

	//Open file
	unsigned char *data;
	unsigned int dataSize;
	if(!ReadFile(filename, &data, &dataSize))
	{
		printf("Failed to open %s for reading.\n", filename_short);
		return;
	}

	//Check magic
	if((unsigned int &) data[0] != TEXMAGIC)
	{
		delete[]data;
		return;
	}

	//Fill in info from header
	textureInfo_s texInfo = ReadTexHeader(data);

	//Manage headers
	mipHeader_s *mipHeader = (mipHeader_s *) &data[texInfo.headerSize];
	DDS_HEADER ddsHeader;
	memset(&ddsHeader, 0, sizeof(DDS_HEADER));
	DDS_HEADER_DXT10 dx10Header;
	memset(&dx10Header, 0, sizeof(DDS_HEADER_DXT10));
	bool dx10HeaderPresent = 0;

	//Let's assume every texture is DX10 for now
	dx10HeaderPresent = 1;
	SetDDSHeaderDefaults_BC7(&ddsHeader, &dx10Header);
	if(forceBC7unorm)
		dx10Header.dxgiFormat = DXGI_FORMAT_BC7_UNORM;
	else
		dx10Header.dxgiFormat = texInfo.type;

	/*
	//Figure out type
	bool fail = 0;
	switch(texHeader->type)
	{
	case 71:
		printf("Unsupported texture type %u in %s\n", texHeader->type, filename_short);
		fail = 1;
		break;
	case 98:
	case 99:
		dx10HeaderPresent = 1;
		SetDDSHeaderDefaults_BC7(&ddsHeader, &dx10Header);
		dx10Header.dxgiFormat = texHeader->type;
		break;
	default:
		printf("Unsupported texture type %u in %s\n", texHeader->type, filename_short);
		fail = 1;
		break;
	}
	if(fail)
	{
		delete[]data;
		return;
	}
	*/

	//Define misc DDS header data
	ddsHeader.dwHeight = texInfo.height;
	ddsHeader.dwMipMapCount = texInfo.mipCount;
	ddsHeader.dwPitchOrLinearSize = mipHeader->pitch;

	//Some TEX files have padding. In those cases the width is the render width, and the mip header contain the real width of the image data
	int realWidth = texInfo.width;
	{
		int bitsPerPixel = BitsPerPixelDX10Format(texInfo.type);
		if(bitsPerPixel < 0)
			printf("Warning: Could not determine bits-per-pixel count for %s. It's possible it will have the wrong width value.\n", filename_short);
		else
			realWidth = (mipHeader->imageDataSize * bitsPerPixel) / (texInfo.height * 2);
	}
	ddsHeader.dwWidth = realWidth;

	//Copy everything to DDS file
	char DDSPath[MAXPATH];
	memset(DDSPath, 0, MAXPATH);
	char filenameWithoutExtension[MAXPATH];
	strcpy(filenameWithoutExtension, filename);
	int firstDot = FirstDot(filenameWithoutExtension);
	if(firstDot != 0)
		filenameWithoutExtension[firstDot] = 0;
	sprintf(DDSPath, "%s.dds", filenameWithoutExtension);
	FILE *file;
	fopen_s(&file, DDSPath, "wb");
	if(!file)
	{
		printf("Error: Couldn't open %s for writing.\n", DDSPath);
		delete[]data;
		return;
	}
	fwrite(&ddsHeader, 1, sizeof(DDS_HEADER), file);
	if(dx10HeaderPresent)
		fwrite(&dx10Header, 1, sizeof(DDS_HEADER_DXT10), file);
	fwrite(&data[mipHeader->offsetForImageData], 1, dataSize - mipHeader->offsetForImageData, file);
	fclose(file);
	char *resString;
	if(texInfo.flags & TEXFLAGS_LOWRES) resString = "low-res texture";
	else resString = "high-res texture";
	printf("TEX info: %s, %ux%u, mipcount=%u (%s)", dx10TypeNames[texInfo.type], realWidth, texInfo.height, texInfo.mipCount, resString);
	if(realWidth != texInfo.height)
		printf(" (unique render width: %u)", texInfo.height);
	printf("\n");
	if (texInfo.imgCount > 1)
		printf("Warning: The TEX just written contains multiple images. These may not be correctly converted by REtool\n");
	printf("Wrote %s\n", DDSPath);
	delete[]data;
}