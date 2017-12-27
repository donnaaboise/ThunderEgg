#include "FftwPatchSolver.h"
using namespace std;

FftwPatchSolver::FftwPatchSolver(DomainCollection &dc, double lambda)
{
	n            = dc.n;
	this->lambda = lambda;
	for (auto &p : dc.domains) {
		addDomain(p.second);
	}
}
void FftwPatchSolver::addDomain(Domain &d)
{
	if (!initialized) {
		initialized = true;
		f_copy.resize(n * n);
		tmp.resize(n * n);
		sol.resize(n * n);
	}

	if (!plan1.count(d)) {
		fftw_r2r_kind x_transform     = FFTW_RODFT10;
		fftw_r2r_kind x_transform_inv = FFTW_RODFT01;
		fftw_r2r_kind y_transform     = FFTW_RODFT10;
		fftw_r2r_kind y_transform_inv = FFTW_RODFT01;
		if (d.neumann.to_ulong()) {
			if (d.isNeumann(Side::east) && d.isNeumann(Side::west)) {
				x_transform     = FFTW_REDFT10;
				x_transform_inv = FFTW_REDFT01;
			} else if (d.isNeumann(Side::west)) {
				x_transform     = FFTW_REDFT11;
				x_transform_inv = FFTW_REDFT11;
			} else if (d.isNeumann(Side::east)) {
				x_transform     = FFTW_RODFT11;
				x_transform_inv = FFTW_RODFT11;
			}
			if (d.isNeumann(Side::north) && d.isNeumann(Side::south)) {
				y_transform     = FFTW_REDFT10;
				y_transform_inv = FFTW_REDFT01;
			} else if (d.isNeumann(Side::south)) {
				y_transform     = FFTW_REDFT11;
				y_transform_inv = FFTW_REDFT11;
			} else if (d.isNeumann(Side::north)) {
				y_transform     = FFTW_RODFT11;
				y_transform_inv = FFTW_RODFT11;
			}
		}

		plan1[d]
		= fftw_plan_r2r_2d(n, n, &f_copy[0], &tmp[0], y_transform, x_transform, FFTW_MEASURE);

		plan2[d]
		= fftw_plan_r2r_2d(n, n, &tmp[0], &sol[0], y_transform_inv, x_transform_inv, FFTW_MEASURE);
	}

	double h_x = d.x_length / n;
	double h_y = d.y_length / n;
	if (!denoms.count(d)) {
		valarray<double> &denom = denoms[d];
		denom.resize(n * n);
		// create denom vector
		if (d.neumann.none()) {
			for (int yi = 0; yi < n; yi++) {
				denom[slice(yi * n, n, 1)]
				= -4 / (h_x * h_x) * pow(sin((yi + 1) * M_PI / (2 * n)), 2);
			}
		} else {
			if (d.isNeumann(Side::north) && d.isNeumann(Side::south)) {
				for (int yi = 0; yi < n; yi++) {
					denom[slice(yi * n, n, 1)]
					= -4 / (h_x * h_x) * pow(sin(yi * M_PI / (2 * n)), 2);
				}
			} else if (d.isNeumann(Side::south) || d.isNeumann(Side::north)) {
				for (int yi = 0; yi < n; yi++) {
					denom[slice(yi * n, n, 1)]
					= -4 / (h_x * h_x) * pow(sin((yi + 0.5) * M_PI / (2 * n)), 2);
				}
			} else {
				for (int yi = 0; yi < n; yi++) {
					denom[slice(yi * n, n, 1)]
					= -4 / (h_x * h_x) * pow(sin((yi + 1) * M_PI / (2 * n)), 2);
				}
			}
		}

		valarray<double> ones(n);
		ones = 1;

		if (d.neumann.none()) {
			for (int xi = 0; xi < n; xi++) {
				denom[slice(xi, n, n)]
				-= 4 / (h_y * h_y) * pow(sin((xi + 1) * M_PI / (2 * n)), 2) * ones;
			}
		} else {
			if (d.isNeumann(Side::east) && d.isNeumann(Side::west)) {
				for (int xi = 0; xi < n; xi++) {
					denom[slice(xi, n, n)]
					-= 4 / (h_y * h_y) * pow(sin(xi * M_PI / (2 * n)), 2) * ones;
				}
			} else if (d.isNeumann(Side::west) || d.isNeumann(Side::east)) {
				for (int xi = 0; xi < n; xi++) {
					denom[slice(xi, n, n)]
					-= 4 / (h_y * h_y) * pow(sin((xi + 0.5) * M_PI / (2 * n)), 2) * ones;
				}
			} else {
				for (int xi = 0; xi < n; xi++) {
					denom[slice(xi, n, n)]
					-= 4 / (h_y * h_y) * pow(sin((xi + 1) * M_PI / (2 * n)), 2) * ones;
				}
			}
		}
		denom += lambda;
	}
}
FftwPatchSolver::~FftwPatchSolver()
{
	for (auto p : plan1) {
		fftw_destroy_plan(p.second);
	}
	for (auto p : plan2) {
		fftw_destroy_plan(p.second);
	}
}
void FftwPatchSolver::solve(Domain &d, const Vec f, Vec u, const Vec gamma)
{
	double h_x = d.x_length / n;
	double h_y = d.y_length / n;

	double *f_view, *gamma_view;
	VecGetArray(f, &f_view);
	VecGetArray(gamma, &gamma_view);

	int start = d.id_local * n * n;
	for (int i = 0; i < n * n; i++) {
		f_copy[i] = f_view[start + i];
	}

	if (d.hasNbr(Side::north)) {
		int idx = n * d.index(Side::north);
		for (int i = 0; i < n; i++) {
			f_copy[n * (n - 1) + i] -= 2 / (h_y * h_y) * gamma_view[idx + i];
		}
	}
	if (d.hasNbr(Side::east)) {
		int idx = n * d.index(Side::east);
		for (int i = 0; i < n; i++) {
			f_copy[i * n + (n - 1)] -= 2 / (h_x * h_x) * gamma_view[idx + i];
		}
	}
	if (d.hasNbr(Side::south)) {
		int idx = n * d.index(Side::south);
		for (int i = 0; i < n; i++) {
			f_copy[i] -= 2 / (h_y * h_y) * gamma_view[idx + i];
		}
	}
	if (d.hasNbr(Side::west)) {
		int idx = n * d.index(Side::west);
		for (int i = 0; i < n; i++) {
			f_copy[n * i] -= 2 / (h_x * h_x) * gamma_view[idx + i];
		}
	}

	fftw_execute(plan1[d]);

	tmp /= denoms[d];

	if (d.neumann.all()) {
		tmp[0] = 0;
	}

	fftw_execute(plan2[d]);

	sol /= (4.0 * n * n);

	double *u_view;
	VecGetArray(u, &u_view);
	for (int i = 0; i < n * n; i++) {
		u_view[start + i] = sol[i];
	}
	VecRestoreArray(u, &u_view);
	VecRestoreArray(f, &f_view);
	VecRestoreArray(gamma, &gamma_view);
}
