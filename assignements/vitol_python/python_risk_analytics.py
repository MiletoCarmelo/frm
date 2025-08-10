"""
Modern Python Risk Analytics Engine for Commodity Trading
Uses latest Python features and libraries (Python 3.12+)
Covers: VaR/ES, Backtesting, Monte Carlo, BSM, Forward Curves, Portfolio Greeks
"""

import numpy as np
import pandas as pd
from scipy import stats, interpolate, optimize
from scipy.stats import norm
from dataclasses import dataclass, field
from typing import Protocol, Union, Optional, Dict, List, Tuple
from enum import Enum
import asyncio
import concurrent.futures
from functools import lru_cache, partial
import warnings
warnings.filterwarnings('ignore')

# Python 3.12+ features: type annotations, match statements, dataclasses
from typing import Self  # Python 3.11+

class OptionType(Enum):
    CALL = "call"
    PUT = "put"

class RiskMeasure(Enum):
    VAR = "var"
    ES = "expected_shortfall"
    BOTH = "both"

@dataclass(frozen=True, slots=True)  # Python 3.10+ slots for performance
class MarketData:
    """Immutable market data container"""
    spot_price: float
    forward_curve: pd.Series
    volatility_surface: pd.DataFrame
    risk_free_rate: float
    dividend_yield: float = 0.0
    
    def __post_init__(self):
        if self.spot_price <= 0:
            raise ValueError("Spot price must be positive")

@dataclass
class Position:
    """Trading position with modern Python features"""
    instrument_type: str
    underlying: str
    notional: float
    maturity: float
    strike: Optional[float] = None
    option_type: Optional[OptionType] = None
    
    def __post_init__(self):
        if self.instrument_type == "option" and (self.strike is None or self.option_type is None):
            raise ValueError("Options require strike and option_type")

class PricingEngine(Protocol):
    """Protocol for typing - Python 3.8+ structural subtyping"""
    def price(self, position: Position, market_data: MarketData) -> float: ...
    def greeks(self, position: Position, market_data: MarketData) -> Dict[str, float]: ...

class ForwardCurveBuilder:
    """Modern forward curve construction with multiple interpolation methods"""
    
    def __init__(self, interpolation_method: str = "cubic_spline"):
        self.method = interpolation_method
        self._cache = {}  # Simple caching
    
    @lru_cache(maxsize=128)  # Automatic memoization
    def build_curve(self, market_points: Tuple[Tuple[float, float], ...]) -> interpolate.interp1d:
        """
        Build forward curve from market points
        Returns scipy interpolation function
        """
        tenors, forwards = zip(*market_points)
        tenors, forwards = np.array(tenors), np.array(forwards)
        
        match self.method:  # Python 3.10+ match statement
            case "linear":
                return interpolate.interp1d(tenors, forwards, kind='linear', 
                                         bounds_error=False, fill_value='extrapolate')
            case "cubic_spline":
                return interpolate.CubicSpline(tenors, forwards, 
                                            extrapolate=True)
            case "pchip":
                return interpolate.PchipInterpolator(tenors, forwards, 
                                                  extrapolate=True)
            case _:
                raise ValueError(f"Unknown interpolation method: {self.method}")
    
    def bootstrap_forwards(self, spot: float, zero_rates: pd.Series) -> pd.Series:
        """Bootstrap forward rates from zero curve"""
        tenors = zero_rates.index.values
        forwards = []
        
        for i, tenor in enumerate(tenors):
            if i == 0:
                forward = spot * np.exp(zero_rates.iloc[i] * tenor)
            else:
                prev_tenor = tenors[i-1]
                dt = tenor - prev_tenor
                forward = (np.exp(zero_rates.iloc[i] * tenor) / 
                          np.exp(zero_rates.iloc[i-1] * prev_tenor)) ** (1/dt) - 1
            forwards.append(forward)
        
        return pd.Series(forwards, index=tenors)

class BlackScholesEngine:
    """Modern Black-Scholes implementation with vectorization"""
    
    @staticmethod
    @lru_cache(maxsize=256)
    def _d1_d2(S: float, K: float, T: float, r: float, sigma: float) -> Tuple[float, float]:
        """Cached d1, d2 calculation"""
        if T <= 0 or sigma <= 0:
            return 0.0, 0.0
        
        d1 = (np.log(S/K) + (r + 0.5*sigma**2)*T) / (sigma*np.sqrt(T))
        d2 = d1 - sigma*np.sqrt(T)
        return d1, d2
    
    def price(self, position: Position, market_data: MarketData) -> float:
        """Price European option using Black-Scholes"""
        if position.instrument_type != "option":
            return self._price_forward(position, market_data)
        
        S = market_data.spot_price
        K = position.strike
        T = position.maturity
        r = market_data.risk_free_rate
        sigma = self._get_volatility(position, market_data)
        
        if T <= 0:
            return max(S - K, 0) if position.option_type == OptionType.CALL else max(K - S, 0)
        
        d1, d2 = self._d1_d2(S, K, T, r, sigma)
        
        match position.option_type:
            case OptionType.CALL:
                price = S * norm.cdf(d1) - K * np.exp(-r*T) * norm.cdf(d2)
            case OptionType.PUT:
                price = K * np.exp(-r*T) * norm.cdf(-d2) - S * norm.cdf(-d1)
            case _:
                raise ValueError(f"Unknown option type: {position.option_type}")
        
        return price * position.notional
    
    def greeks(self, position: Position, market_data: MarketData) -> Dict[str, float]:
        """Calculate all Greeks efficiently"""
        if position.instrument_type != "option":
            return {"delta": position.notional, "gamma": 0, "vega": 0, "theta": 0}
        
        S = market_data.spot_price
        K = position.strike
        T = position.maturity
        r = market_data.risk_free_rate
        sigma = self._get_volatility(position, market_data)
        
        if T <= 0:
            delta = 1.0 if (position.option_type == OptionType.CALL and S > K) or \
                           (position.option_type == OptionType.PUT and S < K) else 0.0
            return {"delta": delta * position.notional, "gamma": 0, "vega": 0, "theta": 0}
        
        d1, d2 = self._d1_d2(S, K, T, r, sigma)
        
        # Common terms
        pdf_d1 = norm.pdf(d1)
        exp_rt = np.exp(-r*T)
        sqrt_t = np.sqrt(T)
        
        match position.option_type:
            case OptionType.CALL:
                delta = norm.cdf(d1)
                theta = ((-S * pdf_d1 * sigma / (2 * sqrt_t)) - 
                        (r * K * exp_rt * norm.cdf(d2))) / 365
            case OptionType.PUT:
                delta = norm.cdf(d1) - 1
                theta = ((-S * pdf_d1 * sigma / (2 * sqrt_t)) + 
                        (r * K * exp_rt * norm.cdf(-d2))) / 365
        
        gamma = pdf_d1 / (S * sigma * sqrt_t)
        vega = S * pdf_d1 * sqrt_t / 100  # Per 1% vol change
        
        return {
            "delta": delta * position.notional,
            "gamma": gamma * position.notional,
            "vega": vega * position.notional,
            "theta": theta * position.notional
        }
    
    def _price_forward(self, position: Position, market_data: MarketData) -> float:
        """Price forward contract"""
        forward_price = market_data.spot_price * np.exp(market_data.risk_free_rate * position.maturity)
        return (forward_price - position.strike) * position.notional
    
    def _get_volatility(self, position: Position, market_data: MarketData) -> float:
        """Extract volatility from surface (simplified)"""
        # In practice, would interpolate from vol surface
        return 0.25  # 25% default vol

class MonteCarloEngine:
    """Modern Monte Carlo with async support and vectorization"""
    
    def __init__(self, n_simulations: int = 100_000, n_steps: int = 252, seed: int = 42):
        self.n_simulations = n_simulations
        self.n_steps = n_steps
        self.rng = np.random.RandomState(seed)
    
    async def simulate_paths_async(self, S0: float, mu: float, sigma: float, 
                                 T: float) -> np.ndarray:
        """Async Monte Carlo simulation using numpy vectorization"""
        dt = T / self.n_steps
        
        # Vectorized simulation - all paths at once
        dW = self.rng.normal(0, np.sqrt(dt), (self.n_steps, self.n_simulations))
        
        # Pre-allocate path array
        paths = np.zeros((self.n_steps + 1, self.n_simulations))
        paths[0] = S0
        
        # Vectorized path generation
        for t in range(1, self.n_steps + 1):
            paths[t] = paths[t-1] * np.exp((mu - 0.5*sigma**2)*dt + sigma*dW[t-1])
        
        return paths
    
    def calculate_var_es(self, returns: np.ndarray, confidence_levels: List[float] = [0.95, 0.99]) -> Dict[str, Dict[str, float]]:
        """Calculate VaR and Expected Shortfall"""
        results = {}
        
        for confidence in confidence_levels:
            alpha = 1 - confidence
            var_index = int(alpha * len(returns))
            sorted_returns = np.sort(returns)
            
            var = -sorted_returns[var_index]
            es = -np.mean(sorted_returns[:var_index]) if var_index > 0 else 0
            
            results[f"{confidence:.0%}"] = {"VaR": var, "ES": es}
        
        return results

class PortfolioRiskAnalyzer:
    """Modern portfolio risk analytics with async processing"""
    
    def __init__(self, pricing_engine: PricingEngine):
        self.pricing_engine = pricing_engine
        self.mc_engine = MonteCarloEngine()
        self.curve_builder = ForwardCurveBuilder()
    
    async def calculate_portfolio_greeks(self, positions: List[Position], 
                                       market_data: MarketData) -> pd.DataFrame:
        """Calculate portfolio Greeks with async processing"""
        
        async def process_position(position: Position) -> Dict:
            greeks = self.pricing_engine.greeks(position, market_data)
            return {
                "underlying": position.underlying,
                "instrument": position.instrument_type,
                "notional": position.notional,
                **greeks
            }
        
        # Process positions concurrently
        tasks = [process_position(pos) for pos in positions]
        results = await asyncio.gather(*tasks)
        
        df = pd.DataFrame(results)
        
        # Aggregate by underlying
        aggregated = df.groupby('underlying').agg({
            'delta': 'sum',
            'gamma': 'sum', 
            'vega': 'sum',
            'theta': 'sum',
            'notional': 'sum'
        }).round(2)
        
        return aggregated
    
    def backtest_var_model(self, returns: pd.Series, var_estimates: pd.Series, 
                          confidence_level: float = 0.95) -> Dict[str, float]:
        """Backtest VaR model using multiple tests"""
        
        # Align series
        common_index = returns.index.intersection(var_estimates.index)
        returns_aligned = returns.loc[common_index]
        var_aligned = var_estimates.loc[common_index]
        
        # VaR violations
        violations = returns_aligned < -var_aligned
        violation_rate = violations.mean()
        expected_rate = 1 - confidence_level
        
        # Kupiec POF test
        n = len(violations)
        n_violations = violations.sum()
        if n_violations == 0 or n_violations == n:
            kupiec_stat = 0
            kupiec_pvalue = 1
        else:
            likelihood_ratio = 2 * (n_violations * np.log(violation_rate / expected_rate) + 
                                   (n - n_violations) * np.log((1 - violation_rate) / (1 - expected_rate)))
            kupiec_stat = likelihood_ratio
            kupiec_pvalue = 1 - stats.chi2.cdf(likelihood_ratio, df=1)
        
        # Christoffersen Independence test (simplified)
        violations_int = violations.astype(int)
        n_00 = ((violations_int == 0) & (violations_int.shift(1) == 0)).sum()
        n_01 = ((violations_int == 0) & (violations_int.shift(1) == 1)).sum()
        n_10 = ((violations_int == 1) & (violations_int.shift(1) == 0)).sum()
        n_11 = ((violations_int == 1) & (violations_int.shift(1) == 1)).sum()
        
        if n_01 + n_11 > 0:
            p1 = n_11 / (n_01 + n_11)
            p = (n_01 + n_11) / (n_00 + n_01 + n_10 + n_11)
            if p1 > 0 and p1 < 1 and p > 0 and p < 1:
                cc_stat = 2 * (n_01 * np.log((1-p1)/(1-p)) + n_11 * np.log(p1/p))
                cc_pvalue = 1 - stats.chi2.cdf(cc_stat, df=1)
            else:
                cc_stat, cc_pvalue = 0, 1
        else:
            cc_stat, cc_pvalue = 0, 1
        
        return {
            "violation_rate": violation_rate,
            "expected_rate": expected_rate,
            "kupiec_stat": kupiec_stat,
            "kupiec_pvalue": kupiec_pvalue,
            "christoffersen_stat": cc_stat,
            "christoffersen_pvalue": cc_pvalue,
            "n_violations": n_violations,
            "n_observations": n
        }
    
    async def stress_test_portfolio(self, positions: List[Position], 
                                   base_market_data: MarketData,
                                   stress_scenarios: Dict[str, Dict]) -> pd.DataFrame:
        """Comprehensive stress testing"""
        
        base_pv = sum(self.pricing_engine.price(pos, base_market_data) for pos in positions)
        
        stress_results = []
        
        for scenario_name, stress_params in stress_scenarios.items():
            # Create stressed market data
            stressed_data = MarketData(
                spot_price=base_market_data.spot_price * (1 + stress_params.get('spot_shock', 0)),
                forward_curve=base_market_data.forward_curve * (1 + stress_params.get('curve_shock', 0)),
                volatility_surface=base_market_data.volatility_surface * (1 + stress_params.get('vol_shock', 0)),
                risk_free_rate=base_market_data.risk_free_rate + stress_params.get('rate_shock', 0)
            )
            
            # Calculate stressed PV
            stressed_pv = sum(self.pricing_engine.price(pos, stressed_data) for pos in positions)
            pnl_impact = stressed_pv - base_pv
            
            stress_results.append({
                "scenario": scenario_name,
                "base_pv": base_pv,
                "stressed_pv": stressed_pv,
                "pnl_impact": pnl_impact,
                "pnl_percent": (pnl_impact / abs(base_pv)) * 100 if base_pv != 0 else 0
            })
        
        return pd.DataFrame(stress_results)

# ===== DEMONSTRATION =====

async def main():
    """Main demonstration using modern Python async/await"""
    
    print("=== MODERN PYTHON RISK ANALYTICS ENGINE ===")
    print("Using Python 3.12+ features: async/await, match statements, dataclasses, protocols")
    
    # ===== SETUP MARKET DATA =====
    print("\n--- Setting up Market Data ---")
    
    # Forward curve points (tenor, forward_price)
    curve_points = [(0.25, 75.0), (0.5, 76.0), (1.0, 77.5), (2.0, 80.0), (3.0, 82.0)]
    
    # Build forward curve
    curve_builder = ForwardCurveBuilder("cubic_spline")
    forward_curve_func = curve_builder.build_curve(tuple(curve_points))
    
    # Create market data
    tenors = np.arange(0.1, 3.1, 0.1)
    forward_curve = pd.Series([forward_curve_func(t) for t in tenors], index=tenors)
    
    # Dummy volatility surface
    vol_surface = pd.DataFrame(
        np.random.uniform(0.2, 0.4, (len(tenors), 5)),
        index=tenors,
        columns=[50, 75, 100, 125, 150]  # Strike levels
    )
    
    market_data = MarketData(
        spot_price=75.0,
        forward_curve=forward_curve,
        volatility_surface=vol_surface,
        risk_free_rate=0.05
    )
    
    print(f"✓ Forward curve built with {len(curve_points)} market points")
    print(f"✓ Volatility surface: {vol_surface.shape}")
    
    # ===== CREATE PORTFOLIO =====
    print("\n--- Building Trading Portfolio ---")
    
    positions = [
        # Long call options
        Position("option", "WTI", 1_000_000, 0.25, 80.0, OptionType.CALL),
        Position("option", "WTI", 500_000, 0.5, 85.0, OptionType.CALL),
        
        # Short put options
        Position("option", "WTI", -750_000, 0.25, 70.0, OptionType.PUT),
        
        # Forward contracts
        Position("forward", "WTI", 2_000_000, 1.0, 78.0),
        Position("forward", "BRENT", -1_000_000, 0.5, 80.0),
    ]
    
    print(f"✓ Portfolio with {len(positions)} positions")
    
    # ===== PRICING AND GREEKS =====
    print("\n--- Calculating Prices and Greeks ---")
    
    pricing_engine = BlackScholesEngine()
    risk_analyzer = PortfolioRiskAnalyzer(pricing_engine)
    
    # Calculate portfolio Greeks using async processing
    portfolio_greeks = await risk_analyzer.calculate_portfolio_greeks(positions, market_data)
    print("\nPortfolio Greeks by Underlying:")
    print(portfolio_greeks)
    
    # ===== MONTE CARLO VAR/ES =====
    print("\n--- Monte Carlo VaR/ES Calculation ---")
    
    mc_engine = MonteCarloEngine(n_simulations=50_000)
    
    # Simulate price paths
    S0 = market_data.spot_price
    mu = market_data.risk_free_rate
    sigma = 0.35  # Commodity volatility
    T = 1/252  # Daily
    
    price_paths = await mc_engine.simulate_paths_async(S0, mu, sigma, T)
    final_prices = price_paths[-1]
    
    # Calculate returns
    returns = (final_prices - S0) / S0
    
    # VaR and ES
    var_es_results = mc_engine.calculate_var_es(returns)
    print("\nMonte Carlo VaR/ES Results:")
    for confidence, metrics in var_es_results.items():
        print(f"  {confidence}: VaR = {metrics['VaR']:.2%}, ES = {metrics['ES']:.2%}")
    
    # ===== BACKTESTING =====
    print("\n--- VaR Model Backtesting ---")
    
    # Generate synthetic historical data for backtesting
    n_days = 252
    dates = pd.date_range('2023-01-01', periods=n_days, freq='D')
    
    # Synthetic returns and VaR estimates
    np.random.seed(42)
    historical_returns = pd.Series(
        np.random.normal(-0.001, 0.025, n_days),  # Slightly negative drift
        index=dates
    )
    
    var_estimates = pd.Series(
        np.random.uniform(0.04, 0.06, n_days),  # VaR estimates between 4-6%
        index=dates
    )
    
    backtest_results = risk_analyzer.backtest_var_model(historical_returns, var_estimates)
    print(f"\nBacktest Results (95% confidence):")
    print(f"  Violation Rate: {backtest_results['violation_rate']:.2%}")
    print(f"  Expected Rate: {backtest_results['expected_rate']:.2%}")
    print(f"  Kupiec Test p-value: {backtest_results['kupiec_pvalue']:.4f}")
    print(f"  Christoffersen Test p-value: {backtest_results['christoffersen_pvalue']:.4f}")
    
    # ===== STRESS TESTING =====
    print("\n--- Stress Testing ---")
    
    stress_scenarios = {
        "Market Crash": {"spot_shock": -0.30, "vol_shock": 0.50},
        "Oil Supply Shock": {"spot_shock": 0.25, "curve_shock": 0.15},
        "Rate Hike": {"rate_shock": 0.02, "vol_shock": 0.20},
        "Contango Collapse": {"curve_shock": -0.10, "vol_shock": -0.25}
    }
    
    stress_results = await risk_analyzer.stress_test_portfolio(
        positions, market_data, stress_scenarios
    )
    
    print("\nStress Test Results:")
    for _, row in stress_results.iterrows():
        print(f"  {row['scenario']}: P&L Impact = ${row['pnl_impact']:,.0f} ({row['pnl_percent']:+.1f}%)")
    
    # ===== PERFORMANCE SUMMARY =====
    print("\n=== CAPABILITIES DEMONSTRATED ===")
    print("✓ Modern Python 3.12+ features (dataclasses, protocols, match)")
    print("✓ Async/await for concurrent processing")
    print("✓ Forward curve construction with multiple interpolation methods")
    print("✓ Black-Scholes pricing with full Greeks")
    print("✓ Vectorized Monte Carlo simulation")
    print("✓ VaR/ES calculation with multiple confidence levels")
    print("✓ Statistical backtesting (Kupiec, Christoffersen)")
    print("✓ Comprehensive stress testing framework")
    print("✓ Portfolio-level risk aggregation")
    print("✓ Efficient numpy vectorization and caching")

if __name__ == "__main__":
    asyncio.run(main())