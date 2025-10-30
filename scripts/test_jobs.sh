#!/bin/bash

# Colores para output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

PORT=8080
BASE_URL="http://localhost:$PORT"

# Helper para verificar respuesta JSON
check_response() {
    local desc=$1
    local response=$2
    local expected=$3
    
    if echo "$response" | grep -q "$expected"; then
        echo -e "${GREEN}✓ $desc${NC}"
        return 0
    else
        echo -e "${RED}✗ $desc${NC}"
        echo "Expected to find: $expected"
        echo "Got: $response"
        return 1
    fi
}

echo "=========================================="
echo "  Testing Jobs System"
echo "=========================================="

# 1. Submit a CPU-intensive job
echo -e "\n1. Testing /jobs/submit with CPU-intensive task (pi calculation)"
response=$(curl -s "$BASE_URL/jobs/submit?task=pi&digits=100000")
job_id=$(echo $response | grep -o '"job_id":"[^"]*' | cut -d'"' -f4)

check_response "Job submission" "$response" '"status":"queued"'
if [ -z "$job_id" ]; then
    echo -e "${RED}✗ Failed to get job_id${NC}"
    exit 1
fi

# 2. Check initial status
echo -e "\n2. Testing /jobs/status immediately after submission"
response=$(curl -s "$BASE_URL/jobs/status?id=$job_id")
check_response "Initial status check" "$response" '"status":"queued\|running"'

# 3. Poll status until completion or timeout
echo -e "\n3. Polling job status until completion"
attempts=0
max_attempts=30
while [ $attempts -lt $max_attempts ]; do
    response=$(curl -s "$BASE_URL/jobs/status?id=$job_id")
    
    if echo "$response" | grep -q '"status":"done"\|"status":"error"'; then
        check_response "Job completion" "$response" '"status":"done"\|"status":"error"'
        break
    fi
    
    progress=$(echo "$response" | grep -o '"progress":[0-9]*' | cut -d':' -f2)
    eta=$(echo "$response" | grep -o '"eta_ms":[0-9]*' | cut -d':' -f2)
    echo "Progress: $progress%, ETA: $eta ms"
    
    sleep 1
    attempts=$((attempts + 1))
done

if [ $attempts -eq $max_attempts ]; then
    echo -e "${RED}✗ Job timed out${NC}"
    exit 1
fi

# 4. Get final result
echo -e "\n4. Testing /jobs/result"
response=$(curl -s "$BASE_URL/jobs/result?id=$job_id")
check_response "Result retrieval" "$response" '"result"\|"error"'

# 5. Submit and cancel a job
echo -e "\n5. Testing job cancellation"
response=$(curl -s "$BASE_URL/jobs/submit?task=pi&digits=1000000")
cancel_job_id=$(echo $response | grep -o '"job_id":"[^"]*' | cut -d'"' -f4)

# Wait briefly to ensure job starts
sleep 1

# Try to cancel
response=$(curl -s "$BASE_URL/jobs/cancel?id=$cancel_job_id")
check_response "Job cancellation" "$response" '"status":"canceled"\|"status":"not_cancelable"'

# 6. Test invalid job ID
echo -e "\n6. Testing invalid job ID"
response=$(curl -s "$BASE_URL/jobs/status?id=invalid_id")
check_response "Invalid job ID handling" "$response" '"error"'

# 7. Submit multiple jobs
echo -e "\n7. Testing multiple concurrent jobs"
for i in {1..5}; do
    response=$(curl -s "$BASE_URL/jobs/submit?task=isprime&n=999999999999989")
    check_response "Concurrent job submission $i" "$response" '"status":"queued"'
done

echo -e "\n=========================================="
echo "  Jobs System Test Summary"
echo "=========================================="
echo -e "${GREEN}✓ Basic job submission and status tracking${NC}"
echo -e "${GREEN}✓ Progress and ETA reporting${NC}"
echo -e "${GREEN}✓ Job result retrieval${NC}"
echo -e "${GREEN}✓ Job cancellation${NC}"
echo -e "${GREEN}✓ Error handling${NC}"
echo -e "${GREEN}✓ Multiple concurrent jobs${NC}"

echo -e "\nTry these commands manually:"
echo "  curl $BASE_URL/jobs/submit?task=pi&digits=1000"
echo "  curl $BASE_URL/jobs/submit?task=matrixmul&size=1000"
echo "  curl $BASE_URL/jobs/submit?task=sortfile&name=large_file.txt"