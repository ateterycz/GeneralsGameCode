/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

////// NetCommandWrapperList.cpp ////////////////////////////////
// Bryan Cleveland

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameNetwork/NetCommandWrapperList.h"
#include "GameNetwork/NetPacket.h"

// TheSuperHackers @security ateterycz 29/10/2026 Limit the memory that the pending wrapped commands of one player hold.
// This fits the node of the largest map transfer and leaves about as much again for other wrapped commands in flight.
static const UnsignedInt MAX_PENDING_WRAPPED_COMMAND_MEMORY_PER_PLAYER = 2 * MAX_WRAPPED_COMMAND_DATA_LENGTH;

static UnsignedInt getNodeMemorySize(UnsignedInt totalDataLength, UnsignedInt numChunks)
{
	return sizeof(NetCommandWrapperListNode) + totalDataLength + numChunks * sizeof(Bool);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////// NetCommandWrapperListNode ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

NetCommandWrapperListNode::NetCommandWrapperListNode(NetWrapperCommandMsg *msg)
{
	m_next = nullptr;
	m_numChunks = msg->getNumChunks();
	m_chunksPresent = NEW Bool[m_numChunks];	// pool[]ify
	m_numChunksPresent = 0;

	for (UnsignedInt i = 0; i < m_numChunks; ++i) {
		m_chunksPresent[i] = FALSE;
	}

	m_totalDataLength = msg->getTotalDataLength();
	m_data = NEW UnsignedByte[m_totalDataLength];	// pool[]ify
	// TheSuperHackers @security ateterycz 29/10/2026 Zero the data here instead of relying on the allocator to do it,
	// so that bytes no chunk writes are never leftover memory, even if this allocation is moved to a pool.
	memset(m_data, 0, m_totalDataLength);

	m_playerID = msg->getPlayerID();
	m_commandID = msg->getWrappedCommandID();
	m_isDelivered = FALSE;
	m_deliveredTime = 0;
}

NetCommandWrapperListNode::~NetCommandWrapperListNode() {
	delete[] m_chunksPresent;
	m_chunksPresent = nullptr;

	delete[] m_data;
	m_data = nullptr;
}

Bool NetCommandWrapperListNode::isComplete() {
	return m_numChunksPresent == m_numChunks;
}

Bool NetCommandWrapperListNode::isDelivered() {
	return m_isDelivered;
}

UnsignedInt NetCommandWrapperListNode::getDeliveredTime() {
	return m_deliveredTime;
}

// TheSuperHackers @bugfix ateterycz 29/10/2026 Free the data of a delivered wrapped command, but keep the node to recognize the chunks that its sender resends
void NetCommandWrapperListNode::markDelivered(UnsignedInt deliveredTime) {
	delete[] m_chunksPresent;
	m_chunksPresent = nullptr;

	delete[] m_data;
	m_data = nullptr;

	m_isDelivered = TRUE;
	m_deliveredTime = deliveredTime;
}

Bool NetCommandWrapperListNode::hasSameSizes(NetWrapperCommandMsg *msg) {
	return msg->getNumChunks() == m_numChunks && msg->getTotalDataLength() == m_totalDataLength;
}

Int NetCommandWrapperListNode::getPercentComplete() {
	if (isComplete())
		return 100;
	else
		return min(99, REAL_TO_INT( ((Real)m_numChunksPresent)/((Real)m_numChunks)*100.0f ));
}

UnsignedInt NetCommandWrapperListNode::getPlayerID() {
	return m_playerID;
}

UnsignedShort NetCommandWrapperListNode::getCommandID() {
	return m_commandID;
}

UnsignedInt NetCommandWrapperListNode::getNumChunks() {
	return m_numChunks;
}

UnsignedInt NetCommandWrapperListNode::getRawDataLength() {
	return m_totalDataLength;
}

void NetCommandWrapperListNode::copyChunkData(NetWrapperCommandMsg *msg) {
	if (msg == nullptr) {
		DEBUG_CRASH(("Trying to copy data from a non-existent wrapper command message"));
		return;
	}

	UnsignedInt chunkNumber = msg->getChunkNumber();

	if (chunkNumber >= m_numChunks) {
		DEBUG_CRASH(("Data chunk %u exceeds the expected maximum of %u chunks", chunkNumber, m_numChunks));
		return;
	}

	// TheSuperHackers @security ateterycz 29/10/2026 Reject a chunk that does not belong to the same wrapped command
	if (!hasSameSizes(msg)) {
		DEBUG_CRASH(("Data chunk count %u and total length %u do not match the expected %u and %u", msg->getNumChunks(), msg->getTotalDataLength(), m_numChunks, m_totalDataLength));
		return;
	}

	// we already received this chunk, no need to recopy it.
	if (m_chunksPresent[chunkNumber] == TRUE) {
		return;
	}

	UnsignedInt chunkDataOffset = msg->getDataOffset();
	UnsignedInt chunkDataLength = msg->getDataLength();

	// TheSuperHackers @security Mauller 04/12/2025 Prevent out of bounds memory access
	if (chunkDataOffset >= m_totalDataLength) {
		DEBUG_CRASH(("Data chunk offset %u exceeds the total data length %u", chunkDataOffset, m_totalDataLength));
		return;
	}

	if (chunkDataLength > MAX_PACKET_SIZE ) {
		DEBUG_CRASH(("Data Chunk size %u greater than max packet size %u", chunkDataLength, MAX_PACKET_SIZE));
		return;
	}

	if (chunkDataOffset + chunkDataLength > m_totalDataLength) {
		DEBUG_CRASH(("Data chunk exceeds data array size"));
		return;
	}

	DEBUG_LOG(("NetCommandWrapperListNode::copyChunkData() - copying chunk %u", chunkNumber));

	memcpy(m_data + chunkDataOffset, msg->getData(), chunkDataLength);

	m_chunksPresent[chunkNumber] = TRUE;
	++m_numChunksPresent;
}

UnsignedByte * NetCommandWrapperListNode::getRawData() {
	return m_data;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////// NetCommandWrapperList ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

NetCommandWrapperList::NetCommandWrapperList() {
	m_list = nullptr;
}

NetCommandWrapperList::~NetCommandWrapperList() {
	NetCommandWrapperListNode *temp;
	while (m_list != nullptr) {
		temp = m_list->m_next;
		deleteInstance(m_list);
		m_list = temp;
	}
}

void NetCommandWrapperList::init() {
	m_list = nullptr;
}

void NetCommandWrapperList::reset() {
	NetCommandWrapperListNode *temp;
	while (m_list != nullptr) {
		temp = m_list->m_next;
		deleteInstance(m_list);
		m_list = temp;
	}
}

Int NetCommandWrapperList::getPercentComplete(UnsignedInt playerID, UnsignedShort wrappedCommandID)
{
	NetCommandWrapperListNode *temp = m_list;

	// TheSuperHackers @bugfix ateterycz 29/10/2026 Find the wrapped command of this sender, because players can use the same command ID
	while ((temp != nullptr) && ((temp->getPlayerID() != playerID) || (temp->getCommandID() != wrappedCommandID))) {
		temp = temp->m_next;
	}

	if (!temp)
		return 0;

	return temp->getPercentComplete();
}

void NetCommandWrapperList::processWrapper(NetCommandRef *ref) {
	NetCommandWrapperListNode *temp = m_list;
	NetWrapperCommandMsg *msg = (NetWrapperCommandMsg *)(ref->getCommand());

	// TheSuperHackers @bugfix ateterycz 29/10/2026 Find the wrapped command of this sender, because players can use the same command ID
	while ((temp != nullptr) && ((temp->getPlayerID() != msg->getPlayerID()) || (temp->getCommandID() != msg->getWrappedCommandID()))) {
		temp = temp->m_next;
	}

	// TheSuperHackers @bugfix ateterycz 29/10/2026 Ignore a chunk that the sender resends after the wrapped command was delivered,
	// because its ack got lost. A chunk with other sizes belongs to a new wrapped command that reuses the command ID.
	if ((temp != nullptr) && temp->isDelivered()) {
		if (temp->hasSameSizes(msg)) {
			return;
		}
		removeFromList(temp);
		temp = nullptr;
	}

	if (temp == nullptr) {
		// TheSuperHackers @security ateterycz 29/10/2026 Validate the wrapped command sizes before allocating memory for them
		const UnsignedInt totalDataLength = msg->getTotalDataLength();
		const UnsignedInt numChunks = msg->getNumChunks();

		if (totalDataLength == 0 || totalDataLength > MAX_WRAPPED_COMMAND_DATA_LENGTH) {
			DEBUG_CRASH(("Wrapped command data length %u is not between 1 and %u", totalDataLength, MAX_WRAPPED_COMMAND_DATA_LENGTH));
			return;
		}

		// Every chunk carries at least one byte and at most one packet of data.
		const UnsignedInt minNumChunks = (totalDataLength + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
		if (numChunks < minNumChunks || numChunks > totalDataLength) {
			DEBUG_CRASH(("Wrapped command chunk count %u does not match its data length %u", numChunks, totalDataLength));
			return;
		}

		// TheSuperHackers @security ateterycz 29/10/2026 Limit the memory that the pending wrapped commands of one player hold
		UnsignedInt pendingMemorySize = 0;
		for (NetCommandWrapperListNode *node = m_list; node != nullptr; node = node->m_next) {
			if ((node->getPlayerID() == msg->getPlayerID()) && !node->isDelivered()) {
				pendingMemorySize += getNodeMemorySize(node->getRawDataLength(), node->getNumChunks());
			}
		}

		const UnsignedInt memorySize = getNodeMemorySize(totalDataLength, numChunks);
		if (pendingMemorySize + memorySize > MAX_PENDING_WRAPPED_COMMAND_MEMORY_PER_PLAYER) {
			DEBUG_CRASH(("Wrapped commands of player %u would hold %u bytes, more than the limit of %u", msg->getPlayerID(), pendingMemorySize + memorySize, MAX_PENDING_WRAPPED_COMMAND_MEMORY_PER_PLAYER));
			return;
		}

		temp = newInstance(NetCommandWrapperListNode)(msg);
		temp->m_next = m_list;
		m_list = temp;
	}

	temp->copyChunkData(msg);
}

NetCommandList * NetCommandWrapperList::getReadyCommands(UnsignedInt curTime)
{
	NetCommandList *retlist = newInstance(NetCommandList);
	retlist->init();

	NetCommandWrapperListNode *temp = m_list;
	NetCommandWrapperListNode *next = nullptr;

	while (temp != nullptr) {
		next = temp->m_next;
		if (temp->isDelivered()) {
			// TheSuperHackers @bugfix ateterycz 29/10/2026 Forget a delivered wrapped command once its sender has stopped resending its chunks
			if (curTime - temp->getDeliveredTime() > DELIVERED_WRAPPED_COMMAND_KEEP_TIME) {
				removeFromList(temp);
				temp = nullptr;
			}
		} else if (temp->isComplete()) {
			NetCommandRef *msg = NetPacket::ConstructNetCommandMsgFromRawData(temp->getRawData(), temp->getRawDataLength());
			// TheSuperHackers @security ateterycz 29/10/2026 Discard wrapped data that is not a valid command or is a duplicate
			if (msg != nullptr) {
				NetCommandRef *ret = retlist->addMessage(msg->getCommand());
				if (ret != nullptr) {
					ret->setRelay(msg->getRelay());
				}

				deleteInstance(msg);
				msg = nullptr;
			}

			temp->markDelivered(curTime);
		}
		temp = next;
	}

	return retlist;
}

void NetCommandWrapperList::removeFromList(NetCommandWrapperListNode *node) {
	if (node == nullptr) {
		return;
	}

	NetCommandWrapperListNode *temp = m_list;
	NetCommandWrapperListNode *prev = nullptr;

	// TheSuperHackers @bugfix ateterycz 29/10/2026 Find the node itself, because its command ID alone is not unique
	while ((temp != nullptr) && (temp != node)) {
		prev = temp;
		temp = temp->m_next;
	}

	if (temp == nullptr) {
		return;
	}

	if (prev == nullptr) {
		m_list = temp->m_next;
		deleteInstance(temp);
		temp = nullptr;
	} else {
		prev->m_next = temp->m_next;
		deleteInstance(temp);
		temp = nullptr;
	}
}
