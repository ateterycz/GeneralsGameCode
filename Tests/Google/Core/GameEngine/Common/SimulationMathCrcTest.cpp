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

#include <float.h>
#include <gtest/gtest.h>

#include "Lib/BaseType.h"
#include "Common/Diagnostic/SimulationMathCrc.h"
#include "GameLogic/FPUControl.h"

TEST(SimulationMathCrc, IsStableForAnyFloatingPointMode)
{
	_fpreset();
	const UnsignedInt first = SimulationMathCrc::calculate();

	setFPMode();
	const UnsignedInt second = SimulationMathCrc::calculate();
	_fpreset();

	EXPECT_EQ(first, second);
}

TEST(SimulationMathCrc, KeepsFloatingPointControlWord)
{
	// The game logic floating point mode differs from the default mode that the calculation ends with.
	setFPMode();
	const unsigned int controlWord = _controlfp(0, 0);

	SimulationMathCrc::calculate();

	EXPECT_EQ(_controlfp(0, 0), controlWord);
	_fpreset();
}
