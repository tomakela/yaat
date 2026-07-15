const GAME_ROOT = 'game/';

async function fetchText(path) {
  const response = await fetch(GAME_ROOT + path);
  if (!response.ok) throw new Error(`Failed to load ${path}: ${response.status}`);
  return response.text();
}

function parseIni(text) {
  const data = {};
  let section = data;
  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith(';') || line.startsWith('#')) continue;
    const sectionMatch = line.match(/^\[([^\]]+)\]$/);
    if (sectionMatch) {
      section = data[sectionMatch[1]] ??= {};
      continue;
    }
    const equals = line.indexOf('=');
    if (equals < 0) continue;
    section[line.slice(0, equals).trim()] = line.slice(equals + 1).trim();
  }
  return data;
}

function boolValue(value, fallback = false) {
  if (value == null || value === '') return fallback;
  return /^(1|true|yes|on)$/i.test(value);
}

function colorValue(value) {
  if (!value) return '';
  const match = value.trim().match(/^#?([0-9a-f]{6})$/i);
  return match ? `#${match[1].toLowerCase()}` : '';
}

function intValue(value, fallback = 0) {
  const parsed = Number.parseInt(value ?? '', 10);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function rectValues(value) {
  return (value ?? '0,0,0,0').split(',').map(part => intValue(part));
}

function joinPath(...parts) {
  const stack = [];
  for (const part of parts.join('/').split('/')) {
    if (!part || part === '.') continue;
    if (part === '..') stack.pop();
    else stack.push(part);
  }
  return stack.join('/');
}

function roomPath(roomId, relativePath) {
  return joinPath('rooms', roomId, relativePath);
}


function stripComments(text){ return text.replace(/#.*$/gm, ''); }
function findMatching(text, openIndex){ let depth=0; for(let i=openIndex;i<text.length;i++){ if(text[i]==='{') depth++; else if(text[i]==='}' && --depth===0) return i; } return -1; }
function parseCommands(body){
  const commands=[]; let i=0;
  while(i<body.length){
    const rest=body.slice(i);
    const ws=rest.match(/^\s+/); if(ws){ i+=ws[0].length; continue; }
    const ifm=body.slice(i).match(/^if\s+([A-Za-z0-9_]+)\s*\{/);
    if(ifm){ const open=i+ifm[0].lastIndexOf('{'); const close=findMatching(body, open); let j=close+1; let elseCommands=[]; const em=body.slice(j).match(/^\s*else\s*\{/); if(em){ const eopen=j+em[0].lastIndexOf('{'); const eclose=findMatching(body,eopen); elseCommands=parseCommands(body.slice(eopen+1,eclose)); j=eclose+1; } commands.push({type:'if', varName:ifm[1], then:parseCommands(body.slice(open+1,close)), else:elseCommands}); i=j; continue; }
    const lineEnd=body.indexOf('\n', i); const line=body.slice(i, lineEnd<0?body.length:lineEnd).trim(); i=lineEnd<0?body.length:lineEnd+1; if(!line) continue;
    let m;
    if(m=line.match(/^say\s+(\w+)\s+"([^"]*)"/)) commands.push({type:'say', speaker:m[1], text:m[2]});
    else if(m=line.match(/^set\s+(\w+)\s*=\s*(true|false|-?\d+|"[^"]*")/)){ const raw=m[2]; commands.push({type:'set', name:m[1], value: raw==='true'?true:raw==='false'?false:raw[0]=='"'?raw.slice(1,-1):parseInt(raw,10)}); }
    else if(m=line.match(/^play_sound\s+"([^"]+)"/)) commands.push({type:'play_sound', path:m[1]});
    else if(m=line.match(/^goto\s+(\w+)/)) commands.push({type:'goto', room:m[1]});
    else if(m=line.match(/^pickup\s+(\w+)\s+(\w+)/)) commands.push({type:'pickup', object:m[1], item:m[2]});
    else if(m=line.match(/^consume\s+(\w+)/)) commands.push({type:'consume', item:m[1]});
    else if(m=line.match(/^drop\s+(\w+)\s+(\w+)/)) commands.push({type:'drop', item:m[1], object:m[2]});
    else if(m=line.match(/^hide\s+(\w+)/)) commands.push({type:'hide', id:m[1]});
    else if(m=line.match(/^show\s+(\w+)/)) commands.push({type:'show', id:m[1]});
    else if(m=line.match(/^set_sprite\s+(\w+)\s+"([^"]+)"/)) commands.push({type:'set_sprite', id:m[1], sprite:m[2]});
    else if(m=line.match(/^shake\s+(\d+)\s+(\d+)/)) commands.push({type:'shake', ms:parseInt(m[1],10), mag:parseInt(m[2],10)});
    else if(line==='hide_player') commands.push({type:'hide_player'});
    else if(line==='show_player') commands.push({type:'show_player'});
  }
  return commands;
}
function parseEvents(block){
  const events=[]; const re=/on\s+(\w+)(?:\s+(\w+))?(?:\s+(nowalk))?\s*\{/g; let m;
  while((m=re.exec(block))){ const open=block.indexOf('{', m.index); const close=findMatching(block, open); events.push({name:m[1], item:m[2]&&m[2]!=='nowalk'?m[2]:'', walkBefore:!(m[2]==='nowalk'||m[3]==='nowalk'), commands:parseCommands(block.slice(open+1, close))}); re.lastIndex=close+1; }
  return events;
}
function parseYaatRoom(text){
  const src=stripComments(text); const rm=/room\s+(\w+)\s*\{/.exec(src); const vars={}; for(const vm of src.matchAll(/var\s+(\w+)\s*=\s*(true|false|-?\d+|"[^"]*")/g)){ const raw=vm[2]; vars[vm[1]]=raw==='true'?true:raw==='false'?false:raw[0]=='"'?raw.slice(1,-1):parseInt(raw,10); } if(!rm) return {events:[], entities:{}, vars}; const open=src.indexOf('{', rm.index); const close=findMatching(src, open); const body=src.slice(open+1, close); let roomOnly=body; const entitySpans=[];
  const re=/(object|hotspot)\s+(\w+)\s*\{/g; let m;
  while((m=re.exec(body))){ const eopen=body.indexOf('{', m.index); const eclose=findMatching(body, eopen); entitySpans.push([m.index,eclose+1,m[1],m[2],body.slice(eopen+1,eclose)]); re.lastIndex=eclose+1; }
  for(let i=entitySpans.length-1;i>=0;i--){ const [a,b]=entitySpans[i]; roomOnly=roomOnly.slice(0,a)+roomOnly.slice(b); }
  const room={events:parseEvents(roomOnly), entities:{}, vars};
  for(const span of entitySpans){ room.entities[span[3]]={kind:span[2], events:parseEvents(span[4])}; }
  return room;
}
function verbLabel(verb) {
  return ({ walk: 'Walk to', look: 'Look at', use: 'Use', talk: 'Talk to', take: 'Take', open: 'Open', close: 'Close' })[verb] ?? verb;
}

function loadVerbs(actionsIni) {
  const configured = Object.entries(actionsIni.verbs ?? {})
    .filter(([key]) => /^verb\d+$/.test(key))
    .sort(([a], [b]) => intValue(a.slice(4)) - intValue(b.slice(4)))
    .map(([, value]) => value);
  const verbs = ['walk', ...configured.filter(verb => verb !== 'walk')];
  return { verbs, verbLabels: Object.fromEntries(verbs.map(verb => [verb, verbLabel(verb)])) };
}

function loadInventory(inventoryIni) {
  const inventory = {};
  for (const [id, item] of Object.entries(inventoryIni)) {
    if (typeof item !== 'object') continue;
    inventory[id] = {
      name: item.name ?? id,
      icon: item.icon ? joinPath('inventory', item.icon) : '',
      description: item.description ?? '',
    };
  }
  return inventory;
}

function objectFromIni(roomId, id, data) {
  const object = {
    id,
    name: data.name ?? id,
    sprite: data.sprite ? roomPath(roomId, data.sprite) : undefined,
    x: intValue(data.x),
    y: intValue(data.y),
    w: intValue(data.width),
    h: intValue(data.height),
    walkX: data.walk_x == null ? undefined : intValue(data.walk_x),
    walkY: data.walk_y == null ? undefined : intValue(data.walk_y),
    cursor: data.cursor,
    inventoryItem: data.inventory_item,
    visible: boolValue(data.visible, true),
    transparentColor: colorValue(data.transparent_color ?? data.color_key),
  };
  return object;
}

function hotspotFromIni(id, data) {
  const [x, y, w, h] = rectValues(data.rect ?? [data.x, data.y, data.width, data.height].join(','));
  return {
    id,
    name: data.name ?? id,
    x, y, w, h,
    walkX: data.walk_x == null ? undefined : intValue(data.walk_x),
    walkY: data.walk_y == null ? undefined : intValue(data.walk_y),
    cursor: data.cursor,
    targetRoom: data.target_room,
    targetX: data.target_x == null ? undefined : intValue(data.target_x),
    targetY: data.target_y == null ? undefined : intValue(data.target_y),
  };
}

function applyExit(room, id, data) {
  const hotspotId = data.hotspot ?? id;
  const hotspot = room.hotspots.find(item => item.id === hotspotId);
  if (!hotspot) return;
  hotspot.targetRoom = data.to ?? hotspot.targetRoom;
  hotspot.targetX = data.target_x == null ? hotspot.targetX : intValue(data.target_x);
  hotspot.targetY = data.target_y == null ? hotspot.targetY : intValue(data.target_y);
  if (data.requires_flag === 'door_locked:false') hotspot.requiredFlag = 'door_locked', hotspot.requiredFlagValue = false;
}

async function loadRoom(roomId) {
  const base = `rooms/${roomId}/`;
  const roomIni = await fetchText(base + 'room.ini').then(parseIni);
  const [hotspotsIni, objectsIni, exitsIni, script] = await Promise.all([
    fetchText(base + 'hotspots.ini').then(parseIni),
    fetchText(base + 'objects.ini').then(parseIni),
    fetchText(base + 'exits.ini').then(parseIni).catch(() => ({})),
    fetchText(base + (roomIni.script?.file ?? 'script.yaat')).then(parseYaatRoom),
  ]);
  const room = {
    events: script.events,
    vars: script.vars,
    label: roomIni.id?.label ?? roomId,
    bg: roomPath(roomId, roomIni.display?.background ?? 'background.bmp'),
    hidePlayer: roomId === 'room000_start',
    hideUI: roomId === 'room000_start',
    hotspots: Object.entries(hotspotsIni).filter(([, v]) => typeof v === 'object').map(([id, data]) => hotspotFromIni(id, data)),
    objects: Object.entries(objectsIni).filter(([, v]) => typeof v === 'object').map(([id, data]) => objectFromIni(roomId, id, data)),
  };
  for (const item of [...room.hotspots, ...room.objects]) item.events = script.entities[item.id]?.events ?? [];
  for (const [id, data] of Object.entries(exitsIni)) if (typeof data === 'object') applyExit(room, id, data);
  if (roomId === 'room000_start') for (const item of room.objects) item.walkBefore = false;
  return room;
}

export async function loadGameData() {
  const gameIni = parseIni(await fetchText('game.ini'));
  const playerIni = await fetchText('graphics/sprites/player.ini').then(parseIni).catch(() => ({}));
  const roomRoot = gameIni.paths?.rooms ?? 'rooms';
  const [actionsIni, inventoryIni] = await Promise.all([
    fetchText('actions.ini').then(parseIni),
    fetchText(gameIni.paths?.inventory ?? 'inventory/items.ini').then(parseIni),
  ]);
  const roomIds = ['room000_start', 'room001_intro', 'room002_exit'];
  const loadedRooms = await Promise.all(roomIds.map(roomId => loadRoom(joinPath(roomRoot, roomId).replace(/^rooms\//, ''))));
  return {
    initialVars: Object.assign({}, ...loadedRooms.map(room => room.vars || {})),
    playerTransparentColor: colorValue(playerIni.sprites?.transparent_color ?? playerIni.sprites?.color_key),
    ...loadVerbs(actionsIni),
    inventoryDefs: loadInventory(inventoryIni),
    rooms: Object.fromEntries(roomIds.map((roomId, index) => [roomId, loadedRooms[index]])),
    firstRoom: gameIni.game?.first_room ?? roomIds[0],
  };
}
