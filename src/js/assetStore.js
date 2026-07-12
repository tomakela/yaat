// JavaScript parity harness: asset access boundary.
// Models the C runtime's winning-asset lookup by querying ordered layers where
// earlier layers take precedence over later layers (for example loose files,
// patches, then base archive). The layers are adapters; archive parsing and
// browser fetching are host concerns rather than VM behavior.
export class AssetStore {
  constructor(layers=[]){ this.layers=layers; }
  addLayer(layer){ this.layers.push(layer); return this; }
  async get(path){ for(const layer of this.layers){ if(await layer.has(path)) return layer.get(path); } return null; }
  async text(path){ const data=await this.get(path); if(data == null) return null; return typeof data === 'string' ? data : new TextDecoder().decode(data); }
}
export class MapAssetLayer { constructor(entries={}){ this.entries=new Map(Object.entries(entries)); } async has(p){ return this.entries.has(p); } async get(p){ return this.entries.get(p); } }
export class BrowserAssetLayer { constructor(prefix='game/'){ this.prefix=prefix; } async has(path){ const r=await fetch(this.prefix+path,{method:'HEAD'}); return r.ok; } async get(path){ const r=await fetch(this.prefix+path); return r.ok ? new Uint8Array(await r.arrayBuffer()) : null; } }
