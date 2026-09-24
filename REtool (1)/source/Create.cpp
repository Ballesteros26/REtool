#include <stdio.h>
#include <direct.h>
#include "pakformat.h"
#include "libdeflate\libdeflate.h"
#include "retool.h"
#include "dir.h"
#include "misc.h"
#include <string.h>
#include "PAKFunctions.h"

void CreatePAK(char *dirName)
{
	//Check if the path is valid
	if(dirName == 0 || dirName[0] == 0 || strcmp(dirName, ".") == 0)
	{
		printf("Error: Directory path is invalid.\n");
		return;
	}

	//Count files that we'll be adding
	unsigned int fileTotal = 0;
	CreateFileQueue(dirName);
	for(int i = 0; i < dir_alphaqueNum; i++)
	{
		dir_queue_t *dir = &dir_que[dir_alphaque[i]];
		if (!dir->active)
			continue;
		if(dir->fileSize == 0)
			continue;
		fileTotal++;
	}
	/*if(fileTotal == 0)
	{
		printf("Error: There are no files in the chosen directory.\n");
		return;
	}*/

	//Set up PAK header
	pakHeader_s pakHeader;
	pakHeader.magic = 1095454795;
	pakHeader.flags = 0;
	pakHeader.unknown1 = 0;
	pakHeader.version = 4; //MH Rise has this version number
	pakHeader.entryCount = fileTotal;

	//Open file for writing
	FILE *pakFile = 0;
	char fullPath[MAXPATH] = {0};
	sprintf_s(fullPath, MAXPATH, "%s.pak", dirName);
	fopen_s(&pakFile, fullPath, "wb");
	if(!pakFile)
	{
		printf("Error: Failed to open %s for writing.\n", fullPath);
		return;
	}

	//Setup TOC
	pakEntry_s *pakEntries = new pakEntry_s[fileTotal];

	//Write header and TOC to file so file IO writer is in the correct position for writing file entry data
	fwrite(&pakHeader, sizeof(pakHeader_s), 1, pakFile);
	fwrite(pakEntries, sizeof(pakEntry_s), fileTotal, pakFile); //We'll re-write this later once we've filled in all the correct data
	unsigned long long curOffset = sizeof(pakHeader_s) + (sizeof(pakEntry_s) * fileTotal);

	//Prepare compressor
	libdeflate_compressor *deflateCompressor = libdeflate_alloc_compressor(6);

	//Process all files we'll be adding to the PAK
	unsigned int entryNum = 0;
	for(int i = 0; i < dir_alphaqueNum; i++)
	{
		dir_queue_t *dir = &dir_que[dir_alphaque[i]];
		if (!dir->active)
			continue;
		if(dir->fileSize == 0)
			continue;

		//Convert full file path to murmur hash
		pakEntry_s *pakEntry = &pakEntries[entryNum];
		pakEntry->filenameHashL = ConvertFilePathToMurmurHash(dir->fileName, 1);
		pakEntry->filenameHashU = ConvertFilePathToMurmurHash(dir->fileName, 0);

		//Misc variables for the file entry
		pakEntry->offset = curOffset;
		pakEntry->unknown2 = 0;
		pakEntry->unknown4 = 0;
		pakEntry->flag = 0;

		//Open and read in the file for this entry
		FILE *file = 0;
		char fullEntryPath[MAXPATH] = {0};
		fpos_t entryFileSize = 0;
		sprintf_s(fullEntryPath, MAXPATH, "%s\\%s", dirName, dir->fileName);
		fopen_s(&file, fullEntryPath, "rb");
		if(!file)
		{
			//TODO: Error handling
		}
		fseek(file, 0, SEEK_END);
		fgetpos(file, &entryFileSize);
		fseek(file, 0, SEEK_SET);
		unsigned char *entryFileData = new unsigned char[entryFileSize];
		fread(entryFileData, 1, entryFileSize, file);
		fclose(file);

		//Define entry's real file size
		pakEntry->realSize = entryFileSize;

		//Never compress certain files: modinfo.ini and screenshots in the root directory
		bool tryToCompress = 1;
		if(strcmp(dir->fileName, dir->fileNameWithoutDir) == 0 && (strcmp(dir->fileName, "modinfo.ini") == 0 || strcmp(dir->extension, "jpg") == 0 || strcmp(dir->extension, "png") == 0 || strcmp(dir->extension, "dds") == 0 || strcmp(dir->extension, "jpeg") == 0))
			tryToCompress = 0;
		
		//Try to compress the data
		size_t maxCompressedSize;
		unsigned char *compressedData = 0;
		unsigned int compressedSize = 0;
		if(tryToCompress)
		{
			maxCompressedSize = libdeflate_deflate_compress_bound(deflateCompressor, entryFileSize);
			compressedData = new unsigned char[maxCompressedSize];
			compressedSize = libdeflate_deflate_compress(deflateCompressor, entryFileData, entryFileSize, compressedData, maxCompressedSize);
		}

		//Check if we should keep compressed data or uncompressed data
		if(tryToCompress && compressedSize < entryFileSize) //We compressed the data and it's smaller, so keep that
		{
			pakEntry->compressedSize = compressedSize; //Define compressed size
			fwrite(compressedData, 1, compressedSize, pakFile); //Write compressed data to PAK file
			pakEntry->flag |= TOCFLAGS_DEFLATE; //Set flag with type of compression
			printf("Added %s (deflate)\n", dir->fileName);
		}
		else //Default to uncompressed data
		{
			pakEntry->compressedSize = entryFileSize; //Compressed size matches real size when uncompressed
			fwrite(entryFileData, 1, entryFileSize, pakFile); //Write entry data to PAK file
			printf("Added %s (uncompressed)\n", dir->fileName);
		}

		//Finish
		curOffset += pakEntry->compressedSize;
		delete[]entryFileData;
		if(compressedData)
			delete[]compressedData;
		entryNum++;
	}

	//Jump back to TOC location and re-write it
	fseek(pakFile, sizeof(pakHeader_s), SEEK_SET);
	fwrite(pakEntries, sizeof(pakEntry_s), fileTotal, pakFile);

	//Finish
	fclose(pakFile);
	delete[]pakEntries;
	libdeflate_free_compressor(deflateCompressor);
	printf("Wrote %s with %u file entries.\n", fullPath, fileTotal);
}