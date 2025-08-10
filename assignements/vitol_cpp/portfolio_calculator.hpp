/*
 * portfolio_calculator.hpp - Calculateur de risque au niveau portefeuille
 * 
 * Ce fichier est le "chef d'orchestre" de notre système de risque.
 * Il combine tous les autres modules pour analyser un portefeuille complet
 * de positions financières et calculer les risques globaux.
 * 
 * ANALOGIE : Si les autres fichiers sont des instruments de musique,
 * celui-ci est le chef qui coordonne tout l'orchestre pour jouer une symphonie.
 */

#pragma once

#include "types.hpp"          // Types de base (Position, expected, etc.)
#include "pricing_models.hpp" // BlackScholesModel pour le pricing
#include "monte_carlo.hpp"    // MonteCarloEngine pour les simulations VaR
#include <unordered_map>      // Pour grouper par sous-jacent
#include <vector>             // Pour listes de positions
#include <future>             // Pour calculs asynchrones
#include <set>                // Pour collections uniques (sous-jacents)
#include <span>               // Pour manipulation sûre de tableaux
#include <algorithm>          // Pour filtrage, copie, etc.
#include <execution>          // Pour parallélisation (pas utilisé)
#include <ranges>             // Pour manipulation moderne des données

// ===== CALCULATEUR DE RISQUE PORTEFEUILLE =====
/*
 * Cette classe est le "cerveau central" qui coordonne tous les calculs
 * pour un portefeuille complet de positions financières.
 */
class PortfolioRiskCalculator {
private:
    /*
     * MOTEURS DE CALCUL INTÉGRÉS
     * ===========================
     * La classe contient les outils spécialisés nécessaires
     */
    BlackScholesModel bs_model_;  // Pour pricer chaque position individuelle
    MonteCarloEngine mc_engine_;  // Pour simuler les scénarios de risque
    /*
     * Approche "composition" : au lieu d'hériter, on contient les outils
     * Plus flexible et évite les problèmes d'héritage multiple
     */
    
public:
    /*
     * STRUCTURE POUR TOUTES LES MÉTRIQUES DE RISQUE
     * ==============================================
     * Conteneur qui rassemble TOUS les résultats d'analyse de risque
     */
    struct RiskMetrics {
        /*
         * VALEUR DU PORTEFEUILLE
         * ======================
         */
        double portfolio_value{0.0};  // Valeur totale en $ du portefeuille
        /*
         * {0.0} = initialisation par défaut
         * Évite les valeurs non-initialisées qui causent des bugs
         */
        
        /*
         * GREEKS AGRÉGÉS PAR SOUS-JACENT
         * ===============================
         * Au lieu d'avoir les Greeks de chaque position, on les groupe
         * par actif sous-jacent (WTI, BRENT, NATGAS, etc.)
         */
        std::unordered_map<std::string, double> delta_by_underlying;
        std::unordered_map<std::string, double> gamma_by_underlying;
        std::unordered_map<std::string, double> vega_by_underlying;
        std::unordered_map<std::string, double> theta_by_underlying;
        /*
         * EXEMPLE de delta_by_underlying :
         * {
         *   "WTI": 1500000,      // Exposition nette à WTI
         *   "BRENT": -750000,    // Position courte sur Brent
         *   "NATGAS": 2000000    // Forte exposition au gaz naturel
         * }
         * 
         * UTILITÉ MÉTIER :
         * - Risk manager voit l'exposition par marché
         * - Facilite les décisions de hedging
         * - Surveillance des limites par sous-jacent
         */
        
        /*
         * VAR/ES À DIFFÉRENTS NIVEAUX DE CONFIANCE
         * =========================================
         * Mesures de risque réglementaires et internes
         */
        double var_95{0.0};   // VaR 95% (standard marché)
        double es_95{0.0};    // Expected Shortfall 95%
        double var_99{0.0};   // VaR 99% (régulateur bancaire)
        double es_99{0.0};    // Expected Shortfall 99%
        double var_999{0.0};  // VaR 99.9% (stress extrême)
        double es_999{0.0};   // Expected Shortfall 99.9%
        /*
         * POURQUOI PLUSIEURS NIVEAUX ?
         * - 95% : Usage quotidien des traders
         * - 99% : Reportage réglementaire (Bâle III)
         * - 99.9% : Test de résistance aux crises
         */
        
        /*
         * MÉTRIQUES DE PERFORMANCE
         * ========================
         */
        size_t calculation_time_us{0};      // Temps de calcul en microsecondes
        size_t monte_carlo_simulations{0};  // Nombre de simulations utilisées
        /*
         * UTILITÉ :
         * - Monitoring de performance du système
         * - Optimisation des algorithmes
         * - SLA (Service Level Agreement) avec les utilisateurs
         */
    };
    
    /*
     * STRUCTURE POUR LES DONNÉES DE MARCHÉ
     * =====================================
     * Centralise toutes les données nécessaires aux calculs
     */
    struct MarketData {
        std::unordered_map<std::string, double> spot_prices;  // Prix actuels par sous-jacent
        std::unordered_map<std::string, double> volatilities; // Volatilités par sous-jacent
        double risk_free_rate{0.05};  // Taux sans risque (5% par défaut)
        /*
         * EXEMPLE de contenu :
         * spot_prices = {
         *   "WTI": 75.50,     // $/baril
         *   "BRENT": 78.20,   // $/baril  
         *   "NATGAS": 3.45    // $/MMBtu
         * }
         * 
         * volatilities = {
         *   "WTI": 0.35,      // 35% de volatilité annuelle
         *   "BRENT": 0.33,    // 33% de volatilité annuelle
         *   "NATGAS": 0.60    // 60% de volatilité annuelle (plus volatile)
         * }
         */
        
        /*
         * FONCTION DE VALIDATION
         * ======================
         * Vérifie qu'on a toutes les données pour une position donnée
         */
        [[nodiscard]] bool is_complete_for_position(const Position& pos) const noexcept {
            return spot_prices.contains(pos.underlying) && 
                   volatilities.contains(pos.underlying);
            /*
             * .contains() = méthode C++20 pour vérifier la présence d'une clé
             * Plus lisible que .find() != .end()
             * 
             * LOGIQUE :
             * Pour pricer une option WTI, on a besoin du prix spot WTI ET de sa volatilité
             * Si l'un des deux manque → position invalide
             */
        }
    };
    
    /*
     * CALCUL DE RISQUE ASYNCHRONE
     * ===========================
     * Permet de lancer le calcul en arrière-plan pendant qu'on fait autre chose
     */
    [[nodiscard]] std::future<RiskMetrics> calculate_portfolio_risk_async(
        std::span<const Position> positions,
        const MarketData& market_data) const {
        /*
         * std::future<RiskMetrics> = "promesse" d'un résultat futur
         * Comme commander une pizza : on reçoit un ticket, la pizza arrive plus tard
         */
        
        return std::async(std::launch::async, [=, this]() {
            return calculate_portfolio_risk(positions, market_data);
        });
        /*
         * std::async = lance une fonction dans un thread séparé
         * std::launch::async = force l'exécution immédiate (pas de lazy)
         * [=, this] = capture tout par copie + pointeur vers l'objet actuel
         * 
         * USAGE TYPIQUE :
         * auto future_result = calculator.calculate_portfolio_risk_async(positions, data);
         * // ... faire autre chose pendant que ça calcule ...
         * auto result = future_result.get();  // Récupère le résultat
         */
    }
    
    /*
     * FONCTION PRINCIPALE : CALCUL DE RISQUE PORTEFEUILLE
     * ====================================================
     * C'est le "main" de notre système de risque
     */
    [[nodiscard]] RiskMetrics calculate_portfolio_risk(
        std::span<const Position> positions,     // Toutes les positions du portefeuille
        const MarketData& market_data) const {   // Données de marché actuelles
        
        /*
         * CHRONOMÉTRAGE DE PERFORMANCE
         * ============================
         * On mesure combien de temps prend le calcul complet
         */
        const auto start_time = std::chrono::high_resolution_clock::now();
        /*
         * high_resolution_clock = horloge la plus précise disponible
         * Essentiel en trading où les microsecondes comptent
         */
        
        RiskMetrics metrics;  // Structure de résultats à remplir
        
        /*
         * ÉTAPE 1 : VALIDATION ET FILTRAGE
         * =================================
         * On ne garde que les positions qu'on peut calculer
         */
        const auto valid_positions = filter_valid_positions(positions, market_data);
        if (valid_positions.empty()) {
            return metrics; // Retourne des métriques vides si rien à calculer
        }
        /*
         * POURQUOI FILTRER ?
         * - Position avec underlying inconnu → skip
         * - Données de marché manquantes → skip
         * - Position invalide (strike négatif) → skip
         * 
         * Évite que tout le calcul plante à cause d'une mauvaise position
         */
        
        /*
         * ÉTAPE 2 : CALCUL DES GREEKS DU PORTEFEUILLE
         * ============================================
         * Sensibilités agrégées aux paramètres de marché
         */
        calculate_portfolio_greeks(valid_positions, market_data, metrics);
        
        /*
         * ÉTAPE 3 : CALCUL DU VAR MONTE CARLO
         * ====================================
         * Simulation de milliers de scénarios pour estimer le risque
         */
        calculate_monte_carlo_var(valid_positions, market_data, metrics);
        
        /*
         * ÉTAPE 4 : ENREGISTREMENT DES MÉTRIQUES DE PERFORMANCE
         * ======================================================
         */
        const auto end_time = std::chrono::high_resolution_clock::now();
        metrics.calculation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        /*
         * duration_cast<microseconds> = convertit en microsecondes
         * .count() = extrait le nombre de microsecondes
         * 
         * EXEMPLE : Si le calcul prend 250ms → metrics.calculation_time_us = 250000
         */
        
        return metrics;  // Retourne tous les résultats
    }
    
    /*
     * STRESS TESTING DU PORTEFEUILLE
     * ===============================
     * Teste comment le portefeuille réagit à des chocs de marché extrêmes
     */
    [[nodiscard]] std::vector<std::pair<std::string, double>> stress_test_portfolio(
        std::span<const Position> positions,
        const MarketData& base_market_data,
        const std::unordered_map<std::string, double>& stress_scenarios) const {
        /*
         * PARAMÈTRES :
         * - positions : portefeuille à tester
         * - base_market_data : conditions de marché normales
         * - stress_scenarios : {"Market Crash": -0.30, "Oil Spike": 0.50, ...}
         * 
         * RETOUR :
         * - vector<pair<nom_scénario, impact_P&L>>
         */
        
        /*
         * VALEUR DE BASE DU PORTEFEUILLE
         * ==============================
         * Point de référence pour mesurer les impacts
         */
        const double base_pv = calculate_portfolio_value(positions, base_market_data);
        std::vector<std::pair<std::string, double>> results;
        
        /*
         * TEST DE CHAQUE SCÉNARIO DE STRESS
         * ==================================
         */
        for (const auto& [scenario_name, shock_size] : stress_scenarios) {
            /*
             * Structured binding C++17 : [scenario_name, shock_size]
             * Équivaut à :
             * string scenario_name = pair.first;
             * double shock_size = pair.second;
             */
            
            /*
             * APPLICATION DU CHOC À TOUS LES PRIX SPOT
             * ========================================
             * On modifie tous les prix simultanément
             */
            MarketData stressed_data = base_market_data;  // Copie des données de base
            for (auto& [underlying, price] : stressed_data.spot_prices) {
                price *= (1.0 + shock_size);
                /*
                 * EXEMPLE de "Market Crash" avec shock_size = -0.30 :
                 * - WTI : 75$ → 75$ × (1 - 0.30) = 52.5$
                 * - BRENT : 78$ → 78$ × (1 - 0.30) = 54.6$
                 * - NATGAS : 3.45$ → 3.45$ × (1 - 0.30) = 2.415$
                 */
            }
            
            /*
             * CALCUL DE LA VALEUR SOUS STRESS
             * ================================
             */
            const double stressed_pv = calculate_portfolio_value(positions, stressed_data);
            
            /*
             * STOCKAGE DE L'IMPACT P&L
             * =========================
             */
            results.emplace_back(scenario_name, stressed_pv - base_pv);
            /*
             * EXEMPLE de résultat :
             * - Scénario "Market Crash" → Impact = -15,000,000$ (perte de 15M$)
             * - Scénario "Oil Spike" → Impact = +8,500,000$ (gain de 8.5M$)
             */
        }
        
        return results;
    }
    
private:
    /*
     * FONCTIONS PRIVÉES UTILITAIRES
     * ==============================
     * Fonctions internes qui décomposent les calculs complexes
     */
    
    /*
     * FILTRAGE DES POSITIONS VALIDES
     * ===============================
     * Sépare le bon grain de l'ivraie dans le portefeuille
     */
    [[nodiscard]] std::vector<Position> filter_valid_positions(
        std::span<const Position> positions,
        const MarketData& market_data) const {
        
        std::vector<Position> valid_positions;
        /*
         * std::copy_if = algorithme STL qui copie seulement les éléments
         * qui satisfont une condition (le prédicat lambda)
         */
        std::copy_if(positions.begin(), positions.end(), 
                    std::back_inserter(valid_positions),
                    [&](const Position& pos) {
                        return pos.is_valid() && market_data.is_complete_for_position(pos);
                    });
        /*
         * CONDITION DE VALIDITÉ :
         * 1. pos.is_valid() : position bien formée (strike > 0, etc.)
         * 2. market_data.is_complete_for_position(pos) : données dispo
         * 
         * [&] = capture par référence (accès à market_data)
         * std::back_inserter = ajoute à la fin du vector automatiquement
         */
        
        return valid_positions;
    }
    
    /*
     * CALCUL DES GREEKS DU PORTEFEUILLE
     * ==================================
     * Agrège les sensibilités de toutes les positions par sous-jacent
     */
    void calculate_portfolio_greeks(
        const std::vector<Position>& positions,
        const MarketData& market_data,
        RiskMetrics& metrics) const {  // Modifié par référence
        
        /*
         * PRÉ-ALLOCATION DES VECTEURS
         * ===========================
         * On alloue la mémoire une seule fois pour éviter les réallocations
         */
        std::vector<double> position_values(positions.size());
        std::vector<double> position_deltas(positions.size());
        std::vector<double> position_gammas(positions.size());
        std::vector<double> position_vegas(positions.size());
        std::vector<double> position_thetas(positions.size());
        /*
         * OPTIMISATION MÉMOIRE :
         * - Taille fixe connue à l'avance
         * - Évite les push_back() répétés qui peuvent réallouer
         * - Meilleure localité cache (données contiguës)
         */
        
        /*
         * CALCUL POUR CHAQUE POSITION INDIVIDUELLE
         * =========================================
         * Boucle séquentielle (fallback pour compatibilité)
         */
        for (size_t i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];  // Référence vers la position actuelle
            
            /*
             * RÉCUPÉRATION DES DONNÉES DE MARCHÉ
             * ===================================
             */
            const double S = market_data.spot_prices.at(pos.underlying);
            const double vol = market_data.volatilities.at(pos.underlying);
            /*
             * .at() vs [] :
             * - .at() lance exception si clé manquante
             * - [] crée une entrée vide si clé manquante
             * Ici on veut une exception si données manquantes
             */
            
            /*
             * CALCUL DU PRIX DE LA POSITION
             * =============================
             */
            if (auto price_result = bs_model_.price(S, pos.strike, pos.maturity, 
                                                   market_data.risk_free_rate, vol, pos.is_call);
                price_result.has_value()) {
                position_values[i] = price_result.value() * pos.notional;
                /*
                 * LOGIQUE :
                 * - bs_model_.price() retourne expected<double, RiskError>
                 * - Si succès → .has_value() == true
                 * - .value() extrait le prix unitaire
                 * - × pos.notional = valeur totale de la position
                 * 
                 * EXEMPLE :
                 * - Prix unitaire option = 2.48$
                 * - Notional = 1,000,000 (1M de contrats)
                 * - Valeur position = 2,480,000$
                 */
            }
            
            /*
             * CALCUL DE TOUS LES GREEKS EN UNE FOIS
             * ======================================
             * Plus efficace que 4 appels séparés
             */
            const auto greeks = bs_model_.calculate_all_greeks(S, pos.strike, pos.maturity,
                                                              market_data.risk_free_rate, vol, pos.is_call);
            
            /*
             * MISE À L'ÉCHELLE PAR LE NOTIONAL
             * =================================
             * Les Greeks unitaires × taille de la position
             */
            position_deltas[i] = greeks.delta * pos.notional;
            position_gammas[i] = greeks.gamma * pos.notional;
            position_vegas[i] = greeks.vega * pos.notional;
            position_thetas[i] = greeks.theta * pos.notional;
            /*
             * EXEMPLE :
             * - Delta unitaire = 0.35 (35 cents par dollar du sous-jacent)
             * - Notional = 1,000,000
             * - Delta position = 350,000 (position équivalente 350K$ spot)
             */
        }
        
        /*
         * AGRÉGATION DE LA VALEUR TOTALE DU PORTEFEUILLE
         * ===============================================
         */
        metrics.portfolio_value = std::accumulate(position_values.begin(), position_values.end(), 0.0);
        /*
         * std::accumulate = somme tous les éléments
         * Plus lisible qu'une boucle for manuelle
         */
        
        /*
         * AGRÉGATION DES GREEKS PAR SOUS-JACENT
         * ======================================
         * On groupe toutes les expositions au même underlying
         */
        for (size_t i = 0; const auto& pos : positions) {
            metrics.delta_by_underlying[pos.underlying] += position_deltas[i];
            metrics.gamma_by_underlying[pos.underlying] += position_gammas[i];
            metrics.vega_by_underlying[pos.underlying] += position_vegas[i];
            metrics.theta_by_underlying[pos.underlying] += position_thetas[i];
            ++i;
            /*
             * LOGIQUE D'AGRÉGATION :
             * Si on a 3 positions WTI avec deltas [100K, -50K, 200K]
             * → delta_by_underlying["WTI"] = 100K - 50K + 200K = 250K
             * 
             * C++20 range-based for avec index manuel
             * Plus moderne : structured binding mais plus complexe ici
             */
        }
    }
    
    /*
     * CALCUL VAR MONTE CARLO
     * ======================
     * Simule des milliers de scénarios pour estimer le risque de queue
     */
    void calculate_monte_carlo_var(
        const std::vector<Position>& positions,
        const MarketData& market_data,
        RiskMetrics& metrics) const {
        
        /*
         * PARAMÈTRES DE SIMULATION
         * ========================
         */
        constexpr size_t n_simulations = 10'000;  // Nombre de scénarios simulés
        constexpr double T = 1.0 / 252.0;         // Horizon = 1 jour de trading
        /*
         * constexpr = calculé à la compilation
         * 252 = nombre de jours de trading par an (365 - weekends - jours fériés)
         * 
         * COMPROMIS n_simulations :
         * - Plus = plus précis mais plus lent
         * - 10K = bon équilibre pour usage quotidien
         * - Production : souvent 100K ou plus
         */
        
        metrics.monte_carlo_simulations = n_simulations;  // Pour traçabilité
        
        /*
         * IDENTIFICATION DES SOUS-JACENTS UNIQUES
         * ========================================
         * On doit simuler chaque marché indépendamment
         */
        std::set<std::string> underlyings;
        for (const auto& pos : positions) {
            underlyings.insert(pos.underlying);
        }
        /*
         * std::set = collection unique automatiquement
         * Si portefeuille a 10 positions WTI + 5 BRENT + 3 NATGAS
         * → underlyings = {"WTI", "BRENT", "NATGAS"} (3 éléments)
         */
        
        /*
         * SIMULATION DES RENDEMENTS PAR SOUS-JACENT
         * ==========================================
         * Pour chaque marché, on génère N scénarios de rendements quotidiens
         */
        std::unordered_map<std::string, std::vector<double>> simulated_returns;
        
        for (const auto& underlying : underlyings) {
            const double vol = market_data.volatilities.at(underlying);
            
            std::vector<double> returns(n_simulations);
            mc_engine_.simulate_single_step_returns(returns, market_data.risk_free_rate, vol, T);
            simulated_returns[underlying] = std::move(returns);
            /*
             * RÉSULTAT :
             * simulated_returns = {
             *   "WTI": [0.012, -0.023, 0.008, ...],      // 10K rendements WTI
             *   "BRENT": [0.015, -0.018, 0.011, ...],    // 10K rendements Brent
             *   "NATGAS": [0.045, -0.067, 0.023, ...]    // 10K rendements gaz
             * }
             * 
             * std::move() = transfert de propriété (évite la copie)
             */
        }
        
        /*
         * CALCUL DES RENDEMENTS DU PORTEFEUILLE
         * =====================================
         * Pour chaque scénario, on recalcule la valeur du portefeuille
         */
        std::vector<double> portfolio_returns(n_simulations);
        
        for (size_t sim = 0; sim < n_simulations; ++sim) {
            double base_pv = 0.0;     // Valeur actuelle
            double shocked_pv = 0.0;  // Valeur après choc
            
            /*
             * RÉÉVALUATION DE CHAQUE POSITION DANS CE SCÉNARIO
             * =================================================
             */
            for (const auto& pos : positions) {
                /*
                 * PRIX ACTUELS ET CHOQUÉS
                 * =======================
                 */
                const double S_base = market_data.spot_prices.at(pos.underlying);
                const double return_shock = simulated_returns.at(pos.underlying)[sim];
                const double S_shocked = S_base * (1.0 + return_shock);
                const double vol = market_data.volatilities.at(pos.underlying);
                /*
                 * EXEMPLE scénario #1000 :
                 * - WTI return_shock = -0.023 (-2.3%)
                 * - S_base = 75$, S_shocked = 75$ × (1 - 0.023) = 73.275$
                 */
                
                /*
                 * PRICING DANS LES DEUX ÉTATS
                 * ============================
                 * Valeur avant et après le choc
                 */
                if (auto base_price = bs_model_.price(S_base, pos.strike, pos.maturity, 
                                                     market_data.risk_free_rate, vol, pos.is_call);
                    base_price.has_value()) {
                    base_pv += base_price.value() * pos.notional;
                }
                
                if (auto shocked_price = bs_model_.price(S_shocked, pos.strike, pos.maturity, 
                                                         market_data.risk_free_rate, vol, pos.is_call);
                    shocked_price.has_value()) {
                    shocked_pv += shocked_price.value() * pos.notional;
                }
                /*
                 * LOGIQUE :
                 * - On price la même option avec prix spot différent
                 * - Différence = impact du mouvement de marché
                 * - Répété pour toutes les positions du portefeuille
                 */
            }
            
            /*
             * CALCUL DU RENDEMENT DU PORTEFEUILLE
             * ===================================
             */
            portfolio_returns[sim] = (shocked_pv - base_pv) / std::abs(base_pv);
            /*
             * FORMULE : Rendement = (Valeur_finale - Valeur_initiale) / |Valeur_initiale|
             * 
             * EXEMPLE :
             * - base_pv = 10,000,000$ (10M$)
             * - shocked_pv = 9,500,000$ (9.5M$)
             * - Rendement = (9.5M - 10M) / 10M = -5% (perte de 5%)
             */
        }
        
        /*
         * CALCUL DES MÉTRIQUES VAR/ES
         * ============================
         * Transformation des rendements simulés en mesures de risque
         */
        const std::array confidence_levels = {0.95, 0.99, 0.999};
        const auto var_es_results = mc_engine_.calculate_var_es_batch(portfolio_returns, confidence_levels);
        
        /*
         * STOCKAGE DANS LA STRUCTURE DE RÉSULTATS
         * ========================================
         */
        metrics.var_95 = var_es_results[0].first;
        metrics.es_95 = var_es_results[0].second;
        metrics.var_99 = var_es_results[1].first;
        metrics.es_99 = var_es_results[1].second;
        metrics.var_999 = var_es_results[2].first;
        metrics.es_999 = var_es_results[2].second;
        /*
         * EXEMPLE de résultats :
         * - VaR 95% = 3.2% → "95% de chance de ne pas perdre plus de 3.2%"
         * - ES 95% = 4.1% → "Si on dépasse VaR, perte moyenne de 4.1%"
         * - VaR 99% = 5.8% → "99% de chance de ne pas perdre plus de 5.8%"
         */
    }
    
    /*
     * FONCTION UTILITAIRE : VALEUR DU PORTEFEUILLE
     * =============================================
     * Calcule la valeur totale sans les Greeks (plus rapide)
     */
    [[nodiscard]] double calculate_portfolio_value(
        std::span<const Position> positions,
        const MarketData& market_data) const {
        
        double total_value = 0.0;
        
        for (const auto& pos : positions) {
            // Vérification rapide de données disponibles
            if (!market_data.is_complete_for_position(pos)) continue;
            
            // Récupération des paramètres de marché
            const double S = market_data.spot_prices.at(pos.underlying);
            const double vol = market_data.volatilities.at(pos.underlying);
            
            // Pricing et accumulation
            if (auto price_result = bs_model_.price(S, pos.strike, pos.maturity, 
                                                   market_data.risk_free_rate, vol, pos.is_call);
                price_result.has_value()) {
                total_value += price_result.value() * pos.notional;
            }
            /*
             * DIFFÉRENCE avec calculate_portfolio_greeks :
             * - Ici : seulement le prix total (rapide)
             * - Là-bas : prix + tous les Greeks (complet mais plus lent)
             * 
             * USAGE :
             * - Stress testing : on veut juste la valeur
             * - Analyse complète : on veut prix + sensibilités
             */
        }
        
        return total_value;
    }
};

/*
 * RÉSUMÉ DE CE FICHIER :
 * ======================
 * 
 * CLASSE PortfolioRiskCalculator :
 * 1. CALCULATE_PORTFOLIO_RISK() : Analyse complète du risque portefeuille
 * 2. CALCULATE_PORTFOLIO_RISK_ASYNC() : Version asynchrone non-bloquante
 * 3. STRESS_TEST_PORTFOLIO() : Tests de résistance aux chocs de marché
 * 4. Fonctions privées pour décomposer les calculs complexes
 * 
 * STRUCTURES DE DONNÉES :
 * - RiskMetrics : Tous les résultats d'analyse (valeur, Greeks, VaR/ES)
 * - MarketData : Données de marché centralisées (prix, volatilités, taux)
 * 
 * FLUX DE CALCUL PRINCIPAL :
 * Input: Positions + MarketData
 * ↓
 * 1. Filtrage des positions valides
 * ↓  
 * 2. Calcul des Greeks agrégés par sous-jacent
 * ↓
 * 3. Simulation Monte Carlo pour VaR/ES
 * ↓
 * Output: RiskMetrics complet
 * 
 * OPTIMISATIONS CLÉS :
 * - Validation préalable (évite les plantages)
 * - Calcul groupé des Greeks (évite les appels répétés)
 * - Pré-allocation mémoire (évite les réallocations)
 * - Agrégation par sous-jacent (vue métier)
 * - Chronométrage intégré (monitoring performance)
 * 
 * USAGE TYPIQUE CHEZ VITOL :
 * 
 * // 1. Préparation des données
 * PortfolioRiskCalculator calculator;
 * MarketData market_data{
 *     .spot_prices = {{"WTI", 75.0}, {"BRENT", 78.0}},
 *     .volatilities = {{"WTI", 0.35}, {"BRENT", 0.33}},
 *     .risk_free_rate = 0.05
 * };
 * 
 * vector<Position> positions = load_trading_book();
 * 
 * // 2. Analyse de risque quotidienne
 * auto risk_metrics = calculator.calculate_portfolio_risk(positions, market_data);
 * 
 * // 3. Vérification des limites
 * if (risk_metrics.var_99 > daily_var_limit) {
 *     alert_risk_manager("VaR limit exceeded!");
 * }
 * 
 * // 4. Reporting par sous-jacent
 * for (const auto& [underlying, delta] : risk_metrics.delta_by_underlying) {
 *     cout << underlying << " exposure: $" << delta << endl;
 * }
 * 
 * // 5. Stress testing
 * unordered_map<string, double> stress_scenarios = {
 *     {"Oil Crisis", 0.50},        // +50% sur tous les prix
 *     {"Market Crash", -0.30},     // -30% sur tous les prix
 *     {"Recession", -0.15}         // -15% sur tous les prix
 * };
 * 
 * auto stress_results = calculator.stress_test_portfolio(positions, market_data, stress_scenarios);
 * 
 * for (const auto& [scenario, impact] : stress_results) {
 *     cout << scenario << ": $" << impact << " P&L impact" << endl;
 * }
 * 
 * IMPORTANCE MÉTIER :
 * ===================
 * Ce fichier est le "tableau de bord" du risk manager chez Vitol.
 * Il répond aux questions critiques :
 * 
 * 1. "Combien vaut notre portefeuille ?" → portfolio_value
 * 2. "Quelle est notre exposition par marché ?" → delta_by_underlying  
 * 3. "Quel est notre risque maximum quotidien ?" → var_99
 * 4. "Comment résistons-nous aux crises ?" → stress_test_portfolio
 * 5. "Sommes-nous dans les limites réglementaires ?" → var_95, es_95
 * 
 * CONFORMITÉ RÉGLEMENTAIRE :
 * - VaR 99% : Reporting Bâle III pour les banques
 * - Expected Shortfall : Nouvelle norme post-crise 2008
 * - Stress testing : Obligatoire pour institutions systémiques
 * 
 * DÉCISIONS BUSINESS SUPPORTÉES :
 * - Allocation de capital par desk de trading
 * - Limites de risque par trader
 * - Stratégies de hedging (couverture)
 * - Validation de nouvelles stratégies de trading
 * - Calcul du coût du capital réglementaire
 */