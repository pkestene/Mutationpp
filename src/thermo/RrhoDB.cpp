
#include "Constants.h"
#include "ThermoDB.h"
#include "Species.h"
#include "ParticleRRHO.h"
#include "AutoRegistration.h"
#include "Functors.h"

#include <iostream>
#include <cstdlib>

using namespace std;
using Numerics::Equals;
using Numerics::PlusEquals;
using Numerics::MinusEquals;
using Numerics::EqualsYDivAlpha;
using Numerics::PlusEqualsYDivAlpha;

// Simple for loop over the species just to make life a little easier
#define LOOP(__op__)\
for (int i = 0; i < m_ns; ++i) {\
    __op__ ;\
}

// Loops over molecules. Inside the loop, the index i is zero based and indexes
// internal molecular data.  The index j is the index corresponding to the 
// original species data.
#define LOOP_MOLECULES(__op__)\
for (int i = 0, j = 0; i < m_nm; ++i) {\
    j = mp_indices[m_na+i];\
    __op__ ;\
}

// Loops over heavy particles (non electron species).  Inside the loop, index i
// is zero based and indexes internal molecular data. The index j is the index
// corresponding to the original species data.
#define LOOP_HEAVY(__op__)\
for (int i = 0, j = 0; i < m_na + m_nm; ++i) {\
    j = (m_has_electron ? i+1 : i);\
    __op__ ;\
}

typedef struct {
    double ln_omega_t;  // ln(omega^(2 / L) * theta)
    double linearity;   // L / 2
} RotData;

typedef struct {
    double g;           // degeneracy
    double theta;       // characteristic temperature
} ElecLevel;


/**
 * A thermodynamic database that uses the Rigid-Rotator Harmonic-Oscillator
 * model for computing species thermodynamic properties.  See the individual 
 * thermodynamic functions for specific descriptions of the model.
 */
class RrhoDB : public ThermoDB
{
public:
    
    /**
     * Initializes the database from a list of species objects.  All species
     * should have been given a ParticleRRHO model, otherwise an error is 
     * presented to the user and the program exits.
     */
    RrhoDB(ThermoDB::ARGS species)
        : m_ns(species.size()), m_na(0), m_nm(0), 
          m_has_electron(species[0].type() == ELECTRON)
    {    
        // First make sure that every species contains an RRHO model
        bool complete = true;
        LOOP(complete &= species[i].hasRRHOParameters())
        
        if (!complete) {
            cout << "Error! not all species have RRHO thermodynamic models."
                 << " The following species need RRHO models..." << endl;
            
            LOOP(
                if (!species[i].hasRRHOParameters())
                    cout << "   " << species[i].name() << endl;
            )
            
            exit(1);
        }
        
        // Determine the number and indices of the atoms and molecules
        vector<int> atom_indices;
        vector<int> molecule_indices;
        
        LOOP(
            switch(species[i].type()) {
                case ATOM:
                    atom_indices.push_back(i);
                    break;
                case MOLECULE:
                    molecule_indices.push_back(i);
                    break;
            }
        )
        
        m_na = atom_indices.size();
        m_nm = molecule_indices.size();
        
        // Order the atoms first followed by the molecules
        mp_indices = new int [m_na + m_nm];
        copy(atom_indices.begin(), atom_indices.end(), mp_indices);
        copy(molecule_indices.begin(), molecule_indices.end(), mp_indices+m_na);
        
        // Store the species constants found in the translational entropy term
        mp_lnqtmw = new double [m_ns];
        double qt = 2.5*std::log(KB) + 1.5*std::log(TWOPI/(NA*HP*HP));
        LOOP(mp_lnqtmw[i] = qt + 1.5*std::log(species[i].molecularWeight()))
        
        // Store the species formation enthalpies in K
        mp_hform = new double [m_ns];
        LOOP(
            mp_hform[i] = 
                species[i].getRRHOParameters()->formationEnthalpy() / RU;
        )
        
        // Store the molecule's rotational energy parameters
        mp_rot_data = new RotData [m_nm];
        LOOP_MOLECULES(
            const ParticleRRHO* rrho = species[j].getRRHOParameters();
            int linear = rrho->linearity();            
            mp_rot_data[i].linearity  = linear / 2.0;
            mp_rot_data[i].ln_omega_t = 
                std::log(rrho->rotationalTemperature()) + 2.0 / linear * 
                std::log(rrho->stericFactor());
        )
        
        // Store the vibrational temperatures of all the molecules in a compact
        // form
        mp_nvib = new int [m_nm];
        int nvib = 0;
        LOOP_MOLECULES(
            mp_nvib[i] = species[j].getRRHOParameters()->nVibrationalLevels();
            nvib += mp_nvib[i];
        )
        
        mp_vib_temps = new double [nvib];
        int itemp = 0;
        LOOP_MOLECULES(
            const ParticleRRHO* rrho = species[j].getRRHOParameters();
            for (int k = 0; k < mp_nvib[i]; ++k, itemp++) {
                mp_vib_temps[itemp] = rrho->vibrationalEnergy(k);
            }
        )
        
        // Finally store the electronic energy levels in a compact form like the
        // vibrational energy levels
        mp_nelec = new int [m_na + m_nm];
        int nelec = 0;
        LOOP_HEAVY(
            mp_nelec[i] = species[j].getRRHOParameters()->nElectronicLevels();
            nelec += mp_nelec[i];
        )
        
        mp_elec_levels = new ElecLevel [nelec];
        int ilevel = 0;
        LOOP_HEAVY(
            const ParticleRRHO* rrho = species[j].getRRHOParameters();
            for (int k = 0; k < mp_nelec[i]; ++k, ilevel++) {
                mp_elec_levels[ilevel].g = rrho->electronicEnergy(k).first;
                mp_elec_levels[ilevel].theta = rrho->electronicEnergy(k).second;
            }
        )
    }
    
    /**
     * Destructor.
     */
    ~RrhoDB() 
    {
        delete [] mp_lnqtmw;
        delete [] mp_hform;
        delete [] mp_indices;
        delete [] mp_rot_data;
        delete [] mp_nvib;
        delete [] mp_vib_temps;
        delete [] mp_nelec;
        delete [] mp_elec_levels;
    }
    
    /**
     * Returns the standard state temperature in K.
     */
    double standardTemperature() const {
        return 298.15;
    }
    
    /**
     * Returns the standard state pressure in Pa.
     */
    double standardPressure() const {
        return 101325.0;
    }
    
    /**
     * Computes the unitless species specific heat at constant pressure
     * \f$ C_{P,i} / R_U\f$ in thermal nonequilibrium.
     *
     * @param Th  - heavy particle translational temperature
     * @param Te  - free electron temperature
     * @param Tr  - mixture rotational temperature
     * @param Tv  - mixture vibrational temperature
     * @param Tel - mixture electronic temperature
     * @param cp   - on return, the array of species non-dimensional \f$C_P\f$'s
     * @param cpt  - if not NULL, the array of species translational \f$C_P\f$'s
     * @param cpr  - if not NULL, the array of species rotational \f$C_P\f$'s
     * @param cpv  - if not NULL, the array of species vibrational \f$C_P\f$'s
     * @param cpel - if not NULL, the array of species electronic \f$C_P\f$'s
     *
     * @todo Compute \f$C_P\f$ directly instead of using finite-differencs.
     */
    void cp(
        double Th, double Te, double Tr, double Tv, double Tel, 
        double* const cp, double* const cpt, double* const cpr, 
        double* const cpv, double* const cpel)
    {
        double deltaT = Th * 1.0E-6;
    
        // Special case if we only want total Cp
        if (cpt == NULL && cpr == NULL && cpv == NULL && cpel == NULL) {
            hT(Th + deltaT, Te + deltaT, cp, Eq());
            hT(Th, Te, cp, MinusEq());
            
            hR(Tr + deltaT, cp, PlusEq());
            hR(Tr, cp, MinusEq());
            
            hV(Tv + deltaT, cp, PlusEq());
            hV(Tv, cp, MinusEq());
            
            hE(Tv + deltaT, cp, PlusEq());
            hE(Tv, cp, MinusEq());
            
            LOOP(cp[i] /= deltaT);
            return;
        }
        
        // Otherwise we have to compute each component directly.
        // Translation
        if (cpt == NULL) {
            hT(Th + deltaT, Te + deltaT, cp, Eq());
            hT(Th, Te, cp, MinusEq());
        } else {
            hT(Th + deltaT, Te + deltaT, cpt, Eq());
            hT(Th, Te, cpt, MinusEq());
            LOOP(cp[i] += cpt[i]);
            LOOP(cpt[i] /= deltaT);
        }
        
        // Rotation
        if (cpr == NULL) {
            hR(Tr + deltaT, cp, PlusEq());
            hR(Tr, cp, MinusEq());
        } else {
            LOOP(cpr[i] = 0.0);
            hR(Tr + deltaT, cpr, Eq());
            hR(Tr, cpr, MinusEq());
            LOOP_MOLECULES(cp[j] += cpr[j]);
            LOOP_MOLECULES(cpr[j] /= deltaT);
        }
        
        // Vibration
        if (cpv == NULL) {
            hV(Tv + deltaT, cp, PlusEq());
            hV(Tv, cp, MinusEq());
        } else {
            LOOP(cpv[i] = 0.0);
            hV(Tv + deltaT, cpv, Eq());
            hV(Tv, cpv, MinusEq());
            LOOP_MOLECULES(cp[j] += cpv[j]);
            LOOP_MOLECULES(cpv[j] /= deltaT);
        }
        
        // Electronic
        if (cpel == NULL) {
            hE(Tel + deltaT, cp, PlusEq());
            hE(Tel, cp, MinusEq());
        } else {
            LOOP(cpel[i] = 0.0);
            hE(Tr + deltaT, cpel, Eq());
            hE(Tr, cpel, MinusEq());
            LOOP(cp[i] += cpel[i]);
            LOOP(cpel[i] /= deltaT);
        }
        
        LOOP(cp[i] /= deltaT);
    }
    
    /**
     * Computes the unitless species enthalpy \f$ h_i/R_U T_h\f$ of each
     * species in thermal nonequilibrium, which is non-dimensionalized by the 
     * heavy particle translational temperature. 
     *
     * @param Th  - heavy particle translational temperature
     * @param Te  - free electron temperature
     * @param Tr  - mixture rotational temperature
     * @param Tv  - mixture vibrational temperature
     * @param Tel - mixture electronic temperature
     * @param h   - on return, the array of species non-dimensional enthalpies
     * @param ht  - if not NULL, the array of species translational enthalpies
     * @param hr  - if not NULL, the array of species rotational enthalpies
     * @param hv  - if not NULL, the array of species vibrational enthalpies
     * @param hel - if not NULL, the array of species electronic enthalpies
     * @param hf  - if not NULL, the array of the species formation enthalpies
     */
    void enthalpy(
        double Th, double Te, double Tr, double Tv, double Tel, double* const h,
        double* const ht, double* const hr, double* const hv, double* const hel,
        double* const hf)
    {
        // Special case where we only want the total enthalpy
        if (ht == NULL && hr == NULL && hv == NULL && hel == NULL && 
            hf == NULL) 
        {
            hT(Th, Te, h, Eq());
            hR(Tr, h, PlusEq());
            hV(Tv, h, PlusEq());
            hE(Tel, h, PlusEq());
            hF(h, PlusEq());
            LOOP(h[i] /= Th);
            return;
        }
        
        // Otherwise selectively choose what we want
        // Translational enthalpy
        if (ht == NULL)
            hT(Th, Te, h, Eq());
        else {
            hT(Th, Te, ht, EqDiv(Th));
            LOOP(h[i] = ht[i]);
        }
        
        // Rotatonal enthalpy
        if (hr == NULL)
            hR(Tr, h, PlusEqDiv(Th));
        else {
            LOOP(hr[i] = 0.0);
            hR(Tr, hr, EqDiv(Th));
            LOOP_MOLECULES(h[j] += hr[j]);
        }
        
        // Vibrational enthalpy
        if (hv == NULL)
            hV(Tv, h, PlusEqDiv(Th));
        else {
            LOOP(hv[i] = 0.0);
            hV(Tv, hv, EqDiv(Th));
            LOOP_MOLECULES(h[j] += hv[j]);
        }
            
        // Electronic enthalpy
        if (hel == NULL)
            hE(Tel, h, PlusEqDiv(Th));
        else {
            LOOP(hel[i] = 0.0);
            hE(Tel, hel, EqDiv(Th));
            LOOP(h[i] += hel[i]);
        }
        
        // Formation enthalpy
        if (hf == NULL)
            hF(h, PlusEqDiv(Th));
        else {
            hF(hf, EqDiv(Th));
            LOOP(h[i] += hf[i]);
        }
    }
    
    /**
     * Computes the unitless species entropy \f$s_i/R_u\f$ allowing for thermal 
     * nonequilibrium.
     *
     * @param Th  - heavy particle translational temperature
     * @param Te  - free electron temperature
     * @param Tr  - mixture rotational temperature
     * @param Tv  - mixture vibrational temperature
     * @param Tel - mixture electronic temperature
     * @param s   - on return, the array of species non-dimensional entropies
     * @param st  - if not NULL, the array of species translational entropies
     * @param sr  - if not NULL, the array of species rotational entropies
     * @param sv  - if not NULL, the array of species vibrational entropies
     * @param sel - if not NULL, the array of species electronic entropies
     */
    void entropy(
        double Th, double Te, double Tr, double Tv, double Tel, double P,
        double* const s, double* const st, double* const sr, double* const sv,
        double* const sel)
    {
        // Special case where we only want the total entropy
        if (st == NULL && sr == NULL && sv == NULL && sel == NULL) {
            sT(Th, Te, P, s, Eq());
            sR(Tr, s, PlusEq());
            sV(Tv, s, PlusEq());
            sE(Tel, s, PlusEq());
            
            // Include spin contribution for free electron entropy
            if (m_has_electron)
                s[0] += std::log(2.0);
            
            return;
        }
        
        // Otherwise collect individual components
        // Translational entropy
        if (st == NULL)
            sT(Th, Te, P, s, Eq());
        else {
            sT(Th, Te, P, st, Eq());
            LOOP(s[i] = st[i]);
        }
        
        // Rotational entropy
        if (sr == NULL)
            sR(Tr, s, PlusEq());
        else {
            LOOP(sr[i] = 0.0);
            sR(Tr, sr, Eq());
            LOOP_MOLECULES(s[j] += sr[j]);
        }
        
        // Vibrational entropy
        if (sv == NULL)
            sV(Tv, s, PlusEq());
        else {
            LOOP(sv[i] = 0.0);
            sV(Tv, sv, Eq());
            LOOP_MOLECULES(s[j] += sv[j]);
        }
        
        // Electronic entropy
        if (sel == NULL)
            sE(Tel, s, PlusEq());
        else {
            LOOP(sel[i] = 0.0);
            sE(Tel, sel, Eq());
            LOOP(s[i] += sel[i]);
        }
        
        // Include spin contribution for free electron entropy
        if (m_has_electron)
            s[0] += std::log(2.0);
    }
    
    /**
     * Computes the unitless Gibbs free energy of each species i, 
     * \f$G_i / R_u T_h\f$ where \f$G_i\f$ is non-dimensionalized by the heavy
     * particle translational temperature.
     *
     * @todo Compute the individual components of the Gibbs function directly
     * instead of H - TS.
     */
    void gibbs(
        double Th, double Te, double Tr, double Tv, double Tel, double P,
        double* const g, double* const gt, double* const gr, double* const gv,
        double* const gel)
    {
        //cout << Th << endl;
        //cout << Te << endl;
        //cout << Tr << endl;
        //cout << Tv << endl;
        //cout << Tel << endl;
        //cout << P << endl;
        //cout << endl;
        
        // First compute the non-dimensional enthalpy
        enthalpy(Th, Te, Tr, Tv, Tel, g, NULL, NULL, NULL, NULL, NULL);
        
        // Subtract the entropies
        sT(Th, Te, P, g, MinusEq());
        sR(Tr, g, MinusEq());
        sV(Tv, g, MinusEq());
        sE(Tel, g, MinusEq());
        
        // Account for spin of free electrons
        if (m_has_electron)
            g[0] -= std::log(2.0);
        
        //LOOP(cout << g[i] << endl;)
        //exit(1);   
    }

private:

    typedef Equals<double> Eq;
    typedef EqualsYDivAlpha<double> EqDiv;    
    typedef PlusEqualsYDivAlpha<double> PlusEqDiv;
    typedef PlusEquals<double> PlusEq;
    typedef MinusEquals<double> MinusEq;
    
    /**
     * Computes the translational enthalpy of each species in K.
     */
    template <typename OP>
    void hT(double T, double Te, double* const h, const OP& op) {
        if (m_has_electron)
            op(h[0], 2.5 * Te);
        LOOP_HEAVY(op(h[j], 2.5 * T))
    }
    
    /**
     * Computes the rotational enthalpy of each species in K.
     */
    template <typename OP>
    void hR(double T, double* const h, const OP& op) {
        LOOP_MOLECULES(op(h[j], mp_rot_data[i].linearity * T))
    }
    
    /**
     * Computes the vibrational enthalpy of each species in K.
     */
    template <typename OP>
    void hV(double T, double* const h, const OP& op) {
        int ilevel = 0;
        double sum;
        LOOP_MOLECULES(
            sum = 0.0;
            for (int k = 0; k < mp_nvib[i]; ++k, ilevel++)
                sum += mp_vib_temps[ilevel] / 
                    (std::exp(mp_vib_temps[ilevel] / T) - 1.0);
            op(h[j], sum);
        )
    }
    
    /**
     * Computes the electronic enthalpy of each species in K and applies the
     * value to the enthalpy array using the given operation.
     */
    template <typename OP>
    void hE(double T, double* const h, const OP& op) {
        double fac, sum1, sum2;
        int ilevel = 0;
        LOOP_HEAVY(
            sum1 = sum2 = 0.0;
            for (int k = 0; k < mp_nelec[i]; ++k, ilevel++) {
                fac = mp_elec_levels[ilevel].g * 
                    std::exp(-mp_elec_levels[ilevel].theta / T);
                sum1 += fac;
                sum2 += fac * mp_elec_levels[ilevel].theta;
            }
            op(h[j], sum2 / sum1);
        )
    }
    
    /**
     * Computes the formation enthalpy of each species in K.
     */
    template <typename OP>
    void hF(double* const h, const OP& op) {
        LOOP(op(h[i], mp_hform[i]))
    }
    
    /**
     * Computes the unitless translational entropy of each species.
     */
    template <typename OP>
    void sT(double Th, double Te, double P, double* const s, const OP& op) {
        double fac = 2.5 * (1.0 + std::log(Th)) - std::log(P);               
        if (m_has_electron)
            op(s[0], 2.5 * std::log(Te / Th) + fac + mp_lnqtmw[0]);        
        for (int i = (m_has_electron ? 1 : 0); i < m_ns; ++i)
            op(s[i], fac + mp_lnqtmw[i]);
    }
    
    /**
     * Computes the unitless rotational entropy of each species.
     */
    template <typename OP>
    void sR(double T, double* const s, const OP& op) {
        const double onelnT = 1.0 + std::log(T);
        LOOP_MOLECULES(
            op(s[j], mp_rot_data[i].linearity * (onelnT - 
                mp_rot_data[i].ln_omega_t));
        )
    }
    
    /**
     * Computes the unitless vibrational entropy of each species.
     */
    template <typename OP>
    void sV(double T, double* const s, const OP& op) {
        int ilevel = 0;
        double fac, sum1, sum2;
        LOOP_MOLECULES(
            sum1 = sum2 = 0.0;
            for (int k = 0; k < mp_nvib[i]; ++k, ilevel++) {
                fac  =  std::exp(mp_vib_temps[ilevel] / T);
                sum1 += mp_vib_temps[ilevel] / (fac - 1.0);
                sum2 += std::log(1.0 - 1.0 / fac);
            }
            op(s[j], (sum1 / T - sum2));
        )
    }
    
    /**
     * Computes the unitless electronic entropy of each species.
     */
    template <typename OP>
    void sE(double T, double* const s, const OP& op) {
        int ilevel = 0;
        double fac, sum1, sum2;
        LOOP_HEAVY(
            sum1 = 0.0;
            sum2 = 0.0;
            for (int k = 0; k < mp_nelec[i]; ++k, ilevel++) {
                fac = mp_elec_levels[ilevel].g * 
                    std::exp(-mp_elec_levels[ilevel].theta / T);
                sum1 += fac;
                sum2 += mp_elec_levels[ilevel].theta * fac;
            }
            op(s[j], (sum2 / (sum1 * T) + std::log(sum1)));
        )
    }

private:
    
    int m_ns;
    int m_na;
    int m_nm;
    
    bool m_has_electron;
    
    double* mp_lnqtmw;
    double* mp_hform;
    
    int*       mp_indices;
    RotData*   mp_rot_data;
    
    int*       mp_nvib;
    double*    mp_vib_temps;
    
    int*       mp_nelec;
    ElecLevel* mp_elec_levels;

}; // class RrhoDB

#undef LOOP
#undef LOOP_HEAVY
#undef LOOP_MOLECULES

// Register the RRHO model with the other thermodynamic databases
ObjectProvider<RrhoDB, ThermoDB> rrhoDB("RRHO");
