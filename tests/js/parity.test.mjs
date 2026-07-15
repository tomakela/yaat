import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { mkdirSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { AssetStore, MapAssetLayer, COMMAND_KIND, CONDITION_OP, VALUE_KIND, decodeYaatBytecode, GameState, ScriptVm } from '../../src/js/index.js';
function ensureFixture(){ mkdirSync('build',{recursive:true}); execFileSync('make',['fixtures'],{stdio:'pipe'}); }

test('JavaScript bytecode loader matches C data model dump', async () => {
  ensureFixture();
  const path='tests/fixtures/bytecode/two_room_key_puzzle.yaatbc';
  const js=decodeYaatBytecode(await readFile(path));
  execFileSync('cc',['-std=c89','-Isrc','-o','build/c_data_model_dump','tests/js/c_data_model_dump.c','src/script_bytecode.c','src/script_package.c']);
  const c=JSON.parse(execFileSync('./build/c_data_model_dump',[path],{encoding:'utf8'}));
  assert.equal(js.commands.length, c.commands.length);
  assert.deepEqual(js.rooms.map(r=>r.id), c.rooms.map(r=>r.id));
  assert.deepEqual(js.vars.map(v=>v.name), c.vars.map(v=>v.name));
});
test('JavaScript VM executes fixture event traces deterministically', async () => {
  ensureFixture();
  const pkg=decodeYaatBytecode(await readFile('tests/fixtures/bytecode/two_room_key_puzzle.yaatbc'));
  const state=new GameState(pkg); const vm=new ScriptVm(pkg,state);
  const trace=[];
  for (const room of pkg.rooms) for (const ev of room.events) trace.push(...vm.runEvent(ev));
  assert.ok(Array.isArray(trace));
  assert.deepEqual(state.snapshot(), new GameState(pkg).snapshot());
});


test('JavaScript asset store preserves C-style winning layer precedence', async () => {
  const store = new AssetStore([
    new MapAssetLayer({ 'rooms/start.txt': 'loose version' }),
    new MapAssetLayer({ 'rooms/start.txt': 'patch version', 'rooms/extra.txt': 'patch only' }),
    new MapAssetLayer({ 'rooms/start.txt': 'base version', 'rooms/base.txt': 'base only' }),
  ]);

  assert.equal(await store.text('rooms/start.txt'), 'loose version');
  assert.equal(await store.text('rooms/extra.txt'), 'patch only');
  assert.equal(await store.text('rooms/base.txt'), 'base only');
  assert.equal(await store.get('missing.txt'), null);
});

test('JavaScript VM mutates explicit game state and returns host effects', () => {
  const pkg = {
    vars: [{ name: 'has_key', value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' } }],
    rooms: [{
      id: 'start',
      label: 'Start',
      color: 0,
      events: [{ name: 'on_click', item: '', firstCommand: 0, commandCount: 7 }],
      entities: [{ kind: 0, id: 'key_obj', name: 'Key', x: 10, y: 20, w: 8, h: 8, visible: true, events: [] }],
    }],
    commands: [
      { kind: COMMAND_KIND.SET, a: 'has_key', b: '', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: true, int: 1, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.IF, a: 'has_key', b: '', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: true, int: 0, string: '' }, conditionOp: CONDITION_OP.EQ, firstChild: 7, childCount: 2, firstElseChild: 9, elseChildCount: 1 },
      { kind: COMMAND_KIND.PICKUP, a: 'key_obj', b: 'key', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.MOVE_OBJECT, a: 'key_obj', b: '', stringId: '', boolValue: 30, intValue: 40, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.SHOW, a: 'key_obj', b: '', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.MOVE_PLAYER, a: '', b: '', stringId: '', boolValue: 50, intValue: 60, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.GOTO, a: 'next', b: '', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.SAY, a: 'narrator', b: 'You took the key.', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.TAKE, a: 'bonus', b: '', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
      { kind: COMMAND_KIND.SAY, a: 'narrator', b: 'No key.', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
    ],
    globalEvents: [],
  };
  const state = new GameState(pkg);
  const effects = new ScriptVm(pkg, state).runEvent(pkg.rooms[0].events[0]);

  assert.deepEqual(effects.map(e => e.type), ['set', 'say', 'take', 'pickup', 'moveObject', 'show', 'movePlayer', 'goto']);
  assert.deepEqual(state.inventory, ['bonus', 'key']);
  assert.equal(state.currentRoom, 'next');
  assert.deepEqual(state.object('key_obj'), { visible: true, x: 30, y: 40, w: 8, h: 8, sprite: '' });
  assert.deepEqual(state.player, { x: 50, y: 60, visible: true, facing: 'down' });
});

test('JavaScript VM matches C condition semantics for inventory and mixed ordering', () => {
  const command = (kind, fields = {}) => ({
    kind,
    a: '',
    b: '',
    stringId: '',
    boolValue: 0,
    intValue: 0,
    value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' },
    conditionOp: CONDITION_OP.TRUTHY,
    firstChild: 0,
    childCount: 0,
    firstElseChild: 0,
    elseChildCount: 0,
    ...fields,
  });
  const pkg = {
    vars: [
      { name: 'code', value: { kind: VALUE_KIND.STRING, bool: false, int: 0, string: '10' } },
    ],
    rooms: [{ id: 'start', label: 'Start', color: 0, events: [], entities: [] }],
    commands: [
      command(COMMAND_KIND.IF, { a: 'inventory', b: 'key', firstChild: 2, childCount: 1, firstElseChild: 3, elseChildCount: 1 }),
      command(COMMAND_KIND.IF, { a: 'code', conditionOp: CONDITION_OP.LT, value: { kind: VALUE_KIND.STRING, bool: false, int: 0, string: '2' }, firstChild: 4, childCount: 1, firstElseChild: 5, elseChildCount: 1 }),
      command(COMMAND_KIND.SAY, { a: 'narrator', b: 'has key' }),
      command(COMMAND_KIND.SAY, { a: 'narrator', b: 'missing key' }),
      command(COMMAND_KIND.TAKE, { a: 'lexical-success' }),
      command(COMMAND_KIND.TAKE, { a: 'numeric-mismatch' }),
    ],
    globalEvents: [],
  };
  const state = new GameState(pkg, { inventory: ['key'] });
  const vm = new ScriptVm(pkg, state);

  assert.deepEqual(vm.runCommands(0, 2).map(e => e.type === 'say' ? e.text : e.item), ['has key', 'lexical-success']);
  assert.deepEqual(state.inventory, ['key', 'lexical-success']);
});

test('JavaScript hover target refresh ignores a picked-up ground object', () => {
  const pkg = {
    vars: [],
    rooms: [{
      id: 'start', label: 'Start', color: 0, events: [],
      entities: [
        { kind: 1, id: 'locked_door', name: 'Locked door', x: 10, y: 10, w: 20, h: 20, visible: true, events: [] },
        { kind: 1, id: 'brass_key', name: 'Brass key', x: 10, y: 10, w: 20, h: 20, visible: true, events: [] },
      ],
    }],
    commands: [],
    globalEvents: [],
  };
  const state = new GameState(pkg);

  assert.deepEqual(state.hoverTargetAt(15, 15), { kind: 'object', id: 'brass_key', name: 'Brass key' });
  state.object('brass_key').visible = false;
  state.addItem('brass_key');

  assert.deepEqual(state.hoverTargetAt(15, 15), { kind: 'object', id: 'locked_door', name: 'Locked door' });
});

test('JavaScript VM covers demo refresh visibility, sprite, drop, and consume commands', () => {
  const command = (kind, fields = {}) => ({
    kind,
    a: '',
    b: '',
    stringId: '',
    boolValue: 0,
    intValue: 0,
    value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' },
    conditionOp: CONDITION_OP.TRUTHY,
    firstChild: 0,
    childCount: 0,
    firstElseChild: 0,
    elseChildCount: 0,
    ...fields,
  });
  const pkg = {
    vars: [],
    rooms: [{
      id: 'room002_exit', label: 'Exit Room', color: 0, events: [{ name: 'on_use', item: 'brass_key', firstCommand: 0, commandCount: 6 }],
      entities: [
        { kind: 1, id: 'exit_lamp', name: 'Exit Lamp', x: 236, y: 64, w: 24, h: 48, visible: true, events: [] },
        { kind: 1, id: 'demo_badge_dim', name: 'Dim Demo Badge', x: 210, y: 118, w: 18, h: 12, visible: true, events: [] },
        { kind: 1, id: 'demo_badge_lit', name: 'Lit Demo Badge', x: 210, y: 118, w: 18, h: 12, visible: false, events: [] },
        { kind: 1, id: 'brass_key', name: 'Brass Key', x: 132, y: 148, w: 18, h: 10, visible: false, events: [] },
      ],
    }],
    commands: [
      command(COMMAND_KIND.SET_OBJECT_SPRITE, { a: 'exit_lamp', b: 'rooms/room002_exit/objects/exit_lamp_lit.bmp' }),
      command(COMMAND_KIND.HIDE, { a: 'demo_badge_dim' }),
      command(COMMAND_KIND.SHOW, { a: 'demo_badge_lit' }),
      command(COMMAND_KIND.DROP, { a: 'spare_key', b: 'brass_key' }),
      command(COMMAND_KIND.CONSUME, { a: 'brass_key' }),
      command(COMMAND_KIND.SAY, { a: 'player', b: 'Feature demo complete.' }),
    ],
    globalEvents: [],
  };
  const state = new GameState(pkg, { currentRoom: 'room002_exit', inventory: ['brass_key', 'spare_key'] });
  const effects = new ScriptVm(pkg, state).runEvent(pkg.rooms[0].events[0]);

  assert.deepEqual(effects.map(e => e.type), ['setObjectSprite', 'hide', 'show', 'drop', 'consume', 'say']);
  assert.deepEqual(state.inventory, []);
  assert.deepEqual(state.object('exit_lamp').sprite, 'rooms/room002_exit/objects/exit_lamp_lit.bmp');
  assert.equal(state.object('demo_badge_dim').visible, false);
  assert.equal(state.object('demo_badge_lit').visible, true);
  assert.equal(state.object('brass_key').visible, true);
});


test('browser demo starts from INI metadata and keeps game folder JavaScript-free', async () => {
  const [html, gameIni, dataLoader, mainScript] = await Promise.all([
    readFile('index.html', 'utf8'),
    readFile('game/game.ini', 'utf8'),
    readFile('src/js/browserGameData.js', 'utf8'),
    readFile('src/js/browserGame.js', 'utf8'),
  ]);

  assert.match(html, /<script type="module" src="src\/js\/browserGame\.js"><\/script>/);
  assert.match(gameIni, /^first_room=room000_start$/m);
  assert.match(dataLoader, /fetchText\('game\.ini'\)/);
  assert.match(dataLoader, /parseYaatRoom/);
  assert.match(dataLoader, /transparentColor: colorValue/);
  assert.match(dataLoader, /playerTransparentColor: colorValue/);
  assert.match(mainScript, /const \{verbs, verbLabels, inventoryDefs, rooms, firstRoom, initialVars, playerTransparentColor\}=await loadGameData\(\);/);
  assert.match(mainScript, /go\(firstRoom,160,100\); loop\(\);/);
  assert.match(mainScript, /if\(rooms\[state\.room\]\.hideUI\) return;/);
  assert.match(mainScript, /transparentImg\(path,transparentColor\)/);
  assert.match(mainScript, /transparentImg\(path,playerTransparentColor\)/);
});

test('JavaScript state hides player-owned UI with hide_player', () => {
  const pkg = {
    vars: [],
    rooms: [{ id: 'menu', label: 'Menu', color: 0, events: [{ name: 'enter', item: '', firstCommand: 0, commandCount: 1 }], entities: [] }],
    commands: [
      { kind: COMMAND_KIND.SET_PLAYER_VISIBLE, a: '', b: '', stringId: '', boolValue: 0, intValue: 0, value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' }, conditionOp: 0, firstChild: 0, childCount: 0, firstElseChild: 0, elseChildCount: 0 },
    ],
    globalEvents: [],
  };
  const state = new GameState(pkg);
  const effects = new ScriptVm(pkg, state).runEvent(pkg.rooms[0].events[0]);

  assert.deepEqual(effects, [{ type: 'setPlayerVisible', visible: false }]);
  assert.equal(state.playerLayerVisible(), false);
  assert.equal(state.verbUiVisible(), false);
});

test('JavaScript room-change regions wait for player motion to complete', () => {
  const pkg = {
    vars: [{ name: 'door_locked', value: { kind: VALUE_KIND.BOOL, bool: false, int: 0, string: '' } }],
    rooms: [{ id: 'room001_intro', label: 'Intro', color: 0, events: [], entities: [] }],
    commands: [],
    globalEvents: [],
  };
  const state = new GameState(pkg, { currentRoom: 'room001_intro', playerX: 286, playerY: 159 });
  const exit = { action: 'change_room', targetRoom: 'room002_exit', requiredFlag: 'door_locked', requiredFlagValue: false, targetX: 26, targetY: 180, targetDirection: 'right' };

  state.setPlayerTarget(120, 159);
  assert.equal(state.tryEnterRoomChangeRegion(exit), false);
  assert.equal(state.currentRoom, 'room001_intro');

  state.player.x = 120;
  assert.equal(state.tryEnterRoomChangeRegion(exit), true);
  assert.equal(state.currentRoom, 'room002_exit');
  assert.deepEqual(state.player, { x: 26, y: 180, visible: true, facing: 'right' });
  assert.deepEqual(state.motion, { targetX: 26, targetY: 180 });
  assert.equal(state.playerIdleAnimation(), 'idle_right');
});

test('JavaScript room entry metadata sets player position and facing', () => {
  const pkg = { vars: [], rooms: [{ id: 'start', label: 'Start', color: 0, events: [], entities: [], entryX: 40, entryY: 150, entryDirection: 'left' }], commands: [], globalEvents: [] };
  const state = new GameState(pkg, { currentRoom: 'start', applyRoomEntry: true });

  assert.deepEqual(state.player, { x: 40, y: 150, visible: true, facing: 'left' });
  assert.deepEqual(state.motion, { targetX: 40, targetY: 150 });
  assert.equal(state.playerIdleAnimation(), 'idle_left');
});
