# --- TAHAP 1: BUILD ---
FROM gcc:12-bookworm as builder

# Hanya instal library yang benar-benar dibutuhkan
RUN apt-get update && apt-get install -y \
    cmake \
    libmariadb-dev-compat \
    libmariadb-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Proses Compile (arahkan ke folder karts-backend)
RUN mkdir build && cd build && \
    cmake ../karts-backend && \
    make

# --- TAHAP 2: RUNTIME ---
FROM debian:bookworm-slim
WORKDIR /app

RUN apt-get update && apt-get install -y \
    default-libmysqlclient-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy hasil compile dan ca.pem
COPY --from=builder /app/build/karts-backend .
COPY --from=builder /app/karts-backend/ca.pem . 
COPY --from=builder /app/karts-backend/users.json .

EXPOSE 8080

CMD ["./karts-backend"]