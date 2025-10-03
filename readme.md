docker compose build
docker compose up -d
docker compose exec redis-client /app/build/client/client redis-server 6379