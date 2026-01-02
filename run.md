# Email Server Testing Commands

This document provides a comprehensive list of terminal commands to test the email server functionality.

## Prerequisites Setup

### 1. Install Dependencies (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev libyaml-cpp-dev curl telnet netcat-openbsd mailutils swaks
```

### 2. Install Dependencies (macOS)
```bash
brew install cmake openssl yaml-cpp curl telnet nc mailutils swaks
```

### 3. Install Dependencies (Windows with Chocolatey)
```powershell
choco install cmake git openssl curl telnet nc swaks
```

## Build and Run Commands

### 1. Build the Server (Native)
```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the project
cmake --build . --config Release -- -j$(nproc)

# Return to project root
cd ..
```

### 2. Run the Server (Native)
```bash
# Run from build directory
./build/mailserver

# Or run from project root (after build)
./build/mailserver
```

### 3. Run with Docker
```bash
# Build and run with Docker Compose
docker-compose up --build

# Run in background
docker-compose up -d --build

# View logs
docker-compose logs -f mailserver

# Stop the server
docker-compose down
```

## Configuration Testing

### 4. Validate Configuration
```bash
# Check if config file exists and is valid YAML
cat config/server.yml

# Test YAML syntax
python3 -c "import yaml; yaml.safe_load(open('config/server.yml'))" && echo "YAML is valid"
```

### 5. Create Test Users
```bash
# Create users.yml file
cat > config/users.yml << 'EOF'
users:
  - username: test@example.com
    password: testpass123
  - username: admin@example.com
    password: adminpass123
EOF
```

## SMTP Testing

### 6. Test SMTP Connection (Basic)
```bash
# Test SMTP port connectivity
telnet localhost 25

# Or with netcat
nc -v localhost 25
```

### 7. Send Test Email with swaks
```bash
# Send a simple test email
swaks --to test@example.com --from admin@example.com --server localhost --port 25 --data "Subject: Test Email\n\nThis is a test email body."

# Send email with authentication
swaks --to test@example.com --from admin@example.com --server localhost --port 25 --auth-user admin@example.com --auth-password adminpass123 --tls

# Send email with attachment
echo "Test attachment content" > test.txt
swaks --to test@example.com --from admin@example.com --server localhost --port 25 --attach test.txt --body "Email with attachment"
```

### 8. Send Email with mail Command
```bash
# Send email using mail command
echo "Test email body" | mail -s "Test Subject" -r admin@example.com test@example.com

# Send to local server
echo "Test email body" | mail -s "Test Subject" -S smtp=localhost -r admin@example.com test@example.com
```

## IMAP Testing

### 9. Test IMAP Connection
```bash
# Test IMAP port connectivity
telnet localhost 143

# Or with netcat
nc -v localhost 143
```

### 10. Test IMAP with OpenSSL (TLS)
```bash
# Connect to IMAPS port with TLS
openssl s_client -connect localhost:993 -crlf
```

### 11. Test IMAP with Python Script
```python
# Create a test script
cat > test_imap.py << 'EOF'
import imaplib
import email

# Connect to IMAP server
mail = imaplib.IMAP4('localhost')
mail.login('test@example.com', 'testpass123')

# Select inbox
mail.select('inbox')

# Get emails
status, messages = mail.search(None, 'ALL')
if messages[0]:
    for msg_id in messages[0].split():
        status, msg_data = mail.fetch(msg_id, '(RFC822)')
        email_body = msg_data[0][1]
        mail_message = email.message_from_bytes(email_body)
        print(f"Subject: {mail_message['Subject']}")
        print(f"From: {mail_message['From']}")

mail.logout()
EOF

# Run the IMAP test
python3 test_imap.py
```

## Admin API Testing

### 12. Test Admin API Health Check
```bash
# Check server health
curl -v http://localhost:8080/health

# Check with authentication
curl -H "Authorization: Bearer CHANGE_ME_SUPER_SECRET" http://localhost:8080/health
```

### 13. Test Admin API Endpoints
```bash
# Get server status
curl -H "Authorization: Bearer CHANGE_ME_SUPER_SECRET" http://localhost:8080/status

# List users
curl -H "Authorization: Bearer CHANGE_ME_SUPER_SECRET" http://localhost:8080/users

# Get metrics
curl -H "Authorization: Bearer CHANGE_ME_SUPER_SECRET" http://localhost:8080/metrics

# Test invalid token
curl -H "Authorization: Bearer invalid_token" http://localhost:8080/health
```

## Metrics Testing

### 14. Test Metrics Endpoint
```bash
# Get Prometheus metrics
curl http://localhost:9090/metrics

# Check specific metrics
curl http://localhost:9090/metrics | grep -E "(smtp|imap|emails)"

# Monitor metrics over time
watch -n 5 'curl -s http://localhost:9090/metrics | grep emails_sent_total'
```

## Security Testing

### 15. Test TLS/SSL Configuration
```bash
# Test SMTP over TLS
openssl s_client -connect localhost:587 -starttls smtp

# Test IMAP over TLS
openssl s_client -connect localhost:993

# Check certificate details
openssl s_client -connect localhost:993 -servername localhost | openssl x509 -noout -text
```

### 16. Test Authentication
```bash
# Test SMTP AUTH
swaks --to test@example.com --from admin@example.com --server localhost --port 587 --tls --auth-user admin@example.com --auth-password wrongpassword

# Test with correct password
swaks --to test@example.com --from admin@example.com --server localhost --port 587 --tls --auth-user admin@example.com --auth-password adminpass123
```

## Load Testing

### 17. Basic Load Test with SMTP
```bash
# Send multiple emails
for i in {1..10}; do
  swaks --to test@example.com --from admin@example.com --server localhost --port 25 --data "Subject: Load Test $i\n\nTest email $i" &
done
wait
```

### 18. Concurrent Connection Test
```bash
# Test multiple concurrent connections
for i in {1..5}; do
  (echo "EHLO test.com"; echo "MAIL FROM:<test@example.com>"; echo "RCPT TO:<recipient@example.com>"; echo "DATA"; echo "Subject: Concurrent Test"; echo ""; echo "Test body"; echo "."; echo "QUIT") | nc localhost 25 &
done
wait
```

## Log Monitoring

### 19. Monitor Server Logs
```bash
# Monitor log file in real-time
tail -f mailserver.log

# Search for specific log entries
grep "SMTP" mailserver.log
grep "IMAP" mailserver.log
grep "ERROR" mailserver.log

# Count successful deliveries
grep "delivered successfully" mailserver.log | wc -l
```

### 20. Docker Log Monitoring
```bash
# View Docker container logs
docker-compose logs -f

# View specific service logs
docker-compose logs -f mailserver

# Search logs for errors
docker-compose logs | grep ERROR
```

## Cleanup and Maintenance

### 21. Clean Build
```bash
# Remove build artifacts
rm -rf build/

# Clean Docker
docker-compose down -v
docker system prune -f
```

### 22. Reset Test Data
```bash
# Remove test emails/data
rm -rf data/
rm -f mailserver.log

# Recreate data directory
mkdir -p data
```

### 23. Full System Test Script
```bash
# Create comprehensive test script
cat > full_test.sh << 'EOF'
#!/bin/bash
echo "=== Email Server Full Test ==="

# Build server
echo "Building server..."
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . -- -j$(nproc)
cd ..

# Start server in background
echo "Starting server..."
./build/mailserver &
SERVER_PID=$!
sleep 5

# Test health
echo "Testing health endpoint..."
curl -s http://localhost:8080/health

# Send test email
echo "Sending test email..."
swaks --to test@example.com --from admin@example.com --server localhost --port 25 --data "Subject: Full Test\n\nThis is a comprehensive test."

# Test metrics
echo "Checking metrics..."
curl -s http://localhost:9090/metrics | head -20

# Stop server
echo "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo "=== Test Complete ==="
EOF

# Make executable and run
chmod +x full_test.sh
./full_test.sh
```

## Troubleshooting Commands

### 24. Check Port Usage
```bash
# Check if ports are in use
netstat -tlnp | grep -E "(25|143|993|8080|9090)"

# Kill process using port
sudo fuser -k 25/tcp
```

### 25. Debug Server Startup
```bash
# Run with verbose logging
CONFIG_PATH=config/server.yml ./build/mailserver

# Check server process
ps aux | grep mailserver

# Debug with gdb
gdb ./build/mailserver
```

### 26. Network Connectivity Tests
```bash
# Test local connectivity
ping localhost

# Test port accessibility
nmap -p 25,143,993,8080,9090 localhost

# Check firewall rules
sudo ufw status
```

## Performance Testing

### 27. Benchmark SMTP Performance
```bash
# Install apache benchmark tools
sudo apt-get install apache2-utils

# Benchmark admin API
ab -n 1000 -c 10 http://localhost:8080/health
```

### 28. Memory and CPU Monitoring
```bash
# Monitor server process
top -p $(pgrep mailserver)

# Check memory usage
ps aux --sort=-%mem | head

# Monitor with htop (if installed)
htop
```

## Integration Testing

### 29. Test with Real Email Clients
```bash
# Test with Thunderbird/Mail client
echo "Configure email client with:"
echo "  Server: localhost"
echo "  SMTP Port: 25 or 587"
echo "  IMAP Port: 143 or 993"
echo "  Username: test@example.com"
echo "  Password: testpass123"
```

### 30. Test with Mail Command Line Tools
```bash
# Send email with sendmail-style interface
sendmail -t <<EOF
To: test@example.com
Subject: CLI Test
From: admin@example.com

This is a test email sent via sendmail interface.
EOF
```

## Quick Test Suite

### 31. Run All Basic Tests
```bash
# Quick test script
cat > quick_test.sh << 'EOF'
#!/bin/bash
echo "=== Quick Email Server Test ==="

# Check if server is running
if ! pgrep mailserver > /dev/null; then
    echo "Starting server..."
    ./build/mailserver &
    SERVER_PID=$!
    sleep 3
fi

# Test 1: Health check
echo "Test 1: Health Check"
curl -s http://localhost:8080/health && echo " ✓ PASS" || echo " ✗ FAIL"

# Test 2: SMTP connection
echo "Test 2: SMTP Connection"
echo "QUIT" | nc -w 5 localhost 25 > /dev/null && echo " ✓ PASS" || echo " ✗ FAIL"

# Test 3: IMAP connection
echo "Test 3: IMAP Connection"
echo "a001 LOGOUT" | nc -w 5 localhost 143 > /dev/null && echo " ✓ PASS" || echo " ✗ FAIL"

# Test 4: Metrics
echo "Test 4: Metrics Endpoint"
curl -s http://localhost:9090/metrics > /dev/null && echo " ✓ PASS" || echo " ✗ FAIL"

# Test 5: Send email
echo "Test 5: Send Email"
swaks --to test@example.com --from admin@example.com --server localhost --port 25 --data "Subject: Quick Test\n\nTest" --quiet && echo " ✓ PASS" || echo " ✗ FAIL"

echo "=== Tests Complete ==="

# Cleanup
if [ ! -z "$SERVER_PID" ]; then
    kill $SERVER_PID 2>/dev/null
fi
EOF

chmod +x quick_test.sh
./quick_test.sh
```</content>
<parameter name="filePath">c:\Users\rohan\OneDrive\Desktop\Email_server\run.md