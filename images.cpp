#include "images.h"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"


using namespace fessga;


void fessga::img::load_distribution_from_image(grd::Densities2d& densities, msh::SurfaceMesh& mesh, const char* filename) {
	if (!IO::file_exists(string(filename))) {
		cerr << "Error: image file " + string(filename) + " does not exist";
		throw std::runtime_error("Error: image file " + string(filename) + " does not exist");
	}

	// Load image
	int width, height, numChannels;
	unsigned char* image = stbi_load(filename, &width, &height, &numChannels, 0);
	if (!image) {
		std::cerr << "Failed to load image." << std::endl;
	}

	// Get size of design domain
	int pixels_per_cell = width / densities.dim_x; // The width is leading in determining the size of the cells
	densities.dim_y = height / pixels_per_cell;
	float mesh_width = mesh.diagonal.maxCoeff();
	float width_per_cell = mesh_width / densities.dim_x;
	Vector3d mesh_size = Vector3d(mesh_width, width_per_cell * densities.dim_y, 0);
	mesh = msh::SurfaceMesh(mesh_size);
	grd::Densities2d _densities = grd::Densities2d(densities.dim_x, mesh.diagonal, densities.output_folder);
	_densities.fea_casemanager = densities.fea_casemanager;
	_densities.fea_casemanager->dim_x = _densities.dim_x + 1; _densities.fea_casemanager->dim_y = _densities.dim_y + 1;;
	densities = _densities;

	// Extract red, green, and blue values
	int numPixels = width * height;
	int* redValues = new int[numPixels];
	int* greenValues = new int[numPixels];
	int* blueValues = new int[numPixels];
	for (int i = 0; i < numPixels; ++i) {
		redValues[i] = image[i * numChannels];
		greenValues[i] = image[i * numChannels + 1];
		blueValues[i] = image[i * numChannels + 2];
	}
	stbi_image_free(image);

	// Sample r,g,b values with intervals to derive which cells should be cutout cells, which should be filled, and which should be inactive
	int x_offset = (densities.dim_x / 2);
	x_offset = 0;
	for (int y = 0; y < densities.dim_y; y++) {
		for (int x = 0; x < densities.dim_x; x++) {
			int img_idx = y * width * pixels_per_cell + (x - x_offset) * pixels_per_cell;
			int red_count = 0;
			int green_count = 0;
			int blue_count = 0;
			// Count all pixel values in the cell
			for (int i = 0; i < pixels_per_cell * pixels_per_cell; i++) {
				int _y = i / pixels_per_cell;
				int _x = i % pixels_per_cell;
				int _img_idx = img_idx + _y * width + _x;
				red_count += redValues[_img_idx] * 2 - greenValues[_img_idx] - blueValues[_img_idx];
				green_count += greenValues[_img_idx];
				blue_count += blueValues[_img_idx] * 2 - greenValues[_img_idx] - redValues[_img_idx];
			}
			int coord = (x + 1) * densities.dim_y - y - 1;

			// Store filled cells
			int cell_value = round((float)green_count / (float)(255 * (pixels_per_cell * pixels_per_cell)));
			densities.set(coord, cell_value);
			
			// Store cutout cells
			bool is_cutout = round((float)red_count / (float)(255 * (pixels_per_cell * pixels_per_cell))) > 0;
			if (is_cutout) {
				int coord = (x + 1) * densities.dim_y - y - 1;
				densities.fea_casemanager->cutout_cells.push_back(coord);
				densities.del(coord);
			}

			// Store inactive cells
			float blue_sum = (float)blue_count / (float)(255 * (pixels_per_cell * pixels_per_cell));
			bool is_inactive = round(blue_sum) > 0;
			if (is_inactive) {
				densities.fea_casemanager->inactive_cells.push_back(coord);
				densities.fea_casemanager->keep_cells.push_back(coord);
			}
		}
	}
	densities.update_count();
	densities.do_feasibility_filtering();
	densities.update_count();

	cout << "Loaded densities from image.\n";
	cout << " - filled cell count: " << densities.count() << endl;
	cout << " - no inactive cells: " << densities.fea_casemanager->inactive_cells.size() << endl;
	cout << " - no cutout cells: " << densities.fea_casemanager->cutout_cells.size() << endl;
	cout << " - no keep cells: " << densities.fea_casemanager->keep_cells.size() << endl;

	delete[] blueValues;
	delete[] greenValues;
	delete[] redValues;
}

void fessga::img::convert_distribution_to_single_channel_image(
	grd::Densities2d densities, unsigned char* single_channel, Image* image) {
	for (int y = 0; y < densities.dim_y; y++) {
		for (int x = 0; x < densities.dim_x; x++) {
			int dens_idx = (x + 1) * densities.dim_y - y - 1;
			for (int i = 0; i < image->pixels_per_cell; i++) {
				for (int j = 0; j < image->pixels_per_cell; j++) {
					if (y == image->height - 1) continue;
					int img_idx = (y * image->width * image->pixels_per_cell + i * image->width) + (x * image->pixels_per_cell - j);
					single_channel[img_idx] = densities[dens_idx] * 255;
				}
			}
		}
	}
}

void fessga::img::singlechannel_to_rgb(unsigned char* single_channel, Image* image) {
	// Convert single-channel image to RGB image by copying each value three times
	for (int i = 0; i < image->size; i += 3) {
		image->vals[i] = single_channel[i / 3];
		image->vals[i + 1] = single_channel[i / 3];
		image->vals[i + 2] = single_channel[i / 3];
	}
}

void fessga::img::write_distribution_to_image(grd::Densities2d densities, string path, bool verbose) {
	img::Image image = img::Image(path, densities.dim_x, densities.dim_y, 3, densities);
	unsigned char* single_channel = new unsigned char[image.width * image.height];
	convert_distribution_to_single_channel_image(densities, single_channel, &image);
	singlechannel_to_rgb(single_channel, &image);
	stbi_write_jpg(image.path, image.width, image.height, image.channels, image.vals, 100);
	if (verbose) cout << "Exported distribution to image. Path: " << image.path << endl;
	delete[] single_channel;
	delete[] image.vals;
}
