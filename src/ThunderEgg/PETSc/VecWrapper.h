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

#ifndef THUNDEREGG_PETSC_VECWRAPPER_H
#define THUNDEREGG_PETSC_VECWRAPPER_H
#include <ThunderEgg/PETSc/VecLocalDataManager.h>

namespace ThunderEgg
{
namespace PETSc
{
/**
 * @brief Wrap a PETSc Vec for use as a ThunderEgg Vector
 *
 * @tparam D the number of Cartesian dimensions on a patch.
 */
template <size_t D> class VecWrapper : public Vector<D>

{
	private:
	/**
	 * @brief The petsc vector object
	 */
	Vec vec;
	/**
	 * @brief The number of cells (including ghost) in each patch
	 */
	int patch_stride;
	/**
	 * @brief The number of ghost cell rows on each side of the patch
	 */
	int num_ghost_cells;
	/**
	 * @brief Whether or not to deallocate PETSc Vec on destruction
	 */
	bool own;
	/**
	 * @brief The striding for each patch
	 */
	std::array<int, D> strides;
	/**
	 * @brief The number of (non-ghost) cells in each direction of the patch
	 */
	std::array<int, D> lengths;
	/**
	 * @brief The offset to the first non-ghost cell value
	 */
	int first_offset;

	public:
	/**
	 * @brief Construct a new VecWrapper object
	 *
	 * @param vec_in the allocated PETSc vector
	 * @param lengths_in the number of (non-ghost) cells in each direction of the patch
	 * @param num_ghost_cells_in the number of ghost cells on each side of the patch
	 * @param own_in whether or not to deallocate the PETSc vector when this object is destroyed.
	 */
	VecWrapper(Vec vec_in, const std::array<int, D> &lengths_in, int num_ghost_cells_in,
	           bool own_in)
	: Vector<D>(MPI_COMM_WORLD), vec(vec_in), num_ghost_cells(num_ghost_cells_in), own(own_in),
	  lengths(lengths_in)
	{
		strides[0]   = 1;
		first_offset = num_ghost_cells;
		for (size_t i = 1; i < D; i++) {
			strides[i] = (this->lengths[i - 1] + 2 * num_ghost_cells) * strides[i - 1];
			first_offset += strides[i] * num_ghost_cells;
		}
		patch_stride = strides[D - 1] * (lengths[D - 1] + 2 * num_ghost_cells);
		VecGetLocalSize(vec, &this->num_local_patches);
		this->num_local_patches /= patch_stride;
		int num_cells_in_patch = 1;
		for (size_t i = 0; i < D; i++) {
			num_cells_in_patch *= this->lengths[i];
		}
		this->num_local_cells = this->num_local_patches * num_cells_in_patch;
	}

	/**
	 * @brief Get a new VecWrapper object for a given Domain
	 *
	 * @param domain the Domain
	 * @return std::shared_ptr<VecWrapper<D>> the resulting VecWrapper
	 */
	static std::shared_ptr<VecWrapper<D>> GetNewVector(std::shared_ptr<const Domain<D>> domain)
	{
		Vec u;
		VecCreateMPI(MPI_COMM_WORLD, domain->getNumLocalCellsWithGhost(), PETSC_DETERMINE, &u);
		return std::make_shared<VecWrapper<D>>(u, domain->getNs(), domain->getNumGhostCells(),
		                                       true);
	}
	/**
	 * @brief Get a new boundary condition vector for a given Domain
	 *
	 * @param domain the Domain
	 * @return std::shared_ptr<VecWrapper<D - 1>> the resulting vector
	 */
	static std::shared_ptr<VecWrapper<D - 1>> GetNewBCVector(std::shared_ptr<Domain<D>> domain)
	{
		Vec u;
		VecCreateMPI(MPI_COMM_WORLD, domain->getNumLocalBCCells(), PETSC_DETERMINE, &u);
		std::array<int, D - 1> ns;
		for (size_t i = 0; i < ns.size(); i++) {
			ns[i] = domain->getNs()[i];
		}
		return std::make_shared<VecWrapper<D - 1>>(u, ns, 0, true);
	}
	/*
	static std::shared_ptr<PetscVector<D>> GetNewSchurVector(std::shared_ptr<SchurHelper<D + 1>> sh)
	{
	    Vec u;
	    VecCreateMPI(MPI_COMM_WORLD, sh->getSchurVecLocalSize(), PETSC_DETERMINE, &u);
	    return std::shared_ptr<PetscVector<D>>(new PetscVector<D>(u, sh->getLengths()));
	}
	*/

	/**
	 * @brief Destroy the VecWrapper object
	 */
	~VecWrapper()
	{
		if (own) {
			VecDestroy(&vec);
		}
	}
	/**
	 * @brief Get the LocalData object for the given local patch index
	 *
	 * @param patch_local_index the local index of the patch
	 * @return LocalData<D> the local data object
	 */
	LocalData<D> getLocalData(int patch_local_index)
	{
		std::shared_ptr<VecLocalDataManager> ldm(new VecLocalDataManager(vec, false));
		double *data = ldm->getVecView() + patch_stride * patch_local_index + first_offset;
		return LocalData<D>(data, strides, lengths, num_ghost_cells, ldm);
	}

	/**
	 * @brief Get the LocalData object for the given local patch index
	 *
	 * @param patch_local_index the local index of the patch
	 * @return LocalData<D> the local data object
	 */
	const LocalData<D> getLocalData(int patch_local_index) const
	{
		std::shared_ptr<VecLocalDataManager> ldm(new VecLocalDataManager(vec, true));
		double *data = ldm->getVecView() + patch_stride * patch_local_index + first_offset;
		return LocalData<D>(data, strides, lengths, num_ghost_cells, std::move(ldm));
	}
	void setNumGhostPatches(int num_ghost_patches) {}
	int  getNumGhostCells() const
	{
		return num_ghost_cells;
	}
	/**
	 * @brief Get the underlying Vec object
	 *
	 * @return Vec the Vec object
	 */
	Vec getVec() const
	{
		return vec;
	}
};

} // namespace PETSc
} // namespace ThunderEgg
#endif