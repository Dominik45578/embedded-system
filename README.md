# Środowisko Lokalne IoT - Instrukcja Uruchomieniowa

## Status projektu 

### Zrobione : podstawowa konfiguracja projektu : 
- github actions
- pliki env
- pliki docker-compose
- pliki configuracyjne obs

### TODO

- [ ] Konfiguracja Docker Hub
- [ ] Backend (2%)
- [ ] Frontend (0%)
- [~] Mobile (30%)
- [~] Firmware (50%)

---
## Uruchomienie środowiska

### 1. Utworzenie sieci współdzielonej

```bash
docker network create iot_shared_net || true
```

### 2. Narzędzia do obserwacji

```bash
docker compose --env-file .env.observability -f docker-compose.obs.yml up -d
```

### 2. Apka

```bash
docker compose --env-file .env.app -f docker-compose.app.yml up -d
```