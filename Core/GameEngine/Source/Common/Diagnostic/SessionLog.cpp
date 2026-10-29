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

#include "PreRTS.h"

#include "Common/Diagnostic/SessionLog.h"
#include "Common/Diagnostic/SimulationMathCrc.h"
#include "Common/GlobalData.h"
#include "Common/RandomValue.h"
#include "Common/Recorder.h"
#include "Common/StackDump.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameInfo.h"
#include "gitinfo.h"

#include <algorithm>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <vector>

static const char *const SessionLogFolderName = "Logs\\";
static const char *const SessionLogFilePrefix = "Session_";

static FILE *theSessionLogFile = nullptr;
static std::string theUserDataPath;

// Deletes the oldest session logs in the folder, so that keepCount of them remain.
static void deleteOldestSessionLogs(const std::string &folder, size_t keepCount)
{
	std::vector<std::string> fileNames;
	WIN32_FIND_DATA findData;
	HANDLE findHandle = ::FindFirstFile((folder + SessionLogFilePrefix + "*.txt").c_str(), &findData);
	if (findHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				fileNames.push_back(findData.cFileName);
		}
		while (::FindNextFile(findHandle, &findData));
		::FindClose(findHandle);
	}

	// The file names continue with the date and time of the session, so they sort from oldest to newest.
	std::sort(fileNames.begin(), fileNames.end());
	for (size_t i = 0; i + keepCount < fileNames.size(); ++i)
	{
		::DeleteFile((folder + fileNames[i]).c_str());
	}
}

static FILE *createSessionLogFile(const std::string &folder, const char *timeText)
{
	// A second session can start in the same second, for example as another client instance.
	for (Int i = 1; i < 100; ++i)
	{
		char path[_MAX_PATH];
		if (i == 1)
			snprintf(path, ARRAY_SIZE(path), "%s%s%s.txt", folder.c_str(), SessionLogFilePrefix, timeText);
		else
			snprintf(path, ARRAY_SIZE(path), "%s%s%s_%d.txt", folder.c_str(), SessionLogFilePrefix, timeText, i);

		if (::_access(path, 0) != 0)
			return fopen(path, "w");
	}
	return nullptr;
}

static const char *getSlotTypeName(const GameSlot *slot)
{
	if (slot->getPlayerTemplate() == PLAYERTEMPLATE_OBSERVER)
		return "Observer";

	switch (slot->getState())
	{
		case SLOT_EASY_AI: return "Easy AI";
		case SLOT_MED_AI: return "Medium AI";
		case SLOT_BRUTAL_AI: return "Brutal AI";
		default: return "Human";
	}
}

static void logCrashDumpFolder()
{
#ifdef RTS_ENABLE_CRASHDUMP
	SessionLog::log("  Crash dumps are in %sCrashDumps\\", theUserDataPath.c_str());
#endif
}

void SessionLog::open(const char *userDataPath)
{
	if (theSessionLogFile != nullptr)
		return;

	theUserDataPath = userDataPath;
	const std::string folder = theUserDataPath + SessionLogFolderName;
	::CreateDirectory(folder.c_str(), nullptr);
	deleteOldestSessionLogs(folder, MaxFileCount - 1);

	const time_t now = time(nullptr);
	char timeText[32];
	strftime(timeText, ARRAY_SIZE(timeText), "%Y%m%d_%H%M%S", localtime(&now));
	theSessionLogFile = createSessionLogFile(folder, timeText);
	if (theSessionLogFile == nullptr)
		return;

	char exePath[_MAX_PATH];
	if (::GetModuleFileName(nullptr, exePath, ARRAY_SIZE(exePath)) == 0)
		exePath[0] = '\0';

	strftime(timeText, ARRAY_SIZE(timeText), "%Y-%m-%d %H:%M:%S", localtime(&now));
	log("Session start on %s", timeText);
	log("  Executable %s, version %s%s", exePath, GitUncommittedChanges ? "~" : "", GitTag[0] ? GitTag : GitShortSHA1);
	log("  Command line %s", ::GetCommandLine());
	log("  Simulation math CRC %8.8X", SimulationMathCrc::calculate());
}

void SessionLog::close(Int exitCode)
{
	if (theSessionLogFile == nullptr)
		return;

	log("Session end with exit code %d", exitCode);
	fclose(theSessionLogFile);
	theSessionLogFile = nullptr;
}

void SessionLog::log(const char *format, ...)
{
	if (theSessionLogFile == nullptr)
		return;

	const time_t now = time(nullptr);
	char timeText[16];
	strftime(timeText, ARRAY_SIZE(timeText), "%H:%M:%S", localtime(&now));
	fprintf(theSessionLogFile, "[%s] ", timeText);

	va_list args;
	va_start(args, format);
	vfprintf(theSessionLogFile, format, args);
	va_end(args);

	fputc('\n', theSessionLogFile);
	fflush(theSessionLogFile);
}

void SessionLog::logLine(const char *line)
{
	if (theSessionLogFile == nullptr)
		return;

	fprintf(theSessionLogFile, "%s\n", line);
	fflush(theSessionLogFile);
}

void SessionLog::logGameStart(Bool loadingSaveGame)
{
	if (theSessionLogFile == nullptr || TheGameLogic->isInShellGame())
		return;

	log("Game start: %s%s on map %s", toString(TheGameLogic->getGameMode()), loadingSaveGame ? " from a save game" : "",
		TheGlobalData->m_mapName.str());
	log("  Seed %u, EXE CRC %8.8X", GetGameLogicRandomSeed(), TheGlobalData->m_exeCRC);

	if (TheRecorder != nullptr && TheRecorder->isPlaybackMode())
	{
		log("  Replay %s", TheRecorder->getCurrentReplayFilename().str());
	}

	if (TheGameInfo != nullptr)
	{
		log("  CRC interval %d frames", TheGameInfo->getCRCInterval());
		for (Int i = 0; i < MAX_SLOTS; ++i)
		{
			const GameSlot *slot = TheGameInfo->getConstSlot(i);
			if (slot == nullptr || !slot->isOccupied())
				continue;

			AsciiString name;
			name.translate(slot->getName());
			log("  Slot %d: %s %s%s", i, getSlotTypeName(slot), name.str(), (i == TheGameInfo->getLocalSlotNum()) ? " (local player)" : "");
		}
	}
}

// The frame pointer of this function lets the stack walk begin at this function instead of at an older frame.
#pragma optimize("y", off)
void SessionLog::logFatalError(const char *reason)
{
	if (theSessionLogFile == nullptr)
		return;

	log("FATAL ERROR: %s", reason);

	// Walks the stack from the captured registers, like for an exception, because FillStackAddresses only finds its own frame.
	// RtlCaptureContext is looked up, because older Windows SDKs do not declare it.
	typedef VOID (WINAPI *RtlCaptureContextFunc)(PCONTEXT);
	const RtlCaptureContextFunc captureContext = (RtlCaptureContextFunc)::GetProcAddress(::GetModuleHandle("kernel32.dll"), "RtlCaptureContext");
	if (captureContext != nullptr)
	{
		CONTEXT context;
		memset(&context, 0, sizeof(context));
		context.ContextFlags = CONTEXT_FULL;
		captureContext(&context);
		StackDumpFromContext(context.Eip, context.Esp, context.Ebp, logLine);
	}

	logCrashDumpFolder();
}
#pragma optimize("", on)

void SessionLog::logException(struct _EXCEPTION_POINTERS *exceptionPointers)
{
	if (theSessionLogFile == nullptr)
		return;

	const EXCEPTION_RECORD *record = exceptionPointers->ExceptionRecord;

	// The crash address as an offset into its module, which stays the same when the module loads at another address.
	char modulePath[_MAX_PATH] = "unknown module";
	UnsignedInt moduleOffset = (UnsignedInt)(uintptr_t)record->ExceptionAddress;
	MEMORY_BASIC_INFORMATION memoryInfo;
	if (::VirtualQuery(record->ExceptionAddress, &memoryInfo, sizeof(memoryInfo)) != 0 && memoryInfo.AllocationBase != nullptr
		&& ::GetModuleFileName((HMODULE)memoryInfo.AllocationBase, modulePath, ARRAY_SIZE(modulePath)) != 0)
	{
		moduleOffset = (UnsignedInt)((const char *)record->ExceptionAddress - (const char *)memoryInfo.AllocationBase);
	}
	const char *moduleName = strrchr(modulePath, '\\');
	moduleName = (moduleName != nullptr) ? moduleName + 1 : modulePath;

	log("CRASH: exception %8.8X at %s+0x%X, game frame %u", (UnsignedInt)record->ExceptionCode, moduleName, moduleOffset,
		(TheGameLogic != nullptr) ? TheGameLogic->getFrame() : 0);

	if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
	{
		const ULONG_PTR accessType = record->ExceptionInformation[0];
		log("  %s of address %8.8X", (accessType == 0) ? "Read" : (accessType == 1) ? "Write" : "Execution",
			(UnsignedInt)record->ExceptionInformation[1]);
	}

	const CONTEXT *context = exceptionPointers->ContextRecord;
	StackDumpFromContext(context->Eip, context->Esp, context->Ebp, logLine);

	logCrashDumpFolder();
}
