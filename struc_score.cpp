#include "CLI/Option.hpp"
#include "CLI/Validators.hpp"
#include <CLI/CLI.hpp>
#include <casmutils/definitions.hpp>
#include <casmutils/xtal/structure.hpp>
#include <casmutils/xtal/structure_tools.hpp>
#include <casmutils/mapping/structure_mapping.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace utilities
{
// TODO: Move somewhere common. You can make a custom callback so that
// fs::path is recognized as PATH instad of TEXT
CLI::Option* add_output_suboption(CLI::App* app, fs::path* output_path)
{
    auto* opt = app->add_option("-o,--output", *output_path, "Target output file");
    return opt;
}

} // namespace utilities

int main(int argc, char* argv[])
{
    CLI::App app;

    utilities::fs::path out_path;
    CLI::Option* out_path_opt = utilities::add_output_suboption(&app, &out_path);

    casmutils::fs::path reference_path;
    std::vector<casmutils::fs::path> mappable_paths;
    casmutils::fs::path batch_path;
    double weight = 0.5;
    CLI::Option* reference_path_opt = app.add_option("-r,--reference",
                                                     reference_path,
                                                     "POS.vasp like file to use as reference structure.")->required();
    CLI::Option* mappable_path_opt = app.add_option("-m,--mappable",
                                                    mappable_paths,
                                                    "POS.vasp like file(s) to map, get structure score for.");
    CLI::Option* batch_path_opt = app.add_option("-b,--batch",
                                                 batch_path,
                                                 "Batch file containing list of structures files to get structure score for.");
    CLI::Option* weight_opt = app.add_option("-w,--weight",
                                             weight,
                                             "Weight w in structure score: w*lattice_score + (1-w)*basis_score.");

    CLI11_PARSE(app, argc, argv);

    if (!batch_path.empty())
    {
        std::ifstream batch_file(batch_path.c_str());
        std::string line;
        while(std::getline(batch_file, line))
        {
            mappable_paths.emplace_back(line);
        }
    }
    if (mappable_paths.empty())
    {
        std::cout << "You must provide at least one structure to map." << std::endl;
        return 2;
    }
    std::set<casmutils::fs::path> mappable_paths_set(mappable_paths.begin(), mappable_paths.end());

    auto reference_struc = casmutils::xtal::Structure::from_poscar(reference_path);
    std::vector<std::pair<double, double>> all_scores;
    casmutils::fs::path::string_type::size_type max_path_length = 0;
    for (const auto& path : mappable_paths_set)
    {
        max_path_length = std::max(max_path_length, path.string().size());
        auto mappable_struc = casmutils::xtal::Structure::from_poscar(path);
        auto best_report = casmutils::mapping::map_structure(reference_struc, mappable_struc).front();
        all_scores.push_back(casmutils::mapping::structure_score(best_report));
    }

    std::ostream* out_stream_ptr=&std::cout;
    std::ofstream specified_out_stream;
    if (!out_path.empty())
    {
        specified_out_stream.open(out_path.c_str());
        out_stream_ptr=&specified_out_stream;
    }
    auto& out_stream=*out_stream_ptr;

    out_stream<<std::left<<std::setw(max_path_length+4)<<"Structure";
    out_stream<<std::left<<std::setw(16)<<"Lattice";
    out_stream<<std::left<<std::setw(16)<<"Basis";
    out_stream<<std::left<<std::setw(16)<<"Weighted"<<std::endl;

    assert(all_scores.size()==mappable_paths_set.size());
    auto path_it = mappable_paths_set.begin();
    for (const auto& lat_basis_score : all_scores)
    {
        auto lat_score=lat_basis_score.first;
        auto basis_score=lat_basis_score.second;
        auto weighted_score=weight*lat_score+(1-weight)*basis_score;
        out_stream<<std::left<<std::setw(max_path_length+4)<<path_it->string();
        out_stream<<std::left<<std::setw(16)<<std::setprecision(8)<<lat_score;
        out_stream<<std::left<<std::setw(16)<<std::setprecision(8)<<basis_score;
        out_stream<<std::left<<std::setw(16)<<std::setprecision(8)<<weighted_score<<std::endl;
        ++path_it;
    }

    return 0;
}
