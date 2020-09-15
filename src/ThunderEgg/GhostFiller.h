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

#ifndef THUNDEREGG_GHOSTEFILLER_H
#define THUNDEREGG_GHOSTEFILLER_H
#include <ThunderEgg/Vector.h>
namespace ThunderEgg
{
/**
 * @brief Fills ghost cells on patches
 *
 * @tparam D the number of Cartesian dimensions in the patches.
 */
template <int D> class GhostFiller
{
	public:
	virtual ~GhostFiller() {}

	/**
	 * @brief Fill ghost cells on a vector
	 *
	 * @param u  the vector
	 */
	virtual void fillGhost(std::shared_ptr<const Vector<D>> u) const = 0;
};
} // namespace ThunderEgg
#endif
