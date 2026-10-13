#!/bin/sh
set -e

ssh-keygen -A >/dev/null

# vsftpd hands the passive address out itself; from the host use the mapped
# ports, from the mc container use "remote" directly.
vsftpd /etc/vsftpd.conf &
smbd --daemon

# A second SSH endpoint is reserved for the SFTP keyboard-interactive cases.
# It deliberately has no password or public-key authentication, so the tests
# can prove that libssh2 invokes the challenge callback.
cat >/run/sshd-kbi.conf <<'EOF'
Port 2222
ListenAddress 0.0.0.0
HostKey /etc/ssh/ssh_host_ed25519_key
PidFile /run/sshd/kbi.pid
UsePAM yes
KbdInteractiveAuthentication yes
PasswordAuthentication no
PubkeyAuthentication no
AuthenticationMethods keyboard-interactive:pam
Subsystem sftp internal-sftp
AllowUsers mc
EOF
/usr/sbin/sshd -f /run/sshd-kbi.conf -D -e >/var/log/sshd-kbi.log 2>&1 &

exec /usr/sbin/sshd -D -e
