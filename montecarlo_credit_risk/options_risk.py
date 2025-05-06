import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import norm
from scipy import stats
import pandas as pd
import seaborn as sns

class EuropeanOptionMonteCarlo:
    def __init__(self, S0, K, T, r, sigma, option_type="call", position='long', type_option="eu", barrier=None, 
                 barrier_type=None, xi=None, rho=None, kappa=None, theta=None, dividend_yield=0):
        """
        Initialisation du modèle de simulation Monte Carlo pour une option européenne
        
        Paramètres:
        - S0: prix initial de l'actif sous-jacent
        - K: prix d'exercice (strike)
        - T: temps jusqu'à maturité (en années)
        - r: taux d'intérêt sans risque (annuel)
        - sigma: volatilité de l'actif sous-jacent (annuelle)
        - option_type: 'call' pour une option d'achat, 'put' pour une option de vente
        - type_option: type d'option : 'eu', 'asian', 'barrier', 'stochastic'
        - dividend_yield: taux de dividende annuel (par défaut 0)
        """
        self.S0 = S0
        self.K = K
        self.T = T
        self.r = r
        self.sigma = sigma # volatilité du sous-jacent initiale. 
        self.dividend_yield = dividend_yield
        self.option_type = option_type.lower()  # 'call' ou 'put'
        self.position = position.lower()  # 'long' ou 'short'
        self.results = []
        self.type_option = type_option  # type d'option : 'eu', 'asian', 'barrier', 'stochastic'
        # type barrier : 
        self.barrier = barrier  # Niveau de barrière
        self.barrier_type = barrier_type  # 'up-and-out', 'down-and-out', 'up-and-in', 'down-and-in'
        # type stochastic volatility (Heston model)
        self.xi = xi        # Volatilité de la volatilité
        self.rho = rho      # Corrélation entre les processus du prix et de la volatilité
        self.kappa = kappa  # vitesse de retour à la moyenne 
        self.theta = theta  # niveau de volatilité à long terme 


    def simulate_price_path(self, steps=252):
        """
        Simule l'évolution du prix selon un mouvement brownien géométrique
        
        Paramètres:
        - steps: nombre d'étapes de temps pour la simulation (par défaut 252, soit environ un jour de trading par année)
        
        Retourne:
        - Trajectoire de prix simulée
        """
        dt = self.T / steps
        price_path = np.zeros(steps + 1)
        price_path[0] = self.S0

        # pour le modèle de options stochastiques
        vol_path = np.zeros(steps + 1)
        vol_path[0] = self.sigma**2  # Variance initiale
        
        for i in range(1, steps + 1):
            # Mouvement brownien géométrique
            z1 = np.random.normal(0, 1)

            if self.type_option == 'stochastic':
                z2 = self.rho * z1 + np.sqrt(1 - self.rho**2) * np.random.normal(0, 1)
        
                # Mettre à jour la volatilité (processus CIR)
                vol_path[i] = max(0, vol_path[i-1] + self.kappa * (self.theta - vol_path[i-1]) * dt 
                                + self.xi * np.sqrt(vol_path[i-1] * dt) * z2)
                
            else: 
                vol_path[i] = self.sigma**2 # volatilité constante 

            # Mettre à jour le prix
            price_path[i] = price_path[i-1] * np.exp((self.r - self.dividend_yield - 0.5 * vol_path[i]) * dt 
                                                    + np.sqrt(vol_path[i] * dt) * z1)    
            
        return price_path
    
    def simulate_single_scenario(self, steps=252):
        """
        Simule un seul scénario pour l'option
        
        Paramètres:
        - steps: nombre d'étapes de temps pour la simulation
        
        Retourne:
        - Un dictionnaire avec les résultats du scénario
        """
        # Simuler le chemin du prix
        price_path = self.simulate_price_path(steps)
        
        if self.type_option in ('eu', 'stochastic'):
            # Prix final
            final_price = price_path[-1]    
            # Payoff de l'option à maturité en fonction du type d'option (call ou put)
            if self.option_type == 'call':
                payoff = max(0, final_price - self.K)
            else:  # put
                payoff = max(0, self.K - final_price)

        elif self.type_option == 'asian':
            # Calculer la moyenne du prix
            average_price = np.mean(price_path)
            # Prix final pour référence
            final_price = price_path[-1]
            # Payoff en fonction du type d'option (call ou put)
            if self.option_type == 'call':
                payoff = max(0, average_price - self.K)
            else:  # put
                payoff = max(0, self.K - average_price)

        elif self.type_option == 'barrier': 
            barrier_hit = False
            if self.barrier_type.startswith('up'):
                barrier_hit = np.any(price_path >= self.barrier)
            else:  # down
                barrier_hit = np.any(price_path <= self.barrier)
            
            # Déterminer si l'option est active
            is_active = False
            if self.barrier_type.endswith('in'):
                is_active = barrier_hit
            else:  # out
                is_active = not barrier_hit
            
            # Calculer le payoff en fonction du type d'option (call ou put)
            final_price = price_path[-1]
            if self.option_type == 'call':
                payoff = max(0, final_price - self.K) if is_active else 0
            else:  # put
                payoff = max(0, self.K - final_price) if is_active else 0

        # Valeur actualisée du payoff
        present_value = payoff * np.exp(-self.r * self.T)
        
        # Ajuster le payoff et la valeur présente en fonction de la position (long/short)
        if self.position == "short":
            payoff = -payoff
            present_value = -present_value

        return {
            'price_path': price_path,
            'final_price': final_price,
            'payoff': payoff,
            'present_value': present_value
        }

    def calculate_VaR(self, confidence_level=0.95):
        """
        Calcule la Value at Risk (VaR) pour l'option
        
        Paramètres:
        - confidence_level: niveau de confiance pour la VaR (par défaut 0.95)
        
        Retourne:
        - VaR au niveau de confiance spécifié
        """
        if not self.results:
            raise ValueError("Exécutez d'abord la simulation avec simulate_option_price()")
        
        # Extraire les valeurs actualisées des résultats
        present_values = [r['present_value'] for r in self.results]
        
        # Calculer la VaR
        var = np.percentile(present_values, (1 - confidence_level) * 100)
        
        return var

    def calculate_expected_shortfall(self, confidence_level=0.95):
        """
        Calcule l'Expected Shortfall (ES) pour l'option
        
        Paramètres:
        - confidence_level: niveau de confiance pour l'ES (par défaut 0.95)
        
        Retourne:
        - Expected Shortfall au niveau de confiance spécifié
        """
        if not self.results:
            raise ValueError("Exécutez d'abord la simulation avec simulate_option_price()")
        
        # Extraire les valeurs actualisées des résultats
        present_values = np.array([r['present_value'] for r in self.results])
        
        # Calculer la VaR
        var = self.calculate_VaR(confidence_level)
        
        # Calculer l'Expected Shortfall (moyenne des valeurs inférieures à la VaR)
        expected_shortfall = np.mean(present_values[present_values <= var])
        
        return expected_shortfall
    
    def simulate_option_price(self, num_simulations, steps=252, verbose=True):
        """
        Exécute la simulation Monte Carlo pour estimer le prix de l'option
        
        Paramètres:
        - num_simulations: nombre de simulations à effectuer
        - steps: nombre d'étapes de temps pour chaque simulation
        - verbose: affiche des informations de progression si True
        
        Retourne:
        - DataFrame avec les résultats
        """
        self.results = []
        
        # Boucle de simulation avec affichage si verbose=True
        if verbose:
            print(f"Exécution de {num_simulations} simulations...")
        
        for i in range(num_simulations):
            # Mise à jour de progression tous les 10% si verbose
            if verbose and i % (num_simulations // 10) == 0 and i > 0:
                print(f"  {i / num_simulations * 100:.0f}% terminé...")
                
            # Simuler un seul scénario et ajouter les résultats
            scenario_result = self.simulate_single_scenario(steps)
            self.results.append(scenario_result)
        
        if verbose:
            print("Simulations terminées.")
        
        results_df = pd.DataFrame({
            'final_price': [r['final_price'] for r in self.results],
            'payoff': [r['payoff'] for r in self.results],
            'present_value': [r['present_value'] for r in self.results]
        })
        
        # Calculer le prix estimé de l'option
        option_price = results_df['present_value'].mean()
        option_price_std = results_df['present_value'].std()
        
        # Calculer l'erreur standard
        standard_error = option_price_std / np.sqrt(num_simulations)
        
        # Intervalle de confiance à 95%
        ci_95_lower = option_price - 1.96 * standard_error
        ci_95_upper = option_price + 1.96 * standard_error
        
        # Ajouter ces valeurs comme attributs de l'instance plutôt que comme colonnes
        self.option_price = option_price
        self.option_price_std = option_price_std
        self.standard_error = standard_error
        self.ci_95_lower = ci_95_lower
        self.ci_95_upper = ci_95_upper
        
        # Calculer le prix théorique avec Black-Scholes
        self.calculate_bs_price()
        
        # Calculer VaR et Expected Shortfall
        self.var_95 = self.calculate_VaR(0.95)
        self.var_99 = self.calculate_VaR(0.99)
        self.es_95 = self.calculate_expected_shortfall(0.95)
        self.es_99 = self.calculate_expected_shortfall(0.99)
        
        return results_df

    def calculate_bs_price(self):
        """
        Calcule le prix théorique de l'option avec Black-Scholes
        """
        d1 = (np.log(self.S0 / self.K) + (self.r - self.dividend_yield + 0.5 * self.sigma**2) * self.T) / (self.sigma * np.sqrt(self.T))
        d2 = d1 - self.sigma * np.sqrt(self.T)

        if self.option_type == 'call':
            bs_price = self.S0 * np.exp(-self.dividend_yield * self.T) * norm.cdf(d1) - self.K * np.exp(-self.r * self.T) * norm.cdf(d2)
        else:  # put
            bs_price = self.K * np.exp(-self.r * self.T) * norm.cdf(-d2) - self.S0 * np.exp(-self.dividend_yield * self.T) * norm.cdf(-d1)

        # Ajuster le prix en fonction de la position
        if self.position == "short":
            bs_price = -bs_price
        
        self.bs_price = bs_price

    def plot_risk_metrics(self):
        """
        Affiche les métriques de risque (VaR et Expected Shortfall)
        """
        if not self.results:
            raise ValueError("Exécutez d'abord la simulation avec simulate_option_price()")
        
        present_values = [r['present_value'] for r in self.results]
        
        plt.figure(figsize=(15, 10))
        
        # Histogramme des valeurs présentes
        plt.subplot(2, 1, 1)
        sns.histplot(present_values, kde=True, bins=50)
        
        # Ajouter des lignes verticales pour les VaR
        plt.axvline(x=self.var_95, color='r', linestyle='--', 
                   label=f'VaR 95%: {self.var_95:.4f}')
        plt.axvline(x=self.var_99, color='g', linestyle='--', 
                   label=f'VaR 99%: {self.var_99:.4f}')
        
        plt.title('Distribution des Valeurs Présentes avec VaR')
        plt.xlabel('Valeur Présente')
        plt.ylabel('Fréquence')
        plt.legend()
        
        # Distribution cumulée pour montrer clairement les niveaux de VaR
        plt.subplot(2, 1, 2)
        sns.ecdfplot(present_values)
        
        # Ajouter des lignes horizontales pour les niveaux de confiance
        plt.axhline(y=0.05, color='r', linestyle='--', 
                   label='5% (pour VaR 95%)')
        plt.axhline(y=0.01, color='g', linestyle='--', 
                   label='1% (pour VaR 99%)')
        
        # Ajouter des lignes verticales pour les VaR
        plt.axvline(x=self.var_95, color='r', linestyle=':')
        plt.axvline(x=self.var_99, color='g', linestyle=':')
        
        plt.title('Distribution Cumulée des Valeurs Présentes')
        plt.xlabel('Valeur Présente')
        plt.ylabel('Probabilité Cumulée')
        plt.legend()
        
        plt.tight_layout()
        plt.show()
        
        # Afficher les métriques de risque
        risk_metrics = {
            'VaR 95%': self.var_95,
            'VaR 99%': self.var_99,
            'Expected Shortfall 95%': self.es_95,
            'Expected Shortfall 99%': self.es_99
        }
        
        return risk_metrics
    
    def plot_price_distribution(self):
        """
        Affiche la distribution des prix finaux et des payoffs
        """
        if not self.results:
            raise ValueError("Exécutez d'abord la simulation avec simulate_option_price()")
        
        results_df = pd.DataFrame({
            'final_price': [r['final_price'] for r in self.results],
            'payoff': [r['payoff'] for r in self.results],
            'present_value': [r['present_value'] for r in self.results]
        })
        
        # Utiliser les attributs stockés lors de l'appel à simulate_option_price
        if not hasattr(self, 'option_price'):
            self.option_price = results_df['present_value'].mean()
            
        if not hasattr(self, 'bs_price'):
            # Calculer le prix Black-Scholes
            self.calculate_bs_price()
        
        plt.figure(figsize=(15, 10))
        
        # Distribution des prix finaux
        plt.subplot(2, 2, 1)
        sns.histplot(results_df['final_price'], kde=True)
        plt.axvline(x=self.K, color='r', linestyle='--', label=f'Prix d\'exercice: {self.K}')
        plt.axvline(x=self.S0, color='g', linestyle='--', label=f'Prix initial: {self.S0}')
        plt.title('Distribution des Prix Finaux')
        plt.xlabel('Prix Final')
        plt.ylabel('Fréquence')
        plt.legend()
        
        # Distribution des payoffs
        plt.subplot(2, 2, 2)
        sns.histplot(results_df['payoff'], kde=True)
        plt.title('Distribution des Payoffs')
        plt.xlabel('Payoff')
        plt.ylabel('Fréquence')
        
        # Distribution des valeurs actualisées
        plt.subplot(2, 2, 3)
        sns.histplot(results_df['present_value'], kde=True)
        plt.axvline(x=self.option_price, color='b', linestyle='--', 
                   label=f'Prix estimé: {self.option_price:.4f}')
        plt.axvline(x=self.bs_price, color='r', linestyle='--', 
                   label=f'Prix Black-Scholes: {self.bs_price:.4f}')
        plt.title('Distribution des Valeurs Actualisées')
        plt.xlabel('Valeur Actualisée')
        plt.ylabel('Fréquence')
        plt.legend()
        
        # QQ-plot pour vérifier la normalité
        plt.subplot(2, 2, 4)
        stats.probplot(results_df['present_value'], dist="norm", plot=plt)
        plt.title('QQ-Plot des Valeurs Actualisées (vs Distribution Normale)')
        
        plt.tight_layout()
        plt.show()
        
        # Tracer la relation entre le prix final et le payoff
        plt.figure(figsize=(10, 6))
        plt.scatter(results_df['final_price'], results_df['payoff'], alpha=0.3)
        
        # Tracer la fonction de payoff théorique
        x_range = np.linspace(0.5 * self.K, 1.5 * self.K, 100)
        if self.option_type == 'call':
            y_payoff = np.maximum(0, x_range - self.K)
        else:  # put
            y_payoff = np.maximum(0, self.K - x_range)
        
        if self.position == "short":
            y_payoff = -y_payoff

        plt.plot(x_range, y_payoff, 'r-', linewidth=2)
        
        plt.title(f'Relation entre Prix Final et Payoff ({self.option_type.capitalize()} {self.position.capitalize()})')
        plt.xlabel('Prix Final')
        plt.ylabel('Payoff')
        plt.grid(True)
        plt.show()
        
        return {
            'option_price': self.option_price,
            'bs_price': self.bs_price,
            'difference': self.option_price - self.bs_price,
            'percent_difference': 100 * (self.option_price - self.bs_price) / self.bs_price
        }

    def plot_price_paths(self, num_paths=20, steps=252):
        """
        Trace plusieurs chemins de prix simulés et la valeur de l'option
        
        Paramètres:
        - num_paths: nombre de chemins à tracer
        - steps: nombre d'étapes de temps pour chaque simulation
        """
        plt.figure(figsize=(15, 10))
        
        # Créer une grille de temps
        time_grid = np.linspace(0, self.T, steps + 1)
        
        # Subplot pour les chemins de prix
        plt.subplot(2, 1, 1)
        
        # Simuler et tracer les chemins de prix
        for i in range(num_paths):
            price_path = self.simulate_price_path(steps)
            plt.plot(time_grid, price_path, alpha=0.5)
        
        # Ajouter une ligne horizontale pour le prix d'exercice
        plt.axhline(y=self.K, color='r', linestyle='--', label=f'Prix d\'exercice: {self.K}')
        
        plt.title('Simulation de Chemins de Prix')
        plt.xlabel('Temps (années)')
        plt.ylabel('Prix')
        plt.legend()
        plt.grid(True)
        
        # Subplot pour la convergence du prix de l'option
        plt.subplot(2, 1, 2)
        
        # Simuler de nombreux scénarios
        option_prices = []
        running_mean = []
        
        for i in range(1, num_paths * 25 + 1):  # Utiliser plus de simulations pour la convergence
            scenario = self.simulate_single_scenario(steps)
            option_prices.append(scenario['present_value'])
            running_mean.append(np.mean(option_prices))
        
        # Tracer la convergence du prix moyen
        plt.plot(range(1, len(running_mean) + 1), running_mean, 'b-', label='Prix moyen de l\'option')
        
        # Calculer le prix Black-Scholes
        if not hasattr(self, 'bs_price'):
            d1 = (np.log(self.S0 / self.K) + (self.r - self.dividend_yield + 0.5 * self.sigma**2) * self.T) / (self.sigma * np.sqrt(self.T))
            d2 = d1 - self.sigma * np.sqrt(self.T)
            self.bs_price = self.S0 * np.exp(-self.dividend_yield * self.T) * norm.cdf(d1) - self.K * np.exp(-self.r * self.T) * norm.cdf(d2)
        
        plt.axhline(y=self.bs_price, color='r', linestyle='--', label=f'Prix Black-Scholes: {self.bs_price:.4f}')
        
        plt.title('Convergence du Prix de l\'Option')
        plt.xlabel('Nombre de Simulations')
        plt.ylabel('Prix de l\'Option')
        plt.legend()
        plt.grid(True)
        
        plt.tight_layout()
        plt.show()
    
    def plot_time_decay(self):
        """
        Trace l'évolution du prix de l'option en fonction du temps restant jusqu'à l'échéance
        pour différents niveaux de prix du sous-jacent (in-the-money, at-the-money, out-of-the-money)
        Cette méthode illustre comment le prix de l'option tend vers sa valeur intrinsèque à l'échéance
        """
        # Créer une séquence de temps restants (du plus éloigné au plus proche de l'échéance)
        times_to_expiry = np.linspace(self.T, 0.001, 100)  # Éviter 0 pour éviter des problèmes numériques
        
        # Différents prix du sous-jacent
        spot_prices = [0.8 * self.K, 0.9 * self.K, self.K, 1.1 * self.K, 1.2 * self.K, 1.3 * self.K]
        labels = ["Deep OTM", "OTM", "ATM", "ITM", "Deep ITM", "Very Deep ITM"]
        colors = ['red', 'orange', 'green', 'blue', 'purple', 'brown']
        
        plt.figure(figsize=(15, 10))
        
        # Pour chaque prix du sous-jacent
        for i, S in enumerate(spot_prices):
            option_prices = []
            intrinsic_values = []
            
            # Pour chaque temps restant, calculer le prix de l'option et sa valeur intrinsèque
            for t in times_to_expiry:
                # Prix de l'option avec Black-Scholes
                d1 = (np.log(S / self.K) + (self.r - self.dividend_yield + 0.5 * self.sigma**2) * t) / (self.sigma * np.sqrt(t))
                d2 = d1 - self.sigma * np.sqrt(t)
                bs_price = S * np.exp(-self.dividend_yield * t) * norm.cdf(d1) - self.K * np.exp(-self.r * t) * norm.cdf(d2)
                option_prices.append(bs_price)
                
                # Valeur intrinsèque (max(0, S-K))
                intrinsic_value = max(0, S - self.K)
                intrinsic_values.append(intrinsic_value)
            
            # Tracer le prix de l'option et sa valeur intrinsèque
            plt.plot(times_to_expiry, option_prices, label=f"{labels[i]} (S={S})", color=colors[i])
            plt.plot(times_to_expiry, intrinsic_values, linestyle='--', color=colors[i], alpha=0.5)
        
        # Inverser l'axe des x pour avoir "Temps jusqu'à l'échéance" de gauche à droite
        plt.gca().invert_xaxis()
        
        plt.axvline(x=0, color='black', linestyle='-', alpha=0.3)
        plt.title('Prix de l\'Option vs Temps Restant jusqu\'à l\'Échéance')
        plt.xlabel('Temps Restant (années)')
        plt.ylabel('Prix de l\'Option')
        plt.grid(True)
        plt.legend()
        
        # Ajouter des annotations
        plt.annotate('À l\'échéance, le prix de l\'option = max(0, S-K)', 
                    xy=(0.05, 0.95), xycoords='axes fraction', 
                    fontsize=12, ha='left', va='top',
                    bbox=dict(boxstyle='round', fc='white', alpha=0.8))
        
        plt.annotate('Valeur temps diminue à l\'approche de l\'échéance', 
                    xy=(0.05, 0.88), xycoords='axes fraction', 
                    fontsize=12, ha='left', va='top',
                    bbox=dict(boxstyle='round', fc='white', alpha=0.8))
        
        plt.tight_layout()
        plt.show()
        
        # Tracer la décomposition du prix de l'option pour ATM
        S = self.K  # Option ATM
        time_values = []
        intrinsic_values = []
        total_values = []
        
        for t in times_to_expiry:
            d1 = (np.log(S / self.K) + (self.r - self.dividend_yield + 0.5 * self.sigma**2) * t) / (self.sigma * np.sqrt(t))
            d2 = d1 - self.sigma * np.sqrt(t)
            bs_price = S * np.exp(-self.dividend_yield * t) * norm.cdf(d1) - self.K * np.exp(-self.r * t) * norm.cdf(d2)
            
            intrinsic_value = max(0, S - self.K)
            time_value = bs_price - intrinsic_value
            
            total_values.append(bs_price)
            intrinsic_values.append(intrinsic_value)
            time_values.append(time_value)
        
        plt.figure(figsize=(15, 8))
        plt.stackplot(times_to_expiry, intrinsic_values, time_values, 
                     labels=['Valeur Intrinsèque', 'Valeur Temps'],
                     colors=['#ff9999', '#66b3ff'])
        
        plt.gca().invert_xaxis()
        plt.title('Décomposition du Prix de l\'Option (ATM) en Valeur Intrinsèque et Valeur Temps')
        plt.xlabel('Temps Restant (années)')
        plt.ylabel('Prix de l\'Option')
        plt.grid(True)
        plt.legend(loc='upper left')
        
        plt.tight_layout()
        plt.show()
        
    def plot_option_value_over_time(self, num_paths=50, steps=252):
        """
        Trace l'évolution de la valeur de l'option au fil du temps et illustre le comportement à l'approche de l'échéance
        
        Paramètres:
        - num_paths: nombre de chemins à simuler
        - steps: nombre d'étapes de temps pour chaque simulation
        """
        # Créer une grille de temps
        time_grid = np.linspace(0, self.T, steps + 1)
        
        # Matrice pour stocker les valeurs de l'option pour chaque chemin et temps
        option_values = np.zeros((num_paths, steps + 1))
        
        # Pour chaque chemin
        for i in range(num_paths):
            # Simuler un chemin de prix complet
            price_path = self.simulate_price_path(steps)
            
            # Pour chaque point dans le temps, calculer la valeur de l'option
            for j, t in enumerate(time_grid):
                if t == self.T:  # À maturité
                    option_values[i, j] = max(0, price_path[j] - self.K)
                else:  # Avant maturité, utiliser une approximation Monte Carlo
                    # Temps restant jusqu'à maturité
                    remaining_time = self.T - t
                    
                    # Simuler des chemins futurs à partir du prix actuel
                    S_t = price_path[j]
                    future_payoffs = []
                    
                    # Utiliser un mini Monte Carlo pour estimer la valeur à ce point
                    mini_sim_count = 20  # Nombre de sous-simulations
                    for _ in range(mini_sim_count):
                        # Simuler un seul pas de temps jusqu'à maturité
                        future_price = S_t * np.exp((self.r - self.dividend_yield - 0.5 * self.sigma**2) * remaining_time 
                                                  + self.sigma * np.sqrt(remaining_time) * np.random.normal(0, 1))
                        
                        # Calculer le payoff à maturité
                        future_payoff = max(0, future_price - self.K)
                        future_payoffs.append(future_payoff)
                    
                    # Estimation de la valeur actuelle
                    option_values[i, j] = np.mean(future_payoffs) * np.exp(-self.r * remaining_time)
        
        # Calculer la valeur moyenne de l'option à chaque point dans le temps
        mean_option_values = np.mean(option_values, axis=0)
        
        # Calculer l'écart type de la valeur de l'option à chaque point
        std_option_values = np.std(option_values, axis=0)
        
        # Tracer la valeur moyenne de l'option au fil du temps
        plt.figure(figsize=(15, 10))
        
        # Tracer quelques chemins individuels
        for i in range(min(10, num_paths)):
            plt.plot(time_grid, option_values[i], alpha=0.2, color='gray')
        
        # Tracer la moyenne
        plt.plot(time_grid, mean_option_values, 'b-', linewidth=2, label='Valeur moyenne')
        
        # Ajouter les bandes d'incertitude (1 écart-type)
        plt.fill_between(time_grid, 
                        mean_option_values - std_option_values,
                        mean_option_values + std_option_values,
                        alpha=0.3, color='blue')
        
        # Ligne pour la valeur Black-Scholes au début
        d1_0 = (np.log(self.S0 / self.K) + (self.r - self.dividend_yield + 0.5 * self.sigma**2) * self.T) / (self.sigma * np.sqrt(self.T))
        d2_0 = d1_0 - self.sigma * np.sqrt(self.T)
        bs_price_0 = self.S0 * np.exp(-self.dividend_yield * self.T) * norm.cdf(d1_0) - self.K * np.exp(-self.r * self.T) * norm.cdf(d2_0)
        
        plt.axhline(y=bs_price_0, color='r', linestyle='--', label=f'Prix Black-Scholes initial: {bs_price_0:.4f}')
        
        plt.title('Évolution de la Valeur de l\'Option au Fil du Temps')
        plt.xlabel('Temps (années)')
        plt.ylabel('Valeur de l\'Option')
        plt.legend()
        plt.grid(True)
        
        plt.tight_layout()
        plt.show()
        
        return {
            'mean_values': mean_option_values,
            'std_values': std_option_values,
            'time_grid': time_grid
        }
    
    def run_sensitivity_analysis(self, parameter, values, num_simulations=10000, steps=1):
        """
        Exécute une analyse de sensibilité en faisant varier un paramètre
        
        Paramètres:
        - parameter: paramètre à faire varier ('S0', 'K', 'T', 'r', 'sigma', 'dividend_yield')
        - values: liste des valeurs à tester pour le paramètre
        - num_simulations: nombre de simulations pour chaque valeur
        - steps: nombre d'étapes de temps pour chaque simulation
        
        Retourne:
        - DataFrame avec les résultats
        """
        sensitivity_results = []
        
        original_params = {
            'S0': self.S0,
            'K': self.K,
            'T': self.T,
            'r': self.r,
            'sigma': self.sigma,
            'dividend_yield': self.dividend_yield
        }
        
        print(f"Analyse de sensibilité pour {parameter} avec {len(values)} valeurs")
        
        # Pour chaque valeur du paramètre
        for i, value in enumerate(values):
            print(f"  Valeur {i+1}/{len(values)}: {parameter} = {value}")
            
            # Mettre à jour le paramètre
            setattr(self, parameter, value)
            
            # Simuler le prix de l'option
            self.simulate_option_price(num_simulations, steps, verbose=False)
            
            # Ajouter les résultats
            sensitivity_results.append({
                'parameter': parameter,
                'value': value,
                'monte_carlo_price': self.option_price,
                'bs_price': self.bs_price,
                'difference': self.option_price - self.bs_price,
                'percent_difference': 100 * (self.option_price - self.bs_price) / self.bs_price
            })
            
        # Restaurer les paramètres originaux
        for param, value in original_params.items():
            setattr(self, param, value)
            
        # Créer un DataFrame avec les résultats
        sensitivity_df = pd.DataFrame(sensitivity_results)
        
        # Visualisation
        plt.figure(figsize=(12, 10))
        
        plt.subplot(2, 2, 1)
        plt.plot(sensitivity_df['value'], sensitivity_df['monte_carlo_price'], 'b-', label='Monte Carlo')
        plt.plot(sensitivity_df['value'], sensitivity_df['bs_price'], 'r--', label='Black-Scholes')
        plt.title(f'Prix de l\'Option vs {parameter}')
        plt.xlabel(parameter)
        plt.ylabel('Prix de l\'Option')
        plt.legend()
        plt.grid(True)
        
        plt.subplot(2, 2, 2)
        plt.plot(sensitivity_df['value'], sensitivity_df['difference'])
        plt.title(f'Différence (Monte Carlo - Black-Scholes) vs {parameter}')
        plt.xlabel(parameter)
        plt.ylabel('Différence')
        plt.grid(True)
        
        plt.subplot(2, 2, 3)
        plt.plot(sensitivity_df['value'], sensitivity_df['percent_difference'])
        plt.title(f'Différence en % vs {parameter}')
        plt.xlabel(parameter)
        plt.ylabel('Différence en %')
        plt.grid(True)
        
        plt.tight_layout()
        plt.show()
        
        return sensitivity_df
    
    def plot_convergence(self, max_simulations=10000, steps=1, num_points=20):
        """
        Trace la convergence du prix de l'option en fonction du nombre de simulations
        
        Paramètres:
        - max_simulations: nombre maximum de simulations
        - steps: nombre d'étapes de temps pour chaque simulation
        - num_points: nombre de points à tracer sur le graphique
        
        Retourne:
        - DataFrame avec les résultats
        """
        simulation_counts = np.linspace(100, max_simulations, num_points, dtype=int)
        convergence_results = []
        
        # Calculer le prix Black-Scholes
        d1 = (np.log(self.S0 / self.K) + (self.r - self.dividend_yield + 0.5 * self.sigma**2) * self.T) / (self.sigma * np.sqrt(self.T))
        d2 = d1 - self.sigma * np.sqrt(self.T)
        bs_price = self.S0 * np.exp(-self.dividend_yield * self.T) * norm.cdf(d1) - self.K * np.exp(-self.r * self.T) * norm.cdf(d2)
        
        all_present_values = []
        
        print("Analyse de convergence:")
        for i, n in enumerate(simulation_counts):
            print(f"  Point {i+1}/{len(simulation_counts)}: {n} simulations")
            
            if i == 0:
                # Pour la première itération, effectuer n simulations
                num_to_simulate = n
            else:
                # Pour les itérations suivantes, effectuer la différence
                num_to_simulate = n - simulation_counts[i-1]
            
            # Simuler num_to_simulate scénarios
            for _ in range(num_to_simulate):
                scenario_result = self.simulate_single_scenario(steps)
                all_present_values.append(scenario_result['present_value'])
            
            # Calculer le prix moyen et l'erreur standard
            mean_price = np.mean(all_present_values)
            std_dev = np.std(all_present_values)
            standard_error = std_dev / np.sqrt(n)
            
            # Intervalle de confiance à 95%
            ci_95_lower = mean_price - 1.96 * standard_error
            ci_95_upper = mean_price + 1.96 * standard_error
            
            # Ajouter les résultats
            convergence_results.append({
                'num_simulations': n,
                'monte_carlo_price': mean_price,
                'bs_price': bs_price,
                'standard_error': standard_error,
                'ci_95_lower': ci_95_lower,
                'ci_95_upper': ci_95_upper,
                'relative_error': abs(mean_price - bs_price) / bs_price
            })
        
        # Créer un DataFrame avec les résultats
        convergence_df = pd.DataFrame(convergence_results)
        
        # Visualisation
        plt.figure(figsize=(15, 10))
        
        plt.subplot(2, 2, 1)
        plt.plot(convergence_df['num_simulations'], convergence_df['monte_carlo_price'], 'b-', label='Monte Carlo')
        plt.axhline(y=bs_price, color='r', linestyle='--', label='Black-Scholes')
        plt.fill_between(convergence_df['num_simulations'], 
                        convergence_df['ci_95_lower'], 
                        convergence_df['ci_95_upper'], 
                        alpha=0.3)
        plt.title('Convergence du Prix de l\'Option')
        plt.xlabel('Nombre de Simulations')
        plt.ylabel('Prix de l\'Option')
        plt.legend()
        plt.grid(True)
        
        plt.subplot(2, 2, 2)
        plt.plot(convergence_df['num_simulations'], convergence_df['standard_error'])
        plt.title('Erreur Standard vs Nombre de Simulations')
        plt.xlabel('Nombre de Simulations')
        plt.ylabel('Erreur Standard')
        plt.xscale('log')
        plt.yscale('log')
        plt.grid(True)
        
        plt.subplot(2, 2, 3)
        plt.plot(convergence_df['num_simulations'], convergence_df['relative_error'])
        plt.title('Erreur Relative vs Nombre de Simulations')
        plt.xlabel('Nombre de Simulations')
        plt.ylabel('Erreur Relative')
        plt.xscale('log')
        plt.yscale('log')
        plt.grid(True)
        
        plt.tight_layout()
        plt.show()
        
        return convergence_df


# Exemple d'utilisation
if __name__ == "__main__":
    # Paramètres de l'option
    S0 = 100       # Prix initial
    K = 100        # Prix d'exercice
    T = 1.0        # Temps jusqu'à maturité (1 an)
    r = 0.05       # Taux d'intérêt sans risque (5%)
    sigma = 0.20   # Volatilité (20%)
    dividend_yield = 0.02  # Taux de dividende (2%)
    
    # Créer et exécuter la simulation
    sim = EuropeanCallMonteCarlo(S0, K, T, r, sigma, dividend_yield)
    results = sim.simulate_option_price(num_simulations=2000, steps=252)
    
    # Afficher les résultats
    print("\nRésultats de la simulation:")
    print(f"Prix estimé de l'option (Monte Carlo): {sim.option_price:.4f}")
    print(f"Prix théorique (Black-Scholes): {sim.bs_price:.4f}")
    print(f"Différence: {(sim.option_price - sim.bs_price):.4f}")
    print(f"Erreur standard: {sim.standard_error:.6f}")
    print(f"Intervalle de confiance à 95%: [{sim.ci_95_lower:.4f}, {sim.ci_95_upper:.4f}]")
    
    # Visualiser la distribution des prix
    print("\nAffichage de la distribution des prix...")
    metrics = sim.plot_price_distribution()
    
    # Visualiser les chemins de prix et la convergence
    print("\nAffichage des chemins de prix et de la convergence...")
    sim.plot_price_paths(num_paths=30, steps=252)
    
    # Visualiser l'évolution de la valeur de l'option au fil du temps
    print("\nAffichage de l'évolution de la valeur de l'option au fil du temps...")
    sim.plot_option_value_over_time(num_paths=100, steps=252)
    
    # Démontrer le comportement de l'option à l'approche de l'échéance
    print("\nDémonstration du comportement de l'option à l'approche de l'échéance...")
    sim.plot_time_decay()
    
    # Analyse de sensibilité sur le prix initial
    print("\nAnalyse de sensibilité sur le prix initial (S0):")
    s0_values = np.linspace(80, 120, 15)
    s0_sensitivity = sim.run_sensitivity_analysis('S0', s0_values, num_simulations=2000)
    
    # Analyse de sensibilité sur la volatilité
    print("\nAnalyse de sensibilité sur la volatilité (sigma):")
    sigma_values = np.linspace(0.1, 0.5, 15)
    sigma_sensitivity = sim.run_sensitivity_analysis('sigma', sigma_values, num_simulations=2000)
    
    # Analyse de la convergence
    print("\nAnalyse de la convergence:")
    convergence = sim.plot_convergence(max_simulations=10000, num_points=20)