import urllib.request,json,pathlib,hashlib,tarfile,shutil
import argparse
parser=argparse.ArgumentParser(description='Extract pinned official devkitPro target SDK; Linux executables are not run on macOS.')
parser.add_argument('--destination',type=pathlib.Path,required=True)
parser.add_argument('--downloads',type=pathlib.Path,required=True)
args=parser.parse_args()
out=args.downloads.resolve(); sdk=args.destination.resolve()
out.mkdir(parents=True,exist_ok=True); sdk.mkdir(parents=True,exist_ok=True)
repo='devkitpro/devkita64'; digest='sha256:82575ea78651b530b2e232bb3799cfd1fe331514e053d5f724bb4b28191fb79d'
t=json.load(urllib.request.urlopen('https://auth.docker.io/token?service=registry.docker.io&scope=repository:'+repo+':pull'))['token']
def request(path):return urllib.request.urlopen(urllib.request.Request('https://registry-1.docker.io/v2/'+repo+'/'+path,headers={'Authorization':'Bearer '+t,'Accept':'application/vnd.oci.image.manifest.v1+json'}))
raw=request('manifests/'+digest).read()
if hashlib.sha256(raw).hexdigest()!=digest[7:]:raise RuntimeError('Image manifest digest mismatch')
d=json.loads(raw);(out/'image-manifest.json').write_text(json.dumps(d,indent=2));print([(x['digest'],x['size']) for x in d['layers']],flush=True)
for layer in d['layers']:
 p=out/(layer['digest'][7:]+'.tar.gz')
 if not p.exists():
  print('Downloading',p.name,flush=True)
  with request('blobs/'+layer['digest']) as r,p.with_suffix('.part').open('wb') as f:shutil.copyfileobj(r,f)
  p.with_suffix('.part').replace(p)
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 if h.hexdigest()!=layer['digest'][7:]:raise RuntimeError('SDK layer SHA256 mismatch: '+str(p))
 print('Verified, extracting SDK',flush=True)
 with tarfile.open(p) as a:
  members=[m for m in a if m.name.startswith('opt/devkitpro/') and '..' not in pathlib.PurePosixPath(m.name).parts and not '/.wh.' in m.name]
  a.extractall(sdk,members=members)
print('SDK extracted',flush=True)
