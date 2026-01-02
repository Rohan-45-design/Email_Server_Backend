# Email Server

A high-performance, secure, and feature-rich email server implementation in C++ with support for SMTP, IMAP, antivirus scanning, anti-spam filtering, and high availability.

## Features

- **SMTP Server**: Full SMTP implementation with TLS encryption
- **IMAP Server**: Complete IMAP protocol support for email access
- **Security Features**:
  - DKIM signing and verification
  - DMARC evaluation
  - SPF checking
  - TLS enforcement
  - Anti-spam filtering
- **Antivirus Integration**:
  - Cloud-based virus scanning (VirusTotal)
  - Sandbox analysis (Any.Run)
  - Local virus scanning capabilities
- **High Availability**: Leader election and distributed authentication
- **Monitoring & Metrics**: Prometheus-compatible metrics endpoint
- **Admin API**: RESTful API for server management
- **Threat Intelligence**: Hash reputation and sender reputation checking
- **Queue Management**: Robust mail queuing with retry mechanisms
- **Audit Logging**: Comprehensive logging for security and compliance

## Prerequisites

### System Requirements
- **Operating System**: Linux, Windows, or macOS
- **Compiler**: GCC 7+ or Clang 6+ (Linux/macOS), MSVC 2019+ (Windows)
- **CMake**: Version 3.16 or higher
- **OpenSSL**: Development libraries (1.1.1 or higher)

### Dependencies
- OpenSSL (for TLS and cryptography)
- yaml-cpp (for configuration)
- nlohmann/json (for JSON handling)
- hiredis (Redis client, optional for distributed auth)

## Installation

### Option 1: Docker (Recommended)

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd Email_server
   ```

2. Configure the server:
   ```bash
   cp config/example.server.yml config/server.yml
   # Edit config/server.yml with your settings
   ```

3. Build and run with Docker Compose:
   ```bash
   docker-compose up --build
   ```

### Option 2: Native Build

1. Install dependencies:

   **Ubuntu/Debian:**
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential cmake libssl-dev libyaml-cpp-dev
   ```

   **macOS (with Homebrew):**
   ```bash
   brew install cmake openssl yaml-cpp
   ```

   **Windows (with vcpkg):**
   ```bash
   vcpkg install openssl yaml-cpp
   ```

2. Clone and build:
   ```bash
   git clone <repository-url>
   cd Email_server
   mkdir build && cd build
   cmake ..
   cmake --build . --config Release
   ```

## Configuration

The server is configured via YAML files in the `config/` directory. Copy `config/example.server.yml` to `config/server.yml` and modify as needed.

### Key Configuration Options

```yaml
server:
  host: "0.0.0.0"          # Listen address
  smtp_port: 25            # SMTP port
  imap_port: 143           # IMAP port
  domain: "yourdomain.com" # Server domain
  mail_root: "mail_root"   # Mail storage directory

logging:
  file: "mailserver.log"   # Log file path
  level: "info"            # Log level (debug, info, warn, error)

auth:
  users_file: "users.yml"  # User authentication file

admin:
  token: "your-secret-token" # Admin API authentication token
```

### Environment Variables

When using Docker, you can override configuration with environment variables:

- `SMTP_PORT`: SMTP port (default: 25)
- `IMAP_PORT`: IMAP port (default: 143)
- `IMAPS_PORT`: IMAPS port (default: 993)
- `ADMIN_PORT`: Admin API port (default: 8080)
- `METRICS_PORT`: Metrics port (default: 9090)
- `ADMIN_TOKEN`: Admin API token
- `TLS_CERT_PATH`: Path to TLS certificate
- `TLS_KEY_PATH`: Path to TLS private key

## Usage

### Starting the Server

**Docker:**
```bash
docker-compose up -d
```

**Native:**
```bash
./build/mailserver
```

### Admin API

The server provides a RESTful admin API on port 8080 (configurable):

```bash
# Check server health
curl http://localhost:8080/health

# Get server metrics
curl http://localhost:8080/metrics

# List users (requires admin token)
curl -H "Authorization: Bearer YOUR_TOKEN" http://localhost:8080/users
```

### Monitoring

Prometheus-compatible metrics are available on port 9090:

```bash
curl http://localhost:9090/metrics
```

## Ports

The server uses the following ports by default:

- **25**: SMTP (server-to-server email)
- **143**: IMAP (unencrypted)
- **993**: IMAPS (encrypted IMAP)
- **8080**: Admin API
- **9090**: Metrics endpoint

## Security Considerations

1. **TLS Configuration**: Always configure TLS certificates for production use
2. **Admin Token**: Change the default admin token to a strong, random value
3. **Firewall**: Restrict access to admin and metrics ports
4. **User Authentication**: Use strong passwords and consider additional authentication methods
5. **Updates**: Keep dependencies and the server updated with security patches

## High Availability

The server supports high availability through leader election. Multiple instances can run, and one will be elected as the leader for operations requiring single coordination.

To enable HA mode:
1. Configure multiple instances
2. Ensure shared storage for mail data
3. Configure Redis for distributed authentication (optional)

## Development

### Building for Development

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -- -j$(nproc)
```

### Code Structure

```
src/
├── core/           # Core server components
├── smtp/           # SMTP server implementation
├── imap/           # IMAP server implementation
├── antispam/       # Anti-spam features (DKIM, DMARC, SPF)
├── antivirus/      # Virus scanning
├── ha/             # High availability
├── monitoring/     # Metrics and health checks
├── admin/          # Admin API
├── storage/        # Mail storage
├── queue/          # Mail queuing
└── dns/            # DNS resolution
```

### Testing

```bash
# Build tests (if available)
cmake -DBUILD_TESTS=ON ..
cmake --build . --target test
```

## Troubleshooting

### Common Issues

1. **Port conflicts**: Ensure ports 25, 143, 993, 8080, 9090 are available
2. **TLS errors**: Verify certificate paths and permissions
3. **Permission denied**: Check file permissions for config and data directories
4. **Build failures**: Ensure all dependencies are installed

### Logs

Check the log file specified in configuration (default: `mailserver.log`) for detailed error information.

### Health Checks

Use the admin API health endpoint to verify server status:
```bash
curl http://localhost:8080/health
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Support

For support and questions:
- Check the logs for error details
- Review the configuration documentation
- Open an issue on the project repository

## Roadmap

- [ ] Webmail interface
- [ ] Advanced filtering rules
- [ ] Database backend support
- [ ] REST API for email operations
- [ ] Plugin system
- [ ] Multi-domain support</content>
<parameter name="filePath">c:\Users\rohan\OneDrive\Desktop\Email_server\README.md