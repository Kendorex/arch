# Builder stage
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Build POCO from source
ARG POCO_VERSION=1.15.0
WORKDIR /build
RUN git clone --depth 1 --branch poco-${POCO_VERSION}-release https://github.com/pocoproject/poco.git

WORKDIR /build/poco
RUN mkdir cmake-build && cd cmake-build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build . --target install -j$(nproc)

# Build application
WORKDIR /build/app
COPY CMakeLists.txt ./
COPY src ./src/

# Собираем без тестов
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local -DSKIP_TESTS=ON \
    && cmake --build . -j$(nproc)

# Runner stage
FROM ubuntu:24.04 AS runner

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy POCO shared libraries from builder
COPY --from=builder /usr/local/lib/libPoco*.so* /usr/local/lib/

# Copy application binary
COPY --from=builder /build/app/build/poco_template_server /usr/local/bin/

# Create non-root user
RUN groupadd -r carrental && useradd -r -g carrental carrental && \
    chown -R carrental:carrental /usr/local/bin/poco_template_server

ENV LD_LIBRARY_PATH=/usr/local/lib

ENV PORT=8080
ENV LOG_LEVEL=information
ENV JWT_SECRET=change-me-in-production

EXPOSE 8080

USER carrental

CMD ["poco_template_server"]