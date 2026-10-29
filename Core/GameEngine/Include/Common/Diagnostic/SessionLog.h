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

#pragma once

struct _EXCEPTION_POINTERS;

// TheSuperHackers @feature ateterycz 29/10/2026 Writes a short log of each game session in all builds, so that
// game starts, mismatches, disconnects and crashes can be looked at after the session. The logs are written to
// the Logs folder of the user data path, and only the newest MaxFileCount logs are kept.
class SessionLog
{
public:
	enum { MaxFileCount = 10 };

	static void open(const char *userDataPath);
	static void close(Int exitCode);

	// Writes a line that begins with the time of day.
	static void log(const char *format, ...);
	// Writes the line as it is. It can be passed to the stack dump functions.
	static void logLine(const char *line);

	static void logGameStart(Bool loadingSaveGame);
	static void logFatalError(const char *reason);
	static void logException(struct _EXCEPTION_POINTERS *exceptionPointers);
};
