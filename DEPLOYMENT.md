# Deployment Guide

## Prerequisites

- Docker and Docker Compose installed
- TLS certificates (optional but recommended for production)
- Domain name configured

## Quick Start

1. **Configure your domain and admin token:**
   ```bash
   # Edit config/server.yml
   # Set your domain name
   # Change admin token from default
   ```

2. **Set environment variables (recommended for production):**
   ```bash
   export ADMIN_TOKEN="your-secure-admin-token"
   export TLS_CERT_PATH="/path/to/cert.pem"
   export TLS_KEY_PATH="/path/to/key.pem"
   ```

3. **Build and run:**
   ```bash
   docker-compose up -d
   ```

## Port Configuration

The server uses standard email ports:
- **25**: SMTP (requires root or `CAP_NET_BIND_SERVICE` in containers)
- **587**: SMTP submission port (currently not separately configured - use port 25)
- **143**: IMAP
- **993**: IMAP over TLS (configure TLS in server.yml)
- **8080**: Admin API
- **9090**: Metrics endpoint

**Note**: The current implementation supports one SMTP port configured in `server.yml`. For production, you may want to run separate instances for port 25 (server-to-server) and 587 (client submission), or use port 25 for both purposes.

### Running on Non-Privileged Ports

For development or if you can't bind to port 25, update `config/server.yml`:
```yaml
server:
  smtp_port: 2525  # Use non-privileged port
  imap_port: 1143  # Use non-privileged port
```

Then update `docker-compose.yml` port mappings accordingly.

## Security Considerations

1. **Change default admin token** in `config/server.yml` or set `ADMIN_TOKEN` environment variable
2. **Use TLS certificates** for production deployments
3. **Secure user passwords** - consider hashing in `config/users.yml`
4. **Firewall rules** - only expose necessary ports
5. **Volume permissions** - ensure data directory has correct permissions

## Health Checks

The container includes a health check that queries `http://localhost:8080/health`. Monitor this endpoint for service health.

## Troubleshooting

- **Port binding errors**: Ensure ports 25/587 aren't already in use, or use non-privileged ports
- **TLS errors**: Verify certificate paths and permissions
- **Config errors**: Check `config/server.yml` syntax and paths
- **Logs**: Check container logs with `docker-compose logs -f mailserver`

## Production Deployment

For production:
1. Use environment variables for all secrets
2. Mount TLS certificates securely
3. Set up proper backup for `./data` volume
4. Configure reverse proxy if needed
5. Set up monitoring and alerting
6. Review and harden security settings

