from rest_framework import serializers
from .models import Candle, SignalSnapshot, MarketEvent, RegimeState, ExperimentRun, Prediction


class CandleSerializer(serializers.ModelSerializer):
    class Meta:
        model  = Candle
        fields = '__all__'


class SignalSnapshotSerializer(serializers.ModelSerializer):
    class Meta:
        model  = SignalSnapshot
        fields = '__all__'


class MarketEventSerializer(serializers.ModelSerializer):
    class Meta:
        model  = MarketEvent
        fields = '__all__'


class RegimeStateSerializer(serializers.ModelSerializer):
    class Meta:
        model  = RegimeState
        fields = '__all__'


class ExperimentRunSerializer(serializers.ModelSerializer):
    class Meta:
        model  = ExperimentRun
        fields = '__all__'


class PredictionSerializer(serializers.ModelSerializer):
    class Meta:
        model  = Prediction
        fields = '__all__'