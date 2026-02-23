# --- TAHAP 1: BUILD ---
FROM gcc:12-bookworm as builder

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    libmariadb-dev-compat \
    libmariadb-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Process Compile (Now using the current directory as source)
RUN mkdir build && cd build && \
    cmake .. && \
    make

# --- TAHAP 2: RUNTIME ---
FROM debian:bookworm-slim
WORKDIR /app

RUN apt-get update && apt-get install -y \
    default-libmysqlclient-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy build result and necessary files
COPY --from=builder /app/build/karts-backend .
COPY --from=builder /app/ca.pem . 
COPY --from=builder /app/users.json .

EXPOSE 8080

CMD ["./karts-backend"]