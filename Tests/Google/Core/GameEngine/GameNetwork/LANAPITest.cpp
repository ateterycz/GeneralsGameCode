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

#include <cstddef>
#include <cstring>

#include <gtest/gtest.h>

#include "GameNetwork/LANAPI.h"

namespace
{

// A message of the type whose bytes are all set, so that none of its strings is terminated.
LANMessage makeUnterminatedMessage(LANMessage::Type type)
{
	LANMessage msg;
	memset(&msg, 'A', sizeof(msg));
	msg.messageType = type;
	return msg;
}

// The string keeps all of its characters but the last one, which terminates it.
template <typename CharType, size_t Size>
void expectTerminatedAtEnd(const CharType (&str)[Size])
{
	EXPECT_EQ(str[Size - 1], CharType(0));
	for (size_t i = 0; i + 1 < Size; ++i)
	{
		EXPECT_NE(str[i], CharType(0)) << "at index " << i;
	}
}

LANMessage makePreparedMessage(LANMessage::Type type)
{
	LANMessage msg = makeUnterminatedMessage(type);
	EXPECT_TRUE(PrepareReceivedLANMessage(&msg, sizeof(msg)));
	expectTerminatedAtEnd(msg.name);
	expectTerminatedAtEnd(msg.userName);
	expectTerminatedAtEnd(msg.hostName);
	return msg;
}

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

TEST(LANAPI, IgnoresMessageShorterThanALANMessage)
{
	LANMessage msg = makeUnterminatedMessage(LANMessage::MSG_CHAT);
	EXPECT_FALSE(PrepareReceivedLANMessage(&msg, 0));
	EXPECT_FALSE(PrepareReceivedLANMessage(&msg, 1));
	EXPECT_FALSE(PrepareReceivedLANMessage(&msg, sizeof(LANMessage) - 1));
	EXPECT_TRUE(PrepareReceivedLANMessage(&msg, sizeof(LANMessage)));
	EXPECT_TRUE(PrepareReceivedLANMessage(&msg, sizeof(LANMessage) + 1));
}

TEST(LANAPI, TerminatesStringsOfReceivedMessage)
{
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_GAME_ANNOUNCE);
		expectTerminatedAtEnd(msg.GameInfo.gameName);
		expectTerminatedAtEnd(msg.GameInfo.options);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_REQUEST_JOIN);
		expectTerminatedAtEnd(msg.GameToJoin.serial);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_JOIN_ACCEPT);
		expectTerminatedAtEnd(msg.GameJoined.gameName);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_JOIN_DENY);
		expectTerminatedAtEnd(msg.GameNotJoined.gameName);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_REQUEST_GAME_LEAVE);
		expectTerminatedAtEnd(msg.GameToLeave.gameName);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_SET_ACCEPT);
		expectTerminatedAtEnd(msg.Accept.gameName);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_MAP_AVAILABILITY);
		expectTerminatedAtEnd(msg.MapStatus.gameName);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_CHAT);
		expectTerminatedAtEnd(msg.Chat.gameName);
		expectTerminatedAtEnd(msg.Chat.message);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_GAME_OPTIONS);
		expectTerminatedAtEnd(msg.GameOptions.options);
	}
	{
		const LANMessage msg = makePreparedMessage(LANMessage::MSG_REQUEST_GAME_INFO);
		expectTerminatedAtEnd(msg.PlayerInfo.playerName);
	}
}

TEST(LANAPI, KeepsDataOfMessageWithoutStrings)
{
	const LANMessage msg = makePreparedMessage(LANMessage::MSG_GAME_START_TIMER);
	const LANMessage unterminated = makeUnterminatedMessage(LANMessage::MSG_GAME_START_TIMER);
	// The game info is the largest data of a message.
	static_assert(sizeof(LANMessage) == offsetof(LANMessage, GameInfo) + sizeof(LANMessage::GameInfo), "GameInfo must be the largest data");
	EXPECT_EQ(memcmp(&msg.GameInfo, &unterminated.GameInfo, sizeof(msg.GameInfo)), 0);
}
