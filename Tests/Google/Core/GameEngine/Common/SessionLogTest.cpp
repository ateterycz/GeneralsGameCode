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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <windows.h>

#include "Lib/BaseType.h"
#include "Common/Diagnostic/SessionLog.h"

namespace
{

int *volatile InvalidAddress = nullptr;

// Logs the exception of a write to an invalid address, like the unhandled exception filter of the game does.
void logAccessViolation()
{
	__try
	{
		*InvalidAddress = 1;
	}
	__except (SessionLog::logException(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

} // namespace

class SessionLogTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		m_directory = std::filesystem::temp_directory_path() / ("SessionLogTest_" + std::to_string(std::random_device()()));
		std::filesystem::remove_all(m_directory);
		std::filesystem::create_directories(m_directory);
		m_userDataPath = m_directory.string() + "\\";
	}

	virtual void TearDown() override
	{
		SessionLog::close(0);
		std::error_code error;
		std::filesystem::remove_all(m_directory, error);
	}

	std::filesystem::path getLogFolder() const
	{
		return m_directory / "Logs";
	}

	std::vector<std::filesystem::path> getSessionLogs() const
	{
		std::vector<std::filesystem::path> logs;
		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(getLogFolder()))
		{
			if (entry.path().filename().string().rfind("Session_", 0) == 0)
			{
				logs.push_back(entry.path());
			}
		}
		std::sort(logs.begin(), logs.end());
		return logs;
	}

	// Returns the content of the only session log.
	std::string readSessionLog() const
	{
		const std::vector<std::filesystem::path> logs = getSessionLogs();
		EXPECT_EQ(logs.size(), 1u);
		if (logs.size() != 1)
		{
			return std::string();
		}
		std::ifstream file(logs[0]);
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	std::filesystem::path m_directory;
	std::string m_userDataPath;
};

TEST_F(SessionLogTest, WritesSessionStartLinesAndEnd)
{
	SessionLog::open(m_userDataPath.c_str());
	SessionLog::log("Game start on map %s", "Test Map");
	SessionLog::logLine("  A line as it is");
	SessionLog::close(3);

	const std::string content = readSessionLog();
	EXPECT_NE(content.find("] Session start on "), std::string::npos);
	EXPECT_NE(content.find("]   Executable "), std::string::npos);
	EXPECT_NE(content.find("]   Simulation math CRC "), std::string::npos);
	EXPECT_NE(content.find("] Game start on map Test Map\n"), std::string::npos);
	EXPECT_NE(content.find("\n  A line as it is\n"), std::string::npos);
	EXPECT_NE(content.find("] Session end with exit code 3\n"), std::string::npos);
}

TEST_F(SessionLogTest, WritesNothingAfterClose)
{
	SessionLog::open(m_userDataPath.c_str());
	SessionLog::close(0);
	SessionLog::log("Too late");

	EXPECT_EQ(readSessionLog().find("Too late"), std::string::npos);
}

TEST_F(SessionLogTest, StartsANewLogForEachSession)
{
	// The sessions can start in the same second.
	SessionLog::open(m_userDataPath.c_str());
	SessionLog::close(0);
	SessionLog::open(m_userDataPath.c_str());
	SessionLog::close(0);

	EXPECT_EQ(getSessionLogs().size(), 2u);
}

TEST_F(SessionLogTest, KeepsOnlyTheNewestLogs)
{
	std::filesystem::create_directories(getLogFolder());
	for (int i = 0; i < 12; ++i)
	{
		std::ofstream(getLogFolder() / ("Session_20000101_0000" + std::to_string(10 + i) + ".txt")) << "old";
	}
	std::ofstream(getLogFolder() / "Notes.txt") << "not a session log";

	SessionLog::open(m_userDataPath.c_str());
	SessionLog::close(0);

	EXPECT_EQ(getSessionLogs().size(), static_cast<size_t>(SessionLog::MaxFileCount));
	EXPECT_FALSE(std::filesystem::exists(getLogFolder() / "Session_20000101_000012.txt"));
	EXPECT_TRUE(std::filesystem::exists(getLogFolder() / "Session_20000101_000013.txt"));
	EXPECT_TRUE(std::filesystem::exists(getLogFolder() / "Notes.txt"));
}

TEST_F(SessionLogTest, LogsFatalErrorWithStack)
{
	SessionLog::open(m_userDataPath.c_str());
	SessionLog::logFatalError("Test reason");
	SessionLog::close(1);

	const std::string content = readSessionLog();
	EXPECT_NE(content.find("] FATAL ERROR: Test reason\nCall Stack\n"), std::string::npos) << content;
	// The stack begins with the caller of SessionLog::logFatalError.
	EXPECT_NE(content.find("SessionLogTest_LogsFatalErrorWithStack_Test::TestBody"), std::string::npos) << content;
#ifdef RTS_ENABLE_CRASHDUMP
	EXPECT_NE(content.find("]   Crash dumps are in " + m_userDataPath + "CrashDumps\\\n"), std::string::npos);
#endif
}

TEST_F(SessionLogTest, LogsExceptionWithModuleOffsetAndStack)
{
	SessionLog::open(m_userDataPath.c_str());
	logAccessViolation();
	SessionLog::close(1);

	const std::string content = readSessionLog();
	EXPECT_NE(content.find("] CRASH: exception C0000005 at "), std::string::npos);
	EXPECT_NE(content.find("googletest.exe+0x"), std::string::npos);
	EXPECT_NE(content.find("]   Write of address 00000000\nCall Stack\n"), std::string::npos) << content;
	// The stack begins with the function that crashed.
	EXPECT_NE(content.find("logAccessViolation"), std::string::npos) << content;
}
