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

////// NetCommandWrapperList.h ////////////////////////////////
// Bryan Cleveland

#pragma once

#include "GameNetwork/NetCommandList.h"

// TheSuperHackers @security ateterycz 29/10/2026 Limit the size of a wrapped command to the largest legitimate one.
// That is a file transfer of the largest map plus its command header and file name.
static constexpr const UnsignedInt MAX_WRAPPED_COMMAND_DATA_LENGTH = MAX_MAP_TRANSFER_SIZE + 1024;

// TheSuperHackers @bugfix ateterycz 29/10/2026 Keep a delivered wrapped command for this many milliseconds, to ignore the chunks
// that its sender resends because their ack got lost. Connection resends an unacknowledged command every 2 seconds.
static constexpr const UnsignedInt DELIVERED_WRAPPED_COMMAND_KEEP_TIME = 60 * 1000;

class NetCommandWrapperListNode : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(NetCommandWrapperListNode, "NetCommandWrapperListNode")
public:
	NetCommandWrapperListNode(NetWrapperCommandMsg *msg);
	//virtual ~NetCommandWrapperListNode();

	Bool isComplete();
	Bool isDelivered();
	UnsignedInt getDeliveredTime();
	void markDelivered(UnsignedInt deliveredTime);
	Bool hasSameSizes(NetWrapperCommandMsg *msg);
	UnsignedInt getPlayerID();
	UnsignedShort getCommandID();
	UnsignedInt getNumChunks();
	UnsignedInt getRawDataLength();
	void copyChunkData(NetWrapperCommandMsg *msg);
	UnsignedByte * getRawData();

	Int getPercentComplete();

	NetCommandWrapperListNode *m_next;

protected:
	// TheSuperHackers @bugfix ateterycz 29/10/2026 Identify a wrapped command by its sender too, because players can use the same command ID
	UnsignedInt m_playerID;
	UnsignedShort m_commandID;
	UnsignedByte *m_data;
	UnsignedInt m_totalDataLength;
	Bool *m_chunksPresent;
	UnsignedInt m_numChunks;
	UnsignedInt m_numChunksPresent;
	// TheSuperHackers @bugfix ateterycz 29/10/2026 Remember the delivery of a wrapped command, to ignore the chunks that its sender resends
	Bool m_isDelivered;
	UnsignedInt m_deliveredTime;

};

class NetCommandWrapperList : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(NetCommandWrapperList, "NetCommandWrapperList")
public:
	NetCommandWrapperList();
	//virtual ~NetCommandWrapperList();

	void init();
	void reset();

	void processWrapper(NetCommandRef *ref);
	NetCommandList * getReadyCommands(UnsignedInt curTime);

	Int getPercentComplete(UnsignedInt playerID, UnsignedShort wrappedCommandID);

protected:
	void removeFromList(NetCommandWrapperListNode *node);

	NetCommandWrapperListNode *m_list;
};
