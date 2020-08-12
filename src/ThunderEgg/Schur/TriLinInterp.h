/***************************************************************************
 *  ThunderEgg, a library for solving Poisson's equation on adaptively
 *  refined block-structured Cartesian grids
 *
 *  Copyright (C) 2019  ThunderEgg Developers. See AUTHORS.md file at the
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

#ifndef THUNDEREGG_SCHUR_TRILININTERP_H
#define THUNDEREGG_SCHUR_TRILININTERP_H
#include <ThunderEgg/Schur/IfaceInterp.h>
#include <ThunderEgg/Schur/SchurHelper.h>
namespace ThunderEgg
{
namespace Schur
{
class TriLinInterp : public IfaceInterp<3>
{
	private:
	std::shared_ptr<SchurHelper<3>> sh;

	public:
	TriLinInterp(std::shared_ptr<SchurHelper<3>> sh)
	{
		this->sh = sh;
	}
	void interpolateToInterface(std::shared_ptr<const Vector<3>> u,
	                            std::shared_ptr<Vector<2>>       interp) override;
	void interpolate(SchurInfo<3> &d, std::shared_ptr<const Vector<3>> u,
	                 std::shared_ptr<Vector<2>> interp);
	void interpolate(SchurInfo<3> &d, Side<3> s, int local_index, IfaceType<3> itype,
	                 std::shared_ptr<const Vector<3>> u, std::shared_ptr<Vector<2>> interp);
};
} // namespace Schur
} // namespace ThunderEgg
#endif
