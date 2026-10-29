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

#include <gtest/gtest.h>

#include "GameNetwork/NAT.h"
#include "GameNetwork/GameInfo.h"

namespace
{

// Both players are behind the same NAT, so the local node uses the internal address of its target.
const UnsignedInt ExternalIP = 0x01020304;
const UnsignedInt InternalIP = 0x0A000001;

// The first connection round of a NAT negotiation of two nodes, in which the local node 0 connects to node 1.
class NATForTest : public NAT
{
public:
	NATForTest()
	{
		for (Int i = 0; i < MAX_SLOTS; ++i)
		{
			m_connectionStates[i] = NATCONNECTIONSTATE_NOSTATE;
			m_connectionNodes[i].m_slotIndex = i;
			m_slotPointers[i] = &m_slots[i];
		}
		m_slots[0].setIP(ExternalIP);
		m_slots[1].setIP(ExternalIP);
		m_slotList = m_slotPointers;
		m_numNodes = 2;
		m_connectionPairIndex = 0;
		m_connectionRound = 0;
		m_localNodeNumber = 0;
		m_targetNodeNumber = 1;
	}

	// The connection of the local node is done, so a PORT message changes no more than the address of the target.
	void finishLocalConnection() { m_connectionStates[m_localNodeNumber] = NATCONNECTIONSTATE_DONE; }

	NATConnectionState getConnectionState(Int node) const { return m_connectionStates[node]; }
	UnsignedInt getTargetIP() const { return m_slots[1].getIP(); }

private:
	GameSlot m_slots[MAX_SLOTS];
	GameSlot *m_slotPointers[MAX_SLOTS];
};

} // namespace

TEST(NAT, SetsConnectionStateFromConnDoneOfThisRound)
{
	NATForTest nat;
	nat.processGlobalMessage(1, "CONNDONE1 0");
	EXPECT_EQ(nat.getConnectionState(1), NATCONNECTIONSTATE_DONE);
}

TEST(NAT, IgnoresConnDoneForNodeOutsideTheNegotiation)
{
	NATForTest nat;
	// Node 2 does not take part, so its entry in the connection pairs is -1.
	nat.processGlobalMessage(1, "CONNDONE2 -1");
	nat.processGlobalMessage(1, "CONNDONE-1 0");
	nat.processGlobalMessage(1, "CONNDONE");
	nat.processGlobalMessage(1, "CONNDONE1");
	for (Int node = 0; node < MAX_SLOTS; ++node)
	{
		EXPECT_EQ(nat.getConnectionState(node), NATCONNECTIONSTATE_NOSTATE) << "node " << node;
	}
}

TEST(NAT, UsesInternalAddressFromPortMessage)
{
	NATForTest nat;
	nat.finishLocalConnection();
	nat.processGlobalMessage(1, "PORT1 5000 0A000001");
	EXPECT_EQ(nat.getTargetIP(), InternalIP);
}

TEST(NAT, IgnoresPortMessageWithoutPortAndAddress)
{
	NATForTest nat;
	nat.finishLocalConnection();
	// The message ends after the node number. Past its end, the buffer holds what a reader that misses the end parses.
	const char message[] = "PORT1\0 5000 0A000001";
	nat.processGlobalMessage(1, message);
	EXPECT_EQ(nat.getTargetIP(), ExternalIP);
	nat.processGlobalMessage(1, "PORT1 5000");
	EXPECT_EQ(nat.getTargetIP(), ExternalIP);
}
