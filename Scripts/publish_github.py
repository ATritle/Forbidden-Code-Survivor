"""Publish release assets and configure Pages using Git's existing sign-in.

Credentials stay in memory. Run with release, pages, or status from the project.
"""
from pathlib import Path
import argparse
import hashlib
import http.client
import json
import subprocess
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = 'ATritle/Forbidden-Code-Survivor'
API = 'https://api.github.com/repos/' + REPOSITORY
TAG = 'v0.2.0'


def credentials():
    result = subprocess.run(
        ['git', '-c', 'credential.interactive=false', 'credential', 'fill'],
        input='protocol=https\nhost=github.com\n\n', text=True,
        capture_output=True, check=True)
    values = dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)
    token = values.get('password')
    if not token:
        raise RuntimeError('GitHub sign-in is required in Git Credential Manager.')
    return token


def request(token, method, path, data=None, missing_ok=False):
    url = path if path.startswith('https://') else API + path
    headers = {'Authorization': 'Bearer ' + token,
               'Accept': 'application/vnd.github+json',
               'X-GitHub-Api-Version': '2022-11-28',
               'User-Agent': 'ForbiddenCode-Publisher'}
    payload = None if data is None else json.dumps(data).encode()
    if payload is not None:
        headers['Content-Type'] = 'application/json'
    try:
        with urllib.request.urlopen(urllib.request.Request(
                url, data=payload, headers=headers, method=method), timeout=60) as response:
            raw = response.read()
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as error:
        if missing_ok and error.code == 404:
            return None
        raise RuntimeError(f'GitHub {method} {url}: HTTP {error.code}; '
                           + error.read().decode(errors='replace')) from None


def upload(token, release, path, content_type):
    existing = request(token, 'GET', f"/releases/{release['id']}/assets")
    local_digest = hashlib.sha256(path.read_bytes()).hexdigest()
    for asset in existing:
        if asset['name'] != path.name:
            continue
        if asset['size'] != path.stat().st_size or (
                asset.get('digest') and asset['digest'] != 'sha256:' + local_digest):
            raise RuntimeError(f'Remote asset differs; refusing to replace {path.name}.')
        print('ASSET_ALREADY_UPLOADED ' + path.name, flush=True)
        return asset
    base = release['upload_url'].split('{', 1)[0]
    target = urllib.parse.urlsplit(base + '?name=' + urllib.parse.quote(path.name))
    connection = http.client.HTTPSConnection(target.hostname, timeout=240)
    connection.putrequest('POST', target.path + '?' + target.query)
    connection.putheader('Authorization', 'Bearer ' + token)
    connection.putheader('Accept', 'application/vnd.github+json')
    connection.putheader('User-Agent', 'ForbiddenCode-Publisher')
    connection.putheader('Content-Type', content_type)
    connection.putheader('Content-Length', str(path.stat().st_size))
    connection.endheaders()
    sent = 0
    with path.open('rb') as stream:
        while block := stream.read(4 * 1024 * 1024):
            connection.send(block)
            sent += len(block)
            if sent % (64 * 1024 * 1024) == 0:
                print(f'UPLOAD {path.name} {sent // (1024 * 1024)} MiB', flush=True)
    response = connection.getresponse()
    raw = response.read()
    if response.status != 201:
        raise RuntimeError(f'Upload HTTP {response.status}: {raw.decode(errors="replace")}')
    asset = json.loads(raw)
    connection.close()
    assert asset['size'] == path.stat().st_size
    if asset.get('digest'):
        assert asset['digest'] == 'sha256:' + local_digest
    print('ASSET_UPLOADED ' + asset['browser_download_url'], flush=True)
    return asset


def release(token):
    folder = ROOT / 'Artifacts/Installer'
    manifest = json.loads((folder / 'release-manifest.json').read_text(encoding='utf-8'))
    installer = folder / manifest['filename']
    assert installer.stat().st_size == manifest['bytes']
    assert hashlib.sha256(installer.read_bytes()).hexdigest() == manifest['sha256']
    item = request(token, 'GET', '/releases/tags/' + TAG, missing_ok=True)
    body = ('First playable two-edition test release of **Forbidden Code: Survivor**.\n\n'
            '**Play in your browser:** https://ATritle.github.io/Forbidden-Code-Survivor/\n\n'
            '**Windows:** Download `ForbiddenCode-Survivor-Setup.exe` below (464 MiB). '
            'This is the Unreal Engine 5 Shipping edition with DX12/SM6, Nanite, Lumen '
            'and Niagara. Install and launch from the Start Menu.\n\n'
            'The browser version runs locally using WebGL 2 with simplified rendering '
            'and a 90-enemy cap. The editions share card/enemy tuning; saves are separate.\n\n'
            'Both include sixteen energy spells, structural mutations, escalating cyborg '
            'tiers, seven risk contracts and the neon city theme.\n\n'
            'Validation: packaged Windows menu/combat passed; all sixteen browser '
            'base and mutated spells confirmed damage; eight browser rule tests passed.\n\n'
            '**Installer SHA256:** `' + manifest['sha256'] + '`\n\n'
            'The attached manifest records file size, checksum and build configuration.')
    if item is None:
        item = request(token, 'POST', '/releases', {
            'tag_name': TAG, 'target_commitish': 'main',
            'name': 'Forbidden Code: Survivor v0.2.0 — Playable Test',
            'body': body, 'draft': True, 'prerelease': True})
    upload(token, item, installer, 'application/vnd.microsoft.portable-executable')
    upload(token, item, folder / 'release-manifest.json', 'application/json')
    if item['draft']:
        item = request(token, 'PATCH', f"/releases/{item['id']}", {'draft': False})
    print('RELEASE_PUBLISHED ' + item['html_url'])


def pages(token):
    current = request(token, 'GET', '/pages', missing_ok=True)
    current = request(token, 'POST' if current is None else 'PUT', '/pages',
                      {'build_type': 'workflow'})
    print('PAGES_CONFIGURED workflow')
    request(token, 'PATCH', '', {
        'homepage': 'https://ATritle.github.io/Forbidden-Code-Survivor/',
        'description': (ROOT / 'GameData/repository-description.txt').read_text(encoding='utf-8').strip()})


def status(token):
    page = request(token, 'GET', '/pages', missing_ok=True)
    print('PAGES ' + json.dumps(page))
    runs = request(token, 'GET', '/actions/workflows/deploy-browser.yml/runs?per_page=3', missing_ok=True)
    for run in (runs or {}).get('workflow_runs', []):
        print(json.dumps({k: run.get(k) for k in
                          ['id', 'status', 'conclusion', 'html_url', 'head_sha']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['release', 'pages', 'status'])
    arguments = parser.parse_args()
    globals()[arguments.action](credentials())
