# Production Migration Guide

This guide helps you migrate your email server to production-grade security.

## Step 1: Migrate Passwords to Hashed Format

Your current `config/users.yml` contains plaintext passwords. These must be migrated to secure hashes.

### Option A: Use Migration Utility (Recommended)

1. Build the migration tool:
   ```bash
   cd build
   cmake ..
   cmake --build . --target migrate_passwords
   ```

2. Run migration (creates new file):
   ```bash
   ./migrate_passwords config/users.yml config/users.yml.new
   ```

3. Review the new file:
   ```bash
   cat config/users.yml.new
   ```

4. Backup and replace:
   ```bash
   cp config/users.yml config/users.yml.backup
   cp config/users.yml.new config/users.yml
   ```

### Option B: Manual Migration

For each user in `config/users.yml`, replace plaintext passwords:

**Before:**
```yaml
users:
  alice:
    password: "plaintext123"
```

**After:**
```yaml
users:
  alice:
    password: "$pbkdf2-sha256$100000$<salt>$<hash>"
```

The server will automatically hash new passwords, but you should migrate existing ones.

## Step 2: Update Configuration

1. **Change Admin Token:**
   ```yaml
   admin:
     token: "YOUR_SECURE_RANDOM_TOKEN_HERE"
   ```
   Or set via environment variable:
   ```bash
   export ADMIN_TOKEN="your-secure-token"
   ```

2. **Set Your Domain:**
   ```yaml
   server:
     domain: "yourdomain.com"  # CHANGE THIS
   ```

3. **Configure TLS Certificates:**
   ```yaml
   server:
     tls_cert: "/path/to/cert.pem"
     tls_key: "/path/to/key.pem"
   ```
   Or via environment:
   ```bash
   export TLS_CERT_PATH="/path/to/cert.pem"
   export TLS_KEY_PATH="/path/to/key.pem"
   ```

## Step 3: Review Security Settings

1. **Check Audit Logs:**
   - Audit logs are written to `logs/audit.log`
   - Monitor for authentication failures and security events

2. **Review Rate Limits:**
   - Default limits are in `src/core/rate_limiter.cpp`
   - Adjust based on your needs

3. **File Permissions:**
   ```bash
   chmod 600 config/users.yml
   chmod 600 config/server.yml
   ```

## Step 4: Test the Migration

1. **Start the server:**
   ```bash
   ./mailserver
   ```

2. **Test authentication:**
   - Try logging in with existing credentials
   - Check `logs/audit.log` for authentication events

3. **Verify password hashing:**
   - Check server logs for "hashed" vs "plaintext" counts
   - All passwords should show as hashed after migration

## Step 5: Production Deployment

1. **Set up monitoring:**
   - Health check: `http://localhost:8080/health`
   - Metrics: `http://localhost:9090/metrics`

2. **Configure backups:**
   - Backup `data/` directory regularly
   - Backup `config/users.yml` securely
   - Backup audit logs

3. **Set up log rotation:**
   - Logs rotate automatically at 100MB
   - Configure external log aggregation if needed

## Security Checklist

- [ ] All passwords migrated to hashed format
- [ ] Admin token changed from default
- [ ] Domain name configured
- [ ] TLS certificates configured
- [ ] File permissions set correctly
- [ ] Audit logging enabled and monitored
- [ ] Backups configured
- [ ] Monitoring and alerting set up
- [ ] Firewall rules configured
- [ ] Rate limits reviewed and adjusted

## Troubleshooting

### "Plaintext password detected" warnings

This means some users still have plaintext passwords. Run the migration utility again.

### Authentication failures after migration

1. Verify password hashes are correct
2. Check `logs/audit.log` for details
3. Ensure users.yml file is readable

### Health check failures

Check:
- Disk space (needs >1GB free)
- Data directory is writable
- Logger is functioning

## Post-Migration

After successful migration:

1. Remove plaintext password support (optional, for maximum security):
   - Edit `src/core/password_hash.cpp`
   - Remove the legacy plaintext comparison code
   - Rebuild and redeploy

2. Set up automated password rotation policy

3. Review audit logs regularly for security events

