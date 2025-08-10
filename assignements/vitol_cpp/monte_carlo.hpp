/*
 * monte_carlo.hpp - Moteur de simulation Monte Carlo haute performance
 * 
 * Le Monte Carlo = technique pour estimer des probabilités en simulant des milliers
 * de scénarios aléatoires. C'est comme jouer à pile ou face 10,000 fois pour
 * estimer que la probabilité est ~50%.
 * 
 * En finance : on simule des milliers d'évolutions possibles du prix du pétrole
 * pour calculer le risque (VaR = Value at Risk).
 */

#pragma once

#include "types.hpp"    // Pour les concepts et types de base
#include <vector>       // Pour stocker les résultats de simulation
#include <random>       // Pour générer des nombres aléatoires
#include <thread>       // Pour détecter le nombre de cœurs CPU
#include <algorithm>    // Pour trier, transformer, etc.
#include <execution>    // Pour la parallélisation (pas utilisé ici)
#include <ranges>       // Pour manipuler des plages de données (C++20)
#include <span>         // Pour manipuler des tableaux de façon sûre (C++20)
#include <numeric>      // Pour accumulate, reduce, etc.

// ===== MOTEUR MONTE CARLO HAUTE PERFORMANCE =====
/*
 * Cette classe simule l'évolution aléatoire des prix d'actifs financiers
 * en utilisant le modèle mathématique "Geometric Brownian Motion" (GBM).
 */
class MonteCarloEngine {
private:
    /*
     * GÉNÉRATEURS DE NOMBRES ALÉATOIRES
     * ==================================
     * On a besoin de BEAUCOUP de nombres aléatoires de haute qualité
     */
    std::mt19937_64 rng_;  // Générateur principal (Mersenne Twister 64-bit)
    /*
     * mt19937_64 = générateur d'excellence pour la finance
     * - Période : 2^19937 - 1 (pratiquement infinie)
     * - Qualité statistique excellente
     * - Rapide pour les simulations intensives
     */
    
    mutable std::vector<std::mt19937_64> thread_rngs_;  // Un générateur par thread
    /*
     * mutable = peut être modifié même dans des fonctions const
     * vector<mt19937_64> = tableau de générateurs indépendants
     * 
     * Pourquoi plusieurs générateurs ?
     * - Éviter les conflits entre threads parallèles
     * - Chaque thread a son propre flux aléatoire
     * - Performance : pas de synchronisation nécessaire
     */
    
public:
    /*
     * CONSTRUCTEUR : INITIALISE LES GÉNÉRATEURS ALÉATOIRES
     * =====================================================
     */
    explicit MonteCarloEngine(uint64_t seed = std::random_device{}()) 
        : rng_(seed) {
        /*
         * explicit = empêche la conversion implicite
         * std::random_device{}() = graine vraiment aléatoire (hardware)
         * seed = point de départ pour la séquence aléatoire
         */
        
        // Détecte le nombre de cœurs/threads disponibles sur le CPU
        const auto num_threads = std::thread::hardware_concurrency();
        thread_rngs_.reserve(num_threads);  // Pré-alloue la mémoire
        
        /*
         * Crée un générateur indépendant pour chaque thread
         * Chaque générateur a une graine différente (seed + i)
         */
        for (unsigned i = 0; i < num_threads; ++i) {
            thread_rngs_.emplace_back(seed + i);
            /*
             * emplace_back = construit directement dans le vector
             * Plus efficace que push_back(mt19937_64(seed + i))
             */
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
    void simulate_gbm_paths(Container& paths, double S0, double mu, double sigma, 
                           double T, size_t n_steps, size_t n_paths) const {
        /*
         * PARAMÈTRES :
         * - paths : tableau pour stocker tous les prix simulés
         * - S0 : prix initial (ex: pétrole à 75$/baril)
         * - mu : dérive/trend (ex: 5% de croissance annuelle)
         * - sigma : volatilité (ex: 35% de fluctuation annuelle)
         * - T : horizon de temps (ex: 1 an)
         * - n_steps : nombre d'étapes (ex: 252 jours de trading)
         * - n_paths : nombre de trajectoires (ex: 10,000 simulations)
         */
        
        /*
         * PRÉ-CALCULS POUR L'EFFICACITÉ
         * ==============================
         * On calcule une fois les constantes utilisées millions de fois
         */
        const double dt = T / n_steps;  // Pas de temps (ex: 1/252 = 1 jour)
        const double drift = (mu - 0.5 * sigma * sigma) * dt;  // Dérive ajustée d'Itô
        const double vol_sqrt_dt = sigma * std::sqrt(dt);  // Terme de volatilité
        
        /*
         * POURQUOI CES FORMULES ?
         * - drift : correction d'Itô pour la dérive géométrique
         * - vol_sqrt_dt : écart-type des variations sur dt
         */
        
        /*
         * BOUCLE DE SIMULATION PRINCIPALE
         * ================================
         * Pour chaque trajectoire possible (path)
         */
        for (size_t path_idx = 0; path_idx < n_paths; ++path_idx) {
            /*
             * GÉNÉRATEUR ALÉATOIRE LOCAL AU THREAD
             * ====================================
             * thread_local = une copie par thread, évite les conflits
             */
            thread_local std::normal_distribution<double> normal{0.0, 1.0};
            /*
             * normal_distribution = loi normale centrée réduite N(0,1)
             * C'est la "source de hasard" du mouvement brownien
             */
            
            // Choisit le bon générateur pour ce thread
            auto& thread_rng = thread_rngs_[path_idx % thread_rngs_.size()];
            
            /*
             * INITIALISATION DE LA TRAJECTOIRE
             * =================================
             */
            paths[path_idx * (n_steps + 1)] = S0;  // Prix initial
            /*
             * Structure du tableau paths :
             * [S0_path1, S1_path1, ..., Sn_path1, S0_path2, S1_path2, ...]
             * Chaque trajectoire occupe (n_steps + 1) positions
             */
            
            /*
             * GÉNÉRATION DE LA TRAJECTOIRE COMPLÈTE
             * ======================================
             * Pour chaque pas de temps de cette trajectoire
             */
            for (size_t step = 1; step <= n_steps; ++step) {
                /*
                 * GÉNÉRATION D'UN CHOC ALÉATOIRE
                 * ===============================
                 */
                const double dW = normal(thread_rng);  // Nombre aléatoire N(0,1)
                /*
                 * dW = incrément du mouvement brownien
                 * Représente le "hasard" qui affecte le prix
                 */
                
                /*
                 * CALCUL DES INDICES DANS LE TABLEAU
                 * ===================================
                 */
                const size_t idx = path_idx * (n_steps + 1) + step;      // Position actuelle
                const size_t prev_idx = path_idx * (n_steps + 1) + step - 1;  // Position précédente
                
                /*
                 * FORMULE DU MODÈLE GÉOMÉTRIQUE BROWNIEN
                 * ======================================
                 * S(t+dt) = S(t) × exp((μ - σ²/2)×dt + σ×√dt×dW)
                 */
                paths[idx] = paths[prev_idx] * std::exp(drift + vol_sqrt_dt * dW);
                /*
                 * INTERPRÉTATION :
                 * - paths[prev_idx] : prix à l'étape précédente
                 * - drift : tendance déterministe
                 * - vol_sqrt_dt * dW : choc aléatoire
                 * - exp() : garantit que le prix reste positif
                 * 
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
    void simulate_single_step_returns(std::span<double> returns, double mu, double sigma, double dt) const {
        /*
         * PARAMÈTRES :
         * - returns : tableau pour stocker les rendements simulés
         * - mu : dérive annualisée
         * - sigma : volatilité annualisée  
         * - dt : période (ex: 1/252 pour 1 jour)
         */
        
        // Pré-calculs (même logique que GBM complet)
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
            /*
             * FORMULE DU RENDEMENT :
             * R = S(t+dt)/S(t) - 1 = exp(drift + vol×√dt×dW) - 1
             * 
             * EXEMPLE :
             * Si exp(...) = 1.012, alors rendement = 1.2%
             * Si exp(...) = 0.988, alors rendement = -1.2%
             */
        }
    }
    
    /*
     * CALCUL DE VAR ET EXPECTED SHORTFALL
     * ====================================
     * VaR = "Value at Risk" = perte maximale probable à un niveau de confiance donné
     * ES = "Expected Shortfall" = perte moyenne au-delà du VaR
     */
    template<RandomAccessRange Container>
    [[nodiscard]] std::pair<double, double> calculate_var_es(const Container& returns, double confidence) const {
        /*
         * PARAMÈTRES :
         * - returns : tableau des rendements simulés
         * - confidence : niveau de confiance (ex: 0.95 = 95%)
         * 
         * RETOUR :
         * - pair<VaR, ES> : les deux mesures de risque
         */
        
        /*
         * ÉTAPE 1 : TRIER LES RENDEMENTS
         * ===============================
         * On trie du plus mauvais au meilleur pour identifier la "queue" de distribution
         */
        std::vector<double> sorted_returns(returns.begin(), returns.end());
        std::sort(sorted_returns.begin(), sorted_returns.end());
        /*
         * Après tri : [-5%, -3%, -2%, ..., 0%, 1%, 2%, 3%]
         * Les pertes les plus importantes sont au début
         */
        
        /*
         * ÉTAPE 2 : CALCUL DE L'INDEX VAR
         * ================================
         */
        const size_t var_index = static_cast<size_t>((1.0 - confidence) * sorted_returns.size());
        /*
         * LOGIQUE :
         * - confidence = 0.95 → on veut le 5e percentile
         * - (1 - 0.95) = 0.05 = 5%
         * - Sur 10,000 simulations : index = 500
         * → On regarde la 500e pire perte
         */
        
        // Vérification de sécurité
        if (var_index >= sorted_returns.size()) return {0.0, 0.0};
        
        /*
         * ÉTAPE 3 : CALCUL DU VAR
         * ========================
         */
        const double var = -sorted_returns[var_index];
        /*
         * POURQUOI LE SIGNE MOINS ?
         * - sorted_returns contient des rendements (négatifs = pertes)
         * - VaR s'exprime comme un nombre positif
         * - Si sorted_returns[500] = -3%, alors VaR = 3%
         */
        
        /*
         * ÉTAPE 4 : CALCUL DE L'EXPECTED SHORTFALL
         * =========================================
         * ES = moyenne des pertes dans la queue (pire que VaR)
         */
        const double es = var_index > 0 
            ? -std::accumulate(sorted_returns.begin(), 
                              sorted_returns.begin() + var_index, 0.0) / var_index
            : 0.0;
        /*
         * LOGIQUE :
         * - On prend la moyenne des 500 pires pertes
         * - accumulate() fait la somme
         * - Division par var_index pour avoir la moyenne
         * - Signe moins pour exprimer comme perte positive
         * 
         * INTERPRÉTATION :
         * - VaR 95% = 3% : "On risque maximum 3% dans 95% des cas"
         * - ES 95% = 4.5% : "Si on dépasse le VaR, on perd en moyenne 4.5%"
         */
        
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
        /*
         * PARAMÈTRES :
         * - returns : rendements simulés
         * - confidence_levels : {0.90, 0.95, 0.99, 0.995} par exemple
         */
        
        /*
         * OPTIMISATION : TRIER UNE SEULE FOIS
         * ====================================
         * Au lieu de trier à chaque niveau de confiance
         */
        std::vector<double> sorted_returns(returns);
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        // Pré-allocation du résultat
        std::vector<std::pair<double, double>> results;
        results.reserve(confidence_levels.size());
        
        /*
         * CALCUL POUR CHAQUE NIVEAU DE CONFIANCE
         * =======================================
         */
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
            
            results.emplace_back(var, es);
            /*
             * emplace_back = construit directement la paire dans le vector
             * Plus efficace que push_back(make_pair(var, es))
             */
        }
        
        return results;
        /*
         * EXEMPLE DE RÉSULTAT :
         * [(VaR90%, ES90%), (VaR95%, ES95%), (VaR99%, ES99%)]
         * [(2.1%, 2.8%), (3.0%, 3.9%), (4.5%, 5.7%)]
         */
    }
    
    /*
     * QUASI-MONTE CARLO (AVANCÉ)
     * ==========================
     * Placeholder pour une technique plus sophistiquée
     */
    void simulate_qmc_paths(std::span<double> paths, double S0, double mu, double sigma, 
                           double T, size_t n_steps, size_t n_paths) const {
        /*
         * QMC = Quasi-Monte Carlo
         * Utilise des séquences "low-discrepancy" (Sobol, Halton) au lieu
         * de nombres vraiment aléatoires pour une convergence plus rapide.
         * 
         * AVANTAGES :
         * - Convergence en O(1/N) au lieu de O(1/√N)
         * - Moins de simulations nécessaires pour la même précision
         * 
         * EN PRODUCTION :
         * - Implémentation complète des séquences de Sobol
         * - Gestion de la dimension (nombre de variables aléatoires)
         */
        
        // Pour l'instant, fallback vers Monte Carlo classique
        std::vector<double> paths_vec(paths.data(), paths.data() + paths.size());
        simulate_gbm_paths(paths_vec, S0, mu, sigma, T, n_steps, n_paths);
        std::copy(paths_vec.begin(), paths_vec.end(), paths.begin());
    }
    
    /*
     * UTILITAIRE : NOMBRE DE THREADS DISPONIBLES
     * ===========================================
     */
    [[nodiscard]] size_t get_thread_count() const noexcept {
        return thread_rngs_.size();
        /*
         * Utile pour :
         * - Diagnostics de performance
         * - Ajustement des paramètres de simulation
         * - Information à l'utilisateur
         */
    }
};

/*
 * RÉSUMÉ DE CE FICHIER :
 * ======================
 * 
 * CLASSE MonteCarloEngine :
 * 1. SIMULATE_GBM_PATHS() : Simule des trajectoires de prix complètes
 * 2. SIMULATE_SINGLE_STEP_RETURNS() : Simule des rendements sur 1 période
 * 3. CALCULATE_VAR_ES() : Calcule VaR et Expected Shortfall
 * 4. CALCULATE_VAR_ES_BATCH() : VaR/ES pour plusieurs niveaux
 * 5. SIMULATE_QMC_PATHS() : Placeholder pour Quasi-Monte Carlo
 * 
 * OPTIMISATIONS CLÉS :
 * - Générateurs aléatoires per-thread (évite les conflits)
 * - Pré-calcul des constantes (évite les calculs répétés)
 * - Templates pour flexibilité des conteneurs
 * - Tri unique pour calculs VaR multiples
 * 
 * MODÈLE MATHÉMATIQUE :
 * - Geometric Brownian Motion : dS = μ×S×dt + σ×S×dW
 * - Solution : S(t) = S(0) × exp((μ-σ²/2)×t + σ×W(t))
 * 
 * USAGE TYPIQUE :
 * MonteCarloEngine mc;
 * std::vector<double> returns(100000);
 * mc.simulate_single_step_returns(returns, 0.05, 0.35, 1.0/252);
 * auto [var95, es95] = mc.calculate_var_es(returns, 0.95);
 * std::cout << "VaR 95%: " << var95 << ", ES 95%: " << es95 << std::endl;
 */