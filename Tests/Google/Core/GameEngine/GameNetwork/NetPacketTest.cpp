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

#include <gtest/gtest.h>

#include "GameNetwork/NetCommandList.h"
#include "GameNetwork/NetCommandMsg.h"
#include "GameNetwork/NetCommandRef.h"
#include "GameNetwork/NetPacket.h"

namespace
{

// The longest packet that Transport::doRecv receives. It is longer than a packet in retail compatible builds.
const Int MaxReceivedLength = MAX_NETWORK_MESSAGE_LEN - static_cast<Int>(sizeof(TransportMessageHeader));

TransportMessage makeTransportMessage(const UnsignedByte *data, Int length)
{
	TransportMessage msg;
	memset(&msg, 0, sizeof(msg));
	if (data != nullptr)
	{
		memcpy(msg.data, data, length);
	}
	msg.length = length;
	return msg;
}

void expectPacketIsDropped(Int length)
{
	const TransportMessage msg = makeTransportMessage(nullptr, length);
	NetPacket packet(msg);

	// The commands of the packet would be read past the packet buffer.
	ASSERT_EQ(packet.getLength(), 0) << "length " << length;

	NetCommandList *list = packet.getCommandList();
	EXPECT_EQ(list->getFirstMessage(), nullptr);
	deleteInstance(list);
}

} // namespace

TEST(NetPacket, ReadsTheCommandsOfAReceivedPacket)
{
	NetPlayerLeaveCommandMsg *leave = newInstance(NetPlayerLeaveCommandMsg);
	leave->setPlayerID(3);
	leave->setLeavingPlayerID(3);
	leave->setExecutionFrame(100);
	leave->setID(7);
	NetCommandRef *ref = NEW_NETCOMMANDREF(leave);
	leave->detach();

	NetPacket sentPacket;
	ASSERT_TRUE(sentPacket.addCommand(ref));
	deleteInstance(ref);

	const TransportMessage msg = makeTransportMessage(sentPacket.getData(), sentPacket.getLength());
	NetPacket packet(msg);
	EXPECT_EQ(packet.getLength(), sentPacket.getLength());

	NetCommandList *list = packet.getCommandList();
	ASSERT_EQ(list->length(), 1);
	NetPlayerLeaveCommandMsg *received = static_cast<NetPlayerLeaveCommandMsg *>(list->getFirstMessage()->getCommand());
	ASSERT_EQ(received->getNetCommandType(), NETCOMMANDTYPE_PLAYERLEAVE);
	EXPECT_EQ(received->getPlayerID(), 3u);
	EXPECT_EQ(received->getLeavingPlayerID(), 3);
	EXPECT_EQ(received->getExecutionFrame(), 100u);
	deleteInstance(list);
}

TEST(NetPacket, KeepsTheLengthOfAPacketThatFillsThePacketBuffer)
{
	const TransportMessage msg = makeTransportMessage(nullptr, MAX_PACKET_SIZE);
	NetPacket packet(msg);
	EXPECT_EQ(packet.getLength(), MAX_PACKET_SIZE);
}

TEST(NetPacket, DropsAPacketThatIsLongerThanThePacketBuffer)
{
	expectPacketIsDropped(MAX_PACKET_SIZE + 1);
}

TEST(NetPacket, DropsTheLongestPacketThatTheTransportReceivesIfItIsLongerThanThePacketBuffer)
{
	if (MaxReceivedLength <= MAX_PACKET_SIZE)
	{
		GTEST_SKIP() << "The transport receives no packet that is longer than the packet buffer in this build";
	}
	expectPacketIsDropped(MaxReceivedLength);
}

TEST(NetPacket, DropsAPacketWithANegativeLength)
{
	expectPacketIsDropped(-1);
}
