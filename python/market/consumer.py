import os
import sys
import django
import zmq
import json

# Django setup
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'core.settings')
django.setup()

from market.models import Candle, SignalSnapshot, MarketEvent, RegimeState

# Stats engine in Python — mirrors C++ logic
class RollingStats:
    def __init__(self, window):
        self.window = window
        self.data   = []
        self.mean   = 0.0
        self.M2     = 0.0
        self.n      = 0

    def update(self, value):
        self.data.append(value)
        self.n += 1
        delta       = value - self.mean
        self.mean  += delta / self.n
        self.M2    += delta * (value - self.mean)

        if len(self.data) > self.window:
            old        = self.data.pop(0)
            self.n    -= 1
            old_delta  = old - self.mean
            self.mean -= old_delta / self.n
            self.M2   -= old_delta * (old - self.mean)

    def stddev(self):
        return (self.M2 / (self.n - 1)) ** 0.5 if self.n >= 2 else 1.0

    def zscore(self, value):
        sd = self.stddev()
        return (value - self.mean) / sd if sd > 0 else 0.0

    def ready(self):
        return self.n >= self.window


class FIRFilter:
    def __init__(self, taps=21):
        self.taps   = taps
        self.buffer = []
        # Simple moving average as FIR approximation
        self.coeffs = [1.0 / taps] * taps

    def update(self, value):
        self.buffer.append(value)
        if len(self.buffer) > self.taps:
            self.buffer.pop(0)
        if len(self.buffer) < self.taps:
            return None
        return sum(c * v for c, v in zip(self.coeffs, self.buffer))


def run():
    print("AlgoForge consumer starting...")

    context = zmq.Context()
    socket  = context.socket(zmq.PULL)
    socket.connect("tcp://localhost:5555")

    print("Connected to C++ engine on tcp://localhost:5555\n")

    stats  = RollingStats(window=20)
    fir    = FIRFilter(taps=21)

    noise_data   = []
    current_regime = None

    while True:
        msg    = socket.recv_string()
        data   = json.loads(msg)

        # 1. Save candle
        candle, created = Candle.objects.get_or_create(
            symbol       = data['symbol'],
            timestamp    = data['timestamp'],
            interval_sec = data['interval_sec'],
            defaults={
                'open'  : data['open'],
                'high'  : data['high'],
                'low'   : data['low'],
                'close' : data['close'],
                'volume': data['volume'],
                'source': 'synthetic',
            }
        )

        if not created:
            continue

        close = data['close']

        # 2. Compute stats
        stats.update(close)
        filtered = fir.update(close)

        if not stats.ready() or filtered is None:
            print(f"Candle saved: {data['symbol']} C={close:.2f} (warming up...)")
            continue

        noise   = close - filtered
        zscore  = stats.zscore(close)

        # Rolling noise stats
        noise_data.append(noise)
        if len(noise_data) > 20:
            noise_data.pop(0)
        if len(noise_data) >= 2:
            noise_mean = sum(noise_data) / len(noise_data)
            noise_std  = (sum((x - noise_mean)**2 for x in noise_data)
                         / (len(noise_data) - 1)) ** 0.5
            noise_z    = (noise - noise_mean) / noise_std if noise_std > 0 else 0.0
        else:
            noise_z = 0.0

        # 3. Save signal snapshot
        SignalSnapshot.objects.create(
            candle       = candle,
            regime       = current_regime,
            rolling_mean = stats.mean,
            rolling_std  = stats.stddev(),
            zscore       = zscore,
            filtered     = filtered,
            noise        = noise,
            noise_zscore = noise_z,
            fft_period   = 64.0,  # static for now, FFT runs in C++
            fft_power    = 0.0,
        )

        # 4. Detect regime events
        event_type = None
        if zscore >= 2.0 and noise_z >= 1.5:
            event_type = 'BULLISH_EXTREME'
        elif zscore <= -2.0 and noise_z <= -1.5:
            event_type = 'BEARISH_EXTREME'

        if event_type:
            # Save market event
            MarketEvent.objects.create(
                symbol       = data['symbol'],
                event_type   = event_type,
                regime       = current_regime,
                price        = close,
                zscore       = zscore,
                noise        = noise,
                noise_zscore = noise_z,
                confidence   = min(1.0, abs(zscore) / 3.0),
                timestamp    = data['timestamp'],
            )
            print(f"🚨 {event_type} | Z={zscore:.2f} noise={noise:.2f}")

        print(f"✓ {data['symbol']} C={close:.2f} "
              f"Z={zscore:.3f} noise={noise:.3f} noise_z={noise_z:.3f}")


if __name__ == '__main__':
    run()