"""Print the docs pages to PDF: light (for paper) and *_dark.pdf (for the screen).

Needs Microsoft Edge (or Chrome) for the headless print. Run: python build_pdfs.py
"""
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
PAGES = ['wiring_A4.html', 'controls_wiring.html']
BROWSERS = [
    r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
    r'C:\Program Files\Microsoft\Edge\Application\msedge.exe',
    r'C:\Program Files\Google\Chrome\Application\chrome.exe',
]

# Dark theme: overrides the light colours, including the SVG drawings
DARK_CSS = '''
<style>
  html, body { background: #1c1c1c !important; color: #dcdcdc !important;
               -webkit-print-color-adjust: exact; print-color-adjust: exact; }
  @page { margin: 0; }
  .page { background: #1c1c1c !important; box-shadow: none !important; margin: 0 !important; width: auto !important;
          padding: 8mm !important; -webkit-box-decoration-break: clone; box-decoration-break: clone; }
  :root { --ink: #dcdcdc; --mute: #a0a0a0; --line: #5a5a5a; --soft: #2c2c2c; }
  th { background: #2c2c2c !important; }
  tr.grp td { background: #262626 !important; }
  th, td { border-color: #5a5a5a !important; }
  h2 { border-bottom-color: #dcdcdc !important; }
  .tag { border-color: #dcdcdc !important; }
  .sub, .note { color: #a0a0a0 !important; }
  svg text { fill: #dcdcdc !important; }
  svg line, svg polyline, svg polygon, svg circle, svg rect, svg path { stroke: #dcdcdc !important; }
  svg circle[fill="#111"], svg .f { fill: #dcdcdc !important; }
  svg rect[fill="#f3f3f3"] { fill: #2a2a2a !important; }
  svg rect[stroke="#777"] { stroke: #888 !important; }
</style>
'''


def browser():
    for b in BROWSERS:
        if os.path.exists(b):
            return b
    raise SystemExit('Edge / Chrome not found')


def to_pdf(html_path, pdf_path):
    url = html_path.resolve().as_uri()
    subprocess.run([browser(), '--headless', '--disable-gpu', '--no-pdf-header-footer',
                    f'--print-to-pdf={pdf_path}', url], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


for name in PAGES:
    src = HERE / name
    to_pdf(src, HERE / (src.stem + '.pdf'))
    dark = HERE / ('_' + src.stem + '_dark.html')
    dark.write_text(src.read_text(encoding='utf-8').replace('</head>', DARK_CSS + '</head>'), encoding='utf-8')
    try:
        to_pdf(dark, HERE / (src.stem + '_dark.pdf'))
    finally:
        dark.unlink()
    print('built', src.stem + '.pdf', src.stem + '_dark.pdf')
