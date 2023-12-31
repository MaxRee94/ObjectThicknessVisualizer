#pragma once
#include "tests.h"


void Tester::run_tests() {
    cout << "Running tests...\n\n";
    int successes = 0;
    int failures = 0;

    bool _success = test_is_single_piece();
    successes += _success;
    failures += !_success;

    _success = test_init_pieces();
    successes += _success;
    failures += !_success;

    _success = test_remove_smaller_pieces();
    successes += _success;
    failures += !_success;

    _success = test_restore_removed_pieces();
    successes += _success;
    failures += !_success;

    _success = test_remove_isolated_material();
    successes += _success;
    failures += !_success;

    _success = test_fill_voids();
    successes += _success;
    failures += !_success;

    _success = test_feasibility_filtering();
    successes += _success;
    failures += !_success;

    _success = test_repair();
    successes += _success;
    failures += !_success;

    _success = test_init_population();
    successes += _success;
    failures += !_success;

    _success = test_image_loader();
    successes += _success;
    failures += !_success;

    cout << "ALL TESTS FINISHED. " << successes << " / " << (failures + successes) << " tests passed.\n";
}


void Tester::init_dummy_fess(FESS& optimizer) {
    ctrl->run_fess(optimizer);
}

void Tester::init_dummy_evolver(Evolver& evolver) {
    ctrl->run_emma_static(evolver);
}

OptimizerBase Tester::do_setup(string type, string path, bool verbose, int dim_x, int dim_y, string base_folder) {
    _input = ctrl->input;
    _dim_x = ctrl->dim_x; _dim_y = ctrl->dim_y;
    if (dim_x > 0 && dim_y > 0) {
        ctrl->dim_x = dim_x;
        ctrl->dim_y = dim_y;
    }
    // Init optimizer with base teapot distribution
    _base_folder = ctrl->base_folder;
    if (base_folder != "") ctrl->base_folder = base_folder;
    ctrl->input.path = ctrl->base_folder + "/distribution2d.dens";
    ctrl->input.type = "distribution2d";
    ctrl->init_densities();
    FESS optimizer;
    init_dummy_fess(optimizer);

    // Update input and distribution with given type and path
    ctrl->input.path = path;
    ctrl->input.type = type;
    ctrl->init_densities();

    optimizer.densities.copy_from(&ctrl->densities2d);

    if (verbose) optimizer.densities.print();

    return optimizer;
}

Evolver Tester::do_evolver_setup(string type, string path, bool verbose, int dim_x, int dim_y, string base_folder) {
    _input = ctrl->input;
    _dim_x = ctrl->dim_x; _dim_y = ctrl->dim_y;
    if (dim_x > 0 && dim_y > 0) {
        ctrl->dim_x = dim_x;
        ctrl->dim_y = dim_y;
    }
    // Init optimizer with base teapot distribution
    _base_folder = ctrl->base_folder;
    if (base_folder != "") ctrl->base_folder = base_folder;
    ctrl->input.path = ctrl->base_folder + "/distribution2d.dens";
    ctrl->input.type = "distribution2d";
    ctrl->init_densities();
    Evolver evolver;
    init_dummy_evolver(evolver);

    // Update input and distribution with given type and path
    ctrl->input.path = path;
    ctrl->input.type = type;
    ctrl->init_densities();

    evolver.densities.copy_from(&ctrl->densities2d);

    if (verbose) evolver.densities.print();

    return evolver;
}

void Tester::do_teardown() {
    ctrl->input = _input;
    ctrl->dim_x = _dim_x;
    ctrl->dim_y = _dim_y;
    ctrl->densities2d.flush_edit_memory();
    ctrl->base_folder = _base_folder;
}

bool Tester::do_individual_is_single_piece_test(string type, string path) {
    OptimizerBase optimizer = do_setup(type, path);
    bool output = optimizer.densities.is_single_piece();
    do_teardown();

    return output;
}

bool Tester::do_individual_init_pieces_test(string type, string path, int expected_result, int dim_x, int dim_y, bool verbose) {
    OptimizerBase optimizer = do_setup(type, path, verbose, dim_x, dim_y);
    optimizer.densities.init_pieces();
    do_teardown();

    bool success = true;
    if (optimizer.densities.pieces.size() != expected_result) {
        success = false;
        cout << "Test failed because found number of pieces (" << optimizer.densities.pieces.size() << ") is unequal to expected number (" <<
            expected_result << ")\n";
        for (auto& piece : optimizer.densities.pieces) {
            for (auto& cell : piece.cells) optimizer.densities.set(cell, 5);
        }
        if (verbose) optimizer.densities.print();
    }

    return optimizer.densities.pieces.size() == expected_result;
}

bool Tester::do_individual_remove_smaller_pieces_test(string type, string path, bool expected_result, bool verbose) {
    // Setup
    OptimizerBase optimizer = do_setup(type, path);
    optimizer.densities.init_pieces();
    int initial_no_pieces = optimizer.densities.pieces.size();
    optimizer.densities.flush_edit_memory();
    optimizer.densities.remove_and_remember(820);
    optimizer.densities.init_pieces();

    // Test
    optimizer.densities.remove_smaller_pieces(optimizer.densities.pieces, &optimizer.densities.removed_cells);
    optimizer.densities.restore(820);

    // Check success
    bool success = true;
    if (optimizer.densities.is_single_piece() != expected_result) {
        success = false; cout << "Test failed because shape removal was expected to " << (expected_result ? "succeed." : "fail.") << endl;
    }
    if (verbose) { cout << "Densities after piece removal:\n"; optimizer.densities.print(); }

    // Teardown
    ctrl->input = _input;
    optimizer.densities.flush_edit_memory();

    return success;
}

bool Tester::do_individual_restore_pieces_test(string type, string path, bool verbose) {
    // Setup
    OptimizerBase optimizer = do_setup(type, path, verbose);
    int initial_count = optimizer.densities.count();
    optimizer.densities.init_pieces();
    optimizer.densities.remove_smaller_pieces(optimizer.densities.pieces, &optimizer.densities.removed_cells);
    int initial_no_pieces = optimizer.densities.pieces.size();
    int initial_no_removed_pieces = optimizer.densities.removed_pieces.size();

    // Test
    optimizer.densities.restore_removed_pieces(optimizer.densities.removed_pieces);

    // Check success
    bool success = true;
    if (initial_count != optimizer.densities.count()) {
        success = false; cout << "Test failed because count (" << optimizer.densities.count() << ") is unequal to count from before piece removal (" <<
            initial_count << ")\n";
    }
    if (optimizer.densities.pieces.size() != initial_no_removed_pieces + initial_no_pieces) {
        success = false; cout << "Test failed because the number of pieces, namely " <<
            optimizer.densities.pieces.size() << " is not what was expected (" << initial_no_removed_pieces + initial_no_pieces << ")\n";
    }
    if (optimizer.densities.removed_pieces.size() > 0) {
        success = false; cout << "Test failed because the number of removed pieces, namely " <<
            optimizer.densities.removed_pieces.size() << " is larger than 0.\n";
    }
    if (verbose) { cout << "Densities after piece restoration:\n"; optimizer.densities.print(); }

    // Teardown
    do_teardown();

    return success;
}

bool Tester::do_individual_remove_isolated_material_test(string type, string path, bool expected_validity, bool verbose) {
    // Setup
    OptimizerBase optimizer = do_setup(type, path);
    evo::Individual2d individual(&optimizer.densities);
    if (verbose) optimizer.densities.visualize_keep_cells();

    // Test
    bool valid = individual.remove_isolated_material();
    bool success = valid == expected_validity;
    if (success && valid) {
        success = success && individual.is_single_piece();
        if (!success) {
            individual.init_pieces();
            cout << "Test failed because 'remove isolated material' function reported valid removal, but the shape is composed of " <<
                individual.pieces.size() << " pieces.\n";
        }
    }
    else cout << "Test failed because function reported " << (valid ? "valid" : "invalid") << " removal, but " <<
        (expected_validity ? "validity" : "invalidity") << " was expected.\n";
    if (verbose) cout << "distribution after isolated material removal: \n"; individual.print();

    // Teardown
    do_teardown();

    return success;
}

bool Tester::do_individual_fill_voids_test(string type, string path, bool verbose) {
    // Setup
    if (verbose) cout << "Test 1 (target no neighbors = 4):\n";
    OptimizerBase optimizer = do_setup(type, path, verbose);
    evo::Individual2d individual(&optimizer.densities);

    // Test 1 (target no neighbors = 4)
    individual.save_snapshot();
    individual.fill_voids(4);
    if (verbose) {
        cout << endl; individual.print();
    }

    // Test 2 (target no neighbors = 3)
    individual.load_snapshot();
    individual.save_snapshot();
    if (verbose) {
        cout << "\nTest 2 (target no neighbors = 3):\n";
        individual.print();
        cout << endl;
    }
    individual.fill_voids(3);
    if (verbose) individual.print();

    // Evaluate
    bool success = true;

    // Teardown
    do_teardown();

    return success;
}

bool Tester::do_individual_feasibility_filtering_test(string type, string path, bool verbose) {
    // Setup
    OptimizerBase optimizer = do_setup(type, path, verbose);
    evo::Individual2d individual(&optimizer.densities);

    // Test
    individual.do_feasibility_filtering();
    if (verbose) individual.print();

    // Evaluate
    bool success = true;

    // Teardown
    do_teardown();

    return success;
}

bool Tester::do_individual_repair_test(string type, string path, bool verbose) {
    // Setup
    OptimizerBase optimizer = do_setup(type, path, verbose);
    evo::Individual2d individual(&optimizer.densities);

    // Test
    individual.repair();
    cout << endl;
    if (verbose) individual.print();

    // Evaluate
    bool success = true;

    // Teardown
    do_teardown();

    return success;
}

bool Tester::do_individual_init_population_test(string type, string path, bool verbose) {
    // Setup
    Evolver evolver = do_evolver_setup(type, path, verbose);

    // Test
    evolver.init_population();
    if (verbose) {
        for (int i = 0; i < evolver.population.size(); i++) {
            cout << "Individual " + to_string(i) + ":\n";
            evolver.population[i].print();
        }
    }

    // Evaluate
    bool success = true;

    // Teardown
    do_teardown();

    return success;
}

bool Tester::do_individual_image_loader_test(string type, string path, bool verbose) {
    // Setup
    Evolver evolver = do_evolver_setup(type, path, verbose);

    // Test
    evolver.init_population();
    if (verbose) {
        for (int i = 0; i < evolver.population.size(); i++) {
            cout << "Individual " + to_string(i) + ":\n";
            evolver.population[i].print();
        }
    }

    // Evaluate
    bool success = true;

    // Teardown
    do_teardown();

    return success;
}

// Do multi-piece and single-piece tests
bool Tester::test_is_single_piece() {
    bool success = true;
    success = success && !do_individual_is_single_piece_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1.dens");
    success = success && !do_individual_is_single_piece_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2.dens");
    success = success && do_individual_is_single_piece_test("distribution2d", "../data/unit_tests/distribution2d_single_piece.dens");

    cout << "\nTESTING: densities.is_single_piece(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'get_pieces' function
bool Tester::test_init_pieces() {
    bool success = true;
    success = success && do_individual_init_pieces_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1.dens", 5);
    success = success && do_individual_init_pieces_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2.dens", 2);
    success = success && do_individual_init_pieces_test("distribution2d", "../data/unit_tests/distribution2d_single_piece.dens", 1);
    //success = success && do_individual_init_pieces_test("distribution2d", "../data/unit_tests/distribution2d_teapot200elements.dens", 6, 200, 200);

    cout << "\nTESTING: densities.init_pieces(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'remove_smaller_pieces' function
bool Tester::test_remove_smaller_pieces() {
    bool success = true;
    success = success && do_individual_remove_smaller_pieces_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1.dens", false);
    success = success && do_individual_remove_smaller_pieces_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2.dens", true);
    success = success && do_individual_remove_smaller_pieces_test("distribution2d", "../data/unit_tests/distribution2d_single_piece.dens", true);

    cout << "\nTESTING: densities.remove_smaller_pieces(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'restore_pieces' function
bool Tester::test_restore_removed_pieces() {
    bool success = true;
    success = success && do_individual_restore_pieces_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1.dens");
    success = success && do_individual_restore_pieces_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2.dens");
    success = success && do_individual_restore_pieces_test("distribution2d", "../data/unit_tests/distribution2d_single_piece.dens");

    cout << "\nTESTING: individual.restore_pieces(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'remove isolated material' function
bool Tester::test_remove_isolated_material() {
    bool success = true;
    success = success && do_individual_remove_isolated_material_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1.dens", false);
    success = success && do_individual_remove_isolated_material_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2.dens", true);
    success = success && do_individual_remove_isolated_material_test("distribution2d", "../data/unit_tests/distribution2d_single_piece.dens", true);

    cout << "\nTESTING: individual.remove_isolated_material(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}


// Test 'fill voids' function
bool Tester::test_fill_voids() {
    bool success = true;
    success = success && do_individual_fill_voids_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1_mutated.dens");
    success = success && do_individual_fill_voids_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2_mutated.dens");

    cout << "\nTESTING: individual.fill_voids(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'feasibility filtering' function
bool Tester::test_feasibility_filtering() {
    bool success = true;
    success = success && do_individual_feasibility_filtering_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1_mutated.dens");
    success = success && do_individual_feasibility_filtering_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2_mutated.dens");

    cout << "\nTESTING: individual.feasibility_filtering(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'repair' function
bool Tester::test_repair() {
    bool success = true;
    success = success && do_individual_repair_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_1_mutated.dens");
    success = success && do_individual_repair_test("distribution2d", "../data/unit_tests/distribution2d_multi_piece_2_mutated.dens");

    cout << "\nTESTING: individual.repair(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test 'init_population' function
bool Tester::test_init_population() {
    bool success = true;
    success = success && do_individual_init_population_test("distribution2d", "../data/unit_tests/distribution2d_single_piece.dens");

    cout << "\nTESTING: evolver.init_population(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

// Test void loader
bool Tester::test_image_loader() {
    bool success = true;
    success = success && do_individual_image_loader_test("image", "../data/images/t_rex_including_cutouts.jpg");

    cout << "\nTESTING: img::load_distribution_from_image(). Test " << (success ? "passed." : "failed.") << "\n\n";

    return success;
}

/*
Create 2 parent slices from the 3d binary density distribution for 2d test
*/
void Tester::create_parents(grd::Densities2d parent1, grd::Densities2d parent2) {
    int z1 = ctrl->dim_x / 2;
    int z2 = ctrl->dim_x / 2 + (ctrl->dim_x / 5);
    ctrl->densities3d.create_slice(parent1, 2, z1);
    ctrl->densities3d.create_slice(parent2, 2, z2);
}

/*
Test 2-point crossover of two 2d parent solutions. Print parents and children to console
*/
bool Tester::test_2x_crossover() {
    evo::Individual2d parent1(&ctrl->densities2d);
    evo::Individual2d parent2(&ctrl->densities2d);
    double max_stress = 1e9; // arbitrary maximum stress
    int max_iterations = 100;
    int max_iterations_without_change = 10;
    bool export_msh = false;

    create_parents(parent1, parent2);
    cout << "\nParent 1: \n";
    parent1.print();
    cout << "\nParent 2: \n";
    parent2.print();

    string fea_case = "../data/msh_output/case.sif";
    string msh_file = "../data/msh_output/test.msh";
    string base_folder = "../data/msh_output/FESSGA_test_output";

    // Do crossover
    evo::Individual2d child1(&ctrl->densities2d);
    evo::Individual2d child2(&ctrl->densities2d);
    Evolver evolver;
    init_dummy_evolver(evolver);
    evolver.do_2x_crossover(parent1, parent2, child1, child2);

    cout << "\Child 1: \n";
    child1.print();
    cout << "\Child 2: \n";
    child2.print();

    return true;
}

bool Tester::test_evolution() {
    Evolver evolver = Evolver();

    return true;
}
