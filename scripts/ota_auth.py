import re
Import("env")

try:
    with open("include/secrets.h", "r", encoding="utf-8") as f:
        content = f.read()

    match = re.search(r'OTA_PASSWORD\s*=\s*"([^"]+)"', content)
    if match:
        password = match.group(1)
        print("[OTA] Senha encontrada em secrets.h — configurando auth...")
        env.Append(UPLOADERFLAGS=["--auth=" + password])
        env.Append(UPLOAD_FLAGS=["--auth=" + password])
    else:
        print("[OTA] AVISO: OTA_PASSWORD nao encontrado em secrets.h")
except FileNotFoundError:
    print("[OTA] AVISO: secrets.h nao encontrado — senha OTA nao configurada")
except Exception as e:
    print(f"[OTA] ERRO no script ota_auth.py: {e}")
