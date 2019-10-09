/***************************************************************************
 *  Thunderegg, a library for solving Poisson's equation on adaptively
 *  refined block-structured Cartesian grids
 *
 *  Copyright (C) 2019  Thunderegg Developers. See AUTHORS.md file at the
 *  top-level directory.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 ***************************************************************************/

#ifndef THUNDEREGG_GMG_CYCLEFACTORY3D_H
#define THUNDEREGG_GMG_CYCLEFACTORY3D_H
#include <Thunderegg/DomainGenerator.h>
#include <Thunderegg/GMG/Cycle.h>
#include <Thunderegg/GMG/CycleOpts.h>
#include <Thunderegg/Schur/IfaceInterp.h>
#include <Thunderegg/Schur/PatchOperator.h>
#include <Thunderegg/Schur/PatchSolver.h>
namespace Thunderegg
{
namespace GMG
{
class CycleFactory3d
{
	public:
	static std::shared_ptr<Cycle<3>> getCycle(const CycleOpts &                        opts,
	                                          std::shared_ptr<DomainGenerator<3>>      dcg,
	                                          std::shared_ptr<Schur::PatchSolver<3>>   solver,
	                                          std::shared_ptr<Schur::PatchOperator<3>> op,
	                                          std::shared_ptr<Schur::IfaceInterp<3>> interpolator);
};
} // namespace GMG
} // namespace Thunderegg
#endif