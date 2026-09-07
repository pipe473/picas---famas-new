# syntax=docker/dockerfile:1
FROM node:20-bookworm AS web
WORKDIR /src/Tools/Sandbox/web-app
COPY Tools/Sandbox/web-app/package.json ./
RUN npm install
COPY Tools/Sandbox/web-app ./
RUN npm run build

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends g++ ca-certificates \
  && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY Source ./Source
COPY Tools/Sandbox/PFSandbox.cpp Tools/Sandbox/build.sh ./Tools/Sandbox/
COPY --from=web /src/Tools/Sandbox/web ./Tools/Sandbox/web
RUN g++ -std=c++20 -O2 -Wall -Wextra -I Source/PicasyFamas \
  Tools/Sandbox/PFSandbox.cpp Source/PicasyFamas/Core/PFRoundEngine.cpp \
  -o Tools/Sandbox/pf_sandbox
ENV PORT=8080
EXPOSE 8080
WORKDIR /app/Tools/Sandbox
CMD ["./pf_sandbox", "serve"]
