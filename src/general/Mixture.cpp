
#include "Mixture.h"
#include "utilities.h"
#include "XMLite.h"

#include <string>

using namespace std;
using namespace utils;

MixtureOptions::MixtureOptions()
    : m_composition_setter(m_default_composition),
      m_has_default_composition(false)
{ 
    setDefaultOptions();
}

MixtureOptions::MixtureOptions(const string& mixture)
    : m_composition_setter(m_default_composition),
      m_has_default_composition(false)
{
    loadFromFile(mixture);
}

MixtureOptions::MixtureOptions(const char* mixture)
    : m_composition_setter(m_default_composition),
      m_has_default_composition(false)
{
    loadFromFile(string(mixture));
}

void MixtureOptions::setDefaultOptions()
{
    m_species_names.clear();
    m_state_model = "T";
    m_thermo_db   = "RRHO";
    m_mechanism   = "none";
    m_viscosity   = "CG";
    m_thermal_conductivity = "CG";
}

void MixtureOptions::loadFromFile(const string& mixture)
{
    // Initalize to the default options
    setDefaultOptions();
    
    // Get the mixture path on this computer
    string mixture_path = 
        getEnvironmentVariable("MPP_DATA_DIRECTORY") + "/mixtures/" + 
        mixture + ".xml";
        
    // Now load the XML file
    XmlDocument mixture_doc(mixture_path);
    XmlElement root = mixture_doc.root();
    
    // Make sure this is a mixture element
    if (root.tag() != "mixture")
        root.parseError(
            "Root element in mixture file is not of 'mixture' type!");
    
    // Get the name of the mixture reaction mechanism
    root.getAttribute("mechanism", m_mechanism, m_mechanism);
    
    // Get the type of thermodynamic database to use
    root.getAttribute("thermo_db", m_thermo_db, m_thermo_db);
    
    // Get the viscosity algorithm
    root.getAttribute("viscosity", m_viscosity, m_viscosity);
    
    // Get the thermal conductivity algorithm
    root.getAttribute(
        "thermal_conductivity", m_thermal_conductivity, m_thermal_conductivity);
    
    // Get the state model
    root.getAttribute("state_model", m_state_model, m_state_model);
    
    // Loop over all of the mixture child elements
    for (XmlElement::Iterator iter = root.begin(); iter != root.end(); ++iter) {
        // Load the species list
        if (iter->tag() == "species") {
            StringUtils::tokenize(iter->text(), m_species_names, ", \n");
        
        // Load the default element fractions, note that we only check for valid
        // format, not for valid elements or fractions, this is left up to the
        // class that uses this information
        } else if (iter->tag() == "default_element_fractions") {
            vector<string> element_strings;
            StringUtils::tokenize(iter->text(), element_strings, ":, \n");
            
            if (element_strings.size() % 2 != 0) {
                iter->parseError(
                    "Default element fractions should have the format:\n" +
                    string("   name : fraction, name : fraction, ..."));
            }
            
            m_default_composition.clear();
            for (int i = 0; i < element_strings.size(); i+=2) {
                if (isNumeric(element_strings[i+1])) {            
                    m_default_composition.push_back(
                        make_pair(element_strings[i], 
                        atof(element_strings[i+1].c_str())));
                } else {
                    iter->parseError(
                        "Element fraction should be a real value!");
                }
            }
            
            m_has_default_composition = true;
        }
    }
}

