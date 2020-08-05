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

#ifndef THUNDEREGG_SCHUR_SCHURWRAPOP_H
#define THUNDEREGG_SCHUR_SCHURWRAPOP_H

#include <ThunderEgg/Operator.h>
#include <ThunderEgg/Schur/IfaceInterp.h>
#include <ThunderEgg/Schur/PatchSolver.h>
#include <ThunderEgg/Schur/SchurHelper.h>

namespace ThunderEgg
{
namespace Schur
{
/**
 * @brief Base class for operators
 */
template <size_t D> class SchurWrapOp : public Operator<D - 1>
{
	private:
	std::shared_ptr<Domain<D>>      domain;
	std::shared_ptr<SchurHelper<D>> sh;
	std::shared_ptr<PatchSolver<D>> solver;
	std::shared_ptr<IfaceInterp<D>> interp;

	public:
	SchurWrapOp(std::shared_ptr<Domain<D>> domain, std::shared_ptr<SchurHelper<D>> sh,
	            std::shared_ptr<PatchSolver<D>> solver, std::shared_ptr<IfaceInterp<D>> interp)
	{
		this->domain = domain;
		this->sh     = sh;
		this->solver = solver;
		this->interp = interp;
	}
	/**
	 * @brief Apply Schur matrix
	 *
	 * @param x the input vector.
	 * @param b the output vector.
	 */
	void apply(std::shared_ptr<const Vector<D - 1>> x, std::shared_ptr<Vector<D - 1>> b) const
	{
		auto f = PetscVector<D>::GetNewVector(domain);
		auto u = PetscVector<D>::GetNewVector(domain);
		solver->solve(f, u, x);
		interp->interpolateToInterface(u, b);
		b->addScaled(-1, x);
	}
};
} // namespace Schur
} // namespace ThunderEgg
#endif