"""Build the Oracle cloud-init file for Robi (v2) from this PC's working config.

usage: python3 make_cloud_init.py <data_dir> <cloud_dir> <secret> <device_id> <out.yaml>
  <data_dir>  : the PC server's data folder (.config.yaml, robi-prompt.txt)
  <cloud_dir> : folder with setup.sh, robi-host.sh, gemini_audio.py
The API keys are copied from <data_dir>/.config.yaml into the output file only;
nothing secret is printed.

v2: Gemini hears the audio (Groq Whisper as backup), VAD tuned for children,
the public name follows the IP (reserved IP can be attached later), and the
Oracle Run Command user may administer the VM.
"""
import base64
import gzip
import re
import sys

data_dir, cloud_dir, secret, device_id, out_path = sys.argv[1:6]
cfg = open(f"{data_dir}/.config.yaml", encoding="utf-8").read().replace("\r\n", "\n")
prompt = open(f"{data_dir}/robi-prompt.txt", encoding="utf-8").read().replace("\r\n", "\n")
setup = open(f"{cloud_dir}/setup.sh", encoding="utf-8").read().replace("__SECRET__", secret)
host_sh = open(f"{cloud_dir}/robi-host.sh", encoding="utf-8").read()
gemini_py = open(f"{cloud_dir}/gemini_audio.py", encoding="utf-8").read()
assert re.fullmatch(r"[A-Za-z0-9_-]{8,}", secret), "secret must be letters/digits"


def block(text: str, name: str) -> str:
    """the indented lines of a 2-space YAML entry like '  GroqASR:'"""
    m = re.search(rf"(?m)^  {re.escape(name)}:\s*\n((?:    .*\n|\s*\n)+)", text)
    assert m, f"section {name} not found"
    return m.group(1)


def field(blk: str, key: str) -> str:
    m = re.search(rf"(?m)^    {key}:\s*(.+?)\s*$", blk)
    assert m, f"{key} not found"
    return m.group(1)


# ---- server addresses are filled in on the VM, once its public name is known ----
cfg, n1 = re.subn(r"(?m)^(\s*websocket:\s*).*$", r"\1__WS__", cfg, count=1)
cfg, n2 = re.subn(r"(?m)^(\s*vision_explain:\s*).*$", r"\1__VISION__", cfg, count=1)
assert n1 == 1 and n2 == 1, "server lines not found"

# ---- only Robi may connect (the OTA address itself is also secret) ----
if "auth:" not in cfg:
    cfg, n3 = re.subn(
        r"(?m)^server:\s*\n",
        "server:\n  auth:\n    enabled: true\n    allowed_devices:\n"
        f'      - "{device_id}"\n',
        cfg,
        count=1,
    )
    assert n3 == 1, "server: section not found"

# Israel is UTC+3 in summer, +2 in winter; the device gets its clock from here
cfg = re.sub(r"(?m)^(\s*timezone_offset:\s*)\+?\d+", r"\g<1>+2", cfg, count=1)

# ---- Gemini ears with Groq Whisper as backup ----
groq = block(cfg, "GroqASR")
gem = block(cfg, "RobiGemini")
if "RobiGeminiASR:" not in cfg:
    asr = (
        "ASR:\n"
        "  RobiGeminiASR:\n"
        "    type: gemini_audio\n"
        f"    api_key: {field(gem, 'api_key')}\n"
        "    model_name: gemini-3.5-flash-lite\n"
        f"    prompt: {field(groq, 'prompt')}\n"
        "    timeout: 10\n"
        f"    fallback_base_url: {field(groq, 'base_url')}\n"
        f"    fallback_api_key: {field(groq, 'api_key')}\n"
        f"    fallback_model: {field(groq, 'model_name')}\n"
        "    output_dir: tmp/\n"
    )
    cfg, n4 = re.subn(r"(?m)^ASR:[ \t]*\n", asr, cfg, count=1)
    assert n4 == 1, "ASR: section not found"
cfg, n5 = re.subn(r"(?m)^([ \t]+ASR:[ \t]*)\w+", r"\g<1>RobiGeminiASR", cfg, count=1)
assert n5 == 1, "selected_module ASR not found"

# ---- VAD: hear quieter children's voices, still wait for them to finish ----
vad = (
    "VAD:\n  SileroVAD:\n    threshold: 0.4\n    threshold_low: 0.25\n"
    "    min_silence_duration_ms: 700\n"
)
cfg, n6 = re.subn(r"(?m)^VAD:\s*\n  SileroVAD:\s*\n(?:    .*\n)+", vad, cfg, count=1)
assert n6 == 1, "VAD section not found"


def gz64(s: str) -> str:
    return base64.b64encode(gzip.compress(s.encode("utf-8"), 9)).decode("ascii")


out = f"""#cloud-config
package_update: true
packages:
  - python3-venv
  - python3-pip
  - python3-dev
  - build-essential
  - libopus0
  - ffmpeg
  - unzip
  - curl
write_files:
  - path: /etc/sudoers.d/90-robi-runcommand
    permissions: '0440'
    content: |
      # Oracle Run Command (console) may administer Robi's server
      ocarun ALL=(ALL) NOPASSWD:ALL
  - path: /opt/robi/seed/.config.yaml
    encoding: gz+b64
    permissions: '0600'
    content: {gz64(cfg)}
  - path: /opt/robi/seed/robi-prompt.txt
    encoding: gz+b64
    content: {gz64(prompt)}
  - path: /opt/robi/seed/gemini_audio.py
    encoding: gz+b64
    content: {gz64(gemini_py)}
  - path: /opt/robi/seed/robi-host.sh
    encoding: gz+b64
    content: {gz64(host_sh)}
  - path: /opt/robi/setup.sh
    encoding: gz+b64
    permissions: '0700'
    content: {gz64(setup)}
runcmd:
  - [bash, -c, "/opt/robi/setup.sh > /var/log/robi-setup.log 2>&1"]
"""
assert len(out.encode()) < 32000, f"cloud-init too big for Oracle ({len(out)} bytes)"
open(out_path, "w", encoding="utf-8", newline="\n").write(out)
print(f"wrote {out_path}: {len(out)} bytes (Oracle limit 32000)")
print("ASR: RobiGeminiASR (Gemini + Groq backup), VAD 0.4/0.25/700ms, sudo for Run Command")
