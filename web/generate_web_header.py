#!/usr/bin/env python3
"""
Bundle web/index.html, web/style.css, and web/app.js into a single web/web_ui.h header.
"""

import os

def generate_header():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    html_path = os.path.join(script_dir, "index.html")
    css_path = os.path.join(script_dir, "style.css")
    js_path = os.path.join(script_dir, "app.js")
    out_path = os.path.join(script_dir, "web_ui.h")

    with open(html_path, "r", encoding="utf-8") as f:
        html = f.read()
    with open(css_path, "r", encoding="utf-8") as f:
        css = f.read()
    with open(js_path, "r", encoding="utf-8") as f:
        js = f.read()

    # Inline CSS and JS into standalone HTML
    bundle = html.replace('<link rel="stylesheet" href="style.css">', f'<style>\n{css}\n</style>')
    bundle = bundle.replace('<script src="app.js"></script>', f'<script>\n{js}\n</script>')

    header_content = f"""#pragma once

namespace Web {{

// Bundled HTML5/CSS3/JS ChatGPT Dark Mode Web UI
// Stored in Flash DROM (Zero SRAM allocation)
static const char CHAT_HTML[] = R"rawliteral({bundle})rawliteral";

}} // namespace Web
"""

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(header_content)

    print(f"Generated {out_path} ({len(bundle)} bytes) successfully!")

if __name__ == "__main__":
    generate_header()
