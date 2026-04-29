from rest_framework import viewsets, filters
from rest_framework.decorators import action
from rest_framework.response import Response
from django.utils import timezone
from .models import (Candle, SignalSnapshot, MarketEvent,
                     RegimeState, ExperimentRun, Prediction)
from .serializers import (CandleSerializer, SignalSnapshotSerializer,
                           MarketEventSerializer, RegimeStateSerializer,
                           ExperimentRunSerializer, PredictionSerializer)


class CandleViewSet(viewsets.ModelViewSet):
    queryset         = Candle.objects.all()
    serializer_class = CandleSerializer
    filter_backends  = [filters.OrderingFilter]
    ordering_fields  = ['timestamp']

    def get_queryset(self):
        qs     = super().get_queryset()
        symbol = self.request.query_params.get('symbol')
        limit  = self.request.query_params.get('limit')
        if symbol:
            qs = qs.filter(symbol=symbol)
        if limit:
            qs = qs.order_by('-timestamp')[:int(limit)]
        return qs

    @action(detail=False, methods=['get'])
    def latest(self, request):
        symbol = request.query_params.get('symbol', 'AAPL')
        candle = Candle.objects.filter(symbol=symbol).order_by('-timestamp').first()
        if not candle:
            return Response({'error': 'No candles found'}, status=404)
        return Response(CandleSerializer(candle).data)


class SignalSnapshotViewSet(viewsets.ModelViewSet):
    queryset         = SignalSnapshot.objects.all()
    serializer_class = SignalSnapshotSerializer

    def get_queryset(self):
        qs     = super().get_queryset()
        symbol = self.request.query_params.get('symbol')
        limit  = self.request.query_params.get('limit')
        if symbol:
            qs = qs.filter(candle__symbol=symbol)
        if limit:
            qs = qs[:int(limit)]
        return qs


class MarketEventViewSet(viewsets.ModelViewSet):
    queryset         = MarketEvent.objects.all()
    serializer_class = MarketEventSerializer

    def get_queryset(self):
        qs         = super().get_queryset()
        symbol     = self.request.query_params.get('symbol')
        event_type = self.request.query_params.get('event_type')
        if symbol:
            qs = qs.filter(symbol=symbol)
        if event_type:
            qs = qs.filter(event_type=event_type)
        return qs


class RegimeStateViewSet(viewsets.ModelViewSet):
    queryset         = RegimeState.objects.all()
    serializer_class = RegimeStateSerializer

    @action(detail=False, methods=['get'])
    def current(self, request):
        symbol = request.query_params.get('symbol', 'AAPL')
        regime = RegimeState.objects.filter(
            symbol=symbol,
            ended_at__isnull=True
        ).order_by('-started_at').first()
        if not regime:
            return Response({'error': 'No active regime'}, status=404)
        return Response(RegimeStateSerializer(regime).data)


class ExperimentRunViewSet(viewsets.ModelViewSet):
    queryset         = ExperimentRun.objects.all()
    serializer_class = ExperimentRunSerializer


class PredictionViewSet(viewsets.ModelViewSet):
    queryset         = Prediction.objects.all()
    serializer_class = PredictionSerializer

    def get_queryset(self):
        qs     = super().get_queryset()
        symbol = self.request.query_params.get('symbol')
        model  = self.request.query_params.get('model_name')
        if symbol:
            qs = qs.filter(symbol=symbol)
        if model:
            qs = qs.filter(model_name=model)
        return qs