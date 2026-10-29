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
#include <climits>
#include <string>

#include "Common/DataChunk.h"
#include "Common/MapReaderWriterInfo.h"
#include "GameLogic/PolygonTrigger.h"

#include "../Common/DataChunkTestFixture.h"

// The polygon triggers of a map come from the map file.
class PolygonTriggerTest : public DataChunkTestFixture
{
protected:
	virtual void TearDown() override
	{
		PolygonTrigger::deleteTriggers();
		DataChunkTestFixture::TearDown();
	}

	// Returns a trigger of version 3 that states the given number of points, followed by the points.
	static std::string trigger(Int numPoints, Int pointsInData)
	{
		std::string out;
		appendAsciiString(out, "Trigger");
		append<Int>(out, 7); // trigger id
		append<Byte>(out, 0); // is water
		append<Byte>(out, 0); // is river
		append<Int>(out, 0); // river start
		append<Int>(out, numPoints);
		for (Int i = 0; i < pointsInData; ++i)
		{
			append<Int>(out, 10 * i);
			append<Int>(out, 20 * i);
			append<Int>(out, 30 * i);
		}
		return out;
	}

	void parse(const std::string& triggers)
	{
		std::string data;
		append<Int>(data, 1); // trigger count
		data += triggers;
		open(tableOfContents({"PolygonTriggers"}) + chunk(1, data, K_TRIGGERS_VERSION_3));
		DataChunkInput file(&m_stream);
		file.registerParser("PolygonTriggers", AsciiString::TheEmptyString, PolygonTrigger::ParsePolygonTriggersDataChunk);
		EXPECT_TRUE(file.parse());
	}
};

TEST_F(PolygonTriggerTest, ReadsPoints)
{
	parse(trigger(3, 3));

	PolygonTrigger *trigger = PolygonTrigger::getFirstPolygonTrigger();
	ASSERT_NE(trigger, nullptr);
	EXPECT_EQ(trigger->getID(), 7);
	ASSERT_EQ(trigger->getNumPoints(), 3);
	EXPECT_EQ(trigger->getPoint(2)->x, 20);
	EXPECT_EQ(trigger->getPoint(2)->y, 40);
	EXPECT_EQ(trigger->getPoint(2)->z, 60);
}

TEST_F(PolygonTriggerTest, ThrowsOnNegativePointCount)
{
	expectCorruptFileFormat([&] { parse(trigger(-1, 3)); });
}

TEST_F(PolygonTriggerTest, ThrowsOnMorePointsThanChunkHasBeforeAllocatingThem)
{
	// Allocating the points of this count fails in a 32 bit process.
	expectCorruptFileFormat([&] { parse(trigger(INT_MAX / 12, 3)); });
}
