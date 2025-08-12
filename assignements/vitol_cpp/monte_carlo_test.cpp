#include "types.hpp"
#include <vector>
#include <random>
#include <thread>
#include <algorithm>
#include <execution>
#include <ranges>
#include <span>
#include <numeric>
#include <iostream>

#include "gnuplot_plotter.hpp"  // Fichier que vous venez de créer

class MonteCarloEngine {
private:
    
    // Générateur principal (Mersenne Twister 64-bit)
    std::mt19937_64 rng_;  
    
    // Un générateur par thread
    mutable std::vector<std::mt19937_64> thread_rngs_; 
    
public:
 
    // Constructeur : initialise les générateurs aléatoires
    explicit MonteCarloEngine(uint64_t seed = std::random_device{}()) 
        : rng_(seed) {
        
        // Détecte le nombre de cœurs/threads disponibles sur le CPU
        const auto num_threads = std::thread::hardware_concurrency();
        // Pré-alloue la mémoire
        thread_rngs_.reserve(num_threads); 

        // Crée un générateur indépendant pour chaque thread
        for (unsigned i = 0; i < num_threads; ++i) {
            thread_rngs_.emplace_back(seed + i);
        }
    }
    
    /*
     * SIMULATION DE TRAJECTOIRES GÉOMÉTRIQUES BROWNIENNE (GBM)
     * ========================================================
     * Simule l'évolution du prix d'un actif dans le temps selon le modèle :
     * dS = μ×S×dt + σ×S×dW
     * où dW est un mouvement brownien (bruit aléatoire)
     */
    template<RandomAccessRange Container>
    void simulate_gbm_paths(
        Container& paths, 
        double S0, 
        double mu, 
        double sigma, 
        double T, 
        size_t n_steps, // serie length in time
        size_t n_paths // number of paths to simulate
    ) const {
        
        const double dt = T / n_steps;  // Pas de temps (ex: 1/252 = 1 jour)
        const double drift = (mu - 0.5 * sigma * sigma) * dt;  // Dérive ajustée d'Itô : mu - σ²/2 
        const double vol_sqrt_dt = sigma * std::sqrt(dt);  // Terme de volatilité : σ√dt

        // boucle sur chaque trajectoire
        for (size_t path_idx = 0; path_idx < n_paths; ++path_idx) {

            // initialise une copie du thread_local pour éviter les conflits
            thread_local std::normal_distribution<double> normal{0.0, 1.0};
            
            // Choisit le bon générateur pour ce thread
            auto& thread_rng = thread_rngs_[path_idx % thread_rngs_.size()];
            
            // Initialise la trajectoire avec le prix initial
            paths[path_idx * (n_steps + 1)] = S0;  

            // boucle sur chaque pas de temps de cette trajectoire
            for (size_t step = 1; step <= n_steps; ++step) {
                // Génération d'un choc aléatoire : N(0,1)
                const double dW = normal(thread_rng);  

                // Calcul des indices dans le tableau : position actuelle et précédente
                const size_t idx = path_idx * (n_steps + 1) + step; 
                const size_t prev_idx = path_idx * (n_steps + 1) + step - 1; 
                
                // Formule du modèle géométrique brownien
                // - paths[prev_idx] : prix à l'étape précédente
                // - drift : tendance déterministe
                // - vol_sqrt_dt * dW : choc aléatoire
                // - exp() : garantit que le prix reste positif
                paths[idx] = paths[prev_idx] * std::exp(drift + vol_sqrt_dt * dW);
                /*

                 * EXEMPLE CONCRET :
                 * Si pétrole = 75$, drift = 0.0002, dW = 0.5
                 * → Nouveau prix = 75 × exp(0.0002 + 0.35×√(1/252)×0.5)
                 *                ≈ 75 × exp(0.0113) ≈ 75.85$
                 */
            }
        }
    }
    
    /*
     * VERSION OPTIMISÉE POUR VaR QUOTIDIENNE
     * =======================================
     * Simule seulement les rendements sur 1 jour (pas besoin de trajectoires complètes)
     */
    void simulate_single_step_returns(
        std::span<double> returns, 
        double mu, 
        double sigma, 
        double dt
    ) const {
        /*
         * PARAMÈTRES :
         * - returns : tableau pour stocker les rendements simulés
         * - mu : dérive annualisée
         * - sigma : volatilité annualisée  
         * - dt : période (ex: 1/252 pour 1 jour)
         * 
         * FORMULE DU RENDEMENT :
         * R = S(t+dt)/S(t) - 1 = exp(drift + vol×√dt×dW) - 1
         */
        
        // Pré-calculs 
        const double drift = (mu - 0.5 * sigma * sigma) * dt;
        const double vol_sqrt_dt = sigma * std::sqrt(dt);
        
        /*
         * SIMULATION DIRECTE DES RENDEMENTS
         * ==================================
         * Plus rapide car on évite de stocker toute la trajectoire
         */
        for (size_t i = 0; i < returns.size(); ++i) {
            thread_local std::normal_distribution<double> normal{0.0, 1.0};
            auto& thread_rng = thread_rngs_[i % thread_rngs_.size()];
            
            const double dW = normal(thread_rng);  // Choc aléatoire
            returns[i] = std::exp(drift + vol_sqrt_dt * dW) - 1.0;
        }
    }
    
    /*
     * CALCUL DE VAR ET EXPECTED SHORTFALL
     * ====================================
     * VaR = "Value at Risk" = perte maximale probable à un niveau de confiance donné
     * ES = "Expected Shortfall" = perte moyenne au-delà du VaR
     */
    template<RandomAccessRange Container>
    [[nodiscard]] std::pair<double, double> calculate_var_es(
        const Container& returns, 
        double confidence
    ) const {
        /*
         * PARAMÈTRES :
         * - returns : tableau des rendements simulés
         * - confidence : niveau de confiance (ex: 0.95 = 95%)
         * 
         * RETOUR :
         * - pair<VaR, ES> : les deux mesures de risque
         */
        
        // Copie et tri des rendements pour trouver le VaR
        std::vector<double> sorted_returns(returns.begin(), returns.end());
        std::sort(sorted_returns.begin(), sorted_returns.end());

        // Calcul de l'index du VaR
        // => logique : si 95% de confiance, on prend les 5% pires pertes (ex: 100 => 5ème pire)
        const size_t var_index = static_cast<size_t>((1.0 - confidence) * sorted_returns.size());
        
        // Vérification de sécurité
        if (var_index >= sorted_returns.size()) return {0.0, 0.0};
        
        // Calcul de la VaR (i.e. retours élément à la position var_index)
        const double var = -sorted_returns[var_index];
        
        // ES = moyenne des pires pertes (jusqu'à var_index)
        // => logique : somme des pires pertes (accumulate) / nombre de pertes (var_index)
        const double es = var_index > 0 
            ? -std::accumulate(sorted_returns.begin(), 
                              sorted_returns.begin() + var_index, 0.0) / var_index
            : 0.0;
        
        return {var, es};
    }


    /*
    * FONCTION get_returns CORRIGÉE
    * =============================
    */
    [[nodiscard]] std::vector<double> get_returns(
        const std::vector<double>& paths, 
        size_t k, 
        size_t l,
        size_t n_steps  // AJOUT : besoin de connaître la structure
    ) const {
        
        const size_t path_length = n_steps + 1;
        const size_t n_paths = paths.size() / path_length;
        
        std::vector<double> returns;
        returns.reserve(n_paths);
        
        for (size_t path_idx = 0; path_idx < n_paths; ++path_idx) {
            // Calcul des indices corrects
            const size_t base_idx = path_idx * path_length;
            const size_t price_k_idx = base_idx + k;  // Prix à l'étape k
            const size_t price_l_idx = base_idx + l;  // Prix à l'étape l
            
            // Vérification de sécurité
            if (price_l_idx >= paths.size() || l > n_steps) {
                continue;
            }
            
            const double price_k = paths[price_k_idx];
            const double price_l = paths[price_l_idx];
            
            // ✅ FORMULE CORRECTE : (Prix_final - Prix_initial) / Prix_initial
            const double return_val = (price_l - price_k) / price_k;
            returns.push_back(return_val);
        }
        
        return returns;
    }


    /*
     * CALCUL VaR/ES POUR PLUSIEURS NIVEAUX DE CONFIANCE
     * ==================================================
     * Plus efficace que d'appeler calculate_var_es() plusieurs fois
     */
    [[nodiscard]] std::vector<std::pair<double, double>> calculate_var_es_batch(
        const std::vector<double>& returns, 
        std::span<const double> confidence_levels) const {
        
        // trier une seule fois
        std::vector<double> sorted_returns(returns);
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        // Pré-allocation du résultat
        std::vector<std::pair<double, double>> results;
        results.reserve(confidence_levels.size());
        
        // Calcul pour chaque niveau de confiance
        for (double confidence : confidence_levels) {
            const size_t var_index = static_cast<size_t>((1.0 - confidence) * sorted_returns.size());
            
            // Vérification de sécurité
            if (var_index >= sorted_returns.size()) {
                results.emplace_back(0.0, 0.0);
                continue;
            }
            
            // Calculs VaR et ES (même logique qu'avant)
            const double var = -sorted_returns[var_index];
            const double es = var_index > 0 
                ? -std::accumulate(sorted_returns.begin(), 
                                  sorted_returns.begin() + var_index, 0.0) / var_index
                : 0.0;

            // crée une paire et l'ajoute au résultat
            results.emplace_back(var, es);
        }
        
        return results;
    }
    
   
    // Obtenir le nombre de générateurs (utile pour diagnostics)
    [[nodiscard]] size_t get_thread_count() const noexcept {
        return thread_rngs_.size();
    }
};
// CORRECTIONS PRINCIPALES DANS LE MAIN()

int main() {
    int seed = 123;

    std::cout << "Monte Carlo Engine Test" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Using seed: " << seed << std::endl;
    std::cout << "Number of threads: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "Creating MonteCarloEngine..." << std::endl;

    const size_t n_steps = 252;
    const size_t n_paths = 1000000;

    const double T = 1.0;
    const double mu = 0.05;
    const double sigma = 0.2;
    const double s_0 = 100.0;
    
    std::cout << "Simulation parameters:" << std::endl;
    std::cout << "Dérive annuelle (mu) : " << mu * 100 << "%" << std::endl;
    std::cout << "Volatilité annuelle (sigma) : " << sigma * 100 << "%" << std::endl;
    std::cout << "Horizon de simulation : " << T << " an(s)" << std::endl;
    std::cout << "Nombre de pas de temps : " << n_steps << std::endl;
    std::cout << "Prix initial (S0) : " << s_0 << std::endl;

    std::cout << "Simulating " << n_paths << " paths with " << n_steps << " steps each..." << std::endl;

    MonteCarloEngine engine(seed);
    std::vector<double> paths(n_paths * (n_steps + 1));
    engine.simulate_gbm_paths(paths, s_0, mu, sigma, T, n_steps, n_paths);

    // CORRECTION 1: Utiliser un tuple à 3 éléments au lieu de 4
    std::vector<double> confidence_levels({0.995});
    std::cout << "Calculating VaR and ES for confidence levels: ";
    for (const auto& c : confidence_levels) {
        std::cout << c * 100 << "% ";
    }
    std::cout << std::endl;

    // CORRECTION 2: Tuple à 3 éléments : (horizon, var, es)
    std::vector<std::tuple<int, double, double>> var_by_horizon;

    const size_t k = 0;

    // Calcul VaR/ES pour chaque horizon
    for (size_t l = 1; l < n_steps; ++l) {
        const auto step_returns = engine.get_returns(paths, k, l, n_steps);
        const auto step_var_es_results = engine.calculate_var_es_batch(step_returns, confidence_levels);

        // CORRECTION 3: Stocker seulement 3 valeurs par tuple
        for (size_t i = 0; i < confidence_levels.size(); ++i) {
            var_by_horizon.emplace_back(
                static_cast<int>(l),               // horizon (int)
                step_var_es_results[i].first,      // var (double)
                step_var_es_results[i].second      // es (double)
            );
        }
        // print des informations de progression
        if (l % 10 == 0) {
            std::cout << "Progress: " << l << "/" << n_steps << " horizons processed." << std::endl;
        }
    }

    // CORRECTION 4: Structured binding avec 3 variables
    std::cout << "\nVaR/ES Results:\n";
    for (const auto& [l, var, es] : var_by_horizon) {
        std::cout << "  Step " << l << ": VaR = " << std::fixed << std::setprecision(4) 
                  << var * 100 << "%, ES = " << es * 100 << "%" << std::endl;
    }

    std::cout << "\nMonte Carlo simulation completed successfully." << std::endl;
    std::cout << "Number of threads used: " << engine.get_thread_count() << std::endl;

    // CORRECTION 5: Plotting avec données correctes
    GnuplotPlotter plotter("./plots/");
    
    // Extraire les données pour le plot
    std::vector<double> horizons_double;
    std::vector<double> vars_percent;
    std::vector<double> es_percent;
    
    for (const auto& [l, var, es] : var_by_horizon) {
        horizons_double.push_back(static_cast<double>(l));
        vars_percent.push_back(var * 100);  // Convertir en pourcentage
        es_percent.push_back(es * 100);      // Convertir en pourcentage
    }
    
    plotter.plot_timeseries(
        vars_percent,
        "var_by_horizon_corrected", 
        "VaR 99.5% by Investment Horizon",
        "VaR 99.5% (%)",
        "Horizon (days)"
    );

    plotter.plot_timeseries(
        es_percent,
        "es_by_horizon_corrected", 
        "ES 99.5% by Investment Horizon",
        "ES 99.5% (%)",
        "Horizon (days)"
    );

    // BONUS: Vérification de la tendance théorique
    std::cout << "\n=== VÉRIFICATION THÉORIQUE ===\n";
    if (vars_percent.size() >= 5) {
        double var_1d = vars_percent[0];
        double var_5d = vars_percent[4];
        double theoretical_var_5d = var_1d * std::sqrt(5.0);
        
        std::cout << "VaR 1 jour: " << var_1d << "%" << std::endl;
        std::cout << "VaR 5 jours: " << var_5d << "%" << std::endl;
        std::cout << "VaR 5j théorique (√5 scaling): " << theoretical_var_5d << "%" << std::endl;
        std::cout << "Ratio réel/théorique: " << var_5d / theoretical_var_5d << " (devrait être ≈ 1)" << std::endl;
    }

    return 0;
}