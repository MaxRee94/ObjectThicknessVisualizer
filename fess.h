#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <Eigen/Core>
#include <algorithm>
#include <map>
#include <functional>
#include "helpers.h"
#include "optimizerBase.h"


class FESS : public OptimizerBase {
public:
	FESS() = default;
	FESS(
		string _msh_file, string _case_file, mesher::SurfaceMesh _mesh, string _output_folder, double _min_stress_threshold,
		double _max_stress_threshold, uint* _starting_densities, mesher::Grid3D _grid
	) : OptimizerBase(_msh_file, _case_file, _mesh, _output_folder, _max_stress_threshold, _starting_densities, _grid)
	{
		min_stress_threshold = _min_stress_threshold;
	}
	double min_stress_threshold = 1.0;
	void run();
};


void FESS::run() {
	cout << "Beginning FESS run. Saving results to " << output_folder << endl;

	double min_stress = 1.0;
	double max_stress = 1e8;
	string msh = msh_file;
	string cur_iteration_name = "";
	string cur_output_folder = output_folder;
	int i = 0;
	while (max_stress < max_stress_threshold) {
		cout << "\nFESS: Starting iteration " << i << ".\n";

		// Create new subfolder for output of current iteration
		string cur_iteration_name = fessga::help::add_padding("iteration_", i + 1) + to_string(i + 1);
		string cur_output_folder = output_folder + "/" + cur_iteration_name;
		cout << "FESS: Created output folder " << cur_output_folder << " for current iteration.\n";
		IO::create_folder_if_not_exists(cur_output_folder);

		// Copy the case.sif file to the newly created subfolder
		IO::copy_file(case_file, cur_output_folder + "/case.sif");
		if (IO::FileExists(cur_output_folder + "/case.sif")) cout << "FESS: Copied case file to subfolder.\n";
		else cout << "FESS: ERROR: Failed to copy case file to subfolder.\n";
		
		// Generate new FE mesh using modified density distribution
		cout << "FESS: Generating new FE mesh...\n";
		mesher::FEMesh2D fe_mesh;
		mesher::generate_FE_mesh(grid, mesh, densities, fe_mesh);
		cout << "FESS: FE mesh generation done.\n";

		// Export newly generated FE mesh
		string bat_file = mesher::export_as_elmer_files(&fe_mesh, cur_output_folder);
		if (IO::FileExists(cur_output_folder + "/mesh.header")) cout << "FESS: Exported new FE mesh to subfolder.\n";
		else cout << "FESS: ERROR: Failed to export new FE mesh to subfolder.\n";

		// Call Elmer to run FEA on new FE mesh
		cout << "FESS: Calling Elmer .bat file (" << bat_file << ")\n";
		physics::call_elmer(bat_file);

		// Obtain stress distribution from the .vtk file
		Vector2d inv_cell_size = Vector2d(1.0 / grid.cell_size(0), 1.0 / grid.cell_size(1));
		Vector2d offset = Vector2d(mesh.offset(0), mesh.offset(1));
		double* vonmises = new double[(grid.x + 1) * (grid.y + 1)]; // Nodes grid has +1 width along each dimension
		string filename = cur_output_folder + "/case0001.vtk";
		physics::load_2d_physics_data(filename, vonmises, grid.x + 1, grid.y + 1, offset, inv_cell_size);
		cout << "FESS: Finished reading physics data." << endl;

		// Set all cells that have corresponding stress values lower than <min_stress_threshold> to density=0.
		max_stress = 1e10; // Temporary value that will make FESS terminate after one iteration. TODO: remove

		i++;
	}

	cout << "FESS: Algorithm converged. Results were saved to " << output_folder << endl;
}
