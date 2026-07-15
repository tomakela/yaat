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

function actionName(id, kind) {
  const table = {
    new_game: { look: 'newGameLook', use: 'newGameUse' },
    faded_sign: { look: 'fadedSignLook', use: 'fadedSignLook' },
    locked_door: { look: 'lockedDoorLook', use: 'lockedDoorUse', requires: 'doorUnlocked' },
    brass_key: { look: 'brassKeyLook', take: 'takeKey', visible: 'keyVisible' },
    back_door: { look: 'backDoorLook' },
    ending_plaque: { look: 'endingPlaqueLook', use: 'endingPlaqueUse' },
    exit_lamp: { look: 'exitLampLook', use: 'exitLampUse', sprite: 'exitLampSprite' },
    demo_badge_dim: { look: 'dimBadgeLook', visible: 'dimBadgeVisible' },
    demo_badge_lit: { look: 'litBadgeLook', visible: 'litBadgeVisible' },
  };
  return table[id]?.[kind];
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
    lookAction: actionName(id, 'look'),
    useAction: actionName(id, 'use'),
    takeAction: actionName(id, 'take'),
    visibleAction: actionName(id, 'visible'),
    spriteAction: actionName(id, 'sprite'),
  };
  if (object.spriteAction) delete object.sprite;
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
    lookAction: actionName(id, 'look'),
    useAction: actionName(id, 'use'),
    requiresAction: actionName(id, 'requires'),
  };
}

function applyExit(room, id, data) {
  const hotspotId = data.hotspot ?? id;
  const hotspot = room.hotspots.find(item => item.id === hotspotId);
  if (!hotspot) return;
  hotspot.targetRoom = data.to ?? hotspot.targetRoom;
  hotspot.targetX = data.target_x == null ? hotspot.targetX : intValue(data.target_x);
  hotspot.targetY = data.target_y == null ? hotspot.targetY : intValue(data.target_y);
  if (data.requires_flag === 'door_locked:false') hotspot.requiresAction = 'doorUnlocked';
}

async function loadRoom(roomId) {
  const base = `rooms/${roomId}/`;
  const [roomIni, hotspotsIni, objectsIni, exitsIni] = await Promise.all([
    fetchText(base + 'room.ini').then(parseIni),
    fetchText(base + 'hotspots.ini').then(parseIni),
    fetchText(base + 'objects.ini').then(parseIni),
    fetchText(base + 'exits.ini').then(parseIni).catch(() => ({})),
  ]);
  const room = {
    label: roomIni.id?.label ?? roomId,
    bg: roomPath(roomId, roomIni.display?.background ?? 'background.bmp'),
    hidePlayer: roomId === 'room000_start',
    hideUI: roomId === 'room000_start',
    enterAction: roomId === 'room001_intro' ? 'introEnter' : undefined,
    enterText: roomId === 'room000_start' ? 'Welcome to the YAAT placeholder asset demo.' : roomId === 'room002_exit' ? 'You step through the unlocked door into the exit room.' : undefined,
    hotspots: Object.entries(hotspotsIni).filter(([, v]) => typeof v === 'object').map(([id, data]) => hotspotFromIni(id, data)),
    objects: Object.entries(objectsIni).filter(([, v]) => typeof v === 'object').map(([id, data]) => objectFromIni(roomId, id, data)),
  };
  for (const [id, data] of Object.entries(exitsIni)) if (typeof data === 'object') applyExit(room, id, data);
  if (roomId === 'room000_start') for (const item of room.objects) item.walkBefore = false;
  return room;
}

export async function loadGameData() {
  const gameIni = parseIni(await fetchText('game.ini'));
  const roomRoot = gameIni.paths?.rooms ?? 'rooms';
  const [actionsIni, inventoryIni] = await Promise.all([
    fetchText('actions.ini').then(parseIni),
    fetchText(gameIni.paths?.inventory ?? 'inventory/items.ini').then(parseIni),
  ]);
  const roomIds = ['room000_start', 'room001_intro', 'room002_exit'];
  const loadedRooms = await Promise.all(roomIds.map(roomId => loadRoom(joinPath(roomRoot, roomId).replace(/^rooms\//, ''))));
  return {
    ...loadVerbs(actionsIni),
    inventoryDefs: loadInventory(inventoryIni),
    rooms: Object.fromEntries(roomIds.map((roomId, index) => [roomId, loadedRooms[index]])),
    firstRoom: gameIni.game?.first_room ?? roomIds[0],
  };
}
