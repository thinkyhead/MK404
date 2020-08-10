/*
	FatImage.cpp - Quick and dirty utility class to make crude
	(and probably not-officially-conforming) empty FAT32 images
	for use with the SD card simulator.

	Copyright 2020 VintagePC <https://github.com/vintagepc/>

 	This file is part of MK404.

	MK404 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	MK404 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with MK404.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "FatImage.h"
#include <cstdint>      // for perror
#include <cstdlib>     // for exit
#include <cstring>
#include <fcntl.h>      // for open, O_CREAT, O_WRONLY, SEEK_SET
#include <type_traits>  // for __decay_and_strip<>::__type
#include <unistd.h>     // for close, ftruncate, lseek, write
#include <vector>       // for vector

// const map<FatImage::Size, uint32_t>FatImage::SectorsPerFat =
// {
// 	make_pair(Size::M32, 505),
// 	make_pair(Size::M64, 1009),
// 	make_pair(Size::M128, 2017),
// 	make_pair(Size::M256, 4033),
// 	make_pair(Size::M512, 1022),
// 	make_pair(Size::G1, 2044),
// 	make_pair(Size::G2, 4088),
// };

const map<string, FatImage::Size>FatImage::NameToSize =
{
	make_pair("32M",Size::M32),
	make_pair("64M",Size::M64),
	make_pair("128M",Size::M128),
	make_pair("256M",Size::M256),
	make_pair("512M",Size::M512),
	make_pair("1G",Size::G1),
	make_pair("2G",Size::G2)
};


const vector<uint8_t> FatImage::FAT32 = {
	0xEB,0x58,0x90,0x6D,0x6B,0x66,0x73,0x2E,0x66,0x61,0x74,0x00,0x02,0x01,0x20,0x00,
	0x02,0x00,0x00,0x00,0x00,0xF8,0x00,0x00,0x20,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x02,0x00,0xF1,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
	0x01,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x80,0x00,0x29,0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x46,0x41,0x54,0x33,0x32,0x20,0x20,0x20,0x0E,0x1F,0xBE,0x77,0x7C,0xAC,
	0x22,0xC0,0x74,0x0B,0x56,0xB4,0x0E,0xBB,0x07,0x00,0xCD,0x10,0x5E,0xEB,0xF0,0x32,
	0xE4,0xCD,0x16,0xCD,0x19,0xEB,0xFE
};

const vector<uint8_t> FatImage::FATHeader = {0xF8,0xFF,0xFF,0x0F,0xFF,0xFF,0xFF,0x0F,0xF8,0xFF,0xFF,0x0F};
const vector<uint8_t> FatImage::FSInfo_1 = { 0x55, 0xAA, 0x52, 0x52, 0x61, 0x41 };
const vector<uint8_t> FatImage::FSInfo_2 = { 0x72, 0x72, 0x41, 0x61, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
const vector<uint8_t> FatImage::FSInfo_3 = {0x55, 0xAA};
const vector<uint8_t> FatImage::DataRegion = { 0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x08,0x00,0x00,0x57,0x49,
											0xCD,0x50,0xCD,0x50,0x00,0x00,0x57,0x49,0xCD,0x50};

bool FatImage::MakeFatImage(const string &strFile, const string &strSize)
{
	FatImage::Size size = NameToSize.at(strSize);
	int fd = open(strFile.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0644);
	if (fd < 0) {
		perror(strFile.c_str());
		exit(1);
	}

	uint32_t uiSize = GetSizeInBytes(size);

	if (ftruncate(fd, uiSize)==-1)
	{
		perror(strFile.c_str());
		exit(1);
	}

	vector<uint8_t> data;

	// Write main FAT block.
	data.insert(data.end(), FAT32.begin(), FAT32.end());

	// Set setors per cluster.
	data[0x0D] = GetSectorsPerCluster(size);

	// set sectors and sectors per fat.
	uint32_t uiSector = Byte2Sector(uiSize);
	std::vector<uint8_t> uiSectBytes = {0,0,0,0};

	std::memcpy(uiSectBytes.data(), &uiSector, 4);

	for (int i=0; i<4; i++)
		data[0x20+i] = uiSectBytes.at(i);

	uiSector = SectorsPerFat(size);
	std::memcpy(uiSectBytes.data(), &uiSector, 4);

	for (int i=0; i<4; i++)
		data[0x24+i] = uiSectBytes.at(i);


	// Copy the FS info signature
	data.resize(0x1FE);
	data.insert(data.end(),FSInfo_1.begin(), FSInfo_1.end());

	data.resize(0x3E4);
	data.insert(data.end(),FSInfo_2.begin(),FSInfo_2.end());

	data.resize(0x3FE);
	data.insert(data.end(), FSInfo_3.begin(), FSInfo_3.end());


	// Second copy of boot record @ 0xC00
	data.resize(0xC00);
	data.insert(data.end(), data.begin(), data.begin()+0x200);

	// Copy fat header(s)
	data.resize(FirstFATAddr);
	data.insert(data.end(), FATHeader.begin(), FATHeader.end());

	data.resize(GetSecondFatAddr(size));
	data.insert(data.end(), FATHeader.begin(), FATHeader.end());

	// Data start.
	data.resize(GetDataStartAddr(size));
	data.insert(data.end(), DataRegion.begin(), DataRegion.end());

	lseek(fd,SEEK_SET, 0);
	size_t s = write(fd,data.data(),data.size());
	if(s != data.size())
		fprintf(stderr,"Failed to write full file to disk.\n");

	close(fd);
	return true;
}
