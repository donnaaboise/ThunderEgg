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

#include "ClawWriter.h"
#include <fstream>
using namespace std;
ClawWriter::ClawWriter(DomainCollection<2> &dc)
{
	this->dc = dc;
}
void ClawWriter::write(Vec u, Vec resid)
{
	ofstream     t_file("fort.t0000");
	const string tab = "\t";
	t_file << 0.0 << tab << "time" << endl;
	t_file << 2 << tab << "meqn" << endl;
	t_file << dc.domains.size() << tab << "ngrids" << endl;
	t_file << 2 << tab << "num_aux" << endl;
	t_file << 2 << tab << "num_dim" << endl;
	t_file.close();
	ofstream q_file("fort.q0000");

	double *u_view, *resid_view;
	VecGetArray(u, &u_view);
	VecGetArray(resid, &resid_view);
	q_file.precision(10);
	q_file << scientific;
	for (auto &p : dc.domains) {
		Domain<2> &d = *p.second;
		writePatch(d, q_file, u_view, resid_view);
	}
	VecRestoreArray(u, &u_view);
	VecRestoreArray(resid, &resid_view);
	q_file.close();
}
void ClawWriter::writePatch(Domain<2> &d, std::ostream &os, double *u_view, double *resid_view)
{
	const string tab = "\t";
	os << d.id << tab << "grid_number" << endl;
	os << d.refine_level << tab << "AMR_level" << endl;
	os << 0 << tab << "block_number" << endl;
	os << 0 << tab << "mpi_rank" << endl;
	os << d.n << tab << "mx" << endl;
	os << d.n << tab << "my" << endl;
	os << d.starts[0] << tab << "xlow" << endl;
	os << d.starts[1] << tab << "ylow" << endl;
	int    n   = d.n;
	double h_x = d.lengths[0] / n;
	double h_y = d.lengths[1] / n;
	os << h_x << tab << "dx" << endl;
	os << h_y << tab << "dy" << endl;
	os << endl;
	int start = d.id * n * n;
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			int loc = j + i * n;
			os << u_view[start + loc] << tab << resid_view[start + loc] * h_x * h_y << endl;
		}
		os << endl;
	}
}
