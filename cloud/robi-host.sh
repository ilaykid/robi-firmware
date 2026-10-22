#!/bin/bash
# Robi: keep the HTTPS name and the server addresses in step with the public IP.
# Runs at boot and every 2 minutes (robi-host.timer). Does nothing if the IP is unchanged.
ROOT="@ROOT@"
PUB="@PUB@"
SECRET="@SECRET@"

IP=""
for u in https://api.ipify.org https://ifconfig.me/ip https://icanhazip.com; do
  IP=$(curl -fs --max-time 10 "$u" | tr -d '[:space:]')
  [[ "$IP" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] && break
  IP=""
done
[ -z "$IP" ] && exit 0
HOST="$(echo "$IP" | tr . -).sslip.io"
OLD=$(cat "$ROOT/host.txt" 2>/dev/null)
[ "$HOST" = "$OLD" ] && [ -f /etc/caddy/Caddyfile ] && exit 0

echo "$HOST" > "$ROOT/host.txt"
cat > /etc/caddy/Caddyfile <<CF
$HOST {
	handle_path /$SECRET/* {
		handle /xiaozhi/v1/* {
			reverse_proxy 127.0.0.1:8000
		}
		handle_path /_log/* {
			root * $PUB
			file_server browse
		}
		handle {
			reverse_proxy 127.0.0.1:8003
		}
	}
	handle {
		respond "Robi" 404
	}
}
CF
CFG="$ROOT/server/data/.config.yaml"
if [ -f "$CFG" ] && ! grep -q "__WS__" "$CFG"; then
  sed -i -E "s#^([[:space:]]*websocket:).*#\1 wss://$HOST/$SECRET/xiaozhi/v1/#; s#^([[:space:]]*vision_explain:).*#\1 https://$HOST/$SECRET/mcp/vision/explain#" "$CFG"
fi
echo "$(date -u) host ${OLD:-none} -> $HOST" >> "$PUB/host.log"
systemctl restart caddy 2>/dev/null
if [ -n "$OLD" ] && systemctl is-enabled robi >/dev/null 2>&1; then
  systemctl restart robi
fi
exit 0
