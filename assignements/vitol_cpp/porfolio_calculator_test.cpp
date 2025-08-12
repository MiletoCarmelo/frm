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
#include <iostream>          // Pour affichage console

class PortfolioRiskCalculator {
private:

    BlackScholesModel bs_model_;  // Pour pricer chaque position individuelle
    MonteCarloEngine mc_engine_;  // Pour simuler les scénarios de risque
    
public:

    // Structure pour les métriques de risque
    struct RiskMetrics {
        // Valeur initiale du portefeuille en $
        double portfolio_value{0.0}; 

        // creation des greeks par sous-jacent
        std::unordered_map<std::string, double> delta_by_underlying;
        std::unordered_map<std::string, double> gamma_by_underlying;
        std::unordered_map<std::string, double> vega_by_underlying;
        std::unordered_map<std::string, double> theta_by_underlying;
        
        // Mesures de risque VaR et ES pour différents niveaux de confiance
        double var_95{0.0};   // VaR 95% (standard marché)
        double es_95{0.0};    // Expected Shortfall 95%
        double var_99{0.0};   // VaR 99% (régulateur bancaire)
        double es_99{0.0};    // Expected Shortfall 99%
        double var_999{0.0};  // VaR 99.9% (stress extrême)
        double es_999{0.0};   // Expected Shortfall 99.9%

        // performance computation metrics
        size_t calculation_time_us{0};      // Temps de calcul en microsecondes
        size_t monte_carlo_simulations{0};  // Nombre de simulations utilisées
    };
    
    // Structure pour les données de marché
    struct MarketData {
        std::unordered_map<std::string, double> spot_prices;  // Prix actuels par sous-jacent
        std::unordered_map<std::string, double> volatilities; // Volatilités par sous-jacent
        double risk_free_rate{0.05};  // Taux sans risque (5% par défaut)

        // Vérifie qu'on a toutes les données pour une position donnée
        [[nodiscard]] bool is_complete_for_position(const Position& pos) const noexcept {
            return spot_prices.contains(pos.underlying) && 
                   volatilities.contains(pos.underlying);
        }
    };
    
    // Fonction asynchrone pour calculer le risque du portefeuille en background async
    [[nodiscard]] std::future<RiskMetrics> calculate_portfolio_risk_async(
        std::span<const Position> positions,
        const MarketData& market_data) const {
        
        return std::async(std::launch::async, [=, this]() {
            return calculate_portfolio_risk(positions, market_data);
        });
        /* std::async = lance une fonction dans un thread séparé
         * std::launch::async = force l'exécution immédiate (pas de lazy)
         * [=, this] = capture tout par copie + pointeur vers l'objet actuel
         */
    }
    
    // Fonction synchrone pour calculer le risque du portefeuille 
    [[nodiscard]] RiskMetrics calculate_portfolio_risk(
        std::span<const Position> positions,     // Toutes les positions du portefeuille
        const MarketData& market_data) const {   // Données de marché actuelles
        
        // Chrono start
        const auto start_time = std::chrono::high_resolution_clock::now();
        
        // Initialisation des métriques de risque
        RiskMetrics metrics;
        
        // ÉTAPE 1 : FILTRAGE DES POSITIONS VALABLES (I.e. existent market data)
        const auto valid_positions = filter_valid_positions(positions, market_data);

        // Si aucune position valide, on retourne des métriques vides
        if (valid_positions.empty()) {
            return metrics;
        }

        // ÉTAPE 2 : CALCUL DES GREEKS DU PORTEFEUILLE
        calculate_portfolio_greeks(valid_positions, market_data, metrics);
        
        // ÉTAPE 3 : CALCUL DE LA VAR MONTE CARLO
        calculate_monte_carlo_var(valid_positions, market_data, metrics);

        // ÉTAPE 4 : ENREGISTREMENT DES MÉTRIQUES DE PERFORMANCE
        const auto end_time = std::chrono::high_resolution_clock::now();
        metrics.calculation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        
        return metrics; 
    }
    
    // Fonction pour effectuer un stress test du portefeuille
    [[nodiscard]] std::vector<std::pair<std::string, double>> stress_test_portfolio(
        std::span<const Position> positions,
        const MarketData& base_market_data,
        const std::unordered_map<std::string, double>& stress_scenarios) const {
        
        // Vérification des positions valides
        const double base_pv = calculate_portfolio_value(positions, base_market_data);
        std::vector<std::pair<std::string, double>> results;

        // Test de chaque scénario de stress
        for (const auto& [scenario_name, shock_size] : stress_scenarios) {
            
            // Appliquer le choc sur les prix du marché
            MarketData stressed_data = base_market_data;  // Copie des données de base
            for (auto& [underlying, price] : stressed_data.spot_prices) {
                price *= (1.0 + shock_size);
            }
            
            // Recalculer la valeur du portefeuille avec les données stressées
            const double stressed_pv = calculate_portfolio_value(positions, stressed_data);
            
            // Enregistrer le résultat du stress test
            results.emplace_back(scenario_name, stressed_pv - base_pv);
        }
        
        return results;
    }
    
private:
    
    // Fonction utilitaire pour filtrer les positions valides
    [[nodiscard]] std::vector<Position> filter_valid_positions(
        std::span<const Position> positions,
        const MarketData& market_data) const {
        
        std::vector<Position> valid_positions;
        /*
         * std::copy_if = algorithme STL qui copie seulement les éléments
         * qui satisfont une condition (le prédicat lambda)
         */
        std::copy_if(
            positions.begin(), 
            positions.end(), 
            std::back_inserter(valid_positions),
            [&](const Position& pos) {
                return pos.is_valid() && market_data.is_complete_for_position(pos);
            }
        );
        
        return valid_positions;
    }

    // Fonction pour calculer les Greeks du portefeuille
    void calculate_portfolio_greeks(
        const std::vector<Position>& positions,
        const MarketData& market_data,
        RiskMetrics& metrics) const {  // Modifié par référence
        
        // Pré-allocation des vecteurs pour éviter les réallocations
        std::vector<double> position_values(positions.size());
        std::vector<double> position_deltas(positions.size());
        std::vector<double> position_gammas(positions.size());
        std::vector<double> position_vegas(positions.size());
        std::vector<double> position_thetas(positions.size());
        
        // Boucle sur chaque position pour calculer les valeurs et les Greeks
        for (size_t i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];  // Référence vers la position actuelle
            
            // Vérification des données de marché pour cette position
            const double S = market_data.spot_prices.at(pos.underlying);
            const double vol = market_data.volatilities.at(pos.underlying);
            /*
             * .at() vs [] :
             * - .at() lance exception si clé manquante
             * - [] crée une entrée vide si clé manquante
             * Ici on veut une exception si données manquantes
             */
            
            // CALCUL DU PRIX DE L'OPTION
            if (auto price_result = bs_model_.price(S, pos.strike, pos.maturity, 
                                                   market_data.risk_free_rate, vol, pos.is_call);
                price_result.has_value()) {
                
                // Stocke la valeur de la position (prix unitaire × notional)
                position_values[i] = price_result.value() * pos.notional;
            }
            
            // CALCUL DES GREEKS
            const auto greeks = bs_model_.calculate_all_greeks(S, pos.strike, pos.maturity,
                                                              market_data.risk_free_rate, vol, pos.is_call);
            
            // mise à l'échelle des Greeks par la taille de la position
            position_deltas[i] = greeks.delta * pos.notional;
            position_gammas[i] = greeks.gamma * pos.notional;
            position_vegas[i] = greeks.vega * pos.notional;
            position_thetas[i] = greeks.theta * pos.notional;
        }
        
        // AGRÉGATION DE LA VALEUR TOTALE DU PORTEFEUILLE
        metrics.portfolio_value = std::accumulate(position_values.begin(), position_values.end(), 0.0);
        // std::accumulate = somme tous les éléments
        // Plus lisible qu'une boucle for manuelle

        // AGRÉGATION DES GREEKS PAR SOUS-JACENT : somme des Greeks pour chaque sous-jacent
        for (size_t i = 0; const auto& pos : positions) {
            metrics.delta_by_underlying[pos.underlying] += position_deltas[i];
            metrics.gamma_by_underlying[pos.underlying] += position_gammas[i];
            metrics.vega_by_underlying[pos.underlying] += position_vegas[i];
            metrics.theta_by_underlying[pos.underlying] += position_thetas[i];
            ++i;
        }
    }
    
    // Fonction pour calculer la VaR et l'ES par simulation Monte Carlo
    void calculate_monte_carlo_var(
        const std::vector<Position>& positions,
        const MarketData& market_data,
        RiskMetrics& metrics) const {
        
        // Paramètres de simulation
        constexpr size_t n_simulations = 10'000;  // Nombre de scénarios simulés
        constexpr double T = 1.0 / 252.0;         // Horizon = 1 jour de trading
        
        metrics.monte_carlo_simulations = n_simulations;  // Pour traçabilité

        // IDENTIFICATION DES SOUS-JACENTS UNIQUES: On doit simuler chaque marché indépendamment
        std::set<std::string> underlyings;
        for (const auto& pos : positions) {
            underlyings.insert(pos.underlying);
        }
        /*
         * std::set = collection unique automatiquement
         * Si portefeuille a 10 positions WTI + 5 BRENT + 3 NATGAS
         * → underlyings = {"WTI", "BRENT", "NATGAS"} (3 éléments)
         */
        
        // SIMULATION DES RENDEMENTS PAR SOUS-JACENT
        // Pour chaque marché, on génère N scénarios de rendements quotidiens
        std::unordered_map<std::string, std::vector<double>> simulated_returns;
        for (const auto& underlying : underlyings) {
            const double vol = market_data.volatilities.at(underlying);
            
            std::vector<double> returns(n_simulations);
            mc_engine_.simulate_single_step_returns(returns, market_data.risk_free_rate, vol, T);
            simulated_returns[underlying] = std::move(returns);
            /* std::move() = transfert de propriété (évite la copie)
             */
        }

        // CALCUL DES RENDEMENTS DU PORTEFEUILLE
        std::vector<double> portfolio_returns(n_simulations);
        
        for (size_t sim = 0; sim < n_simulations; ++sim) {
            double base_pv = 0.0;     // Valeur actuelle
            double shocked_pv = 0.0;  // Valeur après choc
            
            // RÉÉVALUATION DE CHAQUE POSITION DANS CE SCÉNARIO
            for (const auto& pos : positions) {
                // PRIX ACTUELS ET CHOQUÉS
                const double S_base = market_data.spot_prices.at(pos.underlying);
                const double return_shock = simulated_returns.at(pos.underlying)[sim];
                const double S_shocked = S_base * (1.0 + return_shock);
                const double vol = market_data.volatilities.at(pos.underlying);

                // PRICING DANS LES DEUX ÉTATS : Valeur avant et après le choc
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
            }

            // CALCUL DU RENDEMENT DU PORTEFEUILLE
            portfolio_returns[sim] = (shocked_pv - base_pv) / std::abs(base_pv);
            // FORMULE : Rendement = (Valeur_finale - Valeur_initiale) / |Valeur_initiale|

        }
        
        // CALCUL DES MÉTRIQUES VAR/ES
        const std::array confidence_levels = {0.95, 0.99, 0.999};
        const auto var_es_results = mc_engine_.calculate_var_es_batch(portfolio_returns, confidence_levels);
        
        // STOCKAGE DANS LA STRUCTURE DE RÉSULTATS
        metrics.var_95 = var_es_results[0].first;
        metrics.es_95 = var_es_results[0].second;
        metrics.var_99 = var_es_results[1].first;
        metrics.es_99 = var_es_results[1].second;
        metrics.var_999 = var_es_results[2].first;
        metrics.es_999 = var_es_results[2].second;

    }
    
    // FONCTION UTILITAIRE : VALEUR DU PORTEFEUILLE
    // Calcule la valeur totale sans les Greeks (plus rapide)
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
        }
        
        return total_value;
    }
};

int main() {
    // Exemple d'utilisation de PortfolioRiskCalculator
    PortfolioRiskCalculator calculator;
    
    // Création de données de marché fictives
    PortfolioRiskCalculator::MarketData market_data{
        .spot_prices = {
            {"WTI", 75.0}, 
            {"BRENT", 78.0}, 
            {"NATGAS", 3.0}, 
            {"COAL", 150.0},
            {"GASOLINE", 2.5},
            {"ETHANOL", 1.2},
            {"PROPANE", 0.8},
            {"BUTANE", 0.6},
            {"URANIUM", 50.0},
            {"LNG", 5.0},
            {"GOLD", 1800.0},
            {"SILVER", 25.0},
            {"COPPER", 4.0},
            {"ALUMINUM", 2500.0},
            {"ZINC", 3000.0},
            {"LEAD", 2000.0},
            {"TIN", 25000.0}
        },
        .volatilities = {
            {"WTI", 0.35}, 
            {"BRENT", 0.33}, 
            {"NATGAS", 0.40}, 
            {"COAL", 0.30},
            {"GASOLINE", 0.25},
            {"ETHANOL", 0.20},
            {"PROPANE", 0.15},
            {"BUTANE", 0.10},
            {"URANIUM", 0.50},
            {"LNG", 0.45},
            {"GOLD", 0.15},
            {"SILVER", 0.20},
            {"COPPER", 0.25},
            {"ALUMINUM", 0.30},
            {"ZINC", 0.35},
            {"LEAD", 0.40},
            {"TIN", 0.50}
        },
        .risk_free_rate = 0.05
    };

    // Création d'un portefeuille fictif
    std::vector<Position> positions = {
        {"CALL_WTI_123", "WTI", 100'000, 80.0, 1.0, true},
        {"PUT_BRENT_456", "BRENT", 50'000, 75.0, 0.5, false},
        {"CALL_NATGAS_789", "NATGAS", 20'000, 3.5, 0.25, true},
        {"PUT_COAL_101", "COAL", 15'000, 160.0, 0.75, false}
    };
    // notes : id, underlying, notional, strike, maturity, is_call
    
    // Calcul du risque du portefeuille
    auto risk_metrics = calculator.calculate_portfolio_risk(positions, market_data);
    
    // Affichage des résultats
    std::cout << "Portfolio Value: $" << risk_metrics.portfolio_value << std::endl;
    std::cout << "VaR (95%): $" << risk_metrics.var_95 << std::endl;
    std::cout << "Expected Shortfall (95%): $" << risk_metrics.es_95 << std::endl;

    // Affichage des Greeks par sous-jacent
    for (const auto& [underlying, delta] : risk_metrics.delta_by_underlying) {
        std::cout << "Delta for " << underlying << ": $" << delta << std::endl;
    }
    for (const auto& [underlying, gamma] : risk_metrics.gamma_by_underlying) {
        std::cout << "Gamma for " << underlying << ": $" << gamma << std::endl;
    }
    for (const auto& [underlying, vega] : risk_metrics.vega_by_underlying) {
        std::cout << "Vega for " << underlying << ": $" << vega << std::endl;
    }
    for (const auto& [underlying, theta] : risk_metrics.theta_by_underlying) {
        std::cout << "Theta for " << underlying << ": $" << theta << std::endl;
    }
    std::cout << "Calculation Time: " << risk_metrics.calculation_time_us << " us" << std::endl;
    std::cout << "Monte Carlo Simulations: " << risk_metrics.monte_carlo_simulations << std::endl;

    // Exemple de stress test
    std::unordered_map<std::string, double> stress_scenarios = {
        {"Oil Crisis", 0.50},        // +50% sur tous les prix
        {"Market Crash", -0.30},     // -30% sur tous les prix
        {"Recession", -0.15}         // -15% sur tous les prix
    };
    auto stress_results = calculator.stress_test_portfolio(positions, market_data, stress_scenarios);
    std::cout << "Stress Test Results:" << std::endl;
    for (const auto& [scenario, impact] : stress_results) {
        std::cout << scenario << ": $" << impact << " P&L impact" << std::endl;
    }   
    // Fin du programme
    std::cout << "Portfolio risk analysis completed." << std::endl;
    std::cout << "All metrics calculated successfully." << std::endl;
    return 0;
}

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