# AlgoForge

An end-to-end algorithmic trading engine built as a systematic revision of core 
engineering disciplines — data structures, statistics, digital signal processing, 
machine learning, and systems programming.

**Core:** C++17  
**Backend:** Django REST + ZeroMQ  
**Frontend:** React  

## Architecture
- C++ engine: market data ingestion, ring buffer, stats engine, DSP layer, ML inference
- Python/Django: persistence, API, ML training pipeline
- React: live trading dashboard

## Modules
- [x] Ring buffer (lock-free, lossy, O(1))
- [x] Deterministic synthetic candle generator
- [x] Welford online stats engine (global + rolling window)
- [x] ZeroMQ pub/sub pipeline
- [ ] DSP layer (FFT, FIR/IIR filters)
- [ ] Monte Carlo simulation
- [ ] Rare event characterization
- [ ] Django REST API
- [ ] React dashboard
