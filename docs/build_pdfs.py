"""Print the docs pages to PDF: light (for paper) and *_dark.pdf (for the screen).

Needs Microsoft Edge (or Chrome) for the headless print. Run: python build_pdfs.py
"""
import os
import pathlib
import subprocess
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
PAGES = ['wiring_A4.html', 'controls_wiring.html', 'system_wiring.html', 'preamp_bfg135.html']
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
  svg rect[fill="#f3f3f3"], svg rect[fill="#f6f6f6"], svg rect[fill="#ececec"] { fill: #2a2a2a !important; }
  svg rect[stroke="#777"] { stroke: #888 !important; }
</style>
'''


def browser():
    for b in BROWSERS:
        if os.path.exists(b):
            return b
    raise SystemExit('Edge / Chrome not found')


def to_pdf(html_path, pdf_path):
    """Print to a temporary file first, then replace the PDF. If the PDF is open in a viewer
    (file locked), say so instead of silently keeping the old version."""
    url = html_path.resolve().as_uri()
    tmp = pathlib.Path(str(pdf_path)[:-4] + '.tmp.pdf')
    if tmp.exists():
        tmp.unlink()
    # own profile dir = own browser instance (otherwise a running Edge may take over and return early)
    profile = pathlib.Path(tempfile.gettempdir()) / 'atlas_pdf_profile'
    subprocess.run([browser(), '--headless', '--disable-gpu', '--no-pdf-header-footer',
                    f'--user-data-dir={profile}', f'--print-to-pdf={tmp}', url], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):                    # wait until the file is there and no longer growing
        if tmp.exists() and tmp.stat().st_size > 0:
            size = tmp.stat().st_size
            time.sleep(0.3)
            if tmp.stat().st_size == size:
                break
        time.sleep(0.2)
    if not tmp.exists() or tmp.stat().st_size == 0:
        raise SystemExit(f'printing {html_path.name} failed')
    try:
        os.replace(tmp, pdf_path)
    except PermissionError:
        tmp.unlink()
        raise SystemExit(f'{pathlib.Path(pdf_path).name} is open in a viewer (locked) - close it and run again')


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
