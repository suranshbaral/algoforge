import yfinance as yf
import pandas as pd
import json

print("Fetching Nokia (NOK) data...")
nok = yf.download("NOK", start="2015-01-01", end="2024-01-01", interval="1d")
nok = nok[["Open", "High", "Low", "Close", "Volume"]].dropna()
nok.columns = ["open", "high", "low", "close", "volume"]
nok = nok.reset_index()

candles = []
for _, row in nok.iterrows():
    candles.append({
        "symbol"      : "NOK",
        "open"        : round(float(row["open"]), 4),
        "high"        : round(float(row["high"]), 4),
        "low"         : round(float(row["low"]), 4),
        "close"       : round(float(row["close"]), 4),
        "volume"      : round(float(row["volume"]), 2),
        "timestamp"   : int(pd.Timestamp(row["Date"]).timestamp() * 1000),
        "interval_sec": 86400
    })

with open("nokia_data.json", "w") as f:
    json.dump(candles, f)

print(f"Saved {len(candles)} daily candles to nokia_data.json")
print(f"Date range: {nok['Date'].iloc[0]} to {nok['Date'].iloc[-1]}")
print(f"Price range: ${nok['close'].min():.2f} to ${nok['close'].max():.2f}")