import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.linalg import cholesky
import seaborn as sns

class RateModelParametersGenerator:
    """
    Classe pour générer les paramètres d'un modèle de taux d'intérêt
    à partir d'une liste de tenors.
    """
    
    def __init__(self, tenors, base_rate=3.0, curve_steepness=0.5, 
                 vol_level=0.8, vol_decay=0.15, corr_decay=0.15):
        """
        Initialisation du générateur de paramètres
        
        Paramètres:
        - tenors: liste des tenors (ex: ['1y', '2y', '5y', '10y'])
        - base_rate: taux de base pour la courbe des taux (%)
        - curve_steepness: contrôle la pente de la courbe des taux
        - vol_level: niveau initial de volatilité (%)
        - vol_decay: taux de décroissance de la volatilité avec la maturité
        - corr_decay: taux de décroissance de la corrélation avec l'écart de maturité
        """
        self.tenors = tenors
        self.base_rate = base_rate
        self.curve_steepness = curve_steepness
        self.vol_level = vol_level
        self.vol_decay = vol_decay
        self.corr_decay = corr_decay
        
        # Convertir les tenors en années numériques pour les calculs
        self.tenor_years = self._convert_tenors_to_years()
        
    def _convert_tenors_to_years(self):
        """Convertit les tenors en valeurs numériques en années"""
        tenor_years = []
        
        for tenor in self.tenors:
            if 'y' in tenor:
                # Cas des années (ex: '5y')
                years = float(tenor.replace('y', ''))
            elif 'm' in tenor:
                # Cas des mois (ex: '6m')
                years = float(tenor.replace('m', '')) / 12
            else:
                # Tenter de convertir directement
                try:
                    years = float(tenor)
                except ValueError:
                    raise ValueError(f"Format de tenor non reconnu: {tenor}")
            
            tenor_years.append(years)
        
        return tenor_years
    
    def generate_initial_rates(self):
        """
        Génère des taux d'intérêt initiaux plausibles
        
        Retourne:
        - dictionnaire {tenor: taux_initial}
        """
        initial_rates = {}
        
        for i, tenor in enumerate(self.tenors):
            years = self.tenor_years[i]
            
            # Formule pour une courbe des taux normale (ascendante)
            # Utilise une fonction logarithmique pour ralentir la croissance avec la durée
            rate = self.base_rate + self.curve_steepness * np.log1p(years)
            
            # Arrondir à 2 décimales
            initial_rates[tenor] = round(rate, 2)
        
        return initial_rates
    
    def generate_volatilities(self):
        """
        Génère des volatilités annualisées plausibles
        
        Retourne:
        - dictionnaire {tenor: volatilité}
        """
        volatilities = {}
        
        for i, tenor in enumerate(self.tenors):
            years = self.tenor_years[i]
            
            # Les volatilités diminuent généralement avec la maturité
            vol = self.vol_level - self.vol_decay * np.log1p(years/2)
            vol = max(0.3, vol)  # Plancher à 0.3%
            
            # Arrondir à 2 décimales
            volatilities[tenor] = round(vol, 2)
        
        return volatilities
    
    def generate_correlation_matrix(self):
        """
        Génère une matrice de corrélation plausible entre les tenors
        
        Retourne:
        - matrice numpy de corrélation (définie positive)
        """
        n = len(self.tenors)
        
        # Initialiser une matrice vide
        corr_matrix = np.zeros((n, n))
        
        # Remplir la matrice de corrélation
        for i in range(n):
            for j in range(n):
                # La corrélation décroît avec l'écart entre les maturités
                # Utiliser une fonction exponentielle décroissante
                corr = np.exp(-self.corr_decay * abs(self.tenor_years[i] - self.tenor_years[j]))
                corr_matrix[i, j] = round(corr, 2)
        
        # S'assurer que la matrice est définie positive
        eigenvalues = np.linalg.eigvalsh(corr_matrix)
        if np.any(eigenvalues <= 0):
            # Ajuster les valeurs propres négatives
            eigenvalues[eigenvalues <= 0] = 1e-6
            eigenvectors = np.linalg.eigh(corr_matrix)[1]
            corr_matrix = eigenvectors @ np.diag(eigenvalues) @ eigenvectors.T
            
            # Normaliser pour avoir des 1 sur la diagonale
            d = np.sqrt(np.diag(corr_matrix))
            corr_matrix = corr_matrix / np.outer(d, d)
            
            # Arrondir à 2 décimales
            corr_matrix = np.round(corr_matrix, 2)
        
        return corr_matrix
    
    def generate_all_parameters(self):
        """
        Génère tous les paramètres nécessaires pour un modèle de taux
        
        Retourne:
        - initial_rates: dictionnaire des taux initiaux
        - volatilities: dictionnaire des volatilités
        - correlations: matrice de corrélation
        """
        initial_rates = self.generate_initial_rates()
        volatilities = self.generate_volatilities()
        correlations = self.generate_correlation_matrix()
        
        return initial_rates, volatilities, correlations
    
    def plot_yield_curve(self):
        """Affiche la courbe des taux générée"""
        initial_rates = self.generate_initial_rates()
        
        plt.figure(figsize=(10, 6))
        plt.plot(self.tenor_years, list(initial_rates.values()), 'o-', linewidth=2)
        plt.title('Courbe des Taux Générée')
        plt.xlabel('Maturité (années)')
        plt.ylabel('Taux (%)')
        plt.grid(True)
        plt.show()
        
        return initial_rates
    
    def plot_volatility_structure(self):
        """Affiche la structure des volatilités générée"""
        volatilities = self.generate_volatilities()
        
        plt.figure(figsize=(10, 6))
        plt.plot(self.tenor_years, list(volatilities.values()), 'o-', linewidth=2)
        plt.title('Structure des Volatilités Générée')
        plt.xlabel('Maturité (années)')
        plt.ylabel('Volatilité (%)')
        plt.grid(True)
        plt.show()
        
        return volatilities
    
    def plot_correlation_heatmap(self):
        """Affiche la matrice de corrélation sous forme de heatmap"""
        correlations = self.generate_correlation_matrix()
        
        plt.figure(figsize=(10, 8))
        sns.heatmap(correlations, annot=True, cmap='coolwarm', xticklabels=self.tenors, yticklabels=self.tenors)
        plt.title('Matrice de Corrélation Générée')
        plt.tight_layout()
        plt.show()
        
        return correlations
    
    def display_all_parameters(self):
        """Affiche tous les paramètres générés avec des visualisations"""
        # Générer et afficher les paramètres
        initial_rates, volatilities, correlations = self.generate_all_parameters()
        
        # Afficher les taux initiaux
        print("Taux d'intérêt initiaux (%)")
        for tenor, rate in initial_rates.items():
            print(f"  {tenor}: {rate:.2f}")
        
        print("\nVolatilités annualisées (%)")
        for tenor, vol in volatilities.items():
            print(f"  {tenor}: {vol:.2f}")
        
        print("\nMatrice de corrélation")
        df_corr = pd.DataFrame(correlations, index=self.tenors, columns=self.tenors)
        print(df_corr)
        
        # Créer les visualisations
        plt.figure(figsize=(15, 12))
        
        # Courbe des taux
        plt.subplot(2, 2, 1)
        plt.plot(self.tenor_years, list(initial_rates.values()), 'o-', linewidth=2)
        plt.title('Courbe des Taux Générée')
        plt.xlabel('Maturité (années)')
        plt.ylabel('Taux (%)')
        plt.grid(True)
        
        # Structure des volatilités
        plt.subplot(2, 2, 2)
        plt.plot(self.tenor_years, list(volatilities.values()), 'o-', linewidth=2)
        plt.title('Structure des Volatilités Générée')
        plt.xlabel('Maturité (années)')
        plt.ylabel('Volatilité (%)')
        plt.grid(True)
        
        # Matrice de corrélation
        plt.subplot(2, 2, 3)
        sns.heatmap(correlations, annot=True, cmap='coolwarm', 
                   xticklabels=self.tenors, yticklabels=self.tenors)
        plt.title('Matrice de Corrélation Générée')
        
        plt.tight_layout()
        plt.show()
        
        return initial_rates, volatilities, correlations
    

class InterestRatePathSimulator:

    def __init__(self, initial_rates, volatilities, correlations, mean_reversion=0.1):
        """
        Initialisation du simulateur de trajectoires de taux d'intérêt
        
        Paramètres:
        - initial_rates: dictionnaire {tenor: taux_initial}
        - volatilities: dictionnaire {tenor: volatilité_annualisée}
        - correlations: matrice de corrélation entre les tenors
        - mean_reversion: paramètre de retour à la moyenne (vitesse)
        """
        self.tenors = list(initial_rates.keys())
        self.num_tenors = len(self.tenors)
        self.initial_rates = np.array([initial_rates[tenor] for tenor in self.tenors])
        self.volatilities = np.array([volatilities[tenor] for tenor in self.tenors])
        
        # Vérifier et ajuster la matrice de corrélation si nécessaire
        eigenvalues = np.linalg.eigvalsh(correlations)
        if np.any(eigenvalues <= 0):
            print("Ajustement de la matrice de corrélation pour la rendre définie positive...")
            eigenvalues = np.maximum(eigenvalues, 1e-8)
            eigenvectors = np.linalg.eigh(correlations)[1]
            correlations = eigenvectors @ np.diag(eigenvalues) @ eigenvectors.T
            
            # Normaliser pour avoir des 1 sur la diagonale
            d = np.sqrt(np.diag(correlations))
            correlations = correlations / np.outer(d, d)
        
        self.correlations = correlations
        self.mean_reversion = mean_reversion
        
        # Calculer le facteur de Cholesky pour les simulations corrélées
        try:
            self.chol_factor = cholesky(correlations, lower=True)
        except np.linalg.LinAlgError:
            # Plan B si Cholesky échoue
            eigenvalues, eigenvectors = np.linalg.eigh(correlations)
            eigenvalues = np.maximum(eigenvalues, 1e-8)
            self.chol_factor = eigenvectors @ np.diag(np.sqrt(eigenvalues))
    
    def generate_correlated_normals(self, n_samples):
        """Génère des variables normales corrélées pour tous les tenors"""
        z = np.random.normal(0, 1, size=(n_samples, self.num_tenors))
        return z @ self.chol_factor.T
    
    def simulate_paths(self, n_periods, n_simulations=1, dt=1/252):
        """
        Simule des trajectoires de taux d'intérêt pour tous les tenors
        
        Paramètres:
        - n_periods: nombre de périodes à simuler
        - n_simulations: nombre de simulations
        - dt: pas de temps (1/252 = quotidien en jours ouvrés)
        
        Retourne:
        - Un objet pandas Panel avec les dimensions:
          * items: numéro de simulation
          * major_axis: périodes temporelles
          * minor_axis: tenors
        """
        # Initialiser les résultats
        # dimensions: [n_simulations, n_periods+1, n_tenors]
        all_paths = np.zeros((n_simulations, n_periods+1, self.num_tenors))
        
        # Initialiser avec les taux actuels
        all_paths[:, 0, :] = self.initial_rates
        
        # Simuler les trajectoires
        for sim in range(n_simulations):
            # Obtenir les chocs corrélés pour cette simulation
            corr_shocks = self.generate_correlated_normals(n_periods)
            
            for t in range(n_periods):
                # Pour chaque tenor
                for i in range(self.num_tenors):
                    # Calculer la variation aléatoire avec retour à la moyenne
                    current_rate = all_paths[sim, t, i]
                    
                    # Modèle de Vasicek simplifié pour le retour à la moyenne
                    drift = self.mean_reversion * (self.initial_rates[i] - current_rate) * dt
                    diffusion = self.volatilities[i] * np.sqrt(dt) * corr_shocks[t, i]
                    
                    # Mettre à jour le taux (avec un plancher à 0)
                    all_paths[sim, t+1, i] = max(0, current_rate + drift + diffusion)
        
        # Convertir en DataFrame pour une manipulation plus facile
        paths_dict = {}
        for sim in range(n_simulations):
            paths_dict[f'sim_{sim}'] = pd.DataFrame(
                all_paths[sim], 
                columns=self.tenors
            )
        
        return paths_dict
    
    def plot_simulated_paths(self, paths_dict, simulation_indices=None, figsize=(15, 12)):
        """
        Visualise les trajectoires simulées pour chaque tenor
        
        Paramètres:
        - paths_dict: dictionnaire des chemins simulés renvoyé par simulate_paths
        - simulation_indices: indices des simulations à afficher (None = toutes)
        - figsize: taille de la figure
        """
        if simulation_indices is None:
            simulation_indices = range(len(paths_dict))
        
        fig, axes = plt.subplots(len(self.tenors), 1, figsize=figsize, sharex=True)
        
        for i, tenor in enumerate(self.tenors):
            ax = axes[i]
            
            # Pour stocker les lignes pour la légende
            lines = []
            labels = []
            
            for idx in simulation_indices:
                sim_key = f'sim_{idx}'
                if sim_key in paths_dict:
                    line, = ax.plot(paths_dict[sim_key].index, paths_dict[sim_key][tenor], 
                            alpha=0.7, linewidth=1)
                    
                    # Ajouter à la liste pour la légende
                    lines.append(line)
                    labels.append(f'Simulation {idx}')
            
            ax.set_title(f'Taux {tenor}')
            ax.set_ylabel('Taux (%)')
            ax.grid(True)
            
            # Ajouter la légende pour chaque sous-graphique
            if i == 0:  # Ajouter la légende seulement au premier graphique pour éviter l'encombrement
                ax.legend(lines, labels, loc='best', fontsize='small', ncol=2)
        
        axes[-1].set_xlabel('Période')
        plt.tight_layout()
        plt.show()

class DV01RiskAnalyzer:
    def __init__(self, bonds_portfolio, rate_paths_dict):
        """
        Initialise l'analyseur de risque DV01
        
        Paramètres:
        - bonds_portfolio: DataFrame avec les obligations et leurs sensibilités
        - rate_paths_dict: dictionnaire des trajectoires de taux d'intérêt
        """
        self.bonds = bonds_portfolio
        self.rate_paths = rate_paths_dict
        self.num_simulations = len(rate_paths_dict)
        self.tenors = list(rate_paths_dict['sim_0'].columns)
        
        # Vérifier que les tenors correspondent à ceux du portefeuille
        if 'partial_01s' in bonds_portfolio.columns:
            sample_partial = bonds_portfolio.iloc[0]['partial_01s']
            portfolio_tenors = list(sample_partial.keys())
            if set(portfolio_tenors) != set(self.tenors):
                raise ValueError(f"Les tenors du portefeuille {portfolio_tenors} ne correspondent pas aux tenors des trajectoires {self.tenors}")
    
    def calculate_portfolio_value_changes(self):
        """
        Calcule les variations de valeur du portefeuille pour toutes les trajectoires
        
        Retourne:
        - DataFrame avec les variations de valeur pour chaque simulation et période
        """
        value_changes = {}
        
        for sim_key, rates_df in self.rate_paths.items():
            n_periods = len(rates_df)
            sim_changes = np.zeros(n_periods - 1)
            
            # Pour chaque période (sauf la première)
            for t in range(1, n_periods):
                # Calculer les variations de taux par rapport à la période précédente
                rate_changes = rates_df.iloc[t] - rates_df.iloc[t-1]
                
                # Calculer l'impact sur la valeur du portefeuille
                for i, bond in self.bonds.iterrows():
                    if 'partial_01s' in self.bonds.columns:
                        # Approche par Partial 01s
                        partial_01s = bond['partial_01s']
                        bond_value_change = 0
                        
                        for tenor in self.tenors:
                            if tenor in partial_01s:
                                # Convertir en points de base (100 bp = 1%)
                                rate_change_bp = rate_changes[tenor] * 100
                                bond_value_change += -1 * partial_01s[tenor] * rate_change_bp
                    else:
                        # Approche par DV01 global (simplification)
                        # Calculer une variation moyenne pondérée des taux
                        avg_rate_change = rate_changes.mean() * 100  # en bp
                        bond_value_change = -1 * bond['dv01'] * avg_rate_change
                    
                    sim_changes[t-1] += bond_value_change
            
            value_changes[sim_key] = sim_changes
        
        # Convertir en DataFrame
        return pd.DataFrame(value_changes)
    
    def analyze_risk(self, value_changes_df):
        """
        Analyse les mesures de risque à partir des variations de valeur
        
        Retourne:
        - Un dictionnaire avec les mesures de risque clés par période
        """
        risk_metrics = {}
        
        # Pour chaque période
        for t in range(len(value_changes_df)):
            period_values = value_changes_df.iloc[t, :]
            
            var_95 = -np.percentile(period_values, 5)  # VaR à 95% (queue gauche)
            var_99 = -np.percentile(period_values, 1)  # VaR à 99%
            es_95 = -period_values[period_values <= np.percentile(period_values, 5)].mean()
            
            risk_metrics[t] = {
                'mean': period_values.mean(),
                'std_dev': period_values.std(),
                'VaR_95': var_95,
                'VaR_99': var_99,
                'ES_95': es_95,
                'max_loss': -period_values.min()
            }
        
        return pd.DataFrame(risk_metrics).T
    
    def plot_risk_metrics_over_time(self, risk_metrics_df, figsize=(14, 10)):
        """
        Visualise l'évolution des mesures de risque dans le temps
        
        Paramètres:
        - risk_metrics_df: DataFrame des mesures de risque renvoyé par analyze_risk
        - figsize: taille de la figure
        """
        plt.figure(figsize=figsize)
        
        # Choisir les mesures de risque à afficher
        metrics_to_plot = ['VaR_95', 'VaR_99', 'ES_95']
        
        for metric in metrics_to_plot:
            plt.plot(risk_metrics_df.index, risk_metrics_df[metric], 
                     label=metric, linewidth=2)
        
        plt.title('Évolution des Mesures de Risque dans le Temps')
        plt.xlabel('Période')
        plt.ylabel('Valeur à Risque ($)')
        plt.grid(True)
        plt.legend()
        plt.show()
    
    def plot_value_change_distribution(self, value_changes_df, period_idx=None, figsize=(12, 8)):
        """
        Visualise la distribution des variations de valeur
        
        Paramètres:
        - value_changes_df: DataFrame des variations de valeur
        - period_idx: indice de période spécifique à visualiser (None = toutes)
        - figsize: taille de la figure
        """
        plt.figure(figsize=figsize)
        
        if period_idx is not None:
            # Visualiser une période spécifique
            data = value_changes_df.iloc[period_idx, :]
            sns.histplot(data, kde=True)
            
            var_95 = np.percentile(data, 5)
            var_99 = np.percentile(data, 1)
            
            plt.axvline(x=var_95, color='r', linestyle='--', 
                       label=f'VaR 95%: {-var_95:.2f}')
            plt.axvline(x=var_99, color='g', linestyle='--', 
                       label=f'VaR 99%: {-var_99:.2f}')
            
            plt.title(f'Distribution des Variations de Valeur - Période {period_idx}')
        else:
            # Agréger toutes les périodes
            data = value_changes_df.values.flatten()
            sns.histplot(data, kde=True)
            
            var_95 = np.percentile(data, 5)
            var_99 = np.percentile(data, 1)
            
            plt.axvline(x=var_95, color='r', linestyle='--', 
                       label=f'VaR 95%: {-var_95:.2f}')
            plt.axvline(x=var_99, color='g', linestyle='--', 
                       label=f'VaR 99%: {-var_99:.2f}')
            
            plt.title('Distribution des Variations de Valeur - Toutes Périodes')
        
        plt.xlabel('Variation de Valeur ($)')
        plt.ylabel('Fréquence')
        plt.legend()
        plt.show()