FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends python3 g++ libcairo2 libpangocairo-1.0-0 libsdl2-2.0-0 fonts-dejavu-core fonts-noto-cjk fonts-noto-color-emoji && rm -rf /var/lib/apt/lists/*
WORKDIR /work/explicit-ui
COPY . .
CMD ["./scripts/check.sh"]
