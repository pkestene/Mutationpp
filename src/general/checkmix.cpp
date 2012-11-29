#include <iostream>
#include <iomanip>
#include <typeinfo>

using std::cout;
using std::endl;
using std::string;
using std::setw;

#include "mutation++.h"

int main(int argc, char** argv)
{
    if (argc <= 1) exit(1);
    
    /*cout << "Numerical constants:" << endl;
    cout << "pi:   " << PI << endl;
    cout << "2pi:  " << TWOPI << endl;
    cout << "NA:   " << NA << endl;
    cout << "KB:   " << KB << endl;
    cout << "RU:   " << RU << endl;
    cout << "MU0:  " << MU0 << endl;
    cout << "C0:   " << C0 << endl;
    cout << "EPS0: " << EPS0 << endl;
    cout << "QE:   " << QE << endl;
    cout << endl;*/
    
    std::string mix(argv[argc-1]);
    cout << "Loading mixture file " << mix << " ..." << endl;
    Mixture mixture(mix);
        
    const int ne = mixture.nElements();
    const int ns = mixture.nSpecies();
    const int nr = mixture.nReactions();
 
    cout << ns << " species containing " << ne << " elements" << endl;
    cout << nr << " reactions" << endl;
    cout << endl;
    
    cout << "Species info:" << endl;
    cout << "-------------" << endl;  
    cout << setw(9) << " ";
    for (int i = 0; i < ne; ++i)
        cout << setw(4) << mixture.elementName(i);
    
    cout << setw(12) << "Mw (g/mol)" << setw(10) << "Charge";
    cout << setw(12) << "Phase" << endl;
    for (int i = 0; i < ns; ++i) {
        cout.setf(std::ios::left, std::ios::adjustfield);
        cout << setw(9) << mixture.speciesName(i);
        
        cout.setf(std::ios::right, std::ios::adjustfield); 
        for (int j = 0; j < ne; ++j)
            cout << setw(4) << mixture.elementMatrix()(i,j);
        
        cout << setw(12) << mixture.speciesMw(i) * 1000.0
             << setw(10) << mixture.species()[i].charge();
        
        PhaseType phase = mixture.species()[i].phase();
        cout << setw(12) << (phase == GAS ? "gas" : 
            (phase == LIQUID ? "liquid" : "solid")) << endl;
    }
    
    cout << endl;
    
    cout << "Default elemental composition:" << endl;
    cout << "------------------------------" << endl;
    for (int i = 0; i < ne; ++i) {
        cout.setf(std::ios::left, std::ios::adjustfield);
        cout << "   " << setw(3) << mixture.elementName(i) << ": ";
        cout.setf(std::ios::right, std::ios::adjustfield);
        cout << setw(5) << mixture.getDefaultComposition(i) << endl;
    }
    
    cout << endl;
    
    if (nr == 0) return 0;
    
    cout << "Reaction info:" << endl;
    cout << "--------------" << endl;
    for (int i = 0; i < nr; ++i) {
        const Reaction& r = mixture.reactions()[i];
        
        // Print reaction formula and number
        cout.setf(std::ios::right, std::ios::adjustfield);
        cout << setw(4) << i+1 << ": ";        
        cout.setf(std::ios::left, std::ios::adjustfield);
        cout << setw(20) << r.formula();
        
        // Print out rate constants
        if (typeid(*(r.rateLaw())) == typeid(Arrhenius)) {
            const Arrhenius& rate = 
                dynamic_cast<const Arrhenius&>(*(r.rateLaw()));
            cout << setw(12) << "Arrhenius: ";
            
            cout.setf(std::ios::right, std::ios::adjustfield);
            cout.setf(std::ios::scientific, std::ios::floatfield);
            cout.precision(3);
            cout << setw(10) << rate.A();
            cout.setf(std::ios::fixed, std::ios::floatfield);
            cout.precision(2);
            cout << setw(7)  << rate.n();
            cout.precision(1);
            cout << setw(10) << rate.T();
        }
        
        cout << endl;
        
        // If this is a thirdbody reaction, print out thirdbody efficiency
        // factors
        if (!r.isThirdbody()) continue;
        
        cout << setw(6) << "";
        
        vector<pair<string, double> >::const_iterator iter =
            r.efficiencies().begin();
        
        cout.precision(2);
        if (iter != r.efficiencies().end())
            cout << iter->first << ": " << iter->second;
            
        for (iter++ ; iter != r.efficiencies().end(); ++iter) {
            cout << ", " << iter->first << ": " << iter->second;
        }        
        
        cout << endl;
    }
    
    cout << endl;
}