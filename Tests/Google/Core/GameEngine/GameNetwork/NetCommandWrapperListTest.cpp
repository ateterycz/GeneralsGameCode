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

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Common/Debug.h"
#include "GameNetwork/NetCommandList.h"
#include "GameNetwork/NetCommandMsg.h"
#include "GameNetwork/NetCommandRef.h"
#include "GameNetwork/NetCommandWrapperList.h"
#include "GameNetwork/NetPacket.h"

namespace
{

const char *const DefaultFilename = "maps/test/test.map";

// The rejected messages raise a debug crash in builds with debug crashing, which would fail the test.
class ScopedIgnoreDebugCrash
{
public:
	ScopedIgnoreDebugCrash() : m_previousHandler(DebugGetCrashHandler()) { DebugSetCrashHandler(ignoreCrash); }
	~ScopedIgnoreDebugCrash() { DebugSetCrashHandler(m_previousHandler); }

private:
	static void ignoreCrash(const char *) {}

	DebugCrashHandler m_previousHandler;
};

// The file data depends on the player, so that the files of different players differ.
NetFileCommandMsg *newFileCommand(UnsignedInt playerID, UnsignedShort commandID, UnsignedInt fileLength, const char *portableFilename = DefaultFilename)
{
	NetCommandDataChunk fileData(fileLength);
	for (UnsignedInt i = 0; i < fileLength; ++i)
	{
		fileData.data()[i] = static_cast<UnsignedByte>(i * 7 + playerID);
	}

	NetFileCommandMsg *msg = newInstance(NetFileCommandMsg);
	msg->setPlayerID(playerID);
	msg->setID(commandID);
	msg->setPortableFilename(portableFilename);
	msg->setFileData(fileData);
	return msg;
}

std::vector<UnsignedByte> serializeCommand(NetCommandMsg *msg)
{
	NetCommandRef *ref = NEW_NETCOMMANDREF(msg);
	std::vector<UnsignedByte> buffer(msg->getSizeForNetPacket());
	msg->copyBytesForNetPacket(buffer.data(), *ref);
	deleteInstance(ref);
	return buffer;
}

NetCommandRef *newWrapperChunk(UnsignedInt playerID, UnsignedShort wrappedCommandID, UnsignedInt chunkNumber, UnsignedInt numChunks,
	UnsignedInt totalDataLength, UnsignedInt dataOffset, const std::vector<UnsignedByte> &data)
{
	NetCommandDataChunk chunkData(static_cast<UnsignedInt>(data.size()));
	if (!data.empty())
	{
		memcpy(chunkData.data(), data.data(), data.size());
	}

	NetWrapperCommandMsg *msg = newInstance(NetWrapperCommandMsg);
	msg->setPlayerID(playerID);
	msg->setWrappedCommandID(wrappedCommandID);
	msg->setChunkNumber(chunkNumber);
	msg->setNumChunks(numChunks);
	msg->setTotalDataLength(totalDataLength);
	msg->setDataOffset(dataOffset);
	msg->setData(chunkData);

	NetCommandRef *ref = NEW_NETCOMMANDREF(msg);
	msg->detach();
	return ref;
}

std::vector<NetCommandRef *> getCommands(NetCommandList *list)
{
	std::vector<NetCommandRef *> commands;
	for (NetCommandRef *ref = list->getFirstMessage(); ref != nullptr; ref = ref->getNext())
	{
		commands.push_back(ref);
	}
	return commands;
}

// A file transfer split into wrapper chunks, the same way as Connection::sendNetCommandMsg does it.
class ChunkedFileTransfer
{
public:
	ChunkedFileTransfer(UnsignedInt playerID, UnsignedShort commandID, UnsignedInt fileLength, const char *portableFilename = DefaultFilename)
		: m_fileMsg(newFileCommand(playerID, commandID, fileLength, portableFilename))
		, m_fileRef(NEW_NETCOMMANDREF(m_fileMsg))
		, m_chunkList(nullptr)
	{
		m_fileRef->setRelay(0x06);
		m_chunkList = NetPacket::ConstructBigCommandList(m_fileRef);
		if (m_chunkList != nullptr)
		{
			m_chunks = getCommands(m_chunkList);
		}
	}

	~ChunkedFileTransfer()
	{
		deleteInstance(m_chunkList);
		deleteInstance(m_fileRef);
		m_fileMsg->detach();
	}

	ChunkedFileTransfer(const ChunkedFileTransfer &) = delete;
	ChunkedFileTransfer &operator=(const ChunkedFileTransfer &) = delete;

	const NetFileCommandMsg *getFileMsg() const { return m_fileMsg; }
	const std::vector<NetCommandRef *> &getChunks() const { return m_chunks; }

private:
	NetFileCommandMsg *m_fileMsg;
	NetCommandRef *m_fileRef;
	NetCommandList *m_chunkList;
	std::vector<NetCommandRef *> m_chunks;
};

void expectSameFileCommand(const NetCommandRef *ref, const NetFileCommandMsg *expected)
{
	ASSERT_EQ(ref->getCommand()->getNetCommandType(), NETCOMMANDTYPE_FILE);
	const NetFileCommandMsg *msg = static_cast<const NetFileCommandMsg *>(ref->getCommand());
	EXPECT_EQ(ref->getRelay(), 0x06);
	EXPECT_EQ(msg->getID(), expected->getID());
	EXPECT_EQ(msg->getPlayerID(), expected->getPlayerID());
	EXPECT_STREQ(msg->getPortableFilename().str(), expected->getPortableFilename().str());
	ASSERT_EQ(msg->getFileLength(), expected->getFileLength());
	EXPECT_EQ(memcmp(msg->getFileData(), expected->getFileData(), expected->getFileLength()), 0);
}

} // namespace

class NetCommandWrapperListTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		m_wrapperList = newInstance(NetCommandWrapperList);
		m_curTime = 0;
	}

	virtual void TearDown() override
	{
		deleteInstance(m_wrapperList);
		m_wrapperList = nullptr;
	}

	void processChunk(NetCommandRef *ref)
	{
		m_wrapperList->processWrapper(ref);
		deleteInstance(ref);
	}

	void processAllChunks(const ChunkedFileTransfer &transfer)
	{
		for (size_t i = 0; i < transfer.getChunks().size(); ++i)
		{
			m_wrapperList->processWrapper(transfer.getChunks()[i]);
		}
	}

	void processAllButLastChunk(const ChunkedFileTransfer &transfer)
	{
		for (size_t i = 0; i + 1 < transfer.getChunks().size(); ++i)
		{
			m_wrapperList->processWrapper(transfer.getChunks()[i]);
		}
	}

	void processLastChunk(const ChunkedFileTransfer &transfer)
	{
		m_wrapperList->processWrapper(transfer.getChunks().back());
	}

	NetCommandList *getReadyCommands()
	{
		return m_wrapperList->getReadyCommands(m_curTime);
	}

	size_t countReadyCommands()
	{
		NetCommandList *ready = getReadyCommands();
		const size_t count = getCommands(ready).size();
		deleteInstance(ready);
		return count;
	}

	void expectReadyFileCommand(const ChunkedFileTransfer &transfer)
	{
		NetCommandList *ready = getReadyCommands();
		const std::vector<NetCommandRef *> readyCommands = getCommands(ready);
		EXPECT_EQ(readyCommands.size(), 1u);
		if (readyCommands.size() == 1)
		{
			expectSameFileCommand(readyCommands[0], transfer.getFileMsg());
		}
		deleteInstance(ready);
	}

	// A valid command under the same wrapped command ID only comes out if no node of a rejected chunk blocks it.
	void expectValidCommandIsReady(UnsignedInt playerID, UnsignedShort wrappedCommandID)
	{
		NetFileCommandMsg *fileMsg = newFileCommand(playerID, wrappedCommandID, 16);
		const std::vector<UnsignedByte> raw = serializeCommand(fileMsg);
		fileMsg->detach();

		processChunk(newWrapperChunk(playerID, wrappedCommandID, 0, 1, static_cast<UnsignedInt>(raw.size()), 0, raw));
		EXPECT_EQ(countReadyCommands(), 1u);
	}

	void expectFileTransferIsReassembled(UnsignedShort commandID, UnsignedInt fileLength, const char *portableFilename)
	{
		ChunkedFileTransfer transfer(1, commandID, fileLength, portableFilename);
		ASSERT_GT(transfer.getChunks().size(), 1u);

		processAllButLastChunk(transfer);
		EXPECT_EQ(countReadyCommands(), 0u);
		EXPECT_LT(m_wrapperList->getPercentComplete(1, commandID), 100);

		processLastChunk(transfer);
		expectReadyFileCommand(transfer);
	}

	NetCommandWrapperList *m_wrapperList;
	UnsignedInt m_curTime;
};

TEST_F(NetCommandWrapperListTest, ReassemblesMultiChunkFileTransfer)
{
	expectFileTransferIsReassembled(100, 2000, DefaultFilename);
}

TEST_F(NetCommandWrapperListTest, ReassemblesLargestMapTransfer)
{
	// The largest map that ConnectionManager accepts, with a file name of the longest path it reads.
	const std::string longFilename = std::string("maps/") + std::string(_MAX_PATH - 9, 'a') + ".map";
	ASSERT_EQ(longFilename.size(), static_cast<size_t>(_MAX_PATH));
	expectFileTransferIsReassembled(101, MAX_MAP_TRANSFER_SIZE, longFilename.c_str());
}

TEST_F(NetCommandWrapperListTest, RejectsOversizedTotalDataLength)
{
	const std::vector<UnsignedByte> data(16, 0);
	{
		ScopedIgnoreDebugCrash ignoreCrash;
		processChunk(newWrapperChunk(1, 200, 0, 0x02000000, 0xFFFFFFFF, 0, data));
		processChunk(newWrapperChunk(1, 201, 0, 12000, MAX_WRAPPED_COMMAND_DATA_LENGTH + 1, 0, data));
	}
	EXPECT_EQ(countReadyCommands(), 0u);
	expectValidCommandIsReady(1, 200);
	expectValidCommandIsReady(1, 201);
}

TEST_F(NetCommandWrapperListTest, RejectsZeroSizes)
{
	const std::vector<UnsignedByte> data(16, 0);
	{
		ScopedIgnoreDebugCrash ignoreCrash;
		processChunk(newWrapperChunk(1, 300, 0, 1, 0, 0, data));
		processChunk(newWrapperChunk(1, 301, 0, 0, 100, 0, data));
	}
	EXPECT_EQ(countReadyCommands(), 0u);
	expectValidCommandIsReady(1, 300);
	expectValidCommandIsReady(1, 301);
}

TEST_F(NetCommandWrapperListTest, RejectsNumChunksInconsistentWithTotalDataLength)
{
	const std::vector<UnsignedByte> data(16, 0);
	{
		ScopedIgnoreDebugCrash ignoreCrash;
		// More chunks than bytes.
		processChunk(newWrapperChunk(1, 400, 0, 101, 100, 0, data));
		// Too few chunks to carry the data with one packet per chunk.
		processChunk(newWrapperChunk(1, 401, 0, 1, 100000, 0, data));
	}
	EXPECT_EQ(countReadyCommands(), 0u);
	expectValidCommandIsReady(1, 400);
	expectValidCommandIsReady(1, 401);
}

TEST_F(NetCommandWrapperListTest, IgnoresChunkWithDifferentSizesForSameCommand)
{
	ChunkedFileTransfer transfer(1, 500, 2000);
	ASSERT_GT(transfer.getChunks().size(), 1u);

	processAllButLastChunk(transfer);

	// A forged last chunk that fits the buffer of the real command, but claims other sizes.
	const NetWrapperCommandMsg *lastChunk = static_cast<const NetWrapperCommandMsg *>(transfer.getChunks().back()->getCommand());
	const std::vector<UnsignedByte> garbage(lastChunk->getDataLength(), 0xAA);
	{
		ScopedIgnoreDebugCrash ignoreCrash;
		processChunk(newWrapperChunk(1, 500, lastChunk->getChunkNumber(), lastChunk->getNumChunks() + 1,
			lastChunk->getTotalDataLength() + MAX_PACKET_SIZE, lastChunk->getDataOffset(), garbage));
	}
	EXPECT_EQ(countReadyCommands(), 0u);

	processLastChunk(transfer);
	expectReadyFileCommand(transfer);
}

TEST_F(NetCommandWrapperListTest, DiscardsUnrecognizedReassembledData)
{
	const std::vector<UnsignedByte> data(4, 'X');
	processChunk(newWrapperChunk(1, 600, 0, 1, static_cast<UnsignedInt>(data.size()), 0, data));
	{
		ScopedIgnoreDebugCrash ignoreCrash;
		EXPECT_EQ(countReadyCommands(), 0u);
	}
	// The delivered node only blocks chunks of the same sizes, so a new command can reuse the command ID.
	EXPECT_EQ(m_wrapperList->getPercentComplete(1, 600), 100);
	expectValidCommandIsReady(1, 600);
}

TEST_F(NetCommandWrapperListTest, DiscardsDuplicateReassembledCommand)
{
	NetFileCommandMsg *fileMsg = newFileCommand(1, 700, 16);
	const std::vector<UnsignedByte> raw = serializeCommand(fileMsg);
	fileMsg->detach();

	// Two wrapped commands that carry the same command complete at the same time.
	processChunk(newWrapperChunk(1, 701, 0, 1, static_cast<UnsignedInt>(raw.size()), 0, raw));
	processChunk(newWrapperChunk(1, 702, 0, 1, static_cast<UnsignedInt>(raw.size()), 0, raw));
	EXPECT_EQ(countReadyCommands(), 1u);
}

TEST_F(NetCommandWrapperListTest, ReassemblesSameWrappedCommandIDFromTwoPlayers)
{
	// Every client counts its command IDs from 0, so two players can send wrapped commands with the same ID.
	ChunkedFileTransfer first(1, 800, 2000);
	ChunkedFileTransfer second(2, 800, 3000);
	ASSERT_GT(first.getChunks().size(), 1u);
	ASSERT_GT(second.getChunks().size(), first.getChunks().size());

	for (size_t i = 0; i + 1 < second.getChunks().size(); ++i)
	{
		if (i + 1 < first.getChunks().size())
		{
			m_wrapperList->processWrapper(first.getChunks()[i]);
		}
		m_wrapperList->processWrapper(second.getChunks()[i]);
	}
	EXPECT_EQ(countReadyCommands(), 0u);
	EXPECT_GT(m_wrapperList->getPercentComplete(1, 800), 0);
	EXPECT_LT(m_wrapperList->getPercentComplete(1, 800), 100);
	EXPECT_GT(m_wrapperList->getPercentComplete(2, 800), 0);
	EXPECT_LT(m_wrapperList->getPercentComplete(2, 800), 100);

	processLastChunk(first);
	processLastChunk(second);

	NetCommandList *ready = getReadyCommands();
	const std::vector<NetCommandRef *> readyCommands = getCommands(ready);
	EXPECT_EQ(readyCommands.size(), 2u);
	for (size_t i = 0; i < readyCommands.size(); ++i)
	{
		const Bool isFirst = readyCommands[i]->getCommand()->getPlayerID() == 1;
		expectSameFileCommand(readyCommands[i], isFirst ? first.getFileMsg() : second.getFileMsg());
	}
	deleteInstance(ready);
}

TEST_F(NetCommandWrapperListTest, RejectsWrappedCommandsBeyondPlayerBudget)
{
	// The budget of a player fits two pending transfers of this size, but not three.
	const UnsignedInt fileLength = 5000000;
	ChunkedFileTransfer first(1, 900, fileLength);
	ChunkedFileTransfer second(1, 901, fileLength);
	ChunkedFileTransfer third(1, 902, fileLength);

	processAllButLastChunk(first);
	processAllButLastChunk(second);
	{
		ScopedIgnoreDebugCrash ignoreCrash;
		processAllChunks(third);
	}
	EXPECT_EQ(countReadyCommands(), 0u);

	// The budget of player 1 does not block the transfers of another player.
	ChunkedFileTransfer otherPlayer(2, 902, fileLength);
	processAllChunks(otherPlayer);
	EXPECT_EQ(countReadyCommands(), 1u);

	// The completed transfers free the budget, and the rejected chunks left no node behind.
	processLastChunk(first);
	processLastChunk(second);
	EXPECT_EQ(countReadyCommands(), 2u);

	processAllChunks(third);
	EXPECT_EQ(countReadyCommands(), 1u);
}

TEST_F(NetCommandWrapperListTest, IgnoresChunksResentAfterDelivery)
{
	// The sender resends a chunk whose ack got lost, which can arrive after the wrapped command is delivered.
	const UnsignedInt fileLength = 5000000;
	ChunkedFileTransfer first(1, 1100, fileLength);
	ChunkedFileTransfer second(1, 1101, fileLength);
	ChunkedFileTransfer third(1, 1102, fileLength);

	processAllChunks(first);
	processAllChunks(second);
	EXPECT_EQ(countReadyCommands(), 2u);

	m_wrapperList->processWrapper(first.getChunks().front());
	m_wrapperList->processWrapper(second.getChunks().back());
	EXPECT_EQ(countReadyCommands(), 0u);
	EXPECT_EQ(m_wrapperList->getPercentComplete(1, 1100), 100);
	EXPECT_EQ(m_wrapperList->getPercentComplete(1, 1101), 100);

	// The resent chunks hold none of the budget, which fits two pending transfers of this size.
	processAllButLastChunk(third);
	ChunkedFileTransfer fourth(1, 1103, fileLength);
	processAllChunks(fourth);
	EXPECT_EQ(countReadyCommands(), 1u);
}

TEST_F(NetCommandWrapperListTest, ForgetsDeliveredCommandAfterKeepTime)
{
	// The delivery time is close to the wrap of the millisecond timer.
	m_curTime = 0xFFFFFFFF - 1000;
	ChunkedFileTransfer transfer(1, 1200, 2000);
	processAllChunks(transfer);
	EXPECT_EQ(countReadyCommands(), 1u);

	m_curTime += DELIVERED_WRAPPED_COMMAND_KEEP_TIME;
	EXPECT_EQ(countReadyCommands(), 0u);
	EXPECT_EQ(m_wrapperList->getPercentComplete(1, 1200), 100);

	m_curTime += 1;
	EXPECT_EQ(countReadyCommands(), 0u);
	EXPECT_EQ(m_wrapperList->getPercentComplete(1, 1200), 0);
}

TEST_F(NetCommandWrapperListTest, ZeroFillsDataThatNoChunkWrites)
{
	const UnsignedInt fileLength = 64;
	NetFileCommandMsg *previousMsg = newFileCommand(1, 1000, fileLength);
	const std::vector<UnsignedByte> previousRaw = serializeCommand(previousMsg);
	previousMsg->detach();

	NetFileCommandMsg *fileMsg = newFileCommand(1, 1001, fileLength);
	const std::vector<UnsignedByte> raw = serializeCommand(fileMsg);
	fileMsg->detach();
	ASSERT_EQ(raw.size(), previousRaw.size());
	const UnsignedInt totalDataLength = static_cast<UnsignedInt>(raw.size());

	// A completed wrapped command frees its memory, which the next one of the same size can get again.
	// The data of that memory must not show up in the gap of the next command.
	processChunk(newWrapperChunk(1, 1000, 0, 1, totalDataLength, 0, previousRaw));
	EXPECT_EQ(countReadyCommands(), 1u);

	// The chunks write the command header and the first byte of the file data, but nothing after it.
	const UnsignedInt headerLength = totalDataLength - fileLength;
	const std::vector<UnsignedByte> header(raw.begin(), raw.begin() + headerLength);
	const std::vector<UnsignedByte> firstFileByte(1, 0x11);
	processChunk(newWrapperChunk(1, 1001, 0, 2, totalDataLength, 0, header));
	processChunk(newWrapperChunk(1, 1001, 1, 2, totalDataLength, headerLength, firstFileByte));

	NetCommandList *ready = getReadyCommands();
	const std::vector<NetCommandRef *> readyCommands = getCommands(ready);
	ASSERT_EQ(readyCommands.size(), 1u);
	ASSERT_EQ(readyCommands[0]->getCommand()->getNetCommandType(), NETCOMMANDTYPE_FILE);
	const NetFileCommandMsg *readyMsg = static_cast<const NetFileCommandMsg *>(readyCommands[0]->getCommand());
	ASSERT_EQ(readyMsg->getFileLength(), fileLength);
	EXPECT_EQ(readyMsg->getFileData()[0], 0x11);

	UnsignedInt nonZeroBytes = 0;
	for (UnsignedInt i = 1; i < fileLength; ++i)
	{
		if (readyMsg->getFileData()[i] != 0)
		{
			++nonZeroBytes;
		}
	}
	EXPECT_EQ(nonZeroBytes, 0u);

	deleteInstance(ready);
}
