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
#include <string>

#include "Common/GlobalData.h"
#include "Common/KindOf.h"
#include "Common/NameKeyGenerator.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Scripts.h"

#include "../Common/DataChunkTestFixture.h"

// The scripts of a map come from the map file. The parser checks the conditions and actions against the
// templates of the script engine.
class ScriptsTest : public DataChunkTestFixture
{
protected:
	virtual void SetUp() override
	{
		DataChunkTestFixture::SetUp();
		TheWritableGlobalData = new GlobalData;
		TheNameKeyGenerator = new NameKeyGenerator;
		TheNameKeyGenerator->init();
		TheScriptEngine = new ScriptEngine;
		TheScriptEngine->init();
	}

	virtual void TearDown() override
	{
		delete TheScriptEngine;
		TheScriptEngine = nullptr;
		delete TheNameKeyGenerator;
		TheNameKeyGenerator = nullptr;
		delete TheWritableGlobalData;
		TheWritableGlobalData = nullptr;
		DataChunkTestFixture::TearDown();
	}

	// Returns a parameter that is not a position.
	static std::string parameter(Parameter::ParameterType type, Int value, const std::string& string = std::string())
	{
		std::string out;
		append<Int>(out, type);
		append<Int>(out, value);
		append<Real>(out, 0.0f);
		appendAsciiString(out, string);
		return out;
	}

	// Returns a condition or an action of a version without a name key.
	static std::string item(Int type, Int numParms, const std::string& parameters)
	{
		std::string out;
		append<Int>(out, type);
		append<Int>(out, numParms);
		return out + parameters;
	}

	static std::string flagParameters()
	{
		return parameter(Parameter::FLAG, 0, "Flag") + parameter(Parameter::BOOLEAN, 1);
	}

	// Parses a condition of version 3, which is a version of both games.
	void parseCondition(const std::string& data, OrCondition *orCondition)
	{
		open(tableOfContents({"Condition"}) + chunk(1, data, 3));
		DataChunkInput file(&m_stream);
		file.registerParser("Condition", AsciiString::TheEmptyString, Condition::ParseConditionDataChunk);
		EXPECT_TRUE(file.parse(orCondition));
	}

	// Parses an action of version 1, which is a version of both games.
	void parseAction(const std::string& data, Script *script)
	{
		open(tableOfContents({"ScriptAction"}) + chunk(1, data, 1));
		DataChunkInput file(&m_stream);
		file.registerParser("ScriptAction", AsciiString::TheEmptyString, ScriptAction::ParseActionDataChunk);
		EXPECT_TRUE(file.parse(script));
	}
};

TEST_F(ScriptsTest, ReadsConditionParameters)
{
	OrCondition *orCondition = newInstance(OrCondition);
	parseCondition(item(Condition::FLAG, 2, flagParameters()), orCondition);

	Condition *condition = orCondition->getFirstAndCondition();
	ASSERT_NE(condition, nullptr);
	EXPECT_EQ(condition->getConditionType(), Condition::FLAG);
	ASSERT_EQ(condition->getNumParameters(), 2);
	EXPECT_EQ(condition->getParameter(0)->getString(), "Flag");
	EXPECT_EQ(condition->getParameter(1)->getInt(), 1);
	deleteInstance(orCondition);
}

TEST_F(ScriptsTest, ReadsActionParameters)
{
	Script *script = newInstance(Script);
	parseAction(item(ScriptAction::SET_FLAG, 2, flagParameters()), script);

	ScriptAction *action = script->getAction();
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->getActionType(), ScriptAction::SET_FLAG);
	ASSERT_EQ(action->getNumParameters(), 2);
	EXPECT_EQ(action->getParameter(0)->getString(), "Flag");
	deleteInstance(script);
}

TEST_F(ScriptsTest, ThrowsOnConditionWithMoreParametersThanItHolds)
{
	std::string parameters;
	for (Int i = 0; i <= MAX_PARMS; ++i)
	{
		parameters += parameter(Parameter::INT, i);
	}
	OrCondition *orCondition = newInstance(OrCondition);
	expectCorruptFileFormat([&] { parseCondition(item(Condition::FLAG, MAX_PARMS + 1, parameters), orCondition); });
	deleteInstance(orCondition);
}

TEST_F(ScriptsTest, ThrowsOnConditionWithNegativeParameterCount)
{
	OrCondition *orCondition = newInstance(OrCondition);
	expectCorruptFileFormat([&] { parseCondition(item(Condition::FLAG, -1, flagParameters()), orCondition); });
	deleteInstance(orCondition);
}

TEST_F(ScriptsTest, ReadsKindOfParameterByIndex)
{
	OrCondition *orCondition = newInstance(OrCondition);
	const std::string parameters = parameter(Parameter::FLAG, 0, "Flag") + parameter(Parameter::KIND_OF_PARAM, KINDOF_COUNT - 1);
	parseCondition(item(Condition::FLAG, 2, parameters), orCondition);

	Condition *condition = orCondition->getFirstAndCondition();
	ASSERT_NE(condition, nullptr);
	ASSERT_EQ(condition->getNumParameters(), 2);
	EXPECT_EQ(condition->getParameter(1)->getString(), KindOfMaskType::getBitNames()[KINDOF_COUNT - 1]);
	deleteInstance(orCondition);
}

TEST_F(ScriptsTest, ThrowsOnKindOfParameterOutsideOfKindOfNames)
{
	for (const Int kindOf : {-1, static_cast<Int>(KINDOF_COUNT), 0x10000000})
	{
		SCOPED_TRACE(kindOf);
		OrCondition *orCondition = newInstance(OrCondition);
		const std::string parameters = parameter(Parameter::FLAG, 0, "Flag") + parameter(Parameter::KIND_OF_PARAM, kindOf);
		expectCorruptFileFormat([&] { parseCondition(item(Condition::FLAG, 2, parameters), orCondition); });
		deleteInstance(orCondition);
	}
}

TEST_F(ScriptsTest, ThrowsOnActionWithMoreParametersThanItHolds)
{
	std::string parameters;
	for (Int i = 0; i <= MAX_PARMS; ++i)
	{
		parameters += parameter(Parameter::INT, i);
	}
	Script *script = newInstance(Script);
	expectCorruptFileFormat([&] { parseAction(item(ScriptAction::SET_FLAG, MAX_PARMS + 1, parameters), script); });
	deleteInstance(script);
}
