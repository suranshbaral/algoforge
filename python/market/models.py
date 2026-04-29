from django.db import models


class Candle(models.Model):
    SOURCE_CHOICES = [
        ('synthetic', 'Synthetic'),
        ('alpaca',    'Alpaca'),
        ('yahoo',     'Yahoo Finance'),
        ('manual',    'Manual'),
    ]

    symbol       = models.CharField(max_length=10, db_index=True)
    open         = models.FloatField()
    high         = models.FloatField()
    low          = models.FloatField()
    close        = models.FloatField()
    volume       = models.FloatField()
    timestamp    = models.BigIntegerField(db_index=True)
    interval_sec = models.IntegerField()
    source       = models.CharField(max_length=20,
                                    choices=SOURCE_CHOICES,
                                    default='synthetic')
    created_at   = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['timestamp']
        unique_together = ['symbol', 'timestamp', 'interval_sec']
        indexes = [
            models.Index(fields=['symbol', 'timestamp']),
        ]

    def __str__(self):
        return f"{self.symbol} @ {self.timestamp} C={self.close}"


class RegimeState(models.Model):
    REGIME_CHOICES = [
        ('UNKNOWN',         'Unknown'),
        ('NEUTRAL',         'Neutral'),
        ('BULLISH_IMPULSE', 'Bullish Impulse'),
        ('BEARISH_IMPULSE', 'Bearish Impulse'),
        ('BULLISH_EXTREME', 'Bullish Extreme'),
        ('BEARISH_EXTREME', 'Bearish Extreme'),
        ('REGIME_SHIFT',    'Regime Shift'),
        ('TRENDING',        'Trending'),
        ('CYCLING',         'Cycling'),
        ('NOISY',           'Noisy'),
    ]

    symbol            = models.CharField(max_length=10, db_index=True)
    regime            = models.CharField(max_length=20, choices=REGIME_CHOICES)
    previous_regime   = models.CharField(max_length=20, choices=REGIME_CHOICES,
                                         default='UNKNOWN')
    confidence        = models.FloatField()
    started_at        = models.BigIntegerField()   # Unix ms
    ended_at          = models.BigIntegerField(null=True, blank=True)
    duration_candles  = models.IntegerField(default=0)
    avg_zscore        = models.FloatField(default=0.0)
    avg_noise         = models.FloatField(default=0.0)
    fft_period        = models.FloatField(default=64.0)
    transition_reason = models.TextField(blank=True)
    created_at        = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-started_at']

    def __str__(self):
        return f"{self.symbol} {self.regime} started={self.started_at}"


class SignalSnapshot(models.Model):
    candle        = models.OneToOneField(Candle,
                                         on_delete=models.CASCADE,
                                         related_name='signal')
    regime        = models.ForeignKey(RegimeState,
                                       on_delete=models.SET_NULL,
                                       null=True, blank=True,
                                       related_name='snapshots')
    # Stats engine outputs
    rolling_mean  = models.FloatField()
    rolling_std   = models.FloatField()
    zscore        = models.FloatField()

    # DSP outputs
    filtered      = models.FloatField()   # FIR filtered price
    noise         = models.FloatField()   # raw - filtered
    noise_zscore  = models.FloatField()

    # FFT
    fft_period    = models.FloatField()   # dominant period in candles
    fft_power     = models.FloatField()   # power of dominant bin

    created_at    = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-candle__timestamp']

    def __str__(self):
        return f"Signal for {self.candle}"


class MarketEvent(models.Model):
    EVENT_CHOICES = [
        ('BULLISH_EXTREME', 'Bullish Extreme'),
        ('BEARISH_EXTREME', 'Bearish Extreme'),
        ('REGIME_SHIFT',    'Regime Shift'),
        ('BULLISH_IMPULSE', 'Bullish Impulse'),
        ('BEARISH_IMPULSE', 'Bearish Impulse'),
    ]

    symbol     = models.CharField(max_length=10, db_index=True)
    event_type = models.CharField(max_length=20, choices=EVENT_CHOICES)
    regime     = models.ForeignKey(RegimeState,
                                    on_delete=models.SET_NULL,
                                    null=True, blank=True,
                                    related_name='events')
    price      = models.FloatField()
    zscore     = models.FloatField()
    noise      = models.FloatField()
    noise_zscore = models.FloatField()
    confidence = models.FloatField()
    timestamp  = models.BigIntegerField(db_index=True)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-timestamp']

    def __str__(self):
        return f"{self.symbol} {self.event_type} @ {self.timestamp}"


class ExperimentRun(models.Model):
    STATUS_CHOICES = [
        ('running',   'Running'),
        ('completed', 'Completed'),
        ('failed',    'Failed'),
    ]

    name        = models.CharField(max_length=100)
    symbol      = models.CharField(max_length=10)
    status      = models.CharField(max_length=20,
                                    choices=STATUS_CHOICES,
                                    default='running')
    start_ts    = models.BigIntegerField()
    end_ts      = models.BigIntegerField()
    parameters  = models.JSONField(default=dict)
    results     = models.JSONField(default=dict, blank=True)
    notes       = models.TextField(blank=True)
    created_at  = models.DateTimeField(auto_now_add=True)
    updated_at  = models.DateTimeField(auto_now=True)

    class Meta:
        ordering = ['-created_at']

    def __str__(self):
        return f"{self.name} [{self.status}]"


class Prediction(models.Model):
    symbol        = models.CharField(max_length=10, db_index=True)
    model_name    = models.CharField(max_length=50)
    model_version = models.CharField(max_length=20, default='v1')
    predicted     = models.FloatField()
    actual        = models.FloatField(null=True, blank=True)
    confidence    = models.FloatField()
    horizon       = models.IntegerField()  # candles ahead
    timestamp     = models.BigIntegerField(db_index=True)
    experiment    = models.ForeignKey(ExperimentRun,
                                       on_delete=models.SET_NULL,
                                       null=True, blank=True,
                                       related_name='predictions')
    created_at    = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-timestamp']

    def __str__(self):
        return f"{self.model_name} {self.symbol} pred={self.predicted}"