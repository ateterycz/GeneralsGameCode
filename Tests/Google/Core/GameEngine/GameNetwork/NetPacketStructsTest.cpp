/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "GameNetwork/NetCommandMsg.h"
#include "GameNetwork/NetCommandRef.h"
#include "GameNetwork/NetPacketStructs.h"

namespace
{

const UnsignedByte DataByte = 0x42;
const char *const Filename = "maps/test/test.map";

// Wrapper command data with the given data length, followed by the given number of data bytes in the buffer.
std::vector<UnsignedByte> makeWrapperData(UnsignedInt dataLength, size_t bytesInBuffer)
{
	NetPacketWrapperCommandData::FixedData fixed;
	fixed.wrappedCommandId = 5;
	fixed.chunkNumber = 0;
	fixed.numChunks = 1;
	fixed.totalDataLength = 8;
	fixed.dataLength = dataLength;
	fixed.dataOffset = 0;

	std::vector<UnsignedByte> raw(sizeof(fixed) + bytesInBuffer, DataByte);
	memcpy(raw.data(), &fixed, sizeof(fixed));
	return raw;
}

// File command data with the given file length, followed by the given number of file bytes in the buffer.
std::vector<UnsignedByte> makeFileData(UnsignedInt fileLength, size_t bytesInBuffer)
{
	const size_t filenameSize = strlen(Filename) + 1;
	std::vector<UnsignedByte> raw(filenameSize + sizeof(fileLength) + bytesInBuffer, DataByte);
	memcpy(raw.data(), Filename, filenameSize);
	memcpy(raw.data() + filenameSize, &fileLength, sizeof(fileLength));
	return raw;
}

void expectWrapperMessageReadsEightDataBytes(const std::vector<UnsignedByte> &raw, size_t expectedSize)
{
	NetWrapperCommandMsg *msg = newInstance(NetWrapperCommandMsg);
	NetCommandRef *ref = NEW_NETCOMMANDREF(msg);
	msg->detach();

	EXPECT_EQ(NetPacketWrapperCommandData::readMessage(*ref, NetPacketBuf(raw.data(), raw.size())), expectedSize);
	EXPECT_EQ(msg->getDataLength(), 8u);
	if (msg->getDataLength() == 8)
	{
		EXPECT_EQ(msg->getData()[7], DataByte);
	}

	deleteInstance(ref);
}

void expectFileMessageReadsEightFileBytes(const std::vector<UnsignedByte> &raw, size_t expectedSize)
{
	NetFileCommandMsg *msg = newInstance(NetFileCommandMsg);
	NetCommandRef *ref = NEW_NETCOMMANDREF(msg);
	msg->detach();

	EXPECT_EQ(NetPacketFileCommandData::readMessage(*ref, NetPacketBuf(raw.data(), raw.size())), expectedSize);
	EXPECT_STREQ(msg->getPortableFilename().str(), Filename);
	EXPECT_EQ(msg->getFileLength(), 8u);
	if (msg->getFileLength() == 8)
	{
		EXPECT_EQ(msg->getFileData()[7], DataByte);
	}

	deleteInstance(ref);
}

} // namespace

TEST(NetPacketStructs, WrapperReadMessageReadsItsDataLength)
{
	// The bytes after the data belong to the next command in the packet.
	expectWrapperMessageReadsEightDataBytes(makeWrapperData(8, 12), sizeof(NetPacketWrapperCommandData::FixedData) + 8);
}

TEST(NetPacketStructs, WrapperReadMessageReadsNoMoreDataThanTheBufferHolds)
{
	const std::vector<UnsignedByte> raw = makeWrapperData(0xFFFFFFFF, 8);
	expectWrapperMessageReadsEightDataBytes(raw, raw.size());
}

TEST(NetPacketStructs, FileReadMessageReadsItsFileLength)
{
	// The bytes after the file data belong to the next command in the packet.
	const std::vector<UnsignedByte> raw = makeFileData(8, 12);
	expectFileMessageReadsEightFileBytes(raw, raw.size() - 4);
}

TEST(NetPacketStructs, FileReadMessageReadsNoMoreDataThanTheBufferHolds)
{
	const std::vector<UnsignedByte> raw = makeFileData(0xFFFFFFFF, 8);
	expectFileMessageReadsEightFileBytes(raw, raw.size());
}
