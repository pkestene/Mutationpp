#include "mutation++.h"
#include <iostream>
#include <fstream>

using std::cout;
using std::endl;

#define COLUMN_WIDTH 14

// Defines a value that can be printed
struct OutputQuantity {
    std::string name;
    std::string units;
    std::string description;
    
    OutputQuantity(
        const std::string& n, const std::string& u, const std::string& d)
        : name(n), units(u), description(d)
    { }
};

// List of all mixture output quantities
#define NMIXTURE 31
OutputQuantity mixture_quantities[NMIXTURE] = {
    OutputQuantity("Th", "K", "heavy particle temperature"),
    OutputQuantity("P", "Pa", "pressure"),
    OutputQuantity("rho", "kg/m^3", "density"),
    OutputQuantity("nd", "1/m^3", "number density"),
    OutputQuantity("Mw", "kg/mol", "molecular weight"),
    OutputQuantity("Cp_eq", "J/mol-K", "equilibrium specific heat at constant pressure"),
    OutputQuantity("H", "J/mol", "enthalpy"),
    OutputQuantity("S", "J/mol-K", "entropy"),
    OutputQuantity("Cv_eq", "J/mol-K", "equilibirum specific heat at constant volume"),
    OutputQuantity("Cp_eq", "J/kg-K", "equilibrium specific heat at constant pressure"),
    OutputQuantity("H", "J/kg", "enthalpy"),
    OutputQuantity("S", "J/kg-K", "entropy"),
    OutputQuantity("Cv_eq", "J/kg-K", "equilibrium specific heat at constant volume"),
    OutputQuantity("Cp", "J/mol-K", "frozen specific heat at constant pressure"),
    OutputQuantity("Cv", "J/mol-K", "frozen specific heat at constant volume"),
    OutputQuantity("Cp", "J/kg-K", "frozen specific heat at constant pressure"),
    OutputQuantity("Cv", "J/kg-K", "frozen specific heat at constant volume"),
    OutputQuantity("gam_eq", "", "equilibrium ratio of specific heats"),
    OutputQuantity("gamma", "", "frozen ratio of specific heat"),
    OutputQuantity("Ht", "J/mol", "translational enthalpy"),
    OutputQuantity("Hr", "J/mol", "rotational enthalpy"),
    OutputQuantity("Hv", "J/mol", "vibrational enthalpy"),
    OutputQuantity("Hel", "J/mol", "electronic enthalpy"),
    OutputQuantity("Hf", "J/mol", "formation enthalpy"),
    OutputQuantity("Ht", "J/kg", "translational enthalpy"),
    OutputQuantity("Hr", "J/kg", "rotational enthalpy"),
    OutputQuantity("Hv", "J/kg", "vibrational enthalpy"),
    OutputQuantity("Hel", "J/kg", "electronic enthalpy"),
    OutputQuantity("Hf", "J/kg", "formation enthalpy"),
    OutputQuantity("mu", "Pa-s", "dynamic viscosity"),
    OutputQuantity("lambda", "?", "thermal conductivity")
};

// List of all species output quantities
#define NSPECIES 13
OutputQuantity species_quantities[NSPECIES] = {
    OutputQuantity("X", "", "mole fractions"),
    OutputQuantity("Y", "", "mass fractions"),
    OutputQuantity("rho", "kg/m^3", "mass densities"),
    OutputQuantity("conc", "mol/m^3", "molar concentrations"),
    OutputQuantity("Cp", "J/mol-K", "specific heats at constant pressure"),
    OutputQuantity("H", "J/mol", "enthalpies"),
    OutputQuantity("S", "J/mol-K", "entropies"),
    OutputQuantity("G", "J/mol", "Gibbs free energies"),
    OutputQuantity("Cp", "J/kg-K", "specific heats at constant pressure"),
    OutputQuantity("H", "J/kg", "enthalpies"),
    OutputQuantity("S", "J/kg-K", "entropies"),
    OutputQuantity("G", "J/kg", "Gibbs free energies"),
    OutputQuantity("omega", "kg/m^3-s", "production rates due to reactions")
};

// Simply stores the command line options
typedef struct {
    double T1;
    double T2;
    double dT;
    
    double P1;
    double P2;
    double dP;
    
    std::vector<int> mixture_indices;
    std::vector<int> species_indices;
    
    bool verbose;
    
    MixtureOptions* p_mixture_opts;
} Options;

// Checks if an option is present
bool optionExists(int argc, char** argv, const std::string& option)
{
    return (std::find(argv, argv+argc, option) != argv+argc);
}

// Returns the value associated with a particular option
std::string getOption(int argc, char** argv, const std::string& option)
{
    std::string value;
    char** ptr = std::find(argv, argv+argc, option);
    
    if (ptr == argv+argc || ptr+1 == argv+argc)
        value = "";
    else
        value = *(ptr+1);
    
    return value;
}


// Prints the program's usage information and exits.
void printHelpMessage(const char* const name)
{
    std::string tab("    ");
    
    cout.setf(std::ios::left, std::ios::adjustfield);
    
    cout << endl;
    cout << "Usage: " << name << " [OPTIONS] mixture" << endl;
    cout << "Compute equilibrium properties for mixture over a set of "
         << "temperatures and pressures using the Mutation++ library." << endl;
    cout << endl;
    cout << tab << "-h, --help          prints this help message" << endl;
    cout << tab << "-v, --verbose       toggles verbosity on" << endl;
    cout << tab << "-T                  temperature range in K \"T1:dT:T2\" or simply T" << endl;
    cout << tab << "-P                  pressure range in Pa \"P1:dP:P2\" or simply P" << endl;
    cout << tab << "-m                  list of mixture values to output (see below)" << endl;
    cout << tab << "-s                  list of species values to output (see below)" << endl;
    cout << tab << "    --species_list  instead of mixture name, use this to list species in mixture" << endl;
    //cout << tab << "-c             element fractions (ie: \"N:0.79,O:0.21\")" << endl;
    cout << endl;
    cout << "Mixture values (example format: \"1-3,7,9-11\"):" << endl;
    
    for (int i = 0; i < NMIXTURE; ++i)
        cout << tab << setw(2) << i << ": " << setw(7)
             << mixture_quantities[i].name << setw(12)
             << (mixture_quantities[i].units == "" ? "[-]" : 
                "[" + mixture_quantities[i].units + "]") 
             << mixture_quantities[i].description << endl;

    cout << endl;
    cout << "Species values (same format as mixture values):" << endl;
    
    for (int i = 0; i < NSPECIES; ++i)
        cout << tab << setw(2) << i << ": " << setw(7)
             << species_quantities[i].name << setw(12)
             << (species_quantities[i].units == "" ? "[-]" : 
                "[" + species_quantities[i].units + "]") 
             << species_quantities[i].description << endl;
    
    cout << endl;
    cout << "Example:" << endl;
    cout << tab << name << " -T 300:100:15000 -P 101325 -m 1-3,8 air11" << endl;
    cout << endl;
    
    exit(0);
}

// Parses a temperature or pressure range
bool parseRange(const std::string& range, double& x1, double& x2, double& dx)
{
    std::vector<std::string> tokens;
    utils::StringUtils::tokenize(range, tokens, ":");
    
    if (!isNumeric(tokens))
        return false;
    
    switch (tokens.size()) {
        case 1:
            x1 = atof(tokens[0].c_str());
            x2 = x1;
            dx = 1.0;
            break;
        case 3:
            x1 = atof(tokens[0].c_str());
            x2 = atof(tokens[2].c_str());
            dx = atof(tokens[1].c_str());
            break;
        default:
            return false;
    }
    
    if (dx == 0.0) {
        x2 = x1;
        dx = 1.0;
    }
    
    return true;       
}

// Parses an index list of the form 1-3,7,9 for example
bool parseIndices(const std::string& list, std::vector<int>& indices, int max)
{
    indices.clear();
    
    std::vector<std::string> ranges;
    std::vector<std::string> bounds;
    utils::StringUtils::tokenize(list, ranges, ",");
    
    std::vector<std::string>::const_iterator iter = ranges.begin();
    for ( ; iter != ranges.end(); ++iter) {
        bounds.clear();
        utils::StringUtils::tokenize(*iter, bounds, "-");
        
        if (!isNumeric(bounds))
            return false;
        
        switch (bounds.size()) {
            case 1: {
                int i = atoi(bounds[0].c_str());
                if (i < 0 || i > max)
                    return false;
                indices.push_back(atoi(bounds[0].c_str()));
                break;
            }
            case 2: {
                int i1 = atoi(bounds[0].c_str());
                int i2 = atoi(bounds[1].c_str());
                
                if (i1 >= i2 || i1 < 0 || i1 > max || i2 > max)
                    return false;
                    
                for (int i = i1; i <= i2; ++i)
                    indices.push_back(i);
                
                break;
            }
            default:
                return false;   
        }
    }
    
    return true;
}

// Parse the command line options to determine what the user wants to do
Options parseOptions(int argc, char** argv)
{
    Options opts;
    
    // Print the help message and exit if desired
    if (optionExists(argc, argv, "-h") || optionExists(argc, argv, "--help"))
        printHelpMessage(argv[0]);
    
    // Control verbosity
    opts.verbose = 
        optionExists(argc, argv, "-v") || optionExists(argc, argv, "--verbose");
    
    // The mixture name is given as the only argument (unless --species option
    // is present)
    if (optionExists(argc, argv, "--species_list")) {
        opts.p_mixture_opts = new MixtureOptions();
        std::vector<std::string> tokens;
        utils::StringUtils::tokenize(
            getOption(argc, argv, "--species_list"), tokens, ",");
        opts.p_mixture_opts->setSpeciesNames(tokens);
    } else {
        opts.p_mixture_opts = new MixtureOptions(argv[argc-1]);
    }
    
    // Get the temperature range
    if (optionExists(argc, argv, "-T")) {
        if (!parseRange(
            getOption(argc, argv, "-T"), opts.T1, opts.T2, opts.dT)) {
            cout << "Bad format for temperature range!" << endl;
            printHelpMessage(argv[0]);
        }      
    } else {
        opts.T1 = 300.0;
        opts.T2 = 20000.0;
        opts.dT = 100.0;
    }
    
    // Get the pressure range
    if (optionExists(argc, argv, "-P")) {
        if (!parseRange(
            getOption(argc, argv, "-P"), opts.P1, opts.P2, opts.dP)) {
            cout << "Bad format for pressure range!" << endl;
            printHelpMessage(argv[0]);
        }
    } else {
        opts.P1 = ONEATM;
        opts.P2 = ONEATM;
        opts.dP = ONEATM;
    }
    
    // Get the mixture properties to print
    if (optionExists(argc, argv, "-m")) {
        if (!parseIndices(
            getOption(argc, argv, "-m"), opts.mixture_indices, NMIXTURE-1)) {
            cout << "Bad format for mixture value indices!" << endl;
            printHelpMessage(argv[0]);
        }
    }
    
    // Get the species properties to print
    if (optionExists(argc, argv, "-s")) {
        if (!parseIndices(
            getOption(argc, argv, "-s"), opts.species_indices, NSPECIES-1)) {
            cout << "Bad format for species value indices!" << endl;
            printHelpMessage(argv[0]);
        }
    }
    
    // If 
    
    return opts;
}

// Write out the column headers
void writeHeader(
    const Options& opts, const Mixture& mix, std::vector<int>& column_widths)
{
    std::string name;
    
    std::vector<int>::const_iterator iter = opts.mixture_indices.begin();
    for ( ; iter != opts.mixture_indices.end(); ++iter) {
        name = mixture_quantities[*iter].name +
            (mixture_quantities[*iter].units == "" ? 
                "" : "[" + mixture_quantities[*iter].units + "]");
        column_widths.push_back(
            std::max(COLUMN_WIDTH, static_cast<int>(name.length())+2));
        cout << setw(column_widths.back()) << name;
    }
    
    iter = opts.species_indices.begin();
    for ( ; iter != opts.species_indices.end(); ++iter) {
        for (int i = 0; i < mix.nSpecies(); ++i) {
            name = species_quantities[*iter].name + "_" + mix.speciesName(i) +
                (species_quantities[*iter].units == "" ? 
                    "" : "[" + species_quantities[*iter].units + "]");
            column_widths.push_back(
                std::max(COLUMN_WIDTH, static_cast<int>(name.length())+2));
            cout << setw(column_widths.back()) << name;
        }
    }
    
    cout << endl;
}

int main(int argc, char** argv)
{
    // Parse the command line options and load the mixture
    Options opts = parseOptions(argc, argv);
    Mixture mix(*opts.p_mixture_opts);
    
    // Write out the column headers (and compute the column sizes)
    std::vector<int> column_widths;
    writeHeader(opts, mix, column_widths);
    
    // Now we can actually perform the computations
    std::vector<int>::const_iterator iter;
    double value;
    int cw;
    std::string name, units;
    
    double* species_values = new double [mix.nSpecies()];
    double* temp           = new double [mix.nSpecies()];
    double* sjac_exact     = new double [mix.nSpecies()*mix.nSpecies()];
    double* sjac_fd        = new double [mix.nSpecies()*mix.nSpecies()];
    double* temp2          = new double [mix.nSpecies()];
    
    ofstream jac_file("jac.dat");
    
    for (int i = 0; i < mix.nSpecies(); ++i)
        jac_file << setw(15) << mix.speciesName(i);
    jac_file << endl;
    
    for (double P = opts.P1; P <= opts.P2; P += opts.dP) {
        for (double T = opts.T1; T <= opts.T2; T += opts.dT) {
            // Compute the equilibrium composition
            mix.equilibrate(T, P);
            cw = 0;
        
            // Mixture properties
            iter = opts.mixture_indices.begin(); 
            for ( ; iter < opts.mixture_indices.end(); ++iter) {
                name  = mixture_quantities[*iter].name;
                units = mixture_quantities[*iter].units;
                
                if (name == "Th")
                    value = mix.T();
                else if (name == "P")
                    value = mix.P();
                else if (name == "rho")
                    value = mix.density();
                else if (name == "nd")
                    value = mix.numberDensity();
                else if (name == "Mw")
                    value = mix.mixtureMw();
                else if (name == "H") {
                    if (units == "J/mol")
                        value = mix.mixtureHMole();
                    else if (units == "J/kg")
                        value = mix.mixtureHMass();
                } else if (name == "S") {
                    if (units == "J/mol-K")
                        value = mix.mixtureSMole();
                    else if (units == "J/kg-K")
                        value = mix.mixtureSMass();
                } else if (name == "Cp") {
                    if (units == "J/mol-K")
                        value = mix.mixtureFrozenCpMole();
                    else if (units == "J/kg-K")
                        value = mix.mixtureFrozenCpMass();
                } else if (name == "Cp_eq") {
                    if (units == "J/mol-K")
                        value = mix.mixtureEquilibriumCpMole();
                    else if (units == "J/kg-K")
                        value = mix.mixtureEquilibriumCpMass();
                } else if (name == "Cv") {
                    if (units == "J/mol-K")
                        value = mix.mixtureFrozenCvMole();
                    else if (units == "J/kg-K")
                        value = mix.mixtureFrozenCvMass();
                } else if (name == "Cv_eq") {
                    if (units == "J/mol-K")
                        value = mix.mixtureEquilibriumCvMole();
                    else if (units == "J/kg-K")
                        value = mix.mixtureEquilibriumCvMass();
                } else if (name == "gam_eq")
                    value = mix.mixtureEquilibriumGamma();
                else if (name == "gamma")
                    value = mix.mixtureFrozenGamma();
                else if (name == "mu")
                    value = mix.eta();
                else if (name == "labmda")
                    value = mix.lambda();
                else if (name == "Ht") {
                    mix.speciesHOverRT(temp, species_values);
                    value = 0.0;
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        value += species_values[i] * RU * T * mix.X()[i];
                    if (units == "J/kg")
                        value /= mix.mixtureMw();
                } else if (name == "Hr") {
                    mix.speciesHOverRT(temp, NULL, species_values);
                    value = 0.0;
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        value += species_values[i] * RU * T * mix.X()[i];
                    if (units == "J/kg")
                        value /= mix.mixtureMw();
                } else if (name == "Hv") {
                    mix.speciesHOverRT(temp, NULL, NULL, species_values);
                    value = 0.0;
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        value += species_values[i] * RU * T * mix.X()[i];
                    if (units == "J/kg")
                        value /= mix.mixtureMw();
                } else if (name == "Hel") {
                    mix.speciesHOverRT(temp, NULL, NULL, NULL, species_values);
                    value = 0.0;
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        value += species_values[i] * RU * T * mix.X()[i];
                    if (units == "J/kg")
                        value /= mix.mixtureMw();
                } else if (name == "Hf") {
                    mix.speciesHOverRT(
                        temp, NULL, NULL, NULL, NULL, species_values);
                    value = 0.0;
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        value += species_values[i] * RU * T * mix.X()[i];
                    if (units == "J/kg")
                        value /= mix.mixtureMw();
                }
                
                cout << setw(column_widths[cw++]) << value;
            }
            
            // Species properties
            iter = opts.species_indices.begin();
            for ( ; iter < opts.species_indices.end(); ++iter) {
                name  = species_quantities[*iter].name;
                units = species_quantities[*iter].units;
                
                if (name == "X")
                    std::copy(mix.X(), mix.X()+mix.nSpecies(), species_values);
                else if (name == "Y")
                    mix.convert<X_TO_Y>(mix.X(), species_values);
                else if (name == "rho") {
                    mix.convert<X_TO_Y>(mix.X(), species_values);
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        species_values[i] *= mix.density();
                } else if (name == "conc") {
                    double conc = mix.density() / mix.mixtureMw();
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        species_values[i] = mix.X()[i] * conc;
                } else if (name == "Cp") {
                    mix.speciesCpOverR(species_values);
                    if (units == "J/mol-K")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= RU;
                    else if (units == "J/kg-K")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= (RU / mix.speciesMw(i));                        
                } else if (name == "H") {
                    mix.speciesHOverRT(species_values);
                    if (units == "J/mol")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= (RU * T);
                    else if (units == "J/kg")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= (RU * T / mix.speciesMw(i));                        
                } else if (name == "S") {
                    mix.speciesSOverR(species_values);
                    if (units == "J/mol-K")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= RU;
                    else if (units == "J/kg-K")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= (RU / mix.speciesMw(i));                        
                } else if (name == "G") {
                    mix.speciesGOverRT(species_values);
                    if (units == "J/mol")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= (RU * T);
                    else if (units == "J/kg")
                        for (int i = 0; i < mix.nSpecies(); ++i)
                            species_values[i] *= (RU * T / mix.speciesMw(i));
                } else if (name == "omega") {
                    double conc = mix.density() / mix.mixtureMw();
                    for (int i = 0; i < mix.nSpecies(); ++i)
                        temp[i] = mix.X()[i] * conc;
                    mix.netProductionRates(mix.T(), temp, species_values);
                }
                
                for (int i = 0; i < mix.nSpecies(); ++i)
                    cout << setw(column_widths[cw++]) << species_values[i];
            }
            
            cout << endl;
            
            // Compute the exact jacobian
            /*double conc = mix.density() / mix.mixtureMw();
            for (int i = 0; i < mix.nSpecies(); ++i)
                temp[i] = mix.X()[i] * conc;
            mix.jacobianRho(mix.T(), temp, sjac_exact);
            
            // Now compute the finite-differenced jacobian
            double eps = std::sqrt(Numerics::NumConst<double>::eps);
            double delta, prev;
            mix.netProductionRates(mix.T(), temp, species_values);
            for (int j = 0; j < mix.nSpecies(); ++j) {
                delta = std::max(temp[j] * eps, 1.0e-100);
                prev = temp[j];
                temp[j] += delta;
                mix.netProductionRates(mix.T(), temp, temp2);
                temp[j] = prev;
                for (int i = 0; i < mix.nSpecies(); ++i)
                    sjac_fd[i*mix.nSpecies()+j] = (temp2[i]-species_values[i])/
                        (delta*mix.speciesMw(j));
            }
            
            double sum1 = 0.0;
            double sum2 = 0.0;
            double x;
            for (int i = 0; i < mix.nSpecies(); ++i) {
                for (int j = 0; j < mix.nSpecies(); ++j) {
                    jac_file << setw(15) 
                             //<< (sjac_fd[i*mix.nSpecies()+j] - 
                             //       sjac_exact[i*mix.nSpecies()+j]) / 
                             //       sjac_exact[i*mix.nSpecies()+j];
                             << sjac_exact[i*mix.nSpecies()+j];
                
                    // Compute the Frobenius norm of the difference matrix
                    //x = sjac_exact[i*mix.nSpecies()+j] - sjac_fd[i*mix.nSpecies()+j];
                    //sum1 += x*x;
                    //x = sjac_exact[i*mix.nSpecies()+j];
                    //sum2 += x*x;
                }
                jac_file << endl;
            }
            //jac_file << setw(10) << mix.T() << setw(15) << std::sqrt(sum1/sum2);
            jac_file << endl;*/
        }
    }
    
    // Clean up storage
    delete [] species_values;
    delete [] temp;
    delete [] sjac_exact;
    delete [] sjac_fd;
    delete [] temp2;

    return 0;
}