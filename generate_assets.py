import urllib.request
import gzip
import os

def download_and_gzip(url):
    print(f"Downloading {url}...")
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req) as response:
        content = response.read()
    print(f"Original size: {len(content)} bytes")
    compressed = gzip.compress(content)
    print(f"Gzipped size: {len(compressed)} bytes")
    return compressed

def to_c_array(name, data):
    hex_array = ', '.join([f"0x{b:02x}" for b in data])
    return f"const uint8_t {name}[] PROGMEM = {{\n    {hex_array}\n}};\n"

uplot_js = download_and_gzip("https://unpkg.com/uplot@1.6.30/dist/uPlot.iife.min.js")
uplot_css = download_and_gzip("https://unpkg.com/uplot@1.6.30/dist/uPlot.min.css")

header = """#ifndef UPLOT_ASSETS_H
#define UPLOT_ASSETS_H

#include <pgmspace.h>

"""

header += to_c_array("UPLOT_JS_GZ", uplot_js)
header += "\n"
header += f"const size_t UPLOT_JS_GZ_LEN = {len(uplot_js)};\n\n"

header += to_c_array("UPLOT_CSS_GZ", uplot_css)
header += "\n"
header += f"const size_t UPLOT_CSS_GZ_LEN = {len(uplot_css)};\n\n"

header += "#endif // UPLOT_ASSETS_H\n"

os.makedirs("/home/drewfus/TempSensor/include/web", exist_ok=True)
with open("/home/drewfus/TempSensor/include/web/uplot_assets.h", "w") as f:
    f.write(header)
print("Saved to /home/drewfus/TempSensor/include/web/uplot_assets.h")
