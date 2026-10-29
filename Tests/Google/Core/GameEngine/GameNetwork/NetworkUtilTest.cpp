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

#include "Common/GameCommon.h"
#include "GameNetwork/networkutil.h"

namespace
{

// The retail game sends no run ahead below 10.
const Int RetailMinRunAhead = 10;

const Int LogicFrameRate = LOGICFRAMES_PER_SECOND;

} // namespace

TEST(NetworkUtil, DisconnectVoteIsValidForSlotsThatExist)
{
	EXPECT_TRUE(IsDisconnectVoteValid(0, 0));
	EXPECT_TRUE(IsDisconnectVoteValid(MAX_SLOTS - 1, 0));
	EXPECT_TRUE(IsDisconnectVoteValid(0, MAX_SLOTS - 1));
	EXPECT_TRUE(IsDisconnectVoteValid(MAX_SLOTS - 1, MAX_SLOTS - 1));
}

TEST(NetworkUtil, DisconnectVoteIsInvalidForASlotThatDoesNotExist)
{
	// The voted slot is a byte from the network.
	EXPECT_FALSE(IsDisconnectVoteValid(MAX_SLOTS, 0));
	EXPECT_FALSE(IsDisconnectVoteValid(255, 0));
	EXPECT_FALSE(IsDisconnectVoteValid(-1, 0));
	EXPECT_FALSE(IsDisconnectVoteValid(0, MAX_SLOTS));
	EXPECT_FALSE(IsDisconnectVoteValid(0, -1));
}

TEST(NetworkUtil, PlayerLeaveIsValidForTheSender)
{
	for (Int slot = 0; slot < MAX_SLOTS; ++slot)
	{
		EXPECT_TRUE(IsPlayerLeaveValid(slot, slot));
	}
}

TEST(NetworkUtil, PlayerLeaveIsInvalidForAnotherPlayer)
{
	EXPECT_FALSE(IsPlayerLeaveValid(1, 2));
	EXPECT_FALSE(IsPlayerLeaveValid(0, MAX_SLOTS - 1));
}

TEST(NetworkUtil, PlayerLeaveIsInvalidForASlotThatDoesNotExist)
{
	// The leaving player is a byte from the network.
	EXPECT_FALSE(IsPlayerLeaveValid(MAX_SLOTS, MAX_SLOTS));
	EXPECT_FALSE(IsPlayerLeaveValid(255, 255));
}

TEST(NetworkUtil, RunAheadKeepsTheRangeThatThePacketRouterSends)
{
	EXPECT_EQ(ClampRunAhead(MIN_RUNAHEAD), MIN_RUNAHEAD);
	EXPECT_EQ(ClampRunAhead(RetailMinRunAhead), RetailMinRunAhead);
	EXPECT_EQ(ClampRunAhead(MAX_FRAMES_AHEAD / 2), MAX_FRAMES_AHEAD / 2);
}

TEST(NetworkUtil, RunAheadIsClampedToTheRangeThatThePacketRouterSends)
{
	// The run ahead is an unsigned short from the network.
	EXPECT_EQ(ClampRunAhead(0), MIN_RUNAHEAD);
	EXPECT_EQ(ClampRunAhead(MIN_RUNAHEAD - 1), MIN_RUNAHEAD);
	EXPECT_EQ(ClampRunAhead(MAX_FRAMES_AHEAD / 2 + 1), MAX_FRAMES_AHEAD / 2);
	EXPECT_EQ(ClampRunAhead(0xFFFF), MAX_FRAMES_AHEAD / 2);
}

TEST(NetworkUtil, RunAheadFrameRateKeepsTheRangeThatThePacketRouterSends)
{
	EXPECT_EQ(ClampRunAheadFrameRate(MIN_LOGIC_FRAMES), MIN_LOGIC_FRAMES);
	EXPECT_EQ(ClampRunAheadFrameRate(LogicFrameRate), LogicFrameRate);

	// The frame rate is a byte from the network and has no upper limit, because the frames per second limit is game data.
	EXPECT_EQ(ClampRunAheadFrameRate(255), 255);
}

TEST(NetworkUtil, RunAheadFrameRateIsRaisedToTheMinimumThatThePacketRouterSends)
{
	// The frame rate is a divisor.
	EXPECT_EQ(ClampRunAheadFrameRate(0), MIN_LOGIC_FRAMES);
	EXPECT_EQ(ClampRunAheadFrameRate(MIN_LOGIC_FRAMES - 1), MIN_LOGIC_FRAMES);
}
