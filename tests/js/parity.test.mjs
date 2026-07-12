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
  assert.deepEqual(state.player, { x: 50, y: 60, visible: true });
});
