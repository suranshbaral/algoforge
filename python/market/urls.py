from django.urls import path, include
from rest_framework.routers import DefaultRouter
from .views import (CandleViewSet, SignalSnapshotViewSet, MarketEventViewSet,
                    RegimeStateViewSet, ExperimentRunViewSet, PredictionViewSet)

router = DefaultRouter()
router.register(r'candles',     CandleViewSet)
router.register(r'signals',     SignalSnapshotViewSet)
router.register(r'events',      MarketEventViewSet)
router.register(r'regimes',     RegimeStateViewSet)
router.register(r'experiments', ExperimentRunViewSet)
router.register(r'predictions', PredictionViewSet)

urlpatterns = [
    path('', include(router.urls)),
]