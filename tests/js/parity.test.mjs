import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { mkdirSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { decodeYaatBytecode, GameState, ScriptVm } from '../../src/js/index.js';
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
