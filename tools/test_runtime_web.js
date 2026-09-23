'use strict';
const fs = require('fs'), vm = require('vm'), assert = require('assert');
const elements = new Map();
function element(id) {if(!elements.has(id)) elements.set(id, {textContent:'',getContext:()=>new Proxy({}, {get:()=>()=>{}})}); return elements.get(id);}
const code=fs.readFileSync('web/runtime.js','utf8').replace(/poll\(\);\s*$/, '');
const context=vm.createContext({document:{getElementById:element},setTimeout,clearTimeout,AbortController,
  Date,DataView,Uint8Array,TextDecoder,Map,console,fetch:()=>{throw Error('unset');}});
vm.runInContext(code,context);
(async()=>{
  // Headers succeed immediately, response body never completes until aborted.
  context.fetch=async(_path,opts)=>({ok:true,json:()=>new Promise((_,reject)=>opts.signal.addEventListener('abort',()=>reject(Error('body_aborted'))))});
  await assert.rejects(vm.runInContext("request('/status.json',{timeout:25})",context),/body_aborted/);
  assert.strictEqual(vm.runInContext("crc32(new Uint8Array([49,50,51,52,53,54,55,56,57]))",context),0xcbf43926);
  await vm.runInContext('refresh()',context); // real production refresh cleanup
  assert.strictEqual(vm.runInContext('refreshInFlight',context),false);
  context.fetch=async()=>({ok:true,json:async()=>({})});
  assert.deepStrictEqual(JSON.parse(JSON.stringify(await vm.runInContext("request('/status.json')",context))),{});
  await vm.runInContext('refresh()',context);
  assert.strictEqual(vm.runInContext('latest',context),null);
  assert.strictEqual(element('start').disabled,true);
  assert.match(element('connection').textContent,/状態データ/);
  const valid={state:'READY_TO_MEASURE',export_phase:'empty',running:false,ready:true,downloadable:false,
    controller_fresh:true,command:{pending:false,completed:0,submitted:0},foot:{available:true,zero_ready:true},upright:{}};
  context.fetch=async()=>({ok:true,json:async()=>valid});
  await vm.runInContext('refresh()',context);
  assert.strictEqual(element('connection').textContent,'接続中');
  assert.strictEqual(element('start').disabled,false);
  context.fetch=async()=>({ok:true,json:async()=>({})});
  await vm.runInContext('refresh()',context);
  assert.strictEqual(vm.runInContext('latest.state',context),'READY_TO_MEASURE');
  assert.strictEqual(element('start').disabled,true); // Last good state is evidence, not fresh authority.
  // Even a rendering exception outside refresh's normal recovery must schedule
  // the next poll. The next valid response restores the display without reload.
  let scheduled=0;
  context.setTimeout=(fn,ms)=>{if(ms===800){++scheduled;return 0;} return setTimeout(fn,ms);};
  vm.runInContext('const realControls=controls; controls=()=>{throw Error("render failure")}',context);
  await vm.runInContext('poll()',context); assert.strictEqual(scheduled,1);
  vm.runInContext('controls=realControls',context);
  context.fetch=async()=>({ok:true,json:async()=>valid});
  await vm.runInContext('poll()',context); assert.strictEqual(scheduled,2);
  assert.strictEqual(element('connection').textContent,'接続中');
  const packet=new Uint8Array(19),view=new DataView(packet.buffer);packet.set([1,2,3],16);
  context.packet=packet;view.setUint32(0,0x31484346,true);view.setUint32(4,4096,true);view.setUint32(8,3,true);
  view.setUint32(12,vm.runInContext('crc32(packet.subarray(16))',context),true);
  assert.strictEqual(vm.runInContext('validateChunk(packet,4096,3).length',context),3);
  assert.throws(()=>vm.runInContext('validateChunk(packet,0,3)',context),/不一致/);
  packet[18]^=1;assert.throws(()=>vm.runInContext('validateChunk(packet,4096,3)',context),/CRC/);
  console.log('body timeout, malformed status/render recovery, stale authority, offsets and corrupt chunks PASS');
})().catch(e=>{console.error(e);process.exitCode=1;});
