/* Copyright (C) 2015 Atsushi Togo */
/* All rights reserved. */

/* This file is part of phonopy. */

/* Redistribution and use in source and binary forms, with or without */
/* modification, are permitted provided that the following conditions */
/* are met: */

/* * Redistributions of source code must retain the above copyright */
/*   notice, this list of conditions and the following disclaimer. */

/* * Redistributions in binary form must reproduce the above copyright */
/*   notice, this list of conditions and the following disclaimer in */
/*   the documentation and/or other materials provided with the */
/*   distribution. */

/* * Neither the name of the phonopy project nor the names of its */
/*   contributors may be used to endorse or promote products derived */
/*   from this software without specific prior written permission. */

/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS */
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS */
/* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE */
/* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, */
/* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, */
/* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; */
/* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER */
/* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT */
/* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN */
/* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE */
/* POSSIBILITY OF SUCH DAMAGE. */

#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <numpy/arrayobject.h>
#include <lapacke.h>
#include <lapack_wrapper.h>
#include <phonoc_array.h>
#include <phonoc_utils.h>
#include <phonon3_h/fc3.h>
#include <phonon3_h/frequency_shift.h>
#include <phonon3_h/interaction.h>
#include <phonon3_h/imag_self_energy.h>
#include <phonon3_h/imag_self_energy_with_g.h>
#include <phonon3_h/collision_matrix.h>
#include <other_h/isotope.h>
#include <triplet_h/triplet.h>
#include <tetrahedron_method.h>

/* #define LIBFLAME */
#ifdef LIBFLAME
#include <flame_wrapper.h>
#endif

static PyObject * py_get_interaction(PyObject *self, PyObject *args);
static PyObject * py_get_imag_self_energy(PyObject *self, PyObject *args);
static PyObject * py_get_imag_self_energy_at_bands(PyObject *self,
						   PyObject *args);
static PyObject * py_get_imag_self_energy_with_g(PyObject *self, PyObject *args);
static PyObject * py_get_frequency_shift_at_bands(PyObject *self,
						  PyObject *args);
static PyObject * py_get_collision_matrix(PyObject *self, PyObject *args);
static PyObject * py_get_reducible_collision_matrix(PyObject *self,
						    PyObject *args);
static PyObject * py_symmetrize_collision_matrix(PyObject *self,
						 PyObject *args);
static PyObject * py_distribute_fc3(PyObject *self, PyObject *args);
static PyObject * py_get_isotope_strength(PyObject *self, PyObject *args);
static PyObject * py_get_thm_isotope_strength(PyObject *self, PyObject *args);
static PyObject * py_set_permutation_symmetry_fc3(PyObject *self,
						  PyObject *args);
static PyObject * py_get_neighboring_gird_points(PyObject *self, PyObject *args);
static PyObject * py_set_integration_weights(PyObject *self, PyObject *args);
static PyObject *
py_spg_get_triplets_reciprocal_mesh_at_q(PyObject *self, PyObject *args);
static PyObject * py_spg_get_BZ_triplets_at_q(PyObject *self, PyObject *args);
static PyObject *
py_set_triplets_integration_weights(PyObject *self, PyObject *args);
static PyObject *
py_set_triplets_integration_weights_with_sigma(PyObject *self, PyObject *args);
static PyObject * py_inverse_collision_matrix(PyObject *self, PyObject *args);

#ifdef LIBFLAME
static PyObject * py_inverse_collision_matrix_libflame(PyObject *self, PyObject *args);
#endif

static void get_triplet_tetrahedra_vertices
  (int vertices[2][24][4],
   SPGCONST int relative_grid_address[2][24][4][3],
   const int mesh[3],
   const int triplet[3],
   SPGCONST int bz_grid_address[][3],
   const int bz_map[]);

static PyMethodDef functions[] = {
  {"interaction", py_get_interaction, METH_VARARGS, "Interaction of triplets"},
  {"imag_self_energy", py_get_imag_self_energy, METH_VARARGS,
   "Imaginary part of self energy"},
  {"imag_self_energy_at_bands", py_get_imag_self_energy_at_bands, METH_VARARGS,
   "Imaginary part of self energy at phonon frequencies of bands"},
  {"imag_self_energy_with_g", py_get_imag_self_energy_with_g, METH_VARARGS,
   "Imaginary part of self energy at phonon frequencies of bands for tetrahedron method"},
  {"frequency_shift_at_bands", py_get_frequency_shift_at_bands, METH_VARARGS,
   "Imaginary part of self energy at phonon frequencies of bands"},
  {"collision_matrix", py_get_collision_matrix, METH_VARARGS,
   "Collision matrix with g"},
  {"reducible_collision_matrix", py_get_reducible_collision_matrix, METH_VARARGS,
   "Collision matrix with g for reducible grid points"},
  {"symmetrize_collision_matrix", py_symmetrize_collision_matrix, METH_VARARGS,
   "Symmetrize collision matrix"},
  {"distribute_fc3", py_distribute_fc3, METH_VARARGS,
   "Distribute least fc3 to full fc3"},
  {"isotope_strength", py_get_isotope_strength, METH_VARARGS,
   "Isotope scattering strength"},
  {"thm_isotope_strength", py_get_thm_isotope_strength, METH_VARARGS,
   "Isotope scattering strength for tetrahedron_method"},
  {"permutation_symmetry_fc3", py_set_permutation_symmetry_fc3, METH_VARARGS,
   "Set permutation symmetry for fc3"},
  {"neighboring_grid_points", py_get_neighboring_gird_points, METH_VARARGS,
   "Neighboring grid points by relative grid addresses"},
  {"integration_weights", py_set_integration_weights, METH_VARARGS,
   "Integration weights of tetrahedron method"},
  {"triplets_reciprocal_mesh_at_q", py_spg_get_triplets_reciprocal_mesh_at_q,
   METH_VARARGS, "Triplets on reciprocal mesh points at a specific q-point"},
  {"BZ_triplets_at_q", py_spg_get_BZ_triplets_at_q,
   METH_VARARGS, "Triplets in reciprocal primitive lattice are transformed to those in BZ."},
  {"triplets_integration_weights", py_set_triplets_integration_weights, METH_VARARGS,
   "Integration weights of tetrahedron method for triplets"},
  {"triplets_integration_weights_with_sigma",
   py_set_triplets_integration_weights_with_sigma, METH_VARARGS,
   "Integration weights of smearing method for triplets"},
  {"inverse_collision_matrix", py_inverse_collision_matrix, METH_VARARGS,
   "Pseudo-inverse using Lapack dsyev"},
#ifdef LIBFLAME
  {"inverse_collision_matrix_libflame",
   py_inverse_collision_matrix_libflame, METH_VARARGS,
   "Pseudo-inverse using libflame hevd"},
#endif
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC init_phono3py(void)
{
  Py_InitModule3("_phono3py", functions, "C-extension for phono3py\n\n...\n");
  return;
}

static PyObject * py_get_interaction(PyObject *self, PyObject *args)
{
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies;
  PyArrayObject* eigenvectors;
  PyArrayObject* grid_point_triplets;
  PyArrayObject* grid_address_py;
  PyArrayObject* mesh_py;
  PyArrayObject* shortest_vectors;
  PyArrayObject* multiplicity;
  PyArrayObject* fc3_py;
  PyArrayObject* atomic_masses;
  PyArrayObject* p2s_map;
  PyArrayObject* s2p_map;
  PyArrayObject* band_indicies_py;
  double cutoff_frequency;
  int symmetrize_fc3_q;

  if (!PyArg_ParseTuple(args, "OOOOOOOOOOOOOid",
			&fc3_normal_squared_py,
			&frequencies,
			&eigenvectors,
			&grid_point_triplets,
			&grid_address_py,
			&mesh_py,
			&fc3_py,
			&shortest_vectors,
			&multiplicity,
			&atomic_masses,
			&p2s_map,
			&s2p_map,
			&band_indicies_py,
			&symmetrize_fc3_q,
			&cutoff_frequency)) {
    return NULL;
  }


  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  Darray* freqs = convert_to_darray(frequencies);
  /* npy_cdouble and lapack_complex_double may not be compatible. */
  /* So eigenvectors should not be used in Python side */
  Carray* eigvecs = convert_to_carray(eigenvectors);
  Iarray* triplets = convert_to_iarray(grid_point_triplets);
  const int* grid_address = (int*)grid_address_py->data;
  const int* mesh = (int*)mesh_py->data;
  Darray* fc3 = convert_to_darray(fc3_py);
  Darray* svecs = convert_to_darray(shortest_vectors);
  Iarray* multi = convert_to_iarray(multiplicity);
  const double* masses = (double*)atomic_masses->data;
  const int* p2s = (int*)p2s_map->data;
  const int* s2p = (int*)s2p_map->data;
  const int* band_indicies = (int*)band_indicies_py->data;

  get_interaction(fc3_normal_squared,
		  freqs,
		  eigvecs,
		  triplets,
		  grid_address,
		  mesh,
		  fc3,
		  svecs,
		  multi,
		  masses,
		  p2s,
		  s2p,
		  band_indicies,
		  symmetrize_fc3_q,
		  cutoff_frequency);

  free(fc3_normal_squared);
  free(freqs);
  free(eigvecs);
  free(triplets);
  free(fc3);
  free(svecs);
  free(multi);
  
  Py_RETURN_NONE;
}


static PyObject * py_get_imag_self_energy(PyObject *self, PyObject *args)
{
  PyArrayObject* gamma_py;
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* grid_point_triplets_py;
  PyArrayObject* triplet_weights_py;
  double sigma, unit_conversion_factor, cutoff_frequency, temperature, fpoint;

  if (!PyArg_ParseTuple(args, "OOOOOddddd",
			&gamma_py,
			&fc3_normal_squared_py,
			&grid_point_triplets_py,
			&triplet_weights_py,
			&frequencies_py,
			&fpoint,
			&temperature,
			&sigma,
			&unit_conversion_factor,
			&cutoff_frequency)) {
    return NULL;
  }


  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  double* gamma = (double*)gamma_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* grid_point_triplets = (int*)grid_point_triplets_py->data;
  const int* triplet_weights = (int*)triplet_weights_py->data;

  get_imag_self_energy(gamma,
		       fc3_normal_squared,
		       fpoint,
		       frequencies,
		       grid_point_triplets,
		       triplet_weights,
		       sigma,
		       temperature,
		       unit_conversion_factor,
		       cutoff_frequency);

  free(fc3_normal_squared);
  
  Py_RETURN_NONE;
}

static PyObject * py_get_imag_self_energy_at_bands(PyObject *self,
						   PyObject *args)
{
  PyArrayObject* gamma_py;
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* grid_point_triplets_py;
  PyArrayObject* triplet_weights_py;
  PyArrayObject* band_indices_py;
  double sigma, unit_conversion_factor, cutoff_frequency, temperature;

  if (!PyArg_ParseTuple(args, "OOOOOOdddd",
			&gamma_py,
			&fc3_normal_squared_py,
			&grid_point_triplets_py,
			&triplet_weights_py,
			&frequencies_py,
			&band_indices_py,
			&temperature,
			&sigma,
			&unit_conversion_factor,
			&cutoff_frequency)) {
    return NULL;
  }


  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  double* gamma = (double*)gamma_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* band_indices = (int*)band_indices_py->data;
  const int* grid_point_triplets = (int*)grid_point_triplets_py->data;
  const int* triplet_weights = (int*)triplet_weights_py->data;

  get_imag_self_energy_at_bands(gamma,
				fc3_normal_squared,
				band_indices,
				frequencies,
				grid_point_triplets,
				triplet_weights,
				sigma,
				temperature,
				unit_conversion_factor,
				cutoff_frequency);

  free(fc3_normal_squared);
  
  Py_RETURN_NONE;
}

static PyObject * py_get_imag_self_energy_with_g(PyObject *self, PyObject *args)
{
  PyArrayObject* gamma_py;
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* grid_point_triplets_py;
  PyArrayObject* triplet_weights_py;
  PyArrayObject* g_py;
  double unit_conversion_factor, cutoff_frequency, temperature;

  if (!PyArg_ParseTuple(args, "OOOOOdOdd",
			&gamma_py,
			&fc3_normal_squared_py,
			&grid_point_triplets_py,
			&triplet_weights_py,
			&frequencies_py,
			&temperature,
			&g_py,
			&unit_conversion_factor,
			&cutoff_frequency)) {
    return NULL;
  }

  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  double* gamma = (double*)gamma_py->data;
  const double* g = (double*)g_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* grid_point_triplets = (int*)grid_point_triplets_py->data;
  const int* triplet_weights = (int*)triplet_weights_py->data;

  get_imag_self_energy_at_bands_with_g(gamma,
				       fc3_normal_squared,
				       frequencies,
				       grid_point_triplets,
				       triplet_weights,
				       g,
				       temperature,
				       unit_conversion_factor,
				       cutoff_frequency);

  free(fc3_normal_squared);
  
  Py_RETURN_NONE;
}

static PyObject * py_get_frequency_shift_at_bands(PyObject *self,
						  PyObject *args)
{
  PyArrayObject* shift_py;
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* grid_point_triplets_py;
  PyArrayObject* triplet_weights_py;
  PyArrayObject* band_indices_py;
  double epsilon, unit_conversion_factor, cutoff_frequency, temperature;

  if (!PyArg_ParseTuple(args, "OOOOOOdddd",
			&shift_py,
			&fc3_normal_squared_py,
			&grid_point_triplets_py,
			&triplet_weights_py,
			&frequencies_py,
			&band_indices_py,
			&temperature,
			&epsilon,
			&unit_conversion_factor,
			&cutoff_frequency)) {
    return NULL;
  }


  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  double* shift = (double*)shift_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* band_indices = (int*)band_indices_py->data;
  const int* grid_point_triplets = (int*)grid_point_triplets_py->data;
  const int* triplet_weights = (int*)triplet_weights_py->data;

  get_frequency_shift_at_bands(shift,
			       fc3_normal_squared,
			       band_indices,
			       frequencies,
			       grid_point_triplets,
			       triplet_weights,
			       epsilon,
			       temperature,
			       unit_conversion_factor,
			       cutoff_frequency);

  free(fc3_normal_squared);
  
  Py_RETURN_NONE;
}

static PyObject * py_get_collision_matrix(PyObject *self, PyObject *args)
{
  PyArrayObject* collision_matrix_py;
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* triplets_py;
  PyArrayObject* triplets_map_py;
  PyArrayObject* stabilized_gp_map_py;
  PyArrayObject* g_py;
  PyArrayObject* ir_grid_points_py;
  PyArrayObject* rotated_grid_points_py;
  PyArrayObject* rotations_cartesian_py;
  double temperature, unit_conversion_factor, cutoff_frequency;

  if (!PyArg_ParseTuple(args, "OOOOOOOOOOddd",
			&collision_matrix_py,
			&fc3_normal_squared_py,
			&frequencies_py,
			&g_py,
			&triplets_py,
			&triplets_map_py,
			&stabilized_gp_map_py,
			&ir_grid_points_py,
			&rotated_grid_points_py,
			&rotations_cartesian_py,
			&temperature,
			&unit_conversion_factor,
			&cutoff_frequency)) {
    return NULL;
  }

  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  double* collision_matrix = (double*)collision_matrix_py->data;
  const double* g = (double*)g_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* triplets = (int*)triplets_py->data;
  Iarray* triplets_map = convert_to_iarray(triplets_map_py);
  const int* stabilized_gp_map = (int*)stabilized_gp_map_py->data;
  const int* ir_grid_points = (int*)ir_grid_points_py->data;
  Iarray* rotated_grid_points = convert_to_iarray(rotated_grid_points_py);
  const double* rotations_cartesian = (double*)rotations_cartesian_py->data;

  get_collision_matrix(collision_matrix,
  		       fc3_normal_squared,
  		       frequencies,
  		       triplets,
  		       triplets_map,
  		       stabilized_gp_map,
  		       ir_grid_points,
  		       rotated_grid_points,
  		       rotations_cartesian,
  		       g,
  		       temperature,
  		       unit_conversion_factor,
  		       cutoff_frequency);
  
  free(fc3_normal_squared);
  free(triplets_map);
  free(rotated_grid_points);
  
  Py_RETURN_NONE;
}

static PyObject * py_get_reducible_collision_matrix(PyObject *self, PyObject *args)
{
  PyArrayObject* collision_matrix_py;
  PyArrayObject* fc3_normal_squared_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* triplets_py;
  PyArrayObject* triplets_map_py;
  PyArrayObject* stabilized_gp_map_py;
  PyArrayObject* g_py;
  double temperature, unit_conversion_factor, cutoff_frequency;

  if (!PyArg_ParseTuple(args, "OOOOOOOddd",
			&collision_matrix_py,
			&fc3_normal_squared_py,
			&frequencies_py,
			&g_py,
			&triplets_py,
			&triplets_map_py,
			&stabilized_gp_map_py,
			&temperature,
			&unit_conversion_factor,
			&cutoff_frequency)) {
    return NULL;
  }

  Darray* fc3_normal_squared = convert_to_darray(fc3_normal_squared_py);
  double* collision_matrix = (double*)collision_matrix_py->data;
  const double* g = (double*)g_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* triplets = (int*)triplets_py->data;
  Iarray* triplets_map = convert_to_iarray(triplets_map_py);
  const int* stabilized_gp_map = (int*)stabilized_gp_map_py->data;

  get_reducible_collision_matrix(collision_matrix,
				 fc3_normal_squared,
				 frequencies,
				 triplets,
				 triplets_map,
				 stabilized_gp_map,
				 g,
				 temperature,
				 unit_conversion_factor,
				 cutoff_frequency);
  
  free(fc3_normal_squared);
  free(triplets_map);
  
  Py_RETURN_NONE;
}

static PyObject * py_symmetrize_collision_matrix(PyObject *self, PyObject *args)
{
  PyArrayObject* collision_matrix_py;
  
  if (!PyArg_ParseTuple(args, "O",
			&collision_matrix_py)) {
    return NULL;
  }

  double* collision_matrix = (double*)collision_matrix_py->data;
  const int num_sigma = (int)collision_matrix_py->dimensions[0];
  const int num_temp = (int)collision_matrix_py->dimensions[1];
  const int num_grid_points = (int)collision_matrix_py->dimensions[2];
  const int num_band = (int)collision_matrix_py->dimensions[3];
  int i, j, k, l, num_column, adrs_shift;
  double val;

  if (collision_matrix_py->nd == 8) {
    num_column = num_grid_points * num_band * 3;
  } else {
    num_column = num_grid_points * num_band;
  }
  
  for (i = 0; i < num_sigma; i++) {
    for (j = 0; j < num_temp; j++) {
      adrs_shift = (i * num_column * num_column * num_temp +
		    j * num_column * num_column);
#pragma omp parallel for private(l, val)
      for (k = 0; k < num_column; k++) {
	for (l = k + 1; l < num_column; l++) {
	  val = (collision_matrix[adrs_shift + k * num_column + l] +
		 collision_matrix[adrs_shift + l * num_column + k]) / 2;
	  collision_matrix[adrs_shift + k * num_column + l] = val;
	  collision_matrix[adrs_shift + l * num_column + k] = val;
	}
      }
    }
  }
    
  Py_RETURN_NONE;
}

static PyObject * py_get_isotope_strength(PyObject *self, PyObject *args)
{
  PyArrayObject* gamma_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* eigenvectors_py;
  PyArrayObject* band_indices_py;
  PyArrayObject* mass_variances_py;
  int grid_point;
  int num_grid_points;
  double cutoff_frequency;
  double sigma;

  if (!PyArg_ParseTuple(args, "OiOOOOidd",
			&gamma_py,
			&grid_point,
			&mass_variances_py,
			&frequencies_py,
			&eigenvectors_py,
			&band_indices_py,
			&num_grid_points,
			&sigma,
			&cutoff_frequency)) {
    return NULL;
  }


  double* gamma = (double*)gamma_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const lapack_complex_double* eigenvectors =
    (lapack_complex_double*)eigenvectors_py->data;
  const int* band_indices = (int*)band_indices_py->data;
  const double* mass_variances = (double*)mass_variances_py->data;
  const int num_band = (int)frequencies_py->dimensions[1];
  const int num_band0 = (int)band_indices_py->dimensions[0];

  /* int i, j, k; */
  /* double f, f0; */
  /* int *weights, *ir_grid_points; */
  /* double *integration_weights; */

  /* ir_grid_points = (int*)malloc(sizeof(int) * num_grid_points); */
  /* weights = (int*)malloc(sizeof(int) * num_grid_points); */
  /* integration_weights = (double*)malloc(sizeof(double) * */
  /* 					num_grid_points * num_band0 * num_band); */

  /* for (i = 0; i < num_grid_points; i++) { */
  /*   ir_grid_points[i] = i; */
  /*   weights[i] = 1; */
  /*   for (j = 0; j < num_band0; j++) { */
  /*     f0 = frequencies[grid_point * num_band + band_indices[j]]; */
  /*     for (k = 0; k < num_band; k++) { */
  /* 	f = frequencies[i * num_band + k]; */
  /* 	integration_weights[i * num_band0 * num_band + */
  /* 			    j * num_band + k] = gaussian(f - f0, sigma); */
  /*     } */
  /*   } */
  /* } */

  /* get_thm_isotope_scattering_strength(gamma, */
  /* 				      grid_point, */
  /* 				      ir_grid_points, */
  /* 				      weights, */
  /* 				      mass_variances, */
  /* 				      frequencies, */
  /* 				      eigenvectors, */
  /* 				      num_grid_points, */
  /* 				      band_indices, */
  /* 				      num_band, */
  /* 				      num_band0, */
  /* 				      integration_weights, */
  /* 				      cutoff_frequency); */
      
  /* free(ir_grid_points); */
  /* free(weights); */
  /* free(integration_weights); */
  
  get_isotope_scattering_strength(gamma,
  				  grid_point,
  				  mass_variances,
  				  frequencies,
  				  eigenvectors,
  				  num_grid_points,
  				  band_indices,
  				  num_band,
  				  num_band0,
  				  sigma,
  				  cutoff_frequency);
  
  Py_RETURN_NONE;
}

static PyObject * py_get_thm_isotope_strength(PyObject *self, PyObject *args)
{
  PyArrayObject* gamma_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* eigenvectors_py;
  PyArrayObject* band_indices_py;
  PyArrayObject* mass_variances_py;
  PyArrayObject* ir_grid_points_py;
  PyArrayObject* weights_py;
  int grid_point;
  double cutoff_frequency;
  PyArrayObject* integration_weights_py;


  if (!PyArg_ParseTuple(args, "OiOOOOOOOd",
			&gamma_py,
			&grid_point,
			&ir_grid_points_py,
			&weights_py,
			&mass_variances_py,
			&frequencies_py,
			&eigenvectors_py,
			&band_indices_py,
			&integration_weights_py,
			&cutoff_frequency)) {
    return NULL;
  }


  double* gamma = (double*)gamma_py->data;
  const double* frequencies = (double*)frequencies_py->data;
  const int* ir_grid_points = (int*)ir_grid_points_py->data;
  const int* weights = (int*)weights_py->data;
  const lapack_complex_double* eigenvectors =
    (lapack_complex_double*)eigenvectors_py->data;
  const int* band_indices = (int*)band_indices_py->data;
  const double* mass_variances = (double*)mass_variances_py->data;
  const int num_band = (int)frequencies_py->dimensions[1];
  const int num_band0 = (int)band_indices_py->dimensions[0];
  const double* integration_weights = (double*)integration_weights_py->data;
  const int num_ir_grid_points = (int)ir_grid_points_py->dimensions[0];
    
  get_thm_isotope_scattering_strength(gamma,
				      grid_point,
				      ir_grid_points,
				      weights,
				      mass_variances,
				      frequencies,
				      eigenvectors,
				      num_ir_grid_points,
				      band_indices,
				      num_band,
				      num_band0,
				      integration_weights,
				      cutoff_frequency);
  
  Py_RETURN_NONE;
}

static PyObject * py_distribute_fc3(PyObject *self, PyObject *args)
{
  PyArrayObject* force_constants_third_copy;
  PyArrayObject* force_constants_third;
  int third_atom;
  PyArrayObject* rotation_cart_inv;
  PyArrayObject* atom_mapping_py;

  if (!PyArg_ParseTuple(args, "OOiOO",
			&force_constants_third_copy,
			&force_constants_third,
			&third_atom,
			&atom_mapping_py,
			&rotation_cart_inv)) {
    return NULL;
  }

  double* fc3_copy = (double*)force_constants_third_copy->data;
  const double* fc3 = (double*)force_constants_third->data;
  const double* rot_cart_inv = (double*)rotation_cart_inv->data;
  const int* atom_mapping = (int*)atom_mapping_py->data;
  const int num_atom = (int)atom_mapping_py->dimensions[0];

  distribute_fc3(fc3_copy,
		 fc3,
		 third_atom,
		 atom_mapping,
		 num_atom,
		 rot_cart_inv);
  
  Py_RETURN_NONE;
}

static PyObject * py_set_permutation_symmetry_fc3(PyObject *self, PyObject *args)
{
  PyArrayObject* fc3_py;

  if (!PyArg_ParseTuple(args, "O",
			&fc3_py)) {
    return NULL;
  }

  double* fc3 = (double*)fc3_py->data;
  const int num_atom = (int)fc3_py->dimensions[0];

  set_permutation_symmetry_fc3(fc3, num_atom);

  Py_RETURN_NONE;
}

static PyObject * py_get_neighboring_gird_points(PyObject *self, PyObject *args)
{
  PyArrayObject* relative_grid_points_py;
  PyArrayObject* grid_points_py;
  PyArrayObject* relative_grid_address_py;
  PyArrayObject* mesh_py;
  PyArrayObject* bz_grid_address_py;
  PyArrayObject* bz_map_py;
  if (!PyArg_ParseTuple(args, "OOOOOO",
			&relative_grid_points_py,
			&grid_points_py,
			&relative_grid_address_py,
			&mesh_py,
			&bz_grid_address_py,
			&bz_map_py)) {
    return NULL;
  }

  int* relative_grid_points = (int*)relative_grid_points_py->data;
  const int *grid_points = (int*)grid_points_py->data;
  const int num_grid_points = (int)grid_points_py->dimensions[0];
  SPGCONST int (*relative_grid_address)[3] =
    (int(*)[3])relative_grid_address_py->data;
  const int num_relative_grid_address = relative_grid_address_py->dimensions[0];
  const int *mesh = (int*)mesh_py->data;
  SPGCONST int (*bz_grid_address)[3] = (int(*)[3])bz_grid_address_py->data;
  const int *bz_map = (int*)bz_map_py->data;

  int i;
#pragma omp parallel for
  for (i = 0; i < num_grid_points; i++) {
    thm_get_neighboring_grid_points
      (relative_grid_points + i * num_relative_grid_address,
       grid_points[i],
       relative_grid_address,
       num_relative_grid_address,
       mesh,
       bz_grid_address,
       bz_map);
  }
  
  Py_RETURN_NONE;
}

static PyObject * py_set_integration_weights(PyObject *self, PyObject *args)
{
  PyArrayObject* iw_py;
  PyArrayObject* frequency_points_py;
  PyArrayObject* relative_grid_address_py;
  PyArrayObject* mesh_py;
  PyArrayObject* grid_points_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* bz_grid_address_py;
  PyArrayObject* bz_map_py;
  if (!PyArg_ParseTuple(args, "OOOOOOOO",
			&iw_py,
			&frequency_points_py,
			&relative_grid_address_py,
			&mesh_py,
			&grid_points_py,
			&frequencies_py,
			&bz_grid_address_py,
			&bz_map_py)) {
    return NULL;
  }

  double *iw = (double*)iw_py->data;
  const double *frequency_points = (double*)frequency_points_py->data;
  const int num_band0 = frequency_points_py->dimensions[0];
  SPGCONST int (*relative_grid_address)[4][3] =
    (int(*)[4][3])relative_grid_address_py->data;
  const int *mesh = (int*)mesh_py->data;
  SPGCONST int *grid_points = (int*)grid_points_py->data;
  const int num_gp = (int)grid_points_py->dimensions[0];
  SPGCONST int (*bz_grid_address)[3] = (int(*)[3])bz_grid_address_py->data;
  const int *bz_map = (int*)bz_map_py->data;
  const double *frequencies = (double*)frequencies_py->data;
  const int num_band = (int)frequencies_py->dimensions[1];

  int i, j, k, bi;
  int vertices[24][4];
  double freq_vertices[24][4];
    
#pragma omp parallel for private(j, k, bi, vertices, freq_vertices)
  for (i = 0; i < num_gp; i++) {
    for (j = 0; j < 24; j++) {
      thm_get_neighboring_grid_points(vertices[j],
				      grid_points[i],
				      relative_grid_address[j],
				      4,
				      mesh,
				      bz_grid_address,
				      bz_map);
    }
    for (bi = 0; bi < num_band; bi++) {
      for (j = 0; j < 24; j++) {
	for (k = 0; k < 4; k++) {
	  freq_vertices[j][k] = frequencies[vertices[j][k] * num_band + bi];
	}
      }
      for (j = 0; j < num_band0; j++) {
	iw[i * num_band0 * num_band + j * num_band + bi] =
	  thm_get_integration_weight(frequency_points[j], freq_vertices, 'I');
      }
    }
  }
	    
  Py_RETURN_NONE;
}

static PyObject * 
py_spg_get_triplets_reciprocal_mesh_at_q(PyObject *self, PyObject *args)
{
  PyArrayObject* map_triplets;
  PyArrayObject* grid_address_py;
  PyArrayObject* map_q;
  int fixed_grid_number;
  PyArrayObject* mesh;
  int is_time_reversal;
  PyArrayObject* rotations;
  if (!PyArg_ParseTuple(args, "OOOiOiO",
			&map_triplets,
			&map_q,
			&grid_address_py,
			&fixed_grid_number,
			&mesh,
			&is_time_reversal,
			&rotations)) {
    return NULL;
  }

  int (*grid_address)[3] = (int(*)[3])grid_address_py->data;
  int *map_triplets_int = (int*)map_triplets->data;
  int *map_q_int = (int*)map_q->data;

  const int* mesh_int = (int*)mesh->data;
  SPGCONST int (*rot)[3][3] = (int(*)[3][3])rotations->data;
  const int num_rot = rotations->dimensions[0];
  const int num_ir =
    spg_get_triplets_reciprocal_mesh_at_q(map_triplets_int,
					  map_q_int,
					  grid_address,
					  fixed_grid_number,
					  mesh_int,
					  is_time_reversal,
					  num_rot,
					  rot);

  return PyLong_FromLong((long) num_ir);
}


static PyObject * py_spg_get_BZ_triplets_at_q(PyObject *self, PyObject *args)
{
  PyArrayObject* triplets_py;
  PyArrayObject* bz_grid_address_py;
  PyArrayObject* bz_map_py;
  PyArrayObject* map_triplets_py;
  PyArrayObject* mesh_py;
  int grid_point;
  if (!PyArg_ParseTuple(args, "OiOOOO",
			&triplets_py,
			&grid_point,
			&bz_grid_address_py,
			&bz_map_py,
			&map_triplets_py,
			&mesh_py)) {
    return NULL;
  }

  int (*triplets)[3] = (int(*)[3])triplets_py->data;
  SPGCONST int (*bz_grid_address)[3] = (int(*)[3])bz_grid_address_py->data;
  const int *bz_map = (int*)bz_map_py->data;
  const int *map_triplets = (int*)map_triplets_py->data;
  const int num_map_triplets = (int)map_triplets_py->dimensions[0];
  const int *mesh = (int*)mesh_py->data;
  int num_ir;

  num_ir = spg_get_BZ_triplets_at_q(triplets,
				    grid_point,
				    bz_grid_address,
				    bz_map,
				    map_triplets,
				    num_map_triplets,
				    mesh);

  return PyLong_FromLong((long) num_ir);
}

static PyObject *
py_set_triplets_integration_weights(PyObject *self, PyObject *args)
{
  PyArrayObject* iw_py;
  PyArrayObject* frequency_points_py;
  PyArrayObject* relative_grid_address_py;
  PyArrayObject* mesh_py;
  PyArrayObject* triplets_py;
  PyArrayObject* frequencies_py;
  PyArrayObject* bz_grid_address_py;
  PyArrayObject* bz_map_py;
  if (!PyArg_ParseTuple(args, "OOOOOOOO",
			&iw_py,
			&frequency_points_py,
			&relative_grid_address_py,
			&mesh_py,
			&triplets_py,
			&frequencies_py,
			&bz_grid_address_py,
			&bz_map_py)) {
    return NULL;
  }

  double *iw = (double*)iw_py->data;
  const double *frequency_points = (double*)frequency_points_py->data;
  const int num_band0 = frequency_points_py->dimensions[0];
  SPGCONST int (*relative_grid_address)[4][3] =
    (int(*)[4][3])relative_grid_address_py->data;
  const int *mesh = (int*)mesh_py->data;
  SPGCONST int (*triplets)[3] = (int(*)[3])triplets_py->data;
  const int num_triplets = (int)triplets_py->dimensions[0];
  SPGCONST int (*bz_grid_address)[3] = (int(*)[3])bz_grid_address_py->data;
  const int *bz_map = (int*)bz_map_py->data;
  const double *frequencies = (double*)frequencies_py->data;
  const int num_band = (int)frequencies_py->dimensions[1];
  const int num_iw = (int)iw_py->dimensions[0];

  int i, j, k, l, b1, b2, sign;
  int tp_relative_grid_address[2][24][4][3];
  int vertices[2][24][4];
  int adrs_shift;
  double f0, f1, f2, g0, g1, g2;
  double freq_vertices[3][24][4];
    
  for (i = 0; i < 2; i++) {
    sign = 1 - i * 2;
    for (j = 0; j < 24; j++) {
      for (k = 0; k < 4; k++) {
	for (l = 0; l < 3; l++) {
	  tp_relative_grid_address[i][j][k][l] = 
	    relative_grid_address[j][k][l] * sign;
	}
      }
    }
  }

#pragma omp parallel for private(j, k, b1, b2, vertices, adrs_shift, f0, f1, f2, g0, g1, g2, freq_vertices)
  for (i = 0; i < num_triplets; i++) {
    get_triplet_tetrahedra_vertices(vertices,
				    tp_relative_grid_address,
				    mesh,
				    triplets[i],
				    bz_grid_address,
				    bz_map);
    for (b1 = 0; b1 < num_band; b1++) {
      for (b2 = 0; b2 < num_band; b2++) {
	for (j = 0; j < 24; j++) {
	  for (k = 0; k < 4; k++) {
	    f1 = frequencies[vertices[0][j][k] * num_band + b1];
	    f2 = frequencies[vertices[1][j][k] * num_band + b2];
	    freq_vertices[0][j][k] = f1 + f2;
	    freq_vertices[1][j][k] = -f1 + f2;
	    freq_vertices[2][j][k] = f1 - f2;
	  }
	}
	for (j = 0; j < num_band0; j++) {
	  f0 = frequency_points[j];
	  g0 = thm_get_integration_weight(f0, freq_vertices[0], 'I');
	  g1 = thm_get_integration_weight(f0, freq_vertices[1], 'I');
	  g2 = thm_get_integration_weight(f0, freq_vertices[2], 'I');
	  adrs_shift = i * num_band0 * num_band * num_band +
	    j * num_band * num_band + b1 * num_band + b2;
	  iw[adrs_shift] = g0;
	  adrs_shift += num_triplets * num_band0 * num_band * num_band;
	  iw[adrs_shift] = g1 - g2;
	  if (num_iw == 3) {
	    adrs_shift += num_triplets * num_band0 * num_band * num_band;
	    iw[adrs_shift] = g0 + g1 + g2;
	  }
	}
      }	
    }
  }
	    
  Py_RETURN_NONE;
}

static PyObject *
py_set_triplets_integration_weights_with_sigma(PyObject *self, PyObject *args)
{
  PyArrayObject* iw_py;
  PyArrayObject* frequency_points_py;
  PyArrayObject* triplets_py;
  PyArrayObject* frequencies_py;
  double sigma;
  
  if (!PyArg_ParseTuple(args, "OOOOd",
			&iw_py,
			&frequency_points_py,
			&triplets_py,
			&frequencies_py,
			&sigma)) {
    return NULL;
  }

  double *iw = (double*)iw_py->data;
  const double *frequency_points = (double*)frequency_points_py->data;
  const int num_band0 = frequency_points_py->dimensions[0];
  SPGCONST int (*triplets)[3] = (int(*)[3])triplets_py->data;
  const int num_triplets = (int)triplets_py->dimensions[0];
  const double *frequencies = (double*)frequencies_py->data;
  const int num_band = (int)frequencies_py->dimensions[1];
  const int num_iw = (int)iw_py->dimensions[0];

  int i, j, k, l, adrs_shift;
  double f0, f1, f2, g0, g1, g2;

#pragma omp parallel for private(j, k, l, adrs_shift, f0, f1, f2, g0, g1, g2)
  for (i = 0; i < num_triplets; i++) {
    for (j = 0; j < num_band0; j++) {
      f0 = frequency_points[j];
      for (k = 0; k < num_band; k++) {
	f1 = frequencies[triplets[i][1] * num_band + k];
	for (l = 0; l < num_band; l++) {
	  f2 = frequencies[triplets[i][2] * num_band + l];
	  g0 = gaussian(f0 - f1 - f2, sigma);
	  g1 = gaussian(f0 + f1 - f2, sigma);
	  g2 = gaussian(f0 - f1 + f2, sigma);
	  adrs_shift = i * num_band0 * num_band * num_band +
	    j * num_band * num_band + k * num_band + l;
	  iw[adrs_shift] = g0;
	  adrs_shift += num_triplets * num_band0 * num_band * num_band;
	  iw[adrs_shift] = g1 - g2;
	  if (num_iw == 3) {
	    adrs_shift += num_triplets * num_band0 * num_band * num_band;
	    iw[adrs_shift] = g0 + g1 + g2;
	  }
	}
      }
    }
  }			

  Py_RETURN_NONE;
}

#ifdef LIBFLAME
static PyObject * py_inverse_collision_matrix_libflame(PyObject *self, PyObject *args)
{
  PyArrayObject* collision_matrix_py;
  PyArrayObject* eigenvalues_py;
  int i_sigma, i_temp;
  double cutoff;

  if (!PyArg_ParseTuple(args, "OOiid",
			&collision_matrix_py,
			&eigenvalues_py,
			&i_sigma,
			&i_temp,
			&cutoff)) {
    return NULL;
  }

  
  double* collision_matrix = (double*)collision_matrix_py->data;
  double* eigvals = (double*)eigenvalues_py->data;
  const int num_temp = (int)collision_matrix_py->dimensions[1];
  const int num_ir_grid_points = (int)collision_matrix_py->dimensions[2];
  const int num_band = (int)collision_matrix_py->dimensions[3];
  int num_column, adrs_shift;
  num_column = num_ir_grid_points * num_band * 3;

  adrs_shift = (i_sigma * num_column * num_column * num_temp +
		i_temp * num_column * num_column);
  
  phonopy_pinv_libflame(collision_matrix + adrs_shift,
			eigvals, num_column, cutoff);
  
  Py_RETURN_NONE;
}
#endif

static PyObject * py_inverse_collision_matrix(PyObject *self, PyObject *args)
{
  PyArrayObject* collision_matrix_py;
  PyArrayObject* eigenvalues_py;
  double cutoff;
  int i_sigma, i_temp;

  if (!PyArg_ParseTuple(args, "OOiid",
			&collision_matrix_py,
			&eigenvalues_py,
			&i_sigma,
			&i_temp,
			&cutoff)) {
    return NULL;
  }

  double* collision_matrix = (double*)collision_matrix_py->data;
  double* eigvals = (double*)eigenvalues_py->data;
  const int num_temp = (int)collision_matrix_py->dimensions[1];
  const int num_ir_grid_points = (int)collision_matrix_py->dimensions[2];
  const int num_band = (int)collision_matrix_py->dimensions[3];
  
  int num_column, adrs_shift, info;
  num_column = num_ir_grid_points * num_band * 3;
  adrs_shift = (i_sigma * num_column * num_column * num_temp +
		i_temp * num_column * num_column);
  
  info = phonopy_pinv_dsyev(collision_matrix + adrs_shift,
			    eigvals, num_column, cutoff);

  return PyLong_FromLong((long) info);
}

static void get_triplet_tetrahedra_vertices
  (int vertices[2][24][4],
   SPGCONST int relative_grid_address[2][24][4][3],
   const int mesh[3],
   const int triplet[3],
   SPGCONST int bz_grid_address[][3],
   const int bz_map[])
{
  int i, j;

  for (i = 0; i < 2; i++) {
    for (j = 0; j < 24; j++) {
      thm_get_neighboring_grid_points(vertices[i][j],
				      triplet[i + 1],
				      relative_grid_address[i][j],
				      4,
				      mesh,
				      bz_grid_address,
				      bz_map);
    }
  }
}

