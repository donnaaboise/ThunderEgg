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

#include "MatrixHelper.h"
#include "StencilHelper.h"
#include <iostream>
using namespace std;
MatrixHelper::MatrixHelper(DomainCollection<3> dc)
{
	this->dc = dc;
}
PW_explicit<Mat> MatrixHelper::formCRSMatrix(double lambda)
{
	PW<Mat> A;
	MatCreate(MPI_COMM_WORLD, &A);
	int n           = dc.getN();
	int local_size  = dc.domains.size() * n * n * n;
	int global_size = dc.num_global_domains * n * n * n;
	MatSetSizes(A, local_size, local_size, global_size, global_size);
	MatSetType(A, MATMPIAIJ);
	MatMPIAIJSetPreallocation(A, 19, nullptr, 19, nullptr);

	for (auto &p : dc.domains) {
		Domain<3> &d     = *p.second;
		int        n     = d.n;
		double     h_x   = d.lengths[0] / n;
		double     h_y   = d.lengths[1] / n;
		double     h_z   = d.lengths[2] / n;
		int        start = n * n * n * d.id_global;

		// center coeffs
		double coeff = -2.0 / (h_x * h_x) - 2.0 / (h_y * h_y) - 2.0 / (h_z * h_z);
		for (int z_i = 0; z_i < n; z_i++) {
			for (int y_i = 0; y_i < n; y_i++) {
				for (int x_i = 0; x_i < n; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					MatSetValues(A, 1, &row, 1, &row, &coeff, ADD_VALUES);
				}
			}
		}
		// west coeffs
		coeff = 1.0 / (h_x * h_x);
		for (int z_i = 0; z_i < n; z_i++) {
			for (int y_i = 0; y_i < n; y_i++) {
				for (int x_i = 1; x_i < n; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					int col = start + x_i - 1 + n * y_i + n * n * z_i;
					MatSetValues(A, 1, &row, 1, &col, &coeff, ADD_VALUES);
				}
			}
		}
		// east coeffs
		for (int z_i = 0; z_i < n; z_i++) {
			for (int y_i = 0; y_i < n; y_i++) {
				for (int x_i = 0; x_i < n - 1; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					int col = start + x_i + 1 + n * y_i + n * n * z_i;
					MatSetValues(A, 1, &row, 1, &col, &coeff, ADD_VALUES);
				}
			}
		}
		// north coeffs
		coeff = 1.0 / (h_y * h_y);
		for (int z_i = 0; z_i < n; z_i++) {
			for (int y_i = 0; y_i < n - 1; y_i++) {
				for (int x_i = 0; x_i < n; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					int col = start + x_i + n * (y_i + 1) + n * n * z_i;
					MatSetValues(A, 1, &row, 1, &col, &coeff, ADD_VALUES);
				}
			}
		}
		// south coeffs
		for (int z_i = 0; z_i < n; z_i++) {
			for (int y_i = 1; y_i < n; y_i++) {
				for (int x_i = 0; x_i < n; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					int col = start + x_i + n * (y_i - 1) + n * n * z_i;
					MatSetValues(A, 1, &row, 1, &col, &coeff, ADD_VALUES);
				}
			}
		}
		// top coeffs
		coeff = 1.0 / (h_z * h_z);
		for (int z_i = 0; z_i < n - 1; z_i++) {
			for (int y_i = 0; y_i < n; y_i++) {
				for (int x_i = 0; x_i < n; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					int col = start + x_i + n * y_i + n * n * (z_i + 1);
					MatSetValues(A, 1, &row, 1, &col, &coeff, ADD_VALUES);
				}
			}
		}
		// bottom coeffs
		for (int z_i = 1; z_i < n; z_i++) {
			for (int y_i = 0; y_i < n; y_i++) {
				for (int x_i = 0; x_i < n; x_i++) {
					int row = start + x_i + n * y_i + n * n * z_i;
					int col = start + x_i + n * y_i + n * n * (z_i - 1);
					MatSetValues(A, 1, &row, 1, &col, &coeff, ADD_VALUES);
				}
			}
		}

		// boundaries
		for (Side<3> s : Side<3>::getValues()) {
			StencilHelper *sh = getStencilHelper(d, s);
			for (int yi = 0; yi < n; yi++) {
				for (int xi = 0; xi < n; xi++) {
					int     row    = sh->row(xi, yi);
					int     size   = sh->size(xi, yi);
					double *coeffs = sh->coeffs(xi, yi);
					int *   cols   = sh->cols(xi, yi);
					MatSetValues(A, 1, &row, size, cols, coeffs, ADD_VALUES);
				}
			}
			delete sh;
		}
	}
	MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY);
	MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY);
	return A;
}
