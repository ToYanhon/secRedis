#!/bin/bash

echo "Testing connection to Redis server..."

# 测试SET命令
echo "Testing SET command..."
SET_RESULT=$(echo "SET testkey hello" | docker-compose exec -T redis-client /app/build/client/client redis-server 6379)
echo "SET result: $SET_RESULT"

# 测试GET命令  
echo "Testing GET command..."
GET_RESULT=$(echo "GET testkey" | docker-compose exec -T redis-client /app/build/client/client redis-server 6379)
echo "GET result: $GET_RESULT"

# 测试EXISTS命令
echo "Testing EXISTS command..."
EXISTS_RESULT=$(echo "EXISTS testkey" | docker-compose exec -T redis-client /app/build/client/client redis-server 6379)
echo "EXISTS result: $EXISTS_RESULT"

echo "Connection test completed!"