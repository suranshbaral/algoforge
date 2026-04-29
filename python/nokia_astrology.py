import yfinance as yf
import pandas as pd
import numpy as np
from datetime import datetime

# ── 1. Fetch Nokia data ──────────────────────────────────────────────
print("Fetching Nokia (NOK) historical data...")
nok = yf.download("NOK", start="2000-01-01", end="2024-01-01", interval="1mo")
nok = nok[["Close"]].dropna()
nok.columns = ["close"]
nok["return"] = nok["close"].pct_change()
nok = nok.dropna()
print(f"Got {len(nok)} monthly candles\n")

# ── 2. Hindu Astrology Signal ────────────────────────────────────────
# We use Jupiter's approximate 12-year cycle
# Jupiter transits one zodiac sign roughly every 12 months
# Traditionally bullish signs: Aries, Cancer, Leo, Sagittarius, Pisces
# Traditionally bearish signs: Capricorn, Aquarius, Virgo

JUPITER_BULLISH_YEARS = [
    # Jupiter in traditionally favorable signs (approximate)
    2000, 2001,  # Aries/Taurus
    2003,        # Leo
    2007, 2008,  # Sagittarius
    2010, 2011,  # Pisces/Aries
    2015, 2016,  # Leo
    2019,        # Sagittarius
    2022, 2023,  # Aries/Taurus
]

def jupiter_signal(date):
    """Returns 1 for bullish Jupiter phase, -1 for bearish, 0 for neutral"""
    year = date.year
    if year in JUPITER_BULLISH_YEARS:
        return 1
    else:
        return -1

print("Generating Jupiter astrology signal...")
nok["jupiter_signal"] = nok.index.map(jupiter_signal)

# ── 3. Statistical Correlation ───────────────────────────────────────
print("── Correlation Analysis ──────────────────────────\n")

# Split returns by signal
bullish_returns = nok[nok["jupiter_signal"] ==  1]["return"]
bearish_returns = nok[nok["jupiter_signal"] == -1]["return"]

print(f"Bullish Jupiter periods : {len(bullish_returns)} months")
print(f"  Mean return           : {bullish_returns.mean()*100:.4f}%")
print(f"  Std dev               : {bullish_returns.std()*100:.4f}%")
print(f"  Positive months       : {(bullish_returns > 0).sum()}")
print(f"  Negative months       : {(bullish_returns < 0).sum()}")

print(f"\nBearish Jupiter periods : {len(bearish_returns)} months")
print(f"  Mean return           : {bearish_returns.mean()*100:.4f}%")
print(f"  Std dev               : {bearish_returns.std()*100:.4f}%")
print(f"  Positive months       : {(bearish_returns > 0).sum()}")
print(f"  Negative months       : {(bearish_returns < 0).sum()}")

# ── 4. T-Test (is the difference statistically significant?) ─────────
from scipy import stats

t_stat, p_value = stats.ttest_ind(bullish_returns, bearish_returns)
print(f"\n── T-Test ────────────────────────────────────────")
print(f"  t-statistic : {t_stat:.4f}")
print(f"  p-value     : {p_value:.4f}")
print(f"  Significant : {'YES' if p_value < 0.05 else 'NO'} (threshold: 0.05)")

# ── 5. Point-Biserial Correlation ────────────────────────────────────
corr, corr_p = stats.pointbiserialr(nok["jupiter_signal"], nok["return"])
print(f"\n── Point-Biserial Correlation ────────────────────")
print(f"  Correlation : {corr:.4f}")
print(f"  p-value     : {corr_p:.4f}")
print(f"  Significant : {'YES' if corr_p < 0.05 else 'NO'}")

# ── 6. Honest Interpretation ─────────────────────────────────────────
print(f"\n── Interpretation ────────────────────────────────")
if p_value < 0.05:
    print("  The difference in returns IS statistically significant.")
    print("  This does NOT mean astrology works.")
    print("  It means this signal has non-random correlation with NOK returns.")
    print("  Could be spurious — always check out-of-sample.")
else:
    print("  The difference in returns is NOT statistically significant.")
    print("  p-value > 0.05 means we cannot reject the null hypothesis.")
    print("  The Jupiter signal shows no measurable edge on Nokia data.")
    print("  This is the expected result for a random alternative signal.")

# ── 7. Rolling correlation ────────────────────────────────────────────
print(f"\n── Rolling 24-month correlation ──────────────────")
nok["rolling_corr"] = nok["return"].rolling(24).corr(nok["jupiter_signal"])
print(nok[["close", "return", "jupiter_signal", "rolling_corr"]].tail(20).to_string())