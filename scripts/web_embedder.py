import os
import gzip
import hashlib
import urllib.parse
Import("env")

def collect_web_files(data_dir):
    web_files = []
    for root, dirs, files in os.walk(data_dir):
        dirs.sort()
        for file in sorted(files):
            full_path = os.path.join(root, file)
            rel_path = os.path.relpath(full_path, data_dir).replace("\\", "/")
            web_files.append((full_path, rel_path, file))
    return web_files

def get_web_hash(web_files):
    digest = hashlib.sha1()
    for full_path, rel_path, _ in web_files:
        digest.update(rel_path.encode("utf-8"))
        digest.update(b"\0")
        with open(full_path, "rb") as f:
            digest.update(f.read())
        digest.update(b"\0")
    return digest.hexdigest()[:8]

def embed_web_files():
    data_dir = "data"
    # Use the project's include directory defined by PlatformIO
    output_file = os.path.join(env.get("PROJECT_INCLUDE_DIR"), "web_resources.h")
    
    if not os.path.exists(data_dir):
        print(f"[WebEmbedder] Error: {data_dir} directory not found!")
        return
    
    raw_v = "1.0.0" # fallback
    cpp_defines = env.get("CPPDEFINES", []) 
    for define in cpp_defines:
        if isinstance(define, tuple) and define[0] == "APP_VERSION":
            raw_v = define[1]
            break
        elif isinstance(define, str) and define.startswith("APP_VERSION="):
            raw_v = define.split('=')[1]
            break
    clean_v = raw_v.replace('\\"', '').strip('"')
    web_files = collect_web_files(data_dir)
    env_name = env.get("PIOENV", "unknown")
    asset_version = urllib.parse.quote(f"{clean_v}-{env_name}-{get_web_hash(web_files)}")

    print(f"[WebEmbedder] Generating {output_file} (web assets = {asset_version})...")
    
    files_data = []
    
    with open(output_file, "w", encoding="utf-8") as f:
        # File header and guards
        f.write("// Generated file - do not edit\n")
        f.write("#ifndef WEB_RESOURCES_H\n#define WEB_RESOURCES_H\n\n#include <Arduino.h>\n\n")
        
        # 1. Generate byte arrays for each file
        for full_path, rel_path, file in web_files:
            var_name = rel_path.replace(".", "_").replace("/", "_").replace("-", "_").upper()

            # Determine MIME type based on extension
            MIME_TYPES = {
                ".html": "text/html",
                ".css":  "text/css",
                ".js":   "application/javascript",
                ".json": "application/json",
                ".png":  "image/png",
                ".jpg":  "image/jpeg",
                ".jpeg": "image/jpeg",
                ".gif":  "image/gif",
                ".svg":  "image/svg+xml",
                ".ico":  "image/x-icon",
                ".webp": "image/webp",
                ".woff2": "font/woff2"
            }

            # Get extension and look up MIME type (default to "application/octet-stream")
            ext = os.path.splitext(file)[1].lower()
            mime = MIME_TYPES.get(ext, "application/octet-stream")

            with open(full_path, "rb") as f_in:
                raw_data = f_in.read()

                # replace version
                if ext == ".html":
                    content = raw_data.decode("utf-8").replace("{{VERSION}}", asset_version)
                    raw_data = content.encode("utf-8")

                # Compress data with Gzip
                compressed = gzip.compress(raw_data)
                f.write(f"const uint8_t PAGE_{var_name}[] PROGMEM = {{ ")
                f.write(", ".join([f"0x{b:02x}" for b in compressed]))
                f.write(f" }};\n")
                f.write(f"const uint32_t PAGE_{var_name}_LEN = {len(compressed)};\n\n")
                processor = ""
                if file == "stats.html":
                    processor = file

                files_data.append({
                    "url": "/" + rel_path,
                    "var": f"PAGE_{var_name}",
                    "len": f"PAGE_{var_name}_LEN",
                    "mime": mime
                })

        # 2. Define the Resource structure and the lookup table
        f.write("struct WebResource {\n  const char* url;\n  const uint8_t* data;\n  uint32_t len;\n  const char* mime;\n};\n\n")
        
        f.write(f"const WebResource webResources[] PROGMEM = {{\n")
        for file in files_data:
            f.write(f"  {{ \"{file['url']}\", {file['var']}, {file['len']}, \"{file['mime']}\" }},\n")
        f.write("};\n\n")
        
        total_count = len(files_data)
        f.write(f"const uint16_t webResourcesCount = {total_count};\n\n")
        f.write("#endif\n")
    print(f"[WebEmbedder] Successfully created {output_file}")

# Execute the embedding process immediately when the script is loaded by PlatformIO
embed_web_files()
