# --- TAHAP 1: BUILD ---
# Use an image that has GCC 12 installed on Debian Bookworm as the build environment
FROM gcc:12-bookworm as builder

# Install dependencies
# Update package lists and install CMake and MariaDB development headers
RUN apt-get update && apt-get install -y \
    cmake \
    libmariadb-dev-compat \
    libmariadb-dev \
    && rm -rf /var/lib/apt/lists/* # Clean up apt cache to keep image size small

# Set the working directory inside the container to /app
WORKDIR /app
# Copy all files from the host to the container's working directory
COPY . .

# Process Compile (Now using the current directory as source)
# Create a build directory, change into it, run CMake to configure the project, then compile it
RUN mkdir build && cd build && \
    cmake .. && \
    make

# --- TAHAP 2: RUNTIME ---
# Use a slim version of Debian Bookworm to run the compiled application without compiler bulk
FROM debian:bookworm-slim
# Set the working directory for the runtime container
WORKDIR /app

# Install runtime dependencies: MySQL client library for DB connections, and CA certificates for SSL
RUN apt-get update && apt-get install -y \
    default-libmysqlclient-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* # Clean up apt cache to reduce image size

# Copy build result and necessary files
# Copy the compiled binary from the 'builder' stage
COPY --from=builder /app/build/karts-backend .
# Copy the database SSL certificate from the 'builder' stage 
COPY --from=builder /app/ca.pem . 
# Copy the static users.json file from the 'builder' stage
COPY --from=builder /app/users.json .

# Expose port 8080 so external requests can reach the backend server
EXPOSE 8080

# Specify what command to run when the container starts
CMD ["./karts-backend"]