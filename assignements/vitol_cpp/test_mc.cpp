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
            // plus efficace que push_back(make_pair(var, es))
            results.emplace_back(var, es);
        }
        
        return results;
    }
    
   
    // Obtenir le nombre de générateurs (utile pour diagnostics)
    [[nodiscard]] size_t get_thread_count() const noexcept {
        return thread_rngs_.size();
    }
};

int main() {

    int seed = 123;

    std::cout << "Monte Carlo Engine Test" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Using seed: " << seed << std::endl;
    std::cout << "Number of threads: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "Creating MonteCarloEngine..." << std::endl;

    const size_t n_steps = 252;  // 1 an de trading (jours)
    const size_t n_paths = 10000; // Nombre de trajectoires à simuler

    std::cout << "Simulating " << n_paths << " paths with " << n_steps << " steps each..." << std::endl;

    MonteCarloEngine engine(seed);
    // Pré-alloue un tableau pour stocker les trajectoires
    std::vector<double> paths(n_paths * (n_steps + 1)); // Stocke les trajectoires
    // Simule des trajectoires GBM
    engine.simulate_gbm_paths(paths, 100.0, 0.05, 0.2, 1.0, n_steps, n_paths);

    // Affiche les premiers prix simulés pour la première trajectoire
    for (size_t i = 0; i <= n_steps* n_paths; i += n_steps + 1) {
        std::cout << "etape " << i / (n_steps + 1) << ": prix initial = " << paths[i] << " - prix final = " << paths[i + n_steps] << std::endl;
    }

    return 0;
}