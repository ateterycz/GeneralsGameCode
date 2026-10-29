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

#include "GameNetwork/LANAPI.h"

namespace
{

const UnsignedInt HostIP = 0xC0A80001;
const UnsignedInt PlayerIP = 0xC0A80002;
const UnsignedInt OtherIP = 0xC0A80003;

// The slots of a game with its host in slot 0 and another player in slot 2.
class GameSlots
{
public:
	GameSlots()
	{
		m_slots[0].setState(SLOT_PLAYER, L"Host", HostIP);
		m_slots[2].setState(SLOT_PLAYER, L"Player", PlayerIP);
		for (Int i = 0; i < MAX_SLOTS; ++i)
		{
			m_slotPointers[i] = &m_slots[i];
		}
	}

	const GameSlot *const *get() const { return m_slotPointers; }

private:
	GameSlot m_slots[MAX_SLOTS];
	const GameSlot *m_slotPointers[MAX_SLOTS];
};

} // namespace

TEST(LANAPI, HostMayAnnounceItsGame)
{
	const GameSlots slots;
	EXPECT_TRUE(MayAnnounceGame(slots.get(), HostIP, FALSE));
	EXPECT_TRUE(MayAnnounceGame(slots.get(), HostIP, TRUE));
}

TEST(LANAPI, OtherPlayerMayAnnounceOnlyAGameInProgress)
{
	const GameSlots slots;
	EXPECT_FALSE(MayAnnounceGame(slots.get(), PlayerIP, FALSE));
	EXPECT_TRUE(MayAnnounceGame(slots.get(), PlayerIP, TRUE));
}

TEST(LANAPI, SenderOutsideTheGameMayNotAnnounceIt)
{
	const GameSlots slots;
	EXPECT_FALSE(MayAnnounceGame(slots.get(), OtherIP, FALSE));
	EXPECT_FALSE(MayAnnounceGame(slots.get(), OtherIP, TRUE));
}
